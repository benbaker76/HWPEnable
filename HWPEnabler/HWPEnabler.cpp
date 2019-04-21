#include "HWPEnabler.h"

#define super IOService
OSDefineMetaClassAndStructors (HWPEnabler, IOService)

bool HWPEnabler::init (OSDictionary *dict)
{
    bool res = super::init (dict);
    
#ifdef  DEBUG
    IOLog ("HWPEnabler: Initializing...\n");
#endif
    
    if (rdmsr64(MSR_IA32_PM_ENABLE) == 0)
    {
    }
    
    return(res);
}

void HWPEnabler::free ()
{
#ifdef  DEBUG
    IOLog ("HWPEnabler: Freeing...\n");
#endif
    
    super::free ();
}

bool HWPEnabler::start (IOService *provider)
{
    bool res = super::start (provider);
	
	if (!res)
		return false;
	
	PMinit();
	
	provider->joinPMtree(this);
	registerPowerDriver(this, gPowerStates, kNumPowerStates);
    
    registerService();
    IOLog ("HWPEnabler: Starting...\n");
    
    OSBoolean * key_enableHWP = OSDynamicCast(OSBoolean, getProperty("HWPenable"));
    OSBoolean * key_setdefaultsHWP = OSDynamicCast(OSBoolean, getProperty("HWPSetDeaults"));
    OSString * key_defaultsHWP = OSDynamicCast(OSString, getProperty("HWPDeaultVal"));
    
    if (key_enableHWP)
    {
        if (rdmsr64(MSR_IA32_PM_ENABLE) != 1)
        {
            wrmsr64(MSR_IA32_PM_ENABLE, 0x1);
        }
    }
    if (key_setdefaultsHWP)
    {
        if (!key_defaultsHWP->isEqualTo("0"))
        {
            wrmsr64(MSR_IA32_HWP_REQUEST, hex2int(key_defaultsHWP->getCStringNoCopy()));
        }else{
            wrmsr64(MSR_IA32_HWP_REQUEST, 0x0000000080002301);
        }
    }
    
    mPrefPanelMemoryBufSize = 4096;
    
    return(res);
}

void HWPEnabler::stop (IOService *provider)
{
    IOLog ("HWPEnabler: Stopping...\n");
	
	PMstop();
    
    super::stop (provider);
}

uint64_t HWPEnabler::a_rdmsr (uint32_t msr)
{
    return(rdmsr64(msr));
}

void HWPEnabler::a_wrmsr(uint32_t msr, uint64_t value)
{
    wrmsr64(msr, value);
}

/*
uint64_t HWPEnabler::a_rdcap (void)
{
    return(rdmsr64(MSR_IA32_HWP_CAPABILITIES));
}

void HWPEnabler::a_wrreq(uint64_t msr_val)
{
    wrmsr64(MSR_IA32_HWP_REQUEST, msr_val);
}

uint64_t HWPEnabler::a_rdreq(void)
{
    return(rdmsr64(MSR_IA32_HWP_REQUEST));
}

uint64_t HWPEnabler::a_rdstat(void)
{
    return(rdmsr64(MSR_IA32_HWP_STATUS));
}
*/

int HWPEnabler::power(unsigned value, long long power) {
    unsigned result = 1;
    
    for (unsigned i = 0; i < power; ++i) {
        result *= value;
    }
    
    return ((int)result);
}

