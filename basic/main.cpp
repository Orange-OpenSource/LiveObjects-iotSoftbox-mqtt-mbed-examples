/*
 * Copyright (C) 2016 Orange
 *
 * This software is distributed under the terms and conditions of the 'BSD-3-Clause'
 * license which can be found in the file 'LICENSE.txt' in this package distribution
 * or at 'https://opensource.org/licenses/BSD-3-Clause'.
 */


/**
  * @file  main.cpp
  * @brief A simple user application using all available LiveObjects iotsotbox-mqtt features
  */

#include "liveobjects_sample.h"

#include <stdint.h>
#include <string.h>

#include "mbed-trace/mbed_trace.h"

#include "mbed.h"
#include "rtos.h"

static const char* appv_version = "MBED SAMPLE V02.01";

#if 0
#define DBG_DFT_MAIN_LOG_LEVEL    3
#define DBG_DFT_LOMC_LOG_LEVEL    1
#define DBG_DFT_MBED_LOG_LEVEL   TRACE_ACTIVE_LEVEL_ALL
#else
#define DBG_DFT_MAIN_LOG_LEVEL    0
#define DBG_DFT_LOMC_LOG_LEVEL    0
#define DBG_DFT_MBED_LOG_LEVEL   TRACE_ACTIVE_LEVEL_INFO
#endif

// Two application threads:

/// A very simple application thread
Thread      appli_thread;

/// Thread to manage the input from console
Thread      console_thread;

#if MBED_CONF_APP_NETWORK_INTERFACE == ETHERNET
#include "EthernetInterface.h"
EthernetInterface eth;
#endif

NetworkInterface* appv_network_interface = NULL;
const char*       appv_network_id = NULL;

Serial            output(USBTX, USBRX);
osThreadId        appv_thread_id;
uint8_t           appv_log_level = DBG_DFT_MAIN_LOG_LEVEL;

// ==========================================================
// Green LED blinking according to the LiveObjects Connection state.

/// Application state according to the connectivity.
enum app_state_enum {
    APP_STATE_UNKNOWN = 0,        ///< Unknown state
    APP_STATE_INIT,               ///< Initilalization
    APP_STATE_NETWORK_READY,      ///< Ethernet Network is ready. Device has IP address.
    APP_STATE_CONNECTING,         ///< Connecting to the LiveObjects platform
    APP_STATE_CONNECTED,          ///< Connected to the LiveObjects platform
    APP_STATE_DOWN                ///< the LiveObjects thread is down (or stopped)
}  appv_state = APP_STATE_INIT;


// ----------------------------------------------------------
// Status indication

/// Green Status LED
DigitalOut  app_led_status(LED2, 0);

/// Ticker to blink the Status LED
Ticker      app_led_ticker;

/**
 * Called periodically by the Status LED ticker.
 */
void app_led_blinky(void)
{
static uint32_t  appv_led_cnt = 0;
static int32_t   appv_led_state = -1;

    if (appv_state == APP_STATE_CONNECTED) {
        if (appv_led_state != 4) {
            appv_led_state = 4;
            appv_led_cnt = 0;
            app_led_status = 1;
        }
        else {
            if ((++appv_led_cnt%6) == 0) app_led_status = !app_led_status;
        }
    }
#if 1
    else if (appv_state == APP_STATE_CONNECTING) {
        if (appv_led_state != 3) {
            appv_led_state = 3;
            appv_led_cnt = 0;
            app_led_status = 0;
        }
        app_led_status = !app_led_status;
    }
    else if (appv_state == APP_STATE_NETWORK_READY) {
        if (appv_led_state != 1) {
            appv_led_state = 1;
            appv_led_cnt = 0;
            app_led_status = 0;
        }
        if ((++appv_led_cnt%4) == 0)  app_led_status = !app_led_status;
    }
    else {
        if (appv_led_state != 2) {
            appv_led_state = 2;
            appv_led_cnt = 0;
            app_led_status = 0;
        }
        if ((++appv_led_cnt%2) == 0) app_led_status = !app_led_status;
    }
#endif
}


// ==========================================================
// mbed_trace
//
Mutex trace_mutex;
extern "C"  void trace_mutex_wait(void) {
    trace_mutex.lock();
}

extern "C"  void trace_mutex_release(void) {
    trace_mutex.unlock();
}

