#include <mach/mach_types.h>
#include <mach/machine.h>
#include <pexpert/pexpert.h>
#include <string.h>
#include <libkern/libkern.h>
#include <i386/proc_reg.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IODataQueue.h>

#define BUFSIZE 	512 	//bytes
#define MAXENTRIES	500
#define MAXUSERS 	5

#define MSR_IA32_PM_ENABLE          0x770
#define MSR_IA32_HWP_CAPABILITIES   0x771
#define MSR_IA32_HWP_REQUEST_PKG    0x772
#define MSR_IA32_HWP_INTERRUPT      0x773
#define MSR_IA32_HWP_REQUEST        0x774
#define MSR_IA32_HWP_STATUS         0x777

#define kIOPMPowerOff               0

#define kMethodObjectUserClient ((IOService*) 0 )

enum {
    HWPEnablerActionMethodRDMSR = 0,
    HWPEnablerActionMethodWRMSR = 1,
    HWPEnablerNumMethods
};

typedef struct {
    UInt32 action;
    UInt32 msr;
    UInt64 param;
} inout;

enum
{
	kOffPowerState,
	kOnPowerState,
	kNumPowerStates
};

static IOPMPowerState gPowerStates[kNumPowerStates] =
{
	{kIOPMPowerStateVersion1, kIOPMPowerOff, kIOPMPowerOff, kIOPMPowerOff, 0, 0, 0, 0, 0, 0, 0, 0},	// kOffPowerState
	{kIOPMPowerStateVersion1, kIOPMPowerOn, kIOPMPowerOn, kIOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}		// kOnPowerState
};

class HWPEnablerUserClient;

class HWPEnabler : public IOService
{
    OSDeclareDefaultStructors(HWPEnabler)
public:
    virtual bool init(OSDictionary *dictionary = 0) override;
    virtual void free(void) override;
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
    virtual uint64_t a_rdmsr(uint32_t msr);
    virtual void a_wrmsr(uint32_t msr, uint64_t value);
    virtual IOReturn runAction(UInt32 action, UInt32 *outSize, void **outData, void *extraArg);
    
    virtual IOReturn newUserClient(task_t owningTask, void * securityID, UInt32 type, IOUserClient ** handler) override;
    virtual void setErr(bool set);
    virtual void closeChild(HWPEnablerUserClient *ptr);
	virtual IOReturn setPowerState(unsigned long state, IOService *whatDevice) override;
    
    size_t mPrefPanelMemoryBufSize;
    uint32_t mPrefPanelMemoryBuf[2];
    UInt16 mClientCount;
    bool mErrFlag;
    HWPEnablerUserClient *mClientPtr[MAXUSERS+1];
    virtual long long hex2int(const char *s);
    virtual int power(unsigned value, long long power);
};

class HWPEnablerUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(HWPEnablerUserClient);
    
private:
    HWPEnabler *mDevice;
    
public:
    void messageHandler(UInt32 type, const char *format, ...) __attribute__ ((format (printf, 3, 4)));
    
    static const HWPEnablerUserClient *withTask(task_t owningTask);
    
    virtual void free() override;
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
    
    virtual bool initWithTask(task_t owningTask, void *securityID, UInt32 type, OSDictionary *properties) override;
    virtual IOReturn clientClose() override;
    virtual IOReturn clientDied() override;
	virtual bool set_Q_Size(UInt32 capacity);
    
    virtual bool willTerminate(IOService *provider, IOOptionBits options) override;
    virtual bool didTerminate(IOService *provider, IOOptionBits options, bool *defer) override;;
    virtual bool terminate(IOOptionBits options = 0) override;
    
    virtual IOExternalMethod *getTargetAndMethodForIndex(IOService **targetP, UInt32 index) override;
    
    virtual IOReturn clientMemoryForType(UInt32 type, IOOptionBits *options, IOMemoryDescriptor **memory) override;
    
    virtual IOReturn actionMethodRDMSR(UInt32 *dataIn, UInt32 *dataOut, IOByteCount inputSize, IOByteCount *outputSize);
    virtual IOReturn actionMethodWRMSR(UInt32 *dataIn, UInt32 *dataOut, IOByteCount inputSize, IOByteCount *outputSize);

    task_t fTask;
    IODataQueue * myLogQueue;
    int Q_Err;
};