long long HWPEnabler::hex2int(const char *s)
{
    char charset[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    char altcharset[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    long long i = (int)strlen(s), len = i, num = 0, j = 0;
    while (i >= 0) {
        for (j = 0; j < 16; j++) {
            if ((charset[j] == s[i]) || (altcharset[j] == s[i])) {
                num += j * (int)power(16, len-i-1);
                break;
            }
        }
        i--;
    }
    return num;
}

IOReturn HWPEnabler::runAction(UInt32 action, UInt32 *outSize, void **outData, void *extraArg)
{
#ifdef  DEBUG
    IOLog("Action: %x", (unsigned int)action);
#endif
    
    return kIOReturnSuccess;
}

IOReturn HWPEnabler::newUserClient( task_t owningTask, void * securityID, UInt32 type, IOUserClient ** handler )
{
    IOReturn ioReturn = kIOReturnSuccess;
    HWPEnablerUserClient *client = NULL;
    
    if (mClientCount > MAXUSERS)
    {
        IOLog("HWPEnabler: Client already created, not deleted\n");
        
        return(kIOReturnError);
    }
    
    client = (HWPEnablerUserClient *)HWPEnablerUserClient::withTask(owningTask);
    if (client == NULL) {
        ioReturn = kIOReturnNoResources;
        
        IOLog("HWPEnabler::newUserClient: Can't create user client\n");
    }
    
    if (ioReturn == kIOReturnSuccess) {
        // Start the client so it can accept requests.
        client->attach(this);
        if (client->start(this) == false) {
            ioReturn = kIOReturnError;
            IOLog("HWPEnabler::newUserClient: Can't start user client\n");
        }
    }
    
    if (ioReturn != kIOReturnSuccess && client != NULL) {
        IOLog("HWPEnabler: newUserClient error\n");
        client->detach(this);
        client->release();
    } else {
        mClientPtr[mClientCount] = client;
        
        *handler = client;
        
        client->set_Q_Size(type);
        mClientCount++;
    }
    
#ifdef  DEBUG
    IOLog("HWPEnabler: newUserClient() client = %p\n", mClientPtr[mClientCount]);
#endif
    
    return (ioReturn);
}

void HWPEnabler::setErr( bool set )
{
    mErrFlag = set;
}

void HWPEnabler::closeChild(HWPEnablerUserClient *ptr)
{
    UInt8 i, idx;
    idx = 0;
    
    if (mClientCount == 0)
    {
        IOLog("No clients available to close");
        return;
    }
    
#ifdef  DEBUG
    IOLog("Closing: %p\n",ptr);
    
    for(i=0;i<mClientCount;i++)
    {
        IOLog("userclient ref: %d %p\n", i, mClientPtr[i]);
    }
#endif
    
    for(i=0;i<mClientCount;i++)
    {
        if (mClientPtr[i] == ptr)
        {
            mClientCount--;
            mClientPtr[i] = NULL;
            idx = i;
            i = mClientCount+1;
        }
    }
    
    for(i=idx;i<mClientCount;i++)
    {
        mClientPtr[i] = mClientPtr[i+1];
    }
    mClientPtr[mClientCount+1] = NULL;
}

IOReturn HWPEnabler::setPowerState(unsigned long state, IOService *whatDevice) {
#ifdef  DEBUG
	IOLog("HWPEnabler: Changing power state to %lu\n", state);
#endif
	
	if (state == kOffPowerState)
	{
#ifdef  DEBUG
		IOLog("HWPEnabler: Going to Sleep!\n");
#endif
	}
	else
	{
#ifdef  DEBUG
		IOLog("HWPEnabler: Waking from Sleep!\n");
#endif
		if (rdmsr64(MSR_IA32_PM_ENABLE) != 1)
		{
			wrmsr64(MSR_IA32_PM_ENABLE, 0x1);
		}
	}
	
	return kIOPMAckImplied;
}


#undef  super
#define super IOUserClient
OSDefineMetaClassAndStructors(HWPEnablerUserClient, IOUserClient);

const HWPEnablerUserClient *HWPEnablerUserClient::withTask(task_t owningTask)
{
    HWPEnablerUserClient *client;
    
    client = new HWPEnablerUserClient;
    if (client != NULL)
    {
        if (client->init() == false)
        {
            client->release();
            client = NULL;
        }
    }
    if (client != NULL)
    {
        client->fTask = owningTask;
    }
    return (client);
}


bool HWPEnablerUserClient::set_Q_Size(UInt32 capacity)
{
    bool res;
    
    if (capacity == 0)
    {
        return true;
    }
    
#ifdef  DEBUG
    IOLog("HWPEnabler: Reseting size of data queue, all data in queue is lost");
#endif
    
    myLogQueue->release();
    
    //Get mem for new queue of calcuated size
    myLogQueue = new IODataQueue;
    if (myLogQueue == 0)
    {
        IOLog("HWPEnabler: [ERR]  Failed to allocate memory for buffer\n");
        Q_Err++;
        return false;
    }
    
    res = myLogQueue->initWithCapacity(capacity);
    if (res == false)
    {
        IOLog("HWPEnabler: [ERR] Could not initWithEntries\n");
        Q_Err++;
        return false;
    }
    
    return true;
}

void HWPEnablerUserClient::messageHandler(UInt32 type, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

bool HWPEnablerUserClient::initWithTask(task_t owningTask, void *securityID, UInt32 type,
                                    OSDictionary *properties)
{
    //	logMsg("HWPEnablerUserClient[%p]::initWithTask(%ld)\n", this, type);
    
    return super::initWithTask(owningTask, securityID, type, properties);
}

bool HWPEnablerUserClient::start(IOService *provider)
{
    //	logMsg("HWPEnablerUserClient[%p]::start\n", this);
    
    if (!super::start(provider))
        return false;
    
    mDevice = OSDynamicCast(HWPEnabler, provider);
    mDevice->retain();
    
    return true;
}

bool HWPEnablerUserClient::willTerminate(IOService *provider, IOOptionBits options)
{
    //	logMsg("HWPEnablerUserClient[%p]::willTerminate\n", this);
    
    return super::willTerminate(provider, options);
}

bool HWPEnablerUserClient::didTerminate(IOService *provider, IOOptionBits options, bool *defer)
{
    //	logMsg("HWPEnablerUserClient[%p]::didTerminate\n", this);
    
    // if defer is true, stop will not be called on the user client
    *defer = false;
    
    return super::didTerminate(provider, options, defer);
}

bool HWPEnablerUserClient::terminate(IOOptionBits options)
{
    //	logMsg("HWPEnablerUserClient[%p]::terminate\n", this);
    
    return super::terminate(options);
}

// clientClose is called when the user process calls IOServiceClose
IOReturn HWPEnablerUserClient::clientClose()
{
    //    logMsg("HWPEnablerUserClient[%p]::clientClose\n", this);
    
    if (mDevice != NULL)
    {
        mDevice->closeChild(this);
    }
    
    if (!isInactive())
        terminate();
    
    return kIOReturnSuccess;
}

// clientDied is called when the user process terminates unexpectedly, the default
// implementation simply calls clientClose
IOReturn HWPEnablerUserClient::clientDied()
{
    //	logMsg("HWPEnablerUserClient[%p]::clientDied\n", this);
    
    return clientClose();
}

void HWPEnablerUserClient::free(void)
{
    //	logMsg("HWPEnablerUserClient[%p]::free\n", this);
    
    mDevice->release();
    
    super::free();
}

// stop will be called during the termination process, and should free all resources
// associated with this client
void HWPEnablerUserClient::stop(IOService *provider)
{
    //	logMsg("HWPEnablerUserClient[%p]::stop\n", this);
    
    super::stop(provider);
}

// getTargetAndMethodForIndex looks up the external methods - supply a description of the parameters
// available to be called
IOExternalMethod * HWPEnablerUserClient::getTargetAndMethodForIndex(IOService **target, UInt32 index)
{
    static const IOExternalMethod methodDescs[3] = {
        { NULL, (IOMethod) &HWPEnablerUserClient::actionMethodRDMSR, kIOUCStructIStructO,
            kIOUCVariableStructureSize, kIOUCVariableStructureSize },
        { NULL, (IOMethod) &HWPEnablerUserClient::actionMethodWRMSR, kIOUCStructIStructO,
            kIOUCVariableStructureSize, kIOUCVariableStructureSize },
    };
    
    *target = this;
    if (index < 3)
        return (IOExternalMethod *) (methodDescs + index);
    
    return NULL;
}

IOReturn HWPEnablerUserClient::actionMethodRDMSR(UInt32 *dataIn, UInt32 *dataOut, IOByteCount inputSize,
                                             IOByteCount *outputSize)
{
    inout * msrdata = (inout *)dataIn;
    inout * msroutdata = (inout *)dataOut;
    
#ifdef  DEBUG
    IOLog("HWPEnabler RDCAP called\n");
#endif
    
    if (!dataIn)
    {
        return kIOReturnUnsupported;
    }
    
    //logMsg("HWPEnablerUserClient[%p]::actionMethod(%ld, %ld)\n", this, inputSize, *outputSize);
    
    msrdata->param = mDevice->a_rdmsr(msrdata->msr);
    
#ifdef  DEBUG
    IOLog("HWPEnabler: RDMSR %X : 0x%llX\n", msrdata->msr, msrdata->param);
#endif
    
    if (!dataOut)
    {
        return kIOReturnUnsupported;
    }
    
    msroutdata->param = msrdata->param;
    
    return kIOReturnSuccess;
}

IOReturn HWPEnablerUserClient::actionMethodWRMSR(UInt32 *dataIn, UInt32 *dataOut, IOByteCount inputSize,
                                             IOByteCount *outputSize)
{
    inout * msrdata = (inout *)dataIn;
    
#ifdef  DEBUG
    IOLog("HWPEnabler WRMSR called\n");
#endif
    
    if (!dataIn)
    {
        return kIOReturnUnsupported;
    }
    
    //logMsg("HWPEnablerUserClient[%p]::actionMethod(%ld, %ld)\n", this, inputSize, *outputSize);
    
    mDevice->a_wrmsr(msrdata->msr, msrdata->param);
    
#ifdef  DEBUG
    IOLog("HWPEnabler: WRMSR 0x%llX to %X\n", msrdata->param, msrdata->msr);
#endif
    
    return kIOReturnSuccess;
}

IOReturn HWPEnablerUserClient::clientMemoryForType(UInt32 type, IOOptionBits *options,
                                               IOMemoryDescriptor **memory)
{
    IOBufferMemoryDescriptor *memDesc;
    char *msgBuffer;
    
    *options = 0;
    *memory = NULL;
    
    memDesc = IOBufferMemoryDescriptor::withOptions(kIOMemoryKernelUserShared, mDevice->mPrefPanelMemoryBufSize);
    
    if (!memDesc)
    {
        return kIOReturnUnsupported;
    }
    
    msgBuffer = (char *) memDesc->getBytesNoCopy();
    bcopy(mDevice->mPrefPanelMemoryBuf, msgBuffer, mDevice->mPrefPanelMemoryBufSize);
    *memory = memDesc; // automatically released after memory is mapped into task
    
    return(kIOReturnSuccess);
}