// debug printf function
extern "C"  unsigned int rt_time_get (void);

extern "C"  void trace_printer(const char* str) {
    unsigned int clk = rt_time_get();
    printf("%8u %s\r\n", clk, str);
}

extern "C"  char* trace_prefix(size_t sz) {
    return (char*)" ** ";
}

// ----------------------------------------------------------
//
static void app_trace_setup(void)
{
    mbed_trace_init();

    mbed_trace_print_function_set(trace_printer);
    mbed_trace_prefix_function_set(trace_prefix);

    mbed_trace_mutex_wait_function_set(trace_mutex_wait);
    mbed_trace_mutex_release_function_set(trace_mutex_release);

    uint8_t trace_msk = mbed_trace_config_get();

    output.printf("trace_msk = x%X\r\n", trace_msk);

    // TRACE_ACTIVE_LEVEL_INFO , TRACE_ACTIVE_LEVEL_ALL
    // TRACE_MODE_COLOR  or TRACE_MODE_PLAIN
    // TRACE_CARRIAGE_RETURN
    mbed_trace_config_set(DBG_DFT_MBED_LOG_LEVEL|TRACE_MODE_COLOR);
}


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
// - supported configuration parameters
// - supported commands
// - resources declaration (firmware, text file, etc.)



// ----------------------------------------------------------
// STATUS data
//

int32_t appv_status_counter = 0;
char    appv_status_message[150] = "READY";

/// Set of status
LiveObjectsD_Data_t appv_set_status[] = {
    { LOD_TYPE_STRING_C, "sample_version" ,  (void*)appv_version },
    { LOD_TYPE_INT32,    "sample_counter" ,  &appv_status_counter},
    { LOD_TYPE_STRING_C, "sample_message" ,  appv_status_message}
};
#define SET_STATUS_NB (sizeof(appv_set_status) / sizeof(LiveObjectsD_Data_t))

int     appv_hdl_status = -1;


// ----------------------------------------------------------
// 'COLLECTED DATA'
//
#define STREAM_PREFIX   0


uint8_t  appv_measures_enabled = 1;

int32_t  appv_measures_temp_grad = -1;
float    appv_measures_volt_grad = -0.2;

// contains a counter incremented after each data sent
uint32_t appv_measures_counter = 0;

// contains the temperature level
int32_t  appv_measures_temp = 20;

// contains the battery level
float    appv_measures_volt = 5.0;

/// Set of Collected data (published on a data stream)
LiveObjectsD_Data_t appv_set_measures[] = {
    { LOD_TYPE_UINT32, "counter" ,        &appv_measures_counter},
    { LOD_TYPE_INT32,  "temperature" ,    &appv_measures_temp},
    { LOD_TYPE_FLOAT,  "battery_level" ,  &appv_measures_volt }
};
#define SET_MEASURES_NB (sizeof(appv_set_measures) / sizeof(LiveObjectsD_Data_t))

int      appv_hdl_data = -1;


// ----------------------------------------------------------
// CONFIGURATION data
//
volatile uint32_t appv_cfg_timeout = 10;

// a structure containg various kind of parameters (char[], int and float)
struct conf_s {
    char     name[20];
    int32_t  threshold;
    float    gain;
} appv_conf = {
        "TICTAC",
        -3,
        1.05
};

// definition of identifer for each kind of parameters
#define PARM_IDX_NAME        1
#define PARM_IDX_TIMEOUT     2
#define PARM_IDX_THRESHOLD   3
#define PARM_IDX_GAIN        4

/// Set of configuration parameters
LiveObjectsD_Param_t appv_set_param[] = {
    { PARM_IDX_NAME,      { LOD_TYPE_STRING_C, "name"    ,   appv_conf.name } },
    { PARM_IDX_TIMEOUT,   { LOD_TYPE_UINT32,   "timeout" ,   (void*)&appv_cfg_timeout } },
    { PARM_IDX_THRESHOLD, { LOD_TYPE_INT32,    "threshold" , &appv_conf.threshold } },
    { PARM_IDX_GAIN,      { LOD_TYPE_FLOAT,    "gain" ,      &appv_conf.gain } }
};
#define SET_PARAM_NB (sizeof(appv_set_param) / sizeof(LiveObjectsD_Param_t))


// ----------------------------------------------------------
// COMMANDS
// Digital output to change the status of the RED LED
DigitalOut  app_led_user(LED1, 0);

