//
//  main.m
//  HWPEnabler
//
//  Created by Andy Vandijck on 12/09/13.
//  Copyright (c) 2013 Andy Vandijck. All rights reserved.
//

#import <Foundation/Foundation.h>

#define kHWPEnablerClassName "HWPEnabler"

#define MSR_IA32_PM_ENABLE          0x770
#define MSR_IA32_HWP_CAPABILITIES   0x771
#define MSR_IA32_HWP_REQUEST_PKG    0x772
#define MSR_IA32_HWP_INTERRUPT      0x773
#define MSR_IA32_HWP_REQUEST        0x774
#define MSR_IA32_HWP_STATUS         0x777

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

io_service_t getService() {
	io_service_t service = 0;
	mach_port_t masterPort;
	io_iterator_t iter;
	kern_return_t ret;
	io_string_t path;
	
	ret = IOMasterPort(MACH_PORT_NULL, &masterPort);
	if (ret != KERN_SUCCESS) {
		printf("Can't get masterport\n");
		goto failure;
	}
	
	ret = IOServiceGetMatchingServices(masterPort, IOServiceMatching(kHWPEnablerClassName), &iter);
	if (ret != KERN_SUCCESS) {
		printf("HWPEnabler is not running\n");
		goto failure;
	}
	
	service = IOIteratorNext(iter);
	IOObjectRelease(iter);
	
	ret = IORegistryEntryGetPath(service, kIOServicePlane, path);
	if (ret != KERN_SUCCESS) {
		printf("Can't get registry-entry path\n");
		goto failure;
	}
	
failure:
	return service;
}

void usage(const char *name)
{
    printf("HWPEnabler V1.0\n");
    printf("Copyright (C) 2016 GoodWin\n");
    printf("Usage:\n");
    printf("Write HWP Request: %s wr_req <HEX_VAL>\n", name);
    printf("Read HWP Request: %s rd_req \n", name);
    printf("Read HWP Capabilities: %s rd_cap \n", name);
    printf("Read HWP Status: %s rd_stat \n", name);
    printf("<HEX_VAL> is hex without 0x\n");
}

long long hex2int(const char *s)
{
    char charset[16] = "0123456789abcdef";
    char altcharset[16] = "0123456789ABCDEF";
    long long i = (int)strlen(s), len = i, num = 0, j = 0;
    while (i >= 0) {
        for (j = 0; j < 16; j++) {
            if ((charset[j] == s[i]) || (altcharset[j] == s[i])) {
                num += j * (int)pow(16, len-i-1);
                break;
            }
        }
        i--;
    }
    return num;
}

