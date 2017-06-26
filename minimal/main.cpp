/*
 * Copyright (C) 2016 Orange
 *
 * This software is distributed under the terms and conditions of the 'BSD-3-Clause'
 * license which can be found in the file 'LICENSE.txt' in this package distribution
 * or at 'https://opensource.org/licenses/BSD-3-Clause'.
 */

/**
  * @file  main.cpp
  * @brief A simple user application using "Data Values" LiveObjects iotsotbox-mqtt feature
  */

#include "liveobjects_sample.h"

#include <stdint.h>
#include <string.h>

#include "mbed-trace/mbed_trace.h"

#include "mbed.h"
#include "rtos.h"

static const char* appv_version = "MBED STM32 sample V04.01";

#define DBG_DFT_MAIN_LOG_LEVEL    0
#define DBG_DFT_LOMC_MSG_DUMP     0 //0x0B
#define DBG_DFT_MBED_LOG_LEVEL    TRACE_ACTIVE_LEVEL_INFO


// Two application threads:

/// A very simple application thread
Thread      appli_thread;


#if MBED_CONF_APP_NETWORK_INTERFACE == ETHERNET
#include "EthernetInterface.h"
EthernetInterface eth;
#endif

NetworkInterface* appv_network_interface = NULL;
const char*       appv_network_id = NULL;

Serial            output(USBTX, USBRX);
osThreadId        appv_thread_id;
uint8_t           appv_log_level = DBG_DFT_MAIN_LOG_LEVEL;


// ----------------------------------------------------------
// CONFIGURATION data
//
volatile uint32_t appv_cfg_timeout = 10;

/// Application state according to the connectivity.
enum app_state_enum {
    APP_STATE_UNKNOWN = 0,        ///< Unknown state
    APP_STATE_INIT,               ///< Initilalization
    APP_STATE_NETWORK_READY,      ///< Ethernet Network is ready. Device has IP address.
    APP_STATE_CONNECTING,         ///< Connecting to the LiveObjects platform
    APP_STATE_CONNECTED,          ///< Connected to the LiveObjects platform
    APP_STATE_DOWN                ///< the LiveObjects thread is down (or stopped)
}  appv_state = APP_STATE_INIT;


// ==========================================================
// Network Initialization

#if MBED_CONF_APP_NETWORK_INTERFACE == ETHERNET
// ----------------------------------------------------------
/// Network Initialization
static int app_net_init(void)
{
    int rc;
    output.printf("Using Ethernet - Get Dynamic IP Address ...\r\n");

    appv_network_interface = NULL;
    rc = eth.connect();
    if (rc != 0) {
        output.printf("\n\rConnection to Network Failed, rc=%d\r\n", rc);
        return rc;
    }
    const char *ip_addr = eth.get_ip_address();
    if (ip_addr) {
        output.printf("Dynamic IP address %s\r\n", ip_addr);
    } else {
        output.printf("No IP address\r\n");
        return -1;
    }

    appv_network_id = eth.get_mac_address();
    if (appv_network_id) {
        output.printf("MAC address %s\r\n", appv_network_id);
    }
    else {
        output.printf("ERROR: No MAC address !!!!!!\r\n");
    }

    appv_network_interface = &eth;

    return 0;
}
#endif


// ==========================================================
//
// Live Objects IoT Client object (using iotsoftbox-mqtt library)
//
// - status information at connection
// - collected data to send

// ----------------------------------------------------------
// STATUS data
//

int32_t appv_status_counter = 0;
char    appv_status_message[150] = "READY";

/// Set of status
LiveObjectsD_Data_t appv_set_status[] = {
    { LOD_TYPE_STRING_C, "sample_version" ,  (void*)appv_version, 1 },
    { LOD_TYPE_INT32,    "sample_counter" ,  &appv_status_counter, 1 },
    { LOD_TYPE_STRING_C, "sample_message" ,  appv_status_message, 1 }
};
#define SET_STATUS_NB (sizeof(appv_set_status) / sizeof(LiveObjectsD_Data_t))

int     appv_hdl_status = -1;


// ----------------------------------------------------------
// 'COLLECTED DATA'
//
#define STREAM_PREFIX   0


// contains a counter incremented after each data sent
uint32_t appv_measures_counter = 0;

// contains the battery level
float    appv_measures_light = 5.0;

/// Set of Collected data (published on a data stream)
LiveObjectsD_Data_t appv_set_measures[] = {
    { LOD_TYPE_FLOAT,  "light" ,  &appv_measures_light, 1 }
};
#define SET_MEASURES_NB (sizeof(appv_set_measures) / sizeof(LiveObjectsD_Data_t))

int      appv_hdl_data = -1;

 /**
 * Declaration of the pin to read
 */
AnalogIn a0(A0);

/**
 * function used to read the light from the pin A0
 */
int read_light_data() {

	uint8_t ret = 0;

	
	float voltage_read=a0.read();
	if (voltage_read!=0)
	{
		float voltage_in=3.3;
		float resistor=1000;
		float light_res=voltage_in*resistor/voltage_read-resistor;		
		light_res=light_res/1000;
		printf("measured resistor: %lf\n",light_res);
		
		appv_measures_light=500/light_res;
	}
	else{
		printf("error reading resistor value\n");
		ret=1;
	}
	return ret;
}

/**
 * Application thread:
 *   - Read the data
 */