/// counter used to postpone the LED command response
static int cmd_cnt = 0;

#define CMD_IDX_RESET       1
#define CMD_IDX_LED         2

/// set of commands
LiveObjectsD_Command_t appv_set_commands[] = {
    { CMD_IDX_RESET, "RESET" , 0},
    { CMD_IDX_LED,   "LED"  , 0}
};
#define SET_COMMANDS_NB (sizeof(appv_set_commands) / sizeof(LiveObjectsD_Command_t))



// ----------------------------------------------------------
// RESOURCE data
//
char appv_rsc_image[5*1024] = "";


char appv_rv_message[10] = "01.00";
char appv_rv_image[10]   = "01.00";

#define RSC_IDX_MESSAGE     1
#define RSC_IDX_IMAGE       2

/// Set of resources
LiveObjectsD_Resource_t appv_set_resources[] = {
    { RSC_IDX_MESSAGE, "message", appv_rv_message, sizeof(appv_rv_message)-1 }, // resource used to update appv_status_message
    { RSC_IDX_IMAGE,   "image",   appv_rv_image,  sizeof(appv_rv_image)-1 }
};
#define SET_RESOURCES_NB (sizeof(appv_set_resources) / sizeof(LiveObjectsD_Resource_t))

// variables used to process the current resource transfer
uint32_t appv_rsc_size = 0;
uint32_t appv_rsc_offset = 0;





// ==========================================================
// IotSoftbox-mqtt callback functions

// ----------------------------------------------------------
// CONFIGURATION PARAMETERS Callback function
// Check value in range, and copy string parameters


/// Called (by the LiveObjects thread) to update configuration parameters
extern "C" int main_cb_param_udp(const LiveObjectsD_Param_t* param_ptr, const void* value, int len)
{
    if (param_ptr == NULL) {
        output.printf("UPDATE  ERROR - invalid parameter x%p\r\n",param_ptr);
        return -1;
    }
    output.printf("UPDATE user_ref=%d %s ....\r\n", param_ptr->parm_uref, param_ptr->parm_data.data_name);
    switch(param_ptr->parm_uref) {
    case PARM_IDX_NAME:
        {
            output.printf("update name = %.*s\r\n", len , (const char*) value);
            if ((len > 0) && (len < (sizeof(appv_conf.name)-1))) {
                // Only c-string parameter must be updated by the user application (to check the string length)
                strncpy(appv_conf.name, (const char*) value, len);
                appv_conf.name[len] = 0;
                return 0; // OK.
            }
        }
        break;

    case PARM_IDX_TIMEOUT:
        {
            uint32_t timeout = *((const uint32_t*)value);
            output.printf("update timeout = %"PRIu32"\r\n", timeout);
            if ((timeout > 0) && (timeout <= 120) && (timeout != appv_cfg_timeout)) {
                return 0; // primitive parameter is updated by library
            }
        }
        break;
    case PARM_IDX_THRESHOLD:
        {
            int32_t threshold = *((const int32_t*)value);
            output.printf("update threshold = %"PRIi32"\r\n", threshold);
            if ((threshold >= -10) && (threshold <= 10) && (threshold != appv_conf.threshold)) {
                return 0; // primitive parameter is updated by library
            }
        }
        break;
    case PARM_IDX_GAIN:
        {
            float gain = *((const float*)value);
            output.printf("update gain = %f\r\n", gain);
            if ((gain > 0.0) && (gain < 10.0) && (gain != appv_conf.gain)) {
                return 0; // primitive parameter is updated by library
            }
        }
        break;
    }
    output.printf("ERROR to update param[%d] %s !!!\r\n", param_ptr->parm_uref, param_ptr->parm_data.data_name);
    return -1;
}

// ----------------------------------------------------------
// RESOURCE Callback Functions

/**
 * Called (by the LiveObjects thread) to notify either,
 *  - state = 0  : the begin of resource request
 *  - state = 1  : the end without error
 *  - state != 1 : the end with an error
 */