int main(int argc, const char * argv[])
{
    char * parameter;
    char * msr;
    char * regvalue;
    io_service_t service = getService();
	if(!service)
		goto failure;
	kern_return_t ret;
	io_connect_t connect = 0;
	ret = IOServiceOpen(service, mach_task_self(), 0, &connect);
	if (ret != KERN_SUCCESS)
    {
        printf("Couldn't open IO Service\n");
    }

    inout in;
    inout out;
    size_t outsize = sizeof(out);

    if (argc >= 1)
    {
        parameter = (char *)argv[1];
    } else {
        usage(argv[0]);

        return(1);
    }

    if (argc >= 2)
    {
        msr = (char *)argv[2];
    } else {
        usage(argv[0]);
        
        return(1);
    }

    if (!strncmp(parameter, "rd_cap", 6))
    {
        in.msr = MSR_IA32_HWP_CAPABILITIES;
        in.action = HWPEnablerActionMethodRDMSR;
        in.param = 0;

#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4
        ret = IOConnectMethodStructureIStructureO( connect, HWPEnablerActionMethodRDCAP,
											  sizeof(in),			/* structureInputSize */
											  &outsize,    /* structureOutputSize */
											  &in,        /* inputStructure */
											  &out);       /* ouputStructure */
#else
        ret = IOConnectCallStructMethod(connect,
									HWPEnablerActionMethodRDMSR,
									&in,
									sizeof(in),
									&out,
									&outsize
									);
#endif

        if (ret != KERN_SUCCESS)
        {
            printf("Can't connect to StructMethod to send commands\n");
        }

        printf("RDCAP %x returns value 0x%llx\n", (unsigned int)in.msr, (unsigned long long)out.param);
    } else if (!strncmp(parameter, "wr_req", 6)) {
        if (argc < 3)
        {
            usage(argv[0]);
            
            return(1);
        }

//        regvalue = (char *)argv[3];

        in.msr = MSR_IA32_HWP_REQUEST;
        in.action = HWPEnablerActionMethodWRMSR;
        in.param = hex2int(msr);

        printf("WRREQ %x with value 0x%llx\n", (unsigned int)in.msr, (unsigned long long)in.param);

#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4
        ret = IOConnectMethodStructureIStructureO( connect, HWPEnablerActionMethodWRMSR,
                                                  sizeof(in),			/* structureInputSize */
                                                  &outsize,    /* structureOutputSize */
                                                  &in,        /* inputStructure */
                                                  &out);       /* ouputStructure */
#else
        ret = IOConnectCallStructMethod(connect,
                                        HWPEnablerActionMethodWRMSR,
                                        &in,
                                        sizeof(in),
                                        &out,
                                        &outsize
                                        );
#endif        

        if (ret != KERN_SUCCESS)
        {
            printf("Can't connect to StructMethod to send commands\n");
        }
    } else if (!strncmp(parameter, "rd_req", 6))
    {
        in.msr = MSR_IA32_HWP_REQUEST;
        in.action = HWPEnablerActionMethodRDMSR;
        in.param = 0;
        
#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4
        ret = IOConnectMethodStructureIStructureO( connect, HWPEnablerActionMethodRDMSR,
                                                  sizeof(in),			/* structureInputSize */
                                                  &outsize,    /* structureOutputSize */
                                                  &in,        /* inputStructure */
                                                  &out);       /* ouputStructure */
#else
        ret = IOConnectCallStructMethod(connect,
                                        HWPEnablerActionMethodRDMSR,
                                        &in,
                                        sizeof(in),
                                        &out,
                                        &outsize
                                        );
#endif
        
        if (ret != KERN_SUCCESS)
        {
            printf("Can't connect to StructMethod to send commands\n");
        }
        
        printf("RDREQ %x returns value 0x%llx\n", (unsigned int)in.msr, (unsigned long long)out.param);
    } else if (!strncmp(parameter, "wr_msr", 6)) {
        if (argc < 3)
        {
            usage(argv[0]);
            
            return(1);
        }

        regvalue = (char *)argv[3];

        in.msr = (UInt32)hex2int(msr);
        in.action = HWPEnablerActionMethodWRMSR;
        in.param = hex2int(regvalue);

        printf("WRMSR %x with value 0x%llx\n", (unsigned int)in.msr, (unsigned long long)in.param);

#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4
        ret = IOConnectMethodStructureIStructureO( connect, HWPEnablerActionMethodWRMSR,
                                                  sizeof(in),			/* structureInputSize */
                                                  &outsize,    /* structureOutputSize */
                                                  &in,        /* inputStructure */
                                                  &out);       /* ouputStructure */
#else
        ret = IOConnectCallStructMethod(connect,
                                        HWPEnablerActionMethodWRMSR,
                                        &in,
                                        sizeof(in),
                                        &out,
                                        &outsize
                                        );
#endif        

        if (ret != KERN_SUCCESS)
        {
            printf("Can't connect to StructMethod to send commands\n");
        }
    } else if (!strncmp(parameter, "rd_msr", 6))
    {
        in.msr = (UInt32)hex2int(msr);
        in.action = HWPEnablerActionMethodRDMSR;
        in.param = 0;
        
#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4
        ret = IOConnectMethodStructureIStructureO( connect, HWPEnablerActionMethodRDMSR,
                                                  sizeof(in),			/* structureInputSize */
                                                  &outsize,    /* structureOutputSize */
                                                  &in,        /* inputStructure */
                                                  &out);       /* ouputStructure */
#else
        ret = IOConnectCallStructMethod(connect,
                                        HWPEnablerActionMethodRDMSR,
                                        &in,
                                        sizeof(in),
                                        &out,
                                        &outsize
                                        );
#endif
        
        if (ret != KERN_SUCCESS)
        {
            printf("Can't connect to StructMethod to send commands\n");
        }
        
        printf("RDMSR %x returns value 0x%llx\n", (unsigned int)in.msr, (unsigned long long)out.param);
    } else if (!strncmp(parameter, "rd_stat", 6))
    {
        in.msr = (UInt32)hex2int(msr);
        in.action = HWPEnablerActionMethodRDMSR;
        in.param = 0;
        
#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4
        ret = IOConnectMethodStructureIStructureO( connect, HWPEnablerActionMethodRDMSR,
                                                  sizeof(in),			/* structureInputSize */
                                                  &outsize,    /* structureOutputSize */
                                                  &in,        /* inputStructure */
                                                  &out);       /* ouputStructure */
#else
        ret = IOConnectCallStructMethod(connect,
                                        HWPEnablerActionMethodRDMSR,
                                        &in,
                                        sizeof(in),
                                        &out,
                                        &outsize
                                        );
#endif
        
        if (ret != KERN_SUCCESS)
        {
            printf("Can't connect to StructMethod to send commands\n");
        }
        
        printf("RDSTAT %x returns value 0x%llx\n", (unsigned int)in.msr, (unsigned long long)out.param);
    } else {
        usage(argv[0]);

        return(1);
    }

    failure:
        if(connect)
        {
            ret = IOServiceClose(connect);
            if (ret != KERN_SUCCESS)
            {
                printf("IOServiceClose failed\n");
            }
        }
        
        if(service)
            IOObjectRelease(service);

    return 0;
}

