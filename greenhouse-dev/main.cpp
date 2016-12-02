/*
 * Copyright (C) 2016 Orange
 *
 * This software is distributed under the terms and conditions of the 'BSD-3-Clause'
 * license which can be found in the file 'LICENSE.txt' in this package distribution
 * or at 'https://opensource.org/licenses/BSD-3-Clause'.
 */


/**
  * @file  main.c
  * @brief LiveOjects IoT Client Demonstrator.
  */

#include "liveobjects_demo.h"

#include <stdint.h>
#include <string.h>

#include "mbed-trace/mbed_trace.h"

#ifndef TRACE_GROUP
#define TRACE_GROUP "LOM"
#endif

#include "mbed.h"
#include "rtos.h"


static const char* appv_version = "MBED GREENHOUSE DEMO V02.01";

#if 0
#define DBG_DFT_MAIN_LOG_LEVEL    3
#define DBG_DFT_LOMC_LOG_LEVEL    1
#define DBG_DFT_MBED_LOG_LEVEL   TRACE_ACTIVE_LEVEL_ALL
#else
#define DBG_DFT_MAIN_LOG_LEVEL    0
#define DBG_DFT_LOMC_LOG_LEVEL    0
#define DBG_DFT_MBED_LOG_LEVEL   TRACE_ACTIVE_LEVEL_INFO
#endif

Serial            output(USBTX, USBRX);

// Two application threads :
Thread            appli_thread;     // a very simple application thread
Thread            console_thread;   // Thread to manage the input from console

#if MBED_CONF_APP_NETWORK_INTERFACE == ETHERNET
#include "EthernetInterface.h"
EthernetInterface eth;
#endif

NetworkInterface* appv_network_interface = NULL;
const char*       appv_network_id = NULL;

osThreadId        appv_thread_id;
uint8_t           appv_log_level = DBG_DFT_MAIN_LOG_LEVEL;

#define DATA_ENABLE_CTRL
#ifdef DATA_ENABLE_CTRL
uint8_t           appv_data_enabled = 1;
#endif


// ==========================================================
//
enum app_state_enum {
    APP_STATE_UNKNOWN = 0,
    APP_STATE_INIT,
    APP_STATE_NETWORK_READY,
    APP_STATE_CONNECTING,
    APP_STATE_CONNECTED
}  appv_state = APP_STATE_INIT;

// ----------------------------------------------------------
// Status indication

// There is a strange issue when several LEDs are used. Red is always on !!!
//DigitalOut  app_led_red(LED1, 0);     // Red LED
DigitalOut  app_led_green(LED2, 0);   // Green LED
//DigitalOut  app_led_blue(LED3, 0);    // Blue LED

Ticker      app_led_ticker;