extern "C"  LiveObjectsD_ResourceRespCode_t main_cb_rsc_ntfy (
        uint8_t state, const LiveObjectsD_Resource_t* rsc_ptr,
        const char* version_old, const char* version_new, uint32_t size)
{
    LiveObjectsD_ResourceRespCode_t ret = RSC_RSP_OK; // OK to update the resource

    output.printf("*** rsc_ntfy: ...\r\n");

    if ((rsc_ptr) && (rsc_ptr->rsc_uref > 0) && (rsc_ptr->rsc_uref <= SET_RESOURCES_NB)) {
        output.printf("***   user ref     = %d\r\n", rsc_ptr->rsc_uref);
        output.printf("***   name         = %s\r\n", rsc_ptr->rsc_name);
        output.printf("***   version_old  = %s\r\n", version_old);
        output.printf("***   version_new  = %s\r\n", version_new);
        output.printf("***   size         = %u\r\n", size);
        if (state) {
            if (state == 1) { // Completed without error
                output.printf("***   state        = COMPLETED without error\r\n");
                // Update version
                output.printf(" ===> UPDATE - version %s to %s\r\n", rsc_ptr->rsc_version_ptr, version_new);
                strncpy((char*)rsc_ptr->rsc_version_ptr, version_new, rsc_ptr->rsc_version_sz);

               if (rsc_ptr->rsc_uref == RSC_IDX_IMAGE) {
                    trace_mutex_wait();
                    output.printf("\r\n\r\n");
                    output.printf(appv_rsc_image);
                    output.printf("\r\n\r\n");
                    trace_mutex_release();
                }
            }
            else {
                output.printf("***   state        = COMPLETED with error !!!!\r\n");
                // Roll back ?
            }
            appv_rsc_offset = 0;
            appv_rsc_size = 0;

            // Push Status (message has been updated or not)
            LiveObjectsClient_PushStatus( appv_hdl_status );
        }
        else {
            appv_rsc_offset = 0;
            ret = RSC_RSP_ERR_NOT_AUTHORIZED;
            switch (rsc_ptr->rsc_uref ) {
                case RSC_IDX_MESSAGE:
                    if (size < (sizeof(appv_status_message)-1)) {
                        ret = RSC_RSP_OK;
                    }
                    break;
                case RSC_IDX_IMAGE:
                    if (size < (sizeof(appv_rsc_image)-1)) {
                        ret = RSC_RSP_OK;
                    }
                   break;
            }
            if (ret == RSC_RSP_OK) {
                appv_rsc_size = size;
                output.printf("***   state        = START - ACCEPTED\r\n");;
            }
            else {
                 appv_rsc_size = 0;
                 output.printf("***   state        = START - REFUSED\r\n");
            }
        }
    }
    else {
        output.printf("***  UNKNOWN USER REF (x%p %d)  in state=%d\r\n", rsc_ptr, rsc_ptr->rsc_uref, state);
        ret = RSC_RSP_ERR_INVALID_RESOURCE;
    }
    return ret;
}

/**
 * Called (by the LiveObjects thread) to request the user
 * to read data from current resource transfer.
 */