void thread_appli(void) {
    uint32_t loop_cnt = 0;
    osThreadId id = osThreadGetId();
    output.printf("thread_appli: running %x\r\n", id);

    while(1) {
        uint32_t delay = appv_cfg_timeout*1000;  // set to milliseconds
        if (delay == 0)          delay = 1000;   // min. 1 seconds
        else if (delay > 120000) delay = 120000; // max. 2 minutes

        wait_ms(delay);

        ++loop_cnt;
        if (appv_log_level > 1) output.printf("thread_appli: %u - period= %u ms\r\n", loop_cnt, delay);


        if (appv_log_level > 2) output.printf("thread_appli: %u - PUBLISH - light=%2.5f temp=%d\r\n", loop_cnt,  appv_measures_light);

        if (!read_light_data()) {
            LiveObjectsClient_PushData( appv_hdl_data );
        }

    }
}


// ==========================================================
//

// ----------------------------------------------------------
/// Called (by LiveOjbects thread) to notify a connectivity state change
static void appli_client_state_cb(LiveObjectsD_State_t state)
{
    enum app_state_enum new_state = APP_STATE_UNKNOWN;
    switch (state) {
    case CSTATE_CONNECTING:    new_state = APP_STATE_CONNECTING; break;
    case CSTATE_CONNECTED:     new_state = APP_STATE_CONNECTED; break;
    case CSTATE_DISCONNECTED:  new_state = APP_STATE_NETWORK_READY; break;
    case CSTATE_DOWN:          new_state = APP_STATE_DOWN; break;
   }
    output.printf("\n\rLIVEOBJECTS CLIENT STATE CHANGE (%d) :  %d -> %d \r\n", state, appv_state, new_state);
    appv_state = new_state;

}

// ----------------------------------------------------------
/// Entry point to the program
int main() {
    int ret;
    
    appv_state = APP_STATE_INIT;

    // Keep track of the main thread
    appv_thread_id = osThreadGetId();

    // Sets the console baud-rate
    output.baud(9600);

    output.printf("\r\n\r\n");
    output.printf("Starting LiveObject Client Example %s  (tid=x%p) ...\r\n", appv_version, appv_thread_id);

    // Start program only if LiveObjet Apikey parameter is well defined
    if (LiveObjectsClient_CheckApiKey(LOC_CLIENT_DEV_API_KEY)) {
        output.printf("apikey not set, '%s'\r\n", LOC_CLIENT_DEV_API_KEY);
        output.printf("\n\rExiting application ....\r\n");
        return -1;
    }

    // Setup the Ethernet Interface
    if (0 == app_net_init()) {
        output.printf("\n\rConnected to Network successfully\r\n");

        LiveObjectsClient_SetDbgMsgDump(DBG_DFT_LOMC_MSG_DUMP);

        // Initialize the LiveObjects Client Context
        // -----------------------------------------
        output.printf("\n\rLiveObjectsClient_Init ...\r\n");
        ret = LiveObjectsClient_Init(appv_network_interface);
        if (ret) {
             output.printf("\n\rLiveObjectsClient_Init Failed !\r\n");
        }
        else {

#if LOC_CLIENT_USE_MAC_ADDR
            if (appv_network_id) {
                // Use the MAC address to set the Device Identifier
                // --------------------------------------------
                output.printf("\n\rLiveObjectsClient_SetDevId '%s' ...\r\n", appv_network_id);
                ret = LiveObjectsClient_SetDevId(appv_network_id);
                if (ret) {
                    output.printf("\n\rLiveObjectsClient_SetDevId Failed !\r\n");
                }
            }
#endif
           

            // Attach my local STATUS data to the LiveObjects Client instance
            // --------------------------------------------------------------
            appv_hdl_status = LiveObjectsClient_AttachStatus(appv_set_status, SET_STATUS_NB);
            if (appv_hdl_status) output.printf(" !!! ERROR (%d) to attach status !\r\n", appv_hdl_status);

            // Attach one set of collected data to the LiveObjects Client instance
            // --------------------------------------------------------------------
            appv_hdl_data = LiveObjectsClient_AttachData(STREAM_PREFIX, "LO_sample_measures", "mV1","\"Test\"", NULL, appv_set_measures, SET_MEASURES_NB);
            if (appv_hdl_data < 0) output.printf(" !!! ERROR (%d) to attach a collected data stream !\r\n", appv_hdl_data);

            // ==================================
            // User Application part.

            wait_ms(1000);

 
            output.printf(" ---- Start thread : thread_appli ....\r\n");
            ret = appli_thread.start(thread_appli);
            if (ret) {
                output.printf("\n\r !!!! ERROR(%d) to start thread : thread_appli\r\n", ret);
            }

            wait_ms(1000);


 
            // ==================================
             // Start a new sthread to run LiveObjects Client (forever ...)
            output.printf("\n\rLiveObjectsClient_ThreadStart ...\r\n");

            ret = LiveObjectsClient_ThreadStart(appli_client_state_cb);

            if (ret == 0) {
                while(LiveObjectsClient_ThreadState() >= 0) {
                    wait_ms(1000);
                }
            }
            output.printf("\n\rLiveObjectsClient_ThreadStart ERROR %d\r\n", ret);

        }
    } else {
        output.printf("\n\rConnection to Network Failed ! \r\n");
    }

    output.printf("\n\rExiting application ....\r\n");

}