void app_led_blinky(void)
{
static uint32_t  appv_led_cnt = 0;
static int32_t   appv_led_state = -1;

//#define APP_LED_BLINKY_PRT(state)  output.printf("\r\napp_led_blinky: " state "APP_STATE_CONNECTED(%d)\r\n\r\n", appv_state);
#define APP_LED_BLINKY_PRT(state)  ((void)0)

    if (appv_state == APP_STATE_CONNECTED) {
        //app_led_red = 0;
        if (appv_led_state != 4) {
            APP_LED_BLINKY_PRT("APP_STATE_CONNECTED");
             appv_led_state = 4;
            appv_led_cnt = 0;
            app_led_green = 1;
        }
        else {
            if ((++appv_led_cnt%6) == 0) app_led_green = !app_led_green;
        }
        //app_led_red = 0;
    }
#if 1
    else if (appv_state == APP_STATE_CONNECTING) {
        if (appv_led_state != 3) {
            APP_LED_BLINKY_PRT("APP_STATE_CONNECTING");
            appv_led_state = 3;
            appv_led_cnt = 0;
            app_led_green = 0;
        }
        app_led_green = !app_led_green;
        //app_led_red = !app_led_green;
    }
    else if (appv_state == APP_STATE_NETWORK_READY) {
        if (appv_led_state != 1) {
            APP_LED_BLINKY_PRT("APP_STATE_NETWORK_READY");
            appv_led_state = 1;
            //app_led_red = 0;
            app_led_green = 0;
            appv_led_cnt = 0;
        }
        else if (appv_log_level > 1) APP_LED_BLINKY_PRT("APP_STATE_NETWORK_READY");

        if ((++appv_led_cnt%4) == 0)  app_led_green = !app_led_green;
    }
    else {
        if (appv_led_state != 2) {
            APP_LED_BLINKY_PRT("APP_STATE_XXX");
            appv_led_state = 2;
            //app_led_red = 0;
            app_led_green = 0;
        }
        else if (appv_log_level > 1) APP_LED_BLINKY_PRT("APP_STATE_XXX");

        if ((++appv_led_cnt%2) == 0) {
            app_led_green = !app_led_green;
            //app_led_red = !app_led_green;
        }
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
//
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
//  Physical/Real Greenhouse Data

// ----------------------------------------------------------
// Sensors and Actuators

AnalogIn    greenhouse_sensor_light(A0);

DigitalIn   greenhouse_sensor_switch(D7, PullDown);    //D7 := PTC3  (D8 := PTA0)

#define GREENHOUSE_LED_NB       4

DigitalOut  greenhouse_led_1(D5);  //PTA2
DigitalOut  greenhouse_led_2(D4);  //PTB23
DigitalOut  greenhouse_led_3(D3);  //PTA1
DigitalOut  greenhouse_led_4(D2);  //TPB9

DigitalOut* greenhouse_led_tab[GREENHOUSE_LED_NB] =
{
        &greenhouse_led_1,
        &greenhouse_led_2,
        &greenhouse_led_3,
        &greenhouse_led_4
};

// ----------------------------------------------------------
// And a few greenhouse parameters

uint32_t              greenhouse_led_on = 0;
uint32_t              greenhouse_night_mode = 0;

LiveObjectsD_GpsFix_t greenhouse_gps_fix = { 1, 45.218208, 5.811437 };

uint32_t              greenhouse_state_sensor_switch = 0;

float                 greenhouse_measure_light_percent = 0;
uint32_t              greenhouse_measure_light_ohm = 0;


char                  greenhouse_status_message[150] = "";

char                  greenhouse_rsc_image[5*1024] = "";


// ==========================================================
// LiveObjet IoT Client Objects (using iotsoftbox-mqtt library)
// - configuration parameters
// - info data
// - collected data
// - commands

// ----------------------------------------------------------
// CONFIGURATION data
//
#define GPS_INT32
#define STREAM_PREFIX     1
#define MIN_PERIOD_SEC 1 // min of 1 second

struct conf_s {
    char         stream_measures[20];
    char         stream_leds[20];
    char         stream_states[20];
    uint32_t     measure_period_sec;
#ifdef GPS_INT32
    int32_t      gps_lat;
    int32_t      gps_long;
#endif
} appv_conf = {
        "measures",
        "leds",
        "states",
        2                  // default period : 2 seconds
#ifdef GPS_INT32
        , 45218208, 5811437
#endif
};

#define CFG_IDX_DATA_MEASURES    1
#define CFG_IDX_DATA_LEDS        2
#define CFG_IDX_DATA_STATES      3
#define CFG_IDX_PERIOD          10
#define CFG_IDX_NIGHT_MODE      11
#define CFG_IDX_POS_LATITUDE    12
#define CFG_IDX_POS_LONGITUDE   13
#ifdef GPS_INT32
#define CFG_IDX_POS_LAT_INT     20
#define CFG_IDX_POS_LON_INT     21
#endif

LiveObjectsD_Param_t appv_set_param[] = {
    { CFG_IDX_DATA_MEASURES, { LOD_TYPE_STRING_C, "stream_measures" ,  appv_conf.stream_measures } },
    { CFG_IDX_DATA_LEDS,     { LOD_TYPE_STRING_C, "stream_leds" ,      appv_conf.stream_leds } },
    { CFG_IDX_DATA_STATES,   { LOD_TYPE_STRING_C, "stream_states" ,    appv_conf.stream_states } },
    { CFG_IDX_PERIOD,        { LOD_TYPE_UINT32,   "period" ,           &appv_conf.measure_period_sec } },
    { CFG_IDX_NIGHT_MODE,    { LOD_TYPE_UINT32,   "night_mode" ,       &greenhouse_night_mode } },
#ifndef GPS_INT32
    { CFG_IDX_POS_LATITUDE,  { LOD_TYPE_FLOAT,    "latitude" ,         &greenhouse_gps_fix.gps_lat  } },
    { CFG_IDX_POS_LONGITUDE, { LOD_TYPE_FLOAT,    "longitude" ,        &greenhouse_gps_fix.gps_long } }
#else
    { CFG_IDX_POS_LAT_INT,   { LOD_TYPE_INT32,    "lat_int32" ,        &appv_conf.gps_lat  } },
    { CFG_IDX_POS_LON_INT,   { LOD_TYPE_INT32,    "lon_int32" ,        &appv_conf.gps_long } }
#endif
};
#define SET_PARAM_NB (sizeof(appv_set_param) / sizeof(LiveObjectsD_Param_t))

// ----------------------------------------------------------
// RESOURCE data
//
char appv_rv_message[10] = "01.00";
char appv_rv_image[10] = "01.00";

uint32_t appv_rsc_size = 0;
uint32_t appv_rsc_offset = 0;

#define RSC_IDX_MESSAGE       1
#define RSC_IDX_IMAGE         2

LiveObjectsD_Resource_t appv_set_resources[] = {
    { RSC_IDX_MESSAGE,    "message",         appv_rv_message, sizeof(appv_rv_message)-1 },
    { RSC_IDX_IMAGE,      "imageARMTechCon", appv_rv_image,   sizeof(appv_rv_image)-1 }
};

#define SET_RESOURCES_NB (sizeof(appv_set_resources) / sizeof(LiveObjectsD_Resource_t))


// ----------------------------------------------------------
// STATUS data
//
LiveObjectsD_Data_t appv_set_status[] = {
    { LOD_TYPE_STRING_C, "greenhouse-version" ,       (void*)appv_version },
    { LOD_TYPE_STRING_C, "greenhouse-message" ,       greenhouse_status_message },
    { LOD_TYPE_UINT32,   "greenhouse-light-on",       (void*)&greenhouse_led_on },
    { LOD_TYPE_UINT32,   "greenhouse-night-mode",     (void*)&greenhouse_night_mode }
#ifdef GPS_INT32
    , { LOD_TYPE_FLOAT,  "greenhouse-gps-latitude" ,  &greenhouse_gps_fix.gps_lat }
    , { LOD_TYPE_FLOAT,  "greenhouse-gps-longitude" , &greenhouse_gps_fix.gps_long }
#endif
};
#define SET_STATUS_NB (sizeof(appv_set_status) / sizeof(LiveObjectsD_Data_t))


// ----------------------------------------------------------
// 'COLLECTED DATA' : 3 streams
//

LiveObjectsD_Data_t appv_set_measures[] = {
    { LOD_TYPE_FLOAT,  "LightSensor" ,      &greenhouse_measure_light_percent },
    { LOD_TYPE_UINT32, "LightIntensity" ,   &greenhouse_measure_light_ohm }
};
#define SET_DATA_MEASURES_NB (sizeof(appv_set_measures) / sizeof(LiveObjectsD_Data_t))

LiveObjectsD_Data_t appv_set_leds[] = {
    { LOD_TYPE_UINT32,  "LEDS" ,            &greenhouse_led_on },
};
#define SET_DATA_LEDS_NB (sizeof(appv_set_leds) / sizeof(LiveObjectsD_Data_t))

LiveObjectsD_Data_t appv_set_states[] = {
    { LOD_TYPE_INT32, "LightSensorState" ,  &greenhouse_state_sensor_switch },
};
#define SET_DATA_STATES_NB (sizeof(appv_set_states) / sizeof(LiveObjectsD_Data_t))


int appv_hdl_measures = -1;
int appv_hdl_leds = -1;
int appv_hdl_states = -1;

// ----------------------------------------------------------
// COMMANDS
//
#define CMD_IDX_RESET       1
#define CMD_IDX_LIGHT_ON    2
#define CMD_IDX_NIGHT_MODE  3

LiveObjectsD_Command_t appv_set_commands[] = {
    { CMD_IDX_RESET,      "RESET" ,      0 },
    { CMD_IDX_LIGHT_ON,   "LIGHT-ON"   , 0 },
    { CMD_IDX_NIGHT_MODE, "NIGHT_MODE" , 0 },

};

#define SET_COMMANDS_NB (sizeof(appv_set_commands) / sizeof(LiveObjectsD_Command_t))


// ==========================================================
// IotSoftbox-mqtt callback functions

// ----------------------------------------------------------
// CONFIGURATION PARAMETERS Callback function
// Check value in range, and copy string parameters

extern "C" int main_cb_param_udp(const LiveObjectsD_Param_t* param_ptr, const void* value, int len)
{
    if (param_ptr == NULL) {
        output.printf("UPDATE  ERROR - invalid parameter x%p\r\n",param_ptr);
        return -1;
    }
    output.printf("UPDATE user_ref=%d %s ....\r\n", param_ptr->parm_uref, param_ptr->parm_data.data_name);
    switch(param_ptr->parm_uref) {
    case CFG_IDX_DATA_MEASURES:
        {
            output.printf("update stream_measures = %.*s\r\n", len , (const char*) value);
            if ((len > 0) && (len < (sizeof(appv_conf.stream_measures)-1))) {
                strncpy(appv_conf.stream_measures, (const char*) value, len);
                appv_conf.stream_measures[len] = 0;
                LiveObjectsClient_ChangeDataStreamId(STREAM_PREFIX, appv_hdl_measures, appv_conf.stream_measures);
                return 0;
            }
        }
        break;
    case CFG_IDX_DATA_LEDS:
        {
            output.printf("update stream_leds = %.*s\r\n", len , (const char*) value);
            if ((len > 0) && (len < (sizeof(appv_conf.stream_leds)-1))) {
                strncpy(appv_conf.stream_leds, (const char*) value, len);
                appv_conf.stream_leds[len] = 0;
                LiveObjectsClient_ChangeDataStreamId(STREAM_PREFIX, appv_hdl_leds, appv_conf.stream_leds);
                return 0;
            }
        }
        break;
    case CFG_IDX_DATA_STATES:
        {
            output.printf("update stream_states = %.*s\r\n", len , (const char*) value);
            if ((len > 0) && (len < (sizeof(appv_conf.stream_states)-1))) {
                strncpy(appv_conf.stream_states, (const char*) value, len);
                appv_conf.stream_states[len] = 0;
                LiveObjectsClient_ChangeDataStreamId(STREAM_PREFIX, appv_hdl_states, appv_conf.stream_states);
                return 0;
            }
        }
        break;
    case CFG_IDX_PERIOD:
        {
            uint32_t measure_period_sec = *((const uint32_t*)value);
            output.printf("update measure_period = %"PRIu32"\r\n", measure_period_sec);
            if ((measure_period_sec >= MIN_PERIOD_SEC) && (measure_period_sec != appv_conf.measure_period_sec)) {
                return 0;
            }
        }
        break;
    case CFG_IDX_NIGHT_MODE:
        {
            uint32_t night_mode = *((const uint32_t*)value);
            output.printf("update night_mode = %"PRIu32"\r\n", night_mode);
            if (night_mode != greenhouse_night_mode) {
                greenhouse_night_mode = night_mode;
            }
            return 0;
        }
        break;
#ifndef GPS_INT32
    case CFG_IDX_POS_LATITUDE:
        {
            float latitude = *((const float*)value);
            output.printf("update latitude = %f\r\n", latitude);
            if (latitude != greenhouse_gps_fix.gps_lat) {
                ;
            }
            return 0;
        }
        break;
    case CFG_IDX_POS_LONGITUDE:
        {
            float longitude = *((const float*)value);
            output.printf("update longitude = %f\r\n", longitude);
            if (longitude != greenhouse_gps_fix.gps_long) {
                //appv_conf.gps_long = longitude * 1000000;
                ;
            }
            return 0;
        }
        break;
#endif
#ifdef GPS_INT32
    case CFG_IDX_POS_LAT_INT:
        {
            int32_t latitude = *((const int32_t*)value);
            output.printf("update latitude = %"PRIi32" to %"PRIi32"\r\n", appv_conf.gps_lat, latitude);
            if (latitude != appv_conf.gps_lat) {
                greenhouse_gps_fix.gps_lat = (float)latitude / 1000000.;
                LiveObjectsClient_PushStatus(0);
            }
            return 0;
        }
        break;
    case CFG_IDX_POS_LON_INT:
        {
            int32_t longitude = *((const int32_t*)value);
            output.printf("update longitude =  %"PRIi32" to %"PRIi32"\r\n", appv_conf.gps_long,  longitude);
            if (longitude != appv_conf.gps_long) {
                greenhouse_gps_fix.gps_long =  (float)longitude / 1000000.;
                LiveObjectsClient_PushStatus(0);
            }
            return 0;
        }
        break;
#endif
    }
    output.printf("ERROR to update param[%d] %s !!!\r\n", param_ptr->parm_uref, param_ptr->parm_data.data_name);
    return -1;
}


// ----------------------------------------------------------
// RESOURCE Callback Functions
//
extern "C"  LiveObjectsD_ResourceRespCode_t main_cb_rsc_ntfy (
        uint8_t state, const LiveObjectsD_Resource_t* rsc_ptr,
        const char* version_old, const char* version_new, uint32_t size)
{
    LiveObjectsD_ResourceRespCode_t ret = RSC_RSP_OK; // OK to update the resource
#if 0
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
                    output.printf(greenhouse_rsc_image);
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
            LiveObjectsClient_PushStatus(0);
        }
        else {
            appv_rsc_offset = 0;
            ret = RSC_RSP_ERR_NOT_AUTHORIZED;
            switch (rsc_ptr->rsc_uref ) {
                case RSC_IDX_MESSAGE:
                    if (size < (sizeof(greenhouse_status_message)-1)) {
                        ret = RSC_RSP_OK;
                    }
                    break;
                case RSC_IDX_IMAGE:
                    if (size < (sizeof(greenhouse_rsc_image)-1)) {
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
#endif
    return ret;
}

extern "C"  int main_cb_rsc_data (const LiveObjectsD_Resource_t* rsc_ptr, uint32_t offset)
{
    int ret;

    if (appv_log_level > 1)  output.printf("*** rsc_data: rsc[%d]='%s' offset=%u - data ready ...\r\n", rsc_ptr->rsc_uref, rsc_ptr->rsc_name, offset);

    if (rsc_ptr->rsc_uref == RSC_IDX_MESSAGE) {
        char buf[40];
        if (offset > (sizeof(greenhouse_status_message)-1)) {
            output.printf("*** rsc_data: rsc[%d]='%s' offset=%u > %d - OUT OF ARRAY\r\n",
                    rsc_ptr->rsc_uref, rsc_ptr->rsc_name, offset, sizeof(greenhouse_status_message)-1);
            return -1;
        }
        ret = LiveObjectsClient_RscGetChunck(rsc_ptr, buf, sizeof(buf)-1);
        if (ret > 0) {
            if ((offset+ret) > (sizeof(greenhouse_status_message)-1)) {
                output.printf("*** rsc_data: rsc[%d]='%s' offset=%u - read=%d => %d > %d - OUT OF ARRAY\r\n",
                        rsc_ptr->rsc_uref, rsc_ptr->rsc_name, offset, ret, offset + ret, sizeof(greenhouse_status_message)-1);
                return -1;
            }
            appv_rsc_offset += ret;
            memcpy(&greenhouse_status_message[offset], buf, ret);
            greenhouse_status_message[offset+ret] = 0;
            output.printf("*** rsc_data: rsc[%d]='%s' offset=%u - read=%d/%d '%s'\r\n",
                    rsc_ptr->rsc_uref, rsc_ptr->rsc_name, offset, ret, sizeof(buf)-1, greenhouse_status_message);
        }
    }
    else if (rsc_ptr->rsc_uref == RSC_IDX_IMAGE) {
        if (offset > (sizeof(greenhouse_rsc_image)-1)) {
             output.printf("*** rsc_data: rsc[%d]='%s' offset=%u > %d - OUT OF ARRAY\r\n",
                     rsc_ptr->rsc_uref, rsc_ptr->rsc_name, offset, sizeof(greenhouse_rsc_image)-1);
             return -1;
         }
        int data_len = sizeof(greenhouse_rsc_image) - offset - 1;
        ret = LiveObjectsClient_RscGetChunck(rsc_ptr, &greenhouse_rsc_image[offset], data_len);
        if (ret > 0) {
            if ((offset+ret) > (sizeof(greenhouse_rsc_image)-1)) {
                 output.printf("*** rsc_data: rsc[%d]='%s' offset=%u - read=%d => %d > %d - OUT OF ARRAY\r\n",
                         rsc_ptr->rsc_uref, rsc_ptr->rsc_name, offset, ret, offset + ret, sizeof(greenhouse_rsc_image)-1);
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
static int main_cmd_doLightOn(LiveObjectsD_CommandRequestBlock_t* pCmdReqBlk);
static int main_cmd_doNightMode(LiveObjectsD_CommandRequestBlock_t* pCmdReqBlk);

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
    case CMD_IDX_RESET:
        output.printf("main_callbackCommand: command[%d] %s\r\n", cmd_ptr->cmd_uref, cmd_ptr->cmd_name);
        ret = main_cmd_doSystemReset(pCmdReqBlk);
        break;

    case CMD_IDX_LIGHT_ON:
        output.printf("main_callbackCommand: do command[%d] %s\r\n", cmd_ptr->cmd_uref, cmd_ptr->cmd_name);
        ret = main_cmd_doLightOn(pCmdReqBlk);
        break;

    case CMD_IDX_NIGHT_MODE:
        output.printf("main_callbackCommand: do command[%d] %s\r\n", cmd_ptr->cmd_uref, cmd_ptr->cmd_name);
        ret = main_cmd_doNightMode(pCmdReqBlk);
        break;

    default :
        output.printf("main_callbackCommand: ERROR, unknown command with cmd_uref=%d\r\n", cmd_ptr->cmd_uref);
        ret = -4;
    }
    return ret;
}

// ----------------------------------------------------------
// Board reset
static void main_SystemReset(void)
{
    NVIC_SystemReset();
}

// ----------------------------------------------------------
// Command: RESET
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
// Command: LightOn
static int main_cmd_doLightOn(LiveObjectsD_CommandRequestBlock_t* pCmdReqBlk)
{
    if ((pCmdReqBlk->hd.cmd_args_nb == 1)
        && (pCmdReqBlk->args_array[0].arg_name)
        && (0 == pCmdReqBlk->args_array[0].arg_type)
        && (6 == strlen(pCmdReqBlk->args_array[0].arg_name))
        && (!strncmp("number", pCmdReqBlk->args_array[0].arg_name, 6)) )
    {
        if ((pCmdReqBlk->args_array[0].arg_value)
                && (1 == strlen(pCmdReqBlk->args_array[0].arg_value))
                && (*pCmdReqBlk->args_array[0].arg_value >= '0')
                && (*pCmdReqBlk->args_array[0].arg_value <= '9') )
        {
            int n = *pCmdReqBlk->args_array[0].arg_value - '0';
#if GREENHOUSE_LED_NB
            int i;
            if (n > GREENHOUSE_LED_NB) n = GREENHOUSE_LED_NB;
            output.printf("*** LEDS %d -> %d \r\n", greenhouse_led_on, n);
            for (i=0; i<n; i++) {
                greenhouse_led_tab[i]->write(1);
            }
            for (; i<GREENHOUSE_LED_NB; i++) {
                greenhouse_led_tab[i]->write(0);
            }
#endif
            greenhouse_led_on = n;

            LiveObjectsClient_PushData(appv_hdl_leds);

            LiveObjectsClient_PushStatus(0);

            return 1; // response = OK
        }
        output.printf("main_cmd_doLightOn: ERROR, invalid value %s for arg[0] %s \r\n",
                pCmdReqBlk->args_array[0].arg_value, pCmdReqBlk->args_array[0].arg_name);
    }
    else {
        output.printf("main_cmd_doLightOn: ERROR, invalid argument - nb=%d\r\n", pCmdReqBlk->hd.cmd_args_nb);
        if (pCmdReqBlk->hd.cmd_args_nb > 0) {
            output.printf("    and arg[0]: %s type=%d\r\n", pCmdReqBlk->args_array[0].arg_name, pCmdReqBlk->args_array[0].arg_type);
        }
    }
    return -1;
}
// ----------------------------------------------------------
// Command: NightMode
static int main_cmd_doNightMode(LiveObjectsD_CommandRequestBlock_t* pCmdReqBlk)
{
    if ((pCmdReqBlk->hd.cmd_args_nb == 1)
        && (pCmdReqBlk->args_array[0].arg_name)
        && (0 == pCmdReqBlk->args_array[0].arg_type)
        && (4 == strlen(pCmdReqBlk->args_array[0].arg_name))
        && (!strncmp("mode", pCmdReqBlk->args_array[0].arg_name, 4)) )
    {
        if ((pCmdReqBlk->args_array[0].arg_value)
                && (strlen(pCmdReqBlk->args_array[0].arg_value) > 0))
        {
            uint32_t night_mode = 0;
            if (!strcasecmp("true", pCmdReqBlk->args_array[0].arg_name)) {
                night_mode = 2;
            }
            if (!strcasecmp("false", pCmdReqBlk->args_array[0].arg_name)) {
                night_mode = 1;
            }
            if (night_mode) {
                night_mode--;
                if (night_mode != greenhouse_night_mode) {
                    greenhouse_night_mode = night_mode;

                    LiveObjectsClient_PushStatus(0);

                    LiveObjectsClient_PushData(appv_hdl_leds);

                }
                return 1; // response = OK
            }
        }
        output.printf("main_cmd_doNightMode: ERROR, invalid value %s for arg[0] %s \r\n",
                pCmdReqBlk->args_array[0].arg_value, pCmdReqBlk->args_array[0].arg_name);
    }
    else {
        output.printf("main_cmd_doNightMode: ERROR, invalid argument - nb=%d\r\n", pCmdReqBlk->hd.cmd_args_nb);
        if (pCmdReqBlk->hd.cmd_args_nb > 0) {
            output.printf("    and arg[0]: %s type=%d\r\n", pCmdReqBlk->args_array[0].arg_name, pCmdReqBlk->args_array[0].arg_type);
        }
    }
    return -1;
}

// ==========================================================
//
void app_init(void)
{
    int i = 0;
    greenhouse_led_tab[i++]->write(1);
    for (; i<GREENHOUSE_LED_NB; i++) {
        greenhouse_led_tab[i]->write(0);
    }
    greenhouse_led_on = 1;

    greenhouse_state_sensor_switch = greenhouse_sensor_switch.read() ? 0 : 1;
    greenhouse_measure_light_percent = 0;
    greenhouse_measure_light_ohm = 0;
}

void thread_appli(void) {
    uint32_t loop_cnt = 0;
    osThreadId id = osThreadGetId();

    uint32_t tick_light = 0;

    output.printf("thread_appli: running %x\r\n", id);



    while(1) {
        int state_sensor_switch = greenhouse_sensor_switch.read() ? 0 : 1;
        if (greenhouse_state_sensor_switch != state_sensor_switch) {
            output.printf("%u LIGHT_SWITCH STATE CHANGE %d -> %d \r\n", loop_cnt, greenhouse_state_sensor_switch, state_sensor_switch);
            greenhouse_state_sensor_switch = state_sensor_switch;

            LiveObjectsClient_PushData(appv_hdl_states);
        }

        if (++tick_light >=  appv_conf.measure_period_sec) {
#if 1
            if (greenhouse_state_sensor_switch == 0) {
                greenhouse_measure_light_percent = 0;
                greenhouse_measure_light_ohm = 0;
            }
            else
#endif
            {
            // Read light intensity
                float light_val = greenhouse_sensor_light.read();
                //uint16_t val_16 = greenhouse_sensor_light.read_u16();

                // Update data
                greenhouse_measure_light_percent = light_val * 100.;                       // Percent
                greenhouse_measure_light_ohm = (uint32_t)(1023.-light_val)*10./light_val;  // ohms

            }

            if (appv_log_level > 0) {
                output.printf("%u (%u/%u) - %d - Sensor reading: %3.4f - %"PRIu32"\r\n",
                        loop_cnt, tick_light, appv_conf.measure_period_sec, greenhouse_state_sensor_switch,
                        greenhouse_measure_light_percent, greenhouse_measure_light_ohm);
            }

#ifdef DATA_ENABLE_CTRL
            if (appv_data_enabled)
#endif
            {
                LiveObjectsClient_PushData(appv_hdl_measures);
            }

            tick_light = 0;
        }

        wait_ms(1000);
        ++loop_cnt;

        if (appv_log_level > 3)
            output.printf("thread_appli: %u s - period = %d/%"PRIu32"\r\n",
                    loop_cnt, tick_light, appv_conf.measure_period_sec);
    }
}


// ==========================================================

// ----------------------------------------------------------
// Interrupts and buttons

InterruptIn app_int_sw2(SW2);

void app_sw2_release(void)
{
    output.printf("On-board button SW2 was released\r\n");

    int state_sensor_switch = (greenhouse_state_sensor_switch) ? 0 : 1;
    output.printf("  *** FLIP-FLOP STATE OF SENSOR SWITCH  %u -> %u\r\n", greenhouse_state_sensor_switch , state_sensor_switch);
    greenhouse_state_sensor_switch = state_sensor_switch;

    LiveObjectsClient_PushData(appv_hdl_states);

    int val = greenhouse_sensor_switch.read();
    output.printf(" *** SENSOR_SWITCH= %u\r\n", val );
}

// ----------------------------------------------------------
//
const char* appli_help =
        " R     : system reset\r\n"
        " d     : send 'collected data' MEASURES\r\n"
        " s     : send 'collected data' STATES (alarm, ..)\r\n"
        " l     : send 'collected data' LEDS\r\n"
        " i     : send 'info'\r\n"
        " c     : send 'config. parameters'\r\n"
        " X|M|m : Enable/Disable the dump of published message (X : add hex dump)\r\n"
        " D|I|W : set debug log level (mbed_trace)\r\n"
        " 0-9   : set appli log level \r\n"
#ifdef DATA_ENABLE_CTRL
        " e     : flip-flop to enable/disable the periodic 'measures' publish process\r\n"
#endif
        ;

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
                    output.printf(appli_help);
                }
#ifdef DATA_ENABLE_CTRL
                else if ( c == 'e' ) {
                    appv_data_enabled = (appv_data_enabled) ? 0 : 1;
                    output.printf(">>> Publish  Collected Data : %s \r\n",  (appv_data_enabled) ? "ENABLED" : "DISABLED");
                }
#endif
                else if ( c == 'd' ) {
                    output.printf(">>> Push DATA MEASURE hdl=%d ...\r\n", appv_hdl_measures);
                    LiveObjectsClient_PushData(appv_hdl_measures);
                }
                else if ( c == 's' ) {
                    output.printf(">>> Push DATA STATES hdl=%d ...\r\n", appv_hdl_states);
                    LiveObjectsClient_PushData(appv_hdl_states);
                }
                else if ( c == 'l' ) {
                     output.printf(">>> Push DATA LEDS hdl=%d ...\r\n", appv_hdl_leds);
                     LiveObjectsClient_PushData(appv_hdl_leds);
                }
                else if (c == 'i' ) {
                    output.printf(">>> Push INFO ...\r\n");
                    LiveObjectsClient_PushStatus(0);
                }
                else if (c == 'c' ) {
                    output.printf(">>> Push CONFIG ...\r\n");
                    LiveObjectsClient_PushCfgParams();
                }
                else if (c == 'X') {
                    output.printf(">>> Enable Message Dump\r\n");
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
static void appli_client_state_cb(LiveObjectsD_State_t state)
{
    enum app_state_enum new_state = APP_STATE_UNKNOWN;
    switch (state) {
    case CSTATE_CONNECTING:    new_state = APP_STATE_CONNECTING; break;
    case CSTATE_CONNECTED:     new_state = APP_STATE_CONNECTED; break;
    case CSTATE_DISCONNECTED:  new_state = APP_STATE_NETWORK_READY; break;
    }
    output.printf("\n\rLIVEOBJECTS CLIENT STATE CHANGE (%d) :  %d -> %d \r\n", state, appv_state, new_state);
    appv_state = new_state;

    if (appv_state == APP_STATE_CONNECTED) {
       LiveObjectsClient_PushData(appv_hdl_states);
       LiveObjectsClient_PushData(appv_hdl_leds);
    }
}

// ----------------------------------------------------------
// Entry point to the program
//
int main() {
    int ret;

    app_led_green = 1;
    //app_led_red = 0;
    //app_led_blue = 0;

    appv_state = APP_STATE_INIT;

    // LiveObjects Toolbox
    LiveObjectsClient_ToolboxInit();

    // Led blink at 250 ms
    app_led_ticker.attach_us(app_led_blinky, 250000);

    // Enable interrupts: SW2
    app_int_sw2.rise(&app_sw2_release);

    // Keep track of the main thread
    appv_thread_id = osThreadGetId();

    // Sets the console baud-rate
    output.baud(9600);

    output.printf("\r\n\r\n");
    output.printf("Starting LiveObjects Client Demonstrator %s ...\r\n", appv_version);

    app_trace_setup();

    // Start program only if LiveObjet Apikey parameter is well defined
    if (LiveObjectsClient_CheckApiKey(LOC_CLIENT_DEV_API_KEY)) {
        output.printf("apikey not set, '%s'\r\n", LOC_CLIENT_DEV_API_KEY);
        output.printf("\n\rExiting application ....\r\n");
        return -1;
    }

    app_init();

    // Start user application :
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

#if 0
    while (1) {
         wait_ms(1000);
    }
#else
    wait_ms(200);

    // Now, start the connectivity to LiveObjects platform
    if (0 == app_net_init())
    {
        output.printf("\n\rConnected to Network successfully\r\n");

        appv_state = APP_STATE_NETWORK_READY;

        LiveObjectsClient_SetDbgLevel(DBG_DFT_LOMC_LOG_LEVEL);

        // Initialize the LiveObjects Client Context
        // ------------------------------------------
        output.printf("\n\rLiveObjectsClient_Init ...\r\n");
        ret = LiveObjectsClient_Init(appv_network_interface);
        if (ret) {
             output.printf("\n\rLiveObjectsClient_Init Failed !\r\n");
        }
        else {

#if LOC_CLIENT_USE_MAC_ADDR
            if (appv_network_id) {
                // Use the MAC address to set the Device Identifier
                // ------------------------------------------------
                output.printf("\n\rLiveObjectsClient_SetDevId '%s' ...\r\n", appv_network_id);
                ret = LiveObjectsClient_SetDevId(appv_network_id);
                if (ret) {
                    output.printf("\n\rLiveObjectsClient_SetDevId Failed !\r\n");
                }
            }
#endif
            // Attach my local INFO data to the LiveObjects Client instance
            // ------------------------------------------------------------
            ret = LiveObjectsClient_AttachStatus(appv_set_status, SET_STATUS_NB);
            if (ret) output.printf(" !!! ERROR (%d) to attach status !\r\n", ret);

            // Attach my local Configuration Parameters to the LiveObjects Client instance
            // ---------------------------------------------------------------------------
            ret = LiveObjectsClient_AttachCfgParams(appv_set_param, SET_PARAM_NB, main_cb_param_udp);
            if (ret) output.printf(" !!! ERROR (%d) to attach Config Parameters !\r\n", ret);

#if 1
            // Attach my local RESOURCES to the LiveObjects Client instance
            // -------------------------------------------------------------
            //main_cb_rsc_ntfy
            ret = LiveObjectsClient_AttachResources(appv_set_resources, SET_RESOURCES_NB, main_cb_rsc_ntfy, main_cb_rsc_data);
            if (ret) output.printf(" !!! ERROR (%d) to attach RESOURCES !\r\n", ret);
#endif

            // Attach one set of collected data to the LiveObjects Client instance
            // -------------------------------------------------------------------
            appv_hdl_measures = LiveObjectsClient_AttachData(STREAM_PREFIX,
                    appv_conf.stream_measures,"mv1","\"grapevine\",\"pineau\"", &greenhouse_gps_fix,
                    appv_set_measures, SET_DATA_MEASURES_NB);
            if (appv_hdl_measures < 0) output.printf(" !!! ERROR (%d) to attach a collected data stream -  MEASURES !\r\n", appv_hdl_measures);

            appv_hdl_leds = LiveObjectsClient_AttachData(STREAM_PREFIX,
                    appv_conf.stream_leds,"mv1",NULL, &greenhouse_gps_fix, appv_set_leds, SET_DATA_LEDS_NB);
            if (appv_hdl_leds < 0) output.printf(" !!! ERROR (%d) to attach a collected data stream - LEDS !\r\n", appv_hdl_leds);

            appv_hdl_states = LiveObjectsClient_AttachData(STREAM_PREFIX,
                    appv_conf.stream_states,"mv1",NULL, &greenhouse_gps_fix, appv_set_states, SET_DATA_STATES_NB);
            if (appv_hdl_states < 0) output.printf(" !!! ERROR (%d) to attach a collected data stream -  STATES !\r\n", appv_hdl_leds);

            // Attach a set of commands to the LiveObjects Client instance
            // -----------------------------------------------------------
            ret = LiveObjectsClient_AttachCommands(appv_set_commands, SET_COMMANDS_NB, main_cb_command);
            if (ret < 0) output.printf(" !!! ERROR (%d) to attach a set of commands !\r\n", ret);

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
                while(1) {
                    wait_ms(1000);
                }
            }
            output.printf("\n\rLiveObjectsClient_ThreadStart ERROR %d\r\n", ret);
#endif
        }
    } else {
        output.printf("\n\rConnection to Network Failed ! \r\n");
    }
#endif

    output.printf("\n\rExiting application ....\r\n");

    app_led_ticker.detach();

    app_led_green = 0;
    //app_led_red = 1;

    main_SystemReset();
}