extern "C"  int main_cb_rsc_data (const LiveObjectsD_Resource_t* rsc_ptr, uint32_t offset)
{
    int ret;

    if (appv_log_level > 1)  output.printf("*** rsc_data: rsc[%d]='%s' offset=%u - data ready ...\r\n", rsc_ptr->rsc_uref, rsc_ptr->rsc_name, offset);

    if (rsc_ptr->rsc_uref == RSC_IDX_MESSAGE) {
        char buf[40];
        if (offset > (sizeof(appv_status_message)-1)) {
            output.printf("*** rsc_data: rsc[%d]='%s' offset=%u > %d - OUT OF ARRAY\r\n",
                    rsc_ptr->rsc_uref, rsc_ptr->rsc_name, offset, sizeof(appv_status_message)-1);
            return -1;
        }
        ret = LiveObjectsClient_RscGetChunck(rsc_ptr, buf, sizeof(buf)-1);
        if (ret > 0) {
            if ((offset+ret) > (sizeof(appv_status_message)-1)) {
                output.printf("*** rsc_data: rsc[%d]='%s' offset=%u - read=%d => %d > %d - OUT OF ARRAY\r\n",
                        rsc_ptr->rsc_uref, rsc_ptr->rsc_name, offset, ret, offset + ret, sizeof(appv_status_message)-1);
                return -1;
            }
            appv_rsc_offset += ret;
            memcpy(&appv_status_message[offset], buf, ret);
            appv_status_message[offset+ret] = 0;
            output.printf("*** rsc_data: rsc[%d]='%s' offset=%u - read=%d/%d '%s'\r\n",
                    rsc_ptr->rsc_uref, rsc_ptr->rsc_name, offset, ret, sizeof(buf)-1, appv_status_message);
        }
    }
    else if (rsc_ptr->rsc_uref == RSC_IDX_IMAGE) {
        if (offset > (sizeof(appv_rsc_image)-1)) {
             output.printf("*** rsc_data: rsc[%d]='%s' offset=%u > %d - OUT OF ARRAY\r\n",
                     rsc_ptr->rsc_uref, rsc_ptr->rsc_name, offset, sizeof(appv_rsc_image)-1);
             return -1;
         }
        int data_len = sizeof(appv_rsc_image) - offset - 1;
        ret = LiveObjectsClient_RscGetChunck(rsc_ptr, &appv_rsc_image[offset], data_len);
        if (ret > 0) {
            if ((offset+ret) > (sizeof(appv_rsc_image)-1)) {
                 output.printf("*** rsc_data: rsc[%d]='%s' offset=%u - read=%d => %d > %d - OUT OF ARRAY\r\n",
                         rsc_ptr->rsc_uref, rsc_ptr->rsc_name, offset, ret, offset + ret, sizeof(appv_rsc_image)-1);
                 return -1;
             }
            appv_rsc_offset += ret;
            if (appv_log_level > 0)
                output.printf("*** rsc_data: rsc[%d]='%s' offset=%u - read=%d/%d - %u/%u\r\n",
                        rsc_ptr->rsc_uref, rsc_ptr->rsc_name, offset, ret, data_len, appv_rsc_offset, appv_rsc_size);
        }
        else {
            output.printf("*** rsc_data: rsc[%d]='%s' offset=%u - read error (%d) - %u/%u\r\n",
                    rsc_ptr->rsc_uref, rsc_ptr->rsc_name, offset, ret, appv_rsc_offset, appv_rsc_size);
        }
    }
    else {
        ret = -1;
    }

    return ret;
}

// ----------------------------------------------------------
// COMMAND Callback Functions

static int main_cmd_doSystemReset(LiveObjectsD_CommandRequestBlock_t* pCmdReqBlk);
static int main_cmd_doLED(LiveObjectsD_CommandRequestBlock_t* pCmdReqBlk);

/// Called (by the LiveObjects thread) to perform an 'attached/registered' command
extern "C" int main_cb_command(LiveObjectsD_CommandRequestBlock_t* pCmdReqBlk)
{
    int ret;
    const LiveObjectsD_Command_t*  cmd_ptr;

    if ((pCmdReqBlk == NULL) || (pCmdReqBlk->hd.cmd_ptr == NULL) || (pCmdReqBlk->hd.cmd_cid == 0) ) {
        output.printf("**** COMMAND : ERROR, Invalid parameter\r\n");
        return -1;
    }

    cmd_ptr = pCmdReqBlk->hd.cmd_ptr;
    output.printf("**** COMMAND %d %s - cid=%d\r\n", cmd_ptr->cmd_uref, cmd_ptr->cmd_name, pCmdReqBlk->hd.cmd_cid);
    {
        int i;
        output.printf("**** ARGS %d : \r\n", pCmdReqBlk->hd.cmd_args_nb);
        for (i=0; i < pCmdReqBlk->hd.cmd_args_nb; i++) {
            output.printf("**** ARG [%d] (%d) :  %s %s\r\n", i, pCmdReqBlk->args_array[i].arg_type,
                     pCmdReqBlk->args_array[i].arg_name, pCmdReqBlk->args_array[i].arg_value);
        }
    }

    switch(cmd_ptr->cmd_uref) {
    case CMD_IDX_RESET: // RESET
        output.printf("main_callbackCommand: command[%d] %s\r\n", cmd_ptr->cmd_uref, cmd_ptr->cmd_name);
        ret = main_cmd_doSystemReset(pCmdReqBlk);
        break;

    case CMD_IDX_LED: // LED
        output.printf("main_callbackCommand: command[%d] %s\r\n", cmd_ptr->cmd_uref, cmd_ptr->cmd_name);
        ret = main_cmd_doLED(pCmdReqBlk);
        break;
    default :
        output.printf("main_callbackCommand: ERROR, unknown command %d\r\n", cmd_ptr->cmd_uref);
        ret = -4;
    }
    return ret;
}

// ----------------------------------------------------------
/// Board reset
static void main_SystemReset(void)
{
    NVIC_SystemReset();
}

// ----------------------------------------------------------
/// do a RESET command
static int main_cmd_doSystemReset(LiveObjectsD_CommandRequestBlock_t* pCmdReqBlk)
{
    if (LiveObjectsClient_Stop()) {
        output.printf("doSystemReset: not running => wait 500 ms and reset ...\r\n");
        wait_ms(200);
        main_SystemReset();
    }
    return 1; // response = OK
}


// ----------------------------------------------------------
/// Delayed Response to the LED command
static void main_cmd_delayed_resp_LED ()
{
    if (cmd_cnt > 0) {
        LiveObjectsD_Command_t* cmd_ptr = &appv_set_commands[CMD_IDX_LED-1];
        if (appv_log_level > 1) output.printf("main_cmd_delayed_resp_LED: cnt=%d cid=%d\r\n", cmd_cnt, cmd_ptr->cmd_cid);
        if ((--cmd_cnt <= 0) && (cmd_ptr->cmd_cid)) {
            uint32_t code = 200;
            char msg [] = "USER LED TEST = OK";
            LiveObjectsD_Data_t cmd_resp[] = {
                { LOD_TYPE_UINT32,    "code" ,  &code},
                { LOD_TYPE_STRING_C,  "msg" ,   msg}
            };
            // switch off the Red LED
            app_led_user = 1;

            output.printf("\r\n*** main_cmd_resp_LED: RESPONSE ...\r\n");
            LiveObjectsClient_CommandResponse(cmd_ptr->cmd_cid, cmd_resp, 2);

            cmd_ptr->cmd_cid = 0;
            cmd_cnt = 0;
        }
    }
}

// ----------------------------------------------------------
/// do a LED command
static int main_cmd_doLED(LiveObjectsD_CommandRequestBlock_t* pCmdReqBlk)
{
    int ret;
    // switch on the Red LED
    app_led_user = 0;

    if (pCmdReqBlk->hd.cmd_args_nb == 0) {
        output.printf("main_cmd_doLED: No ARG\r\n");
        app_led_user = ! app_led_user;
        ret = 1; // Response OK
        cmd_cnt = 0;
     }
     else {
         int i;
         int cnt;
         for (i=0; i<pCmdReqBlk->hd.cmd_args_nb;i++ ) {
             if ( strncasecmp("ticks", pCmdReqBlk->args_array[i].arg_name, 5)
                     && pCmdReqBlk->args_array[i].arg_type == 0 ) {
                 cnt = atoi(pCmdReqBlk->args_array[i].arg_value);
                 if ((cnt >= 0) || (cnt <= 3)) {
                     cmd_cnt = cnt;
                 }
                 else cmd_cnt = 0;
                 output.printf("main_cmd_doLED: cmd_cnt = %di (%d)\r\n", cmd_cnt, cnt);
             }
         }
     }

    if (cmd_cnt == 0) {
        app_led_user = ! app_led_user;
        ret = 1; // Response OK
    }
    else {
        LiveObjectsD_Command_t* cmd_ptr = (LiveObjectsD_Command_t*)(pCmdReqBlk->hd.cmd_ptr);
        app_led_user = 0;
        output.printf("main_cmd_doLED: ccid=%d (%d)\r\n", pCmdReqBlk->hd.cmd_cid, cmd_ptr->cmd_cid);
        cmd_ptr->cmd_cid = pCmdReqBlk->hd.cmd_cid;
        ret = 0; // pending
    }
    return ret; // response = OK
}

// ==========================================================


/// Counter ticker to increment periodically a application counter
Ticker      counter_ticker;

/// Called periodically by the ticker
void counter_inc(void) {
    appv_measures_counter ++;
}


/**
 * Application thread:
 *   - Send the delayed LED command response (if pending)
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

        // Process the LED command response if pending.
        main_cmd_delayed_resp_LED();

        // Simulate measures : Voltage and Temperature ...

        if (appv_measures_volt <= 0.0)       appv_measures_volt_grad = 0.2;
        else if (appv_measures_volt >= 5.0)  appv_measures_volt_grad = -0.3;

        if (appv_measures_temp <= -3)        appv_measures_temp_grad = 1;
         else if (appv_measures_temp >= 20)  appv_measures_temp_grad = -1;

        appv_measures_volt += appv_measures_volt_grad;
        appv_measures_temp += appv_measures_temp_grad;

        if (appv_log_level > 2) output.printf("thread_appli: %u - %s PUBLISH - volt=%2.2f temp=%d\r\n", loop_cnt,
                appv_measures_enabled ? "DATA" : "NO",
                appv_measures_volt, appv_measures_temp);

        if (appv_measures_enabled) {
            LiveObjectsClient_PushData( appv_hdl_data );
        }

    }
}


// ==========================================================
//
const char* appv_help =
        " R     : system reset\r\n"
        " d     : push 'collected data'\r\n"
        " s     : push 'status'\r\n"
        " c     : push 'config. parameters'\r\n"
        " r     : push 'resources'\r\n"
        " p     : publish  STATUS message\r\n"
        " e     : Enable/Disable the data publishing\r\n"
        " X|M|n : Enable/Disable the dump of published message (X to enable also hexa dump)\r\n"
        " D|I|W : set debug log level (mbed_trace)\r\n"
        " 0-9   : set appli log level \r\n"
        ;
/**
 * Console  thread
 *   - wait for an input character from terminal
 *   - perform  the requested operation
 */
void thread_input_cons(void) {
    int c;
    osThreadId id = osThreadGetId();
    output.printf("thread_input_cons: running %x\r\n", id);
    while(1) {
        osDelay(500);
#if 1
        if (output.readable())  {
            // WARNING: getc is a blocking call, and can also block the printf !!!
            c = output.getc();
            if (c) {
                if (appv_log_level > 1) output.printf("thread_input_cons: input x%x\r\n", c);
                if (c == 'h') {
                    output.printf(appv_help);
                }
                else if ( c == 'e' ) {
                    appv_measures_enabled = (appv_measures_enabled) ? 0 : 1;
                    output.printf(">>> %s data publish\r\n", appv_measures_enabled ? "Enable" : "Disable");
                }
                else if ( c == 'p' ) {
                    char msg[40];
                    appv_status_counter++;
                    snprintf(msg,sizeof(msg)-1,"{\"info\":{\"counter\": %u}}", appv_status_counter);
                    output.printf(">>> Publish t=/dev/info msg=%s ...\r\n", msg);
                    LiveObjectsClient_Publish("dev/info", msg); // Publish JSON message built by user
                }
                else if ( c == 'd' ) {
                    appv_measures_counter++;
                    output.printf(">>> Push DATA - cnt %u\r\n", appv_measures_counter);
                    LiveObjectsClient_PushData( appv_hdl_data );
                }
                else if (c == 's' ) {
                    appv_status_counter ++;
                    output.printf(">>> Push STATUS - counter %u\r\n", appv_status_counter);
                    LiveObjectsClient_PushStatus( appv_hdl_status );
                }
                else if (c == 'c' ) {
                    output.printf(">>> Push CONFIG\r\n");
                    LiveObjectsClient_PushCfgParams();
                }
                else if (c == 'r' ) {
                    output.printf(">>> Push RESOURCES\r\n");
                    LiveObjectsClient_PushResources();
                }
                else if (c == 'X') {
                     output.printf(">>> Enable Message Dump (+ hexa dump)\r\n");
                     LiveObjectsClient_SetDbgLevel(3);
                 }
                else if (c == 'M') {
                    output.printf(">>> Enable Message Dump\r\n");
                    LiveObjectsClient_SetDbgLevel(1);
                }
                else if (c == 'm') {
                    output.printf(">>> Disable Message Dump\r\n");
                LiveObjectsClient_SetDbgLevel(0);
                }
                else if (c == 'D') {
                    output.printf(">>> Set trace level : DEBUG\r\n");
                    mbed_trace_config_set(TRACE_ACTIVE_LEVEL_ALL|TRACE_MODE_COLOR);
                }
                else if (c == 'I') {
                    output.printf(">>> Set trace level : INFO\r\n");
                    mbed_trace_config_set(TRACE_ACTIVE_LEVEL_INFO|TRACE_MODE_COLOR);
                }
                else if (c == 'W') {
                    output.printf(">>> Set trace level : WARNING\r\n");
                    mbed_trace_config_set(TRACE_ACTIVE_LEVEL_WARN|TRACE_MODE_COLOR);
                }
                else if ((c >= '0')  && (c <= '9')) {
                    output.printf(">>> Set appli dbg level : %d -> %c\r\n", appv_log_level, c);
                    appv_log_level = c - '0';
                }
                else if (c == 'R') {
                    main_SystemReset();
                }
            }
        }
#endif
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

    app_led_status = 1;
    app_led_user = 1;

    appv_state = APP_STATE_INIT;

    // Led blink at 250 ms
    app_led_ticker.attach_us(app_led_blinky, 250000);

    // Keep track of the main thread
    appv_thread_id = osThreadGetId();

    // Sets the console baud-rate
    output.baud(9600);

    output.printf("\r\n\r\n");
    output.printf("Starting LiveObject Client Example %s  (tid=x%p) ...\r\n", appv_version, appv_thread_id);

    app_trace_setup();

    // Start program only if LiveObjet Apikey parameter is well defined
    if (LiveObjectsClient_CheckApiKey(LOC_CLIENT_DEV_API_KEY)) {
        output.printf("apikey not set, '%s'\r\n", LOC_CLIENT_DEV_API_KEY);
        output.printf("\n\rExiting application ....\r\n");
        return -1;
    }

    // Setup the Ethernet Interface
    if (0 == app_net_init()) {
        output.printf("\n\rConnected to Network successfully\r\n");

        LiveObjectsClient_SetDbgLevel(DBG_DFT_LOMC_LOG_LEVEL);

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
            // Attach my local RESOURCES to the LiveObjects Client instance
            // ------------------------------------------------------------
            ret = LiveObjectsClient_AttachResources(appv_set_resources, SET_RESOURCES_NB, main_cb_rsc_ntfy, main_cb_rsc_data);
            if (ret) output.printf(" !!! ERROR (%d) to attach RESOURCES !\r\n", ret);

            // Attach my local Configuration Parameters to the LiveObjects Client instance
            // ----------------------------------------------------------------------------
            ret = LiveObjectsClient_AttachCfgParams(appv_set_param, SET_PARAM_NB, main_cb_param_udp);
            if (ret) output.printf(" !!! ERROR (%d) to attach Config Parameters !\r\n", ret);

            // Attach a set of commands to the LiveObjects Client instance
            // -----------------------------------------------------------
            ret = LiveObjectsClient_AttachCommands(appv_set_commands, SET_COMMANDS_NB, main_cb_command);
            if (ret < 0) output.printf(" !!! ERROR (%d) to attach a set of commands !\r\n", ret);


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
#if 1
            wait_ms(1000);

            // Start the counter ticker at 500 ms
            counter_ticker.attach_us(counter_inc, 500000);  // appv_measures_counter

            output.printf(" ---- Start thread : thread_input_cons ....\r\n");
            ret = console_thread.start(thread_input_cons);
            if (ret) {
                output.printf("\n\r !!!! ERROR(%d) to start thread : thread_input_cons\r\n", ret);
            }

            output.printf(" ---- Start thread : thread_appli ....\r\n");
            ret = appli_thread.start(thread_appli);
            if (ret) {
                output.printf("\n\r !!!! ERROR(%d) to start thread : thread_appli\r\n", ret);
            }

            wait_ms(1000);
#endif

            // Enable the receipt of commands
            ret = LiveObjectsClient_ControlCommands(true);
            if (ret < 0) output.printf(" !!! ERROR (%d) to enable the receipt of commands !\r\n", ret);

            // Enable the receipt of resource update requests
            ret = LiveObjectsClient_ControlResources(true);
            if (ret < 0) output.printf(" !!! ERROR (%d) to enable the receipt of resource update request !\r\n", ret);

#if 0
            // ==================================
            // Run LiveObjects Client in main thread (forever ...)
            output.printf("\n\rLiveObjectsClient_Run ...\r\n");

            LiveObjectsClient_Run(appli_client_state_cb);

            output.printf("\n\rLiveObjectsClient_Run Failed !!!!\r\n");
#else
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
#endif

        }
    } else {
        output.printf("\n\rConnection to Network Failed ! \r\n");
    }

    output.printf("\n\rExiting application ....\r\n");

    app_led_ticker.detach();

    app_led_status = 0;

    app_led_user = 0;

    main_SystemReset();
}
