/**
  *****************************************************************************
  * @file   sensor2cloud-aws_demo_app_INP301x.c
  * 
  *****************************************************************************
  * @attention
  *
  *
  *  Copyright (c) 2022, InnoPhase, Inc.
  *
  *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  *  POSSIBILITY OF SUCH DAMAGE.
  *
  *****************************************************************************
  */

#define OS_DEBUG PR_LEVEL_INFO
/* aws iot core */
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_shadow_interface.h"
#include "aws_iot_json_utils.h"

/* WIFI INTERFACE*/
#include "wifi/wifi.h"
#include "wifi/wcm.h"
#include "api/wcm.h"

#include <kernel/os.h>
#include "string.h"
#include "errno.h"
#include "fs_utils.h"
#include "wifi_utils.h"

/* sensors */
//#define OS_DEBUG PR_LEVEL_INFO
#include <kernel/chip_monitor.h>
#include <kernel/gpio.h>
#include <stdbool.h>
#include "callout_delay.h"
#include "sensor.h"
#include "sensor2cloud-aws_inp301x.h"

/*############### WCM #############################*/
static struct wcm_handle *h = NULL;
static bool ap_link_up = false;
static bool ap_got_ip = false;

static struct os_semaphore Wifi_Connect; 
static bool sem_wait = false;

static void my_wcm_notify_cb(void *ctx, struct os_msg *msg)
{
    switch(msg->msg_type)
    {

        case(WCM_NOTIFY_MSG_LINK_UP):
            os_printf("wcm_notify_cb to App Layer - WCM_NOTIFY_MSG_LINK_UP\n");
            ap_link_up = true;
            break;

        case(WCM_NOTIFY_MSG_LINK_DOWN):
            os_printf("wcm_notify_cb to App Layer - WCM_NOTIFY_MSG_LINK_DOWN\n");
            ap_link_up = false;
            ap_got_ip = false;
            break;

        case(WCM_NOTIFY_MSG_ADDRESS):
            os_printf("wcm_notify_cb to App Layer - WCM_NOTIFY_MSG_ADDRESS\n");
            break;

        case(WCM_NOTIFY_MSG_DISCONNECT_DONE):
            os_printf("wcm_notify_cb to App Layer - WCM_NOTIFY_MSG_DISCONNECT_DONE\n");
            ap_got_ip = false;
            break;

        case(WCM_NOTIFY_MSG_CONNECTED):
            os_printf("wcm_notify_cb to App Layer - WCM_NOTIFY_MSG_CONNECTED\n");
            /* releasing the semaphore IF it is being waited-upon for wifi-connection after LinkDown */ 
            if(ap_got_ip == false && sem_wait == true){
               os_sem_post( &Wifi_Connect ); 
               sem_wait = false;
            }
            ap_got_ip = true;

            break;

        default:
            break;
    }
    os_msg_release(msg);
}

/**
 * Enable or Disable Rx for reception of multicast frames after DTIM
 *
 * Multicast frame reception is by default enabled when a net is created.
 * Disabling multicast reception will decrease power consumption but it will
 * also imply that incoming broadcast arp request will not be received.
 *
 * @param no_mcast Disable reception of multicast frames if passed as true.
 * Enable reception of multicast frames if passed as false.
 */
void wifi_SetMulticastRX(bool no_mcast)
{
    if (no_mcast)
    wcm_mcast_rx_enable(h, false);
    else
    wcm_mcast_rx_enable(h, true);
}

/**
 * Calls WCM APIs to asynchronously connect to a WiFi network.
 *
 * @return Returns zero on success, negative error code from 
 * errorno.h in case of an error.
 */
int wifi_main()
{
    int rval;
    struct network_profile *profile;
    const char *np_conf_path = os_get_boot_arg_str("np_conf_path")?: NULL;

    h = wcm_create(NULL);
    if( h == NULL ){
        os_printf(" failed.\n");
        return -1;
    }

    os_sleep_us(1000000, OS_TIMEOUT_NO_WAKEUP);
    wcm_notify_enable(h, my_wcm_notify_cb, NULL);

    os_printf("Connecting to WiFi...\n");

    /* Connect to network */
    if (np_conf_path != NULL) {
        /* Create a Network Profile from a configuration file in
         *the file system*/
        rval = network_profile_new_from_file_system(&profile, np_conf_path);
    } else {
        /* Create a Network Profile using BOOT ARGS*/
        rval = network_profile_new_from_boot_args(&profile);
    }
    if (rval < 0) {
        pr_err("could not create network profile %d\n", rval);
        return rval;
    }

    rval = wcm_add_network_profile(h, profile);
    if (rval <  0) {
        pr_err("could not associate network profile to wcm %d\n", rval);
        return rval;
    }

    os_printf("added network profile successfully, will try connecting..\n");

    rval = wcm_auto_connect(h, true);
    os_printf("connecting to network status: %d\n", rval);
    if(rval < 0){
        pr_err("network connection trial Failed, wcm_auto_connect returned %d\n", rval);
        return rval;
    }

    return rval;
}

/**
 * Disconnect and cleanup a WiFi Connection Manager interface.
 * @param state_connected connection state
 */
void wifi_destroy(bool state_connected)
{
    int rval;
    if(state_connected){
        rval = wcm_auto_connect(h, false);
        if(rval != 0){
            os_printf("trying to disconnect to network failed with %d\n", rval);
        }

        rval = wcm_delete_network_profile(h, NULL);
        if(rval != 0){
            os_printf("trying to remove network profile failed with %d\n", rval);
        }
    }
    wcm_destroy(h);
}

//############################  Sensor Code  ###############################//

OS_APPINFO {.stack_size=4096};

static bool no_mcast;
static int init_platform();
static int init_and_connect_aws_iot();
static int validate_inputs();
static AWS_IoT_Client *gpclient;

static bool attemptingReconnect = false;
static bool shadowUpdateInProgress = false;
static bool sensorSwitch_delta_callback_recieved = false;
static bool sensorPollInterval_delta_callback_recieved = false;

sensor_reading_t readings;

inp301x_aws_shadow_params_t inp301x_shadow_params;
char JsonDocumentBuffer[MAX_LENGTH_OF_UPDATE_JSON_BUFFER];

static void process_shadow_delta_callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext);

/* array of shadow attributes used in this Application */
jsonStruct_t inp301x_shadow_attributes[AWS_SHADOW_ATTRIBUTES_MAX_COUNT] = {
    /* pKey                     pData                                           dataLength          type                    cb (callback) */
    {"temperature",             &(inp301x_shadow_params.temperature),           sizeof(float),      SHADOW_JSON_FLOAT,                               NULL},
    {"pressure",                &(inp301x_shadow_params.pressure),              sizeof(float),      SHADOW_JSON_FLOAT,                               NULL},
    {"humidity",                &(inp301x_shadow_params.humidity),              sizeof(float),      SHADOW_JSON_FLOAT,                               NULL},
    {"opticalPower",            &(inp301x_shadow_params.opticalPower),          sizeof(float),      SHADOW_JSON_FLOAT,                               NULL},
    {"sensorPollInterval",      &(inp301x_shadow_params.sensorPollInterval),    sizeof(uint32_t),   SHADOW_JSON_UINT32,     process_shadow_delta_callback},
    {"sensorSwitch",            &(inp301x_shadow_params.sensorOn),              sizeof(bool),       SHADOW_JSON_BOOL,       process_shadow_delta_callback},
};

static struct i2c_bus* init_i2c(void)
{
    /* Enable internal pullups on SCL and SDA */
    os_gpio_set_pull(GPIO_PIN(SCL_PIN) | GPIO_PIN(SDA_PIN));

    /* Route SCL and SDA to the appropiate pins */
    os_gpio_mux_sel(GPIO_MUX_SEL_SCL, SCL_PIN);
    os_gpio_mux_sel(GPIO_MUX_SEL_SDA, SDA_PIN);

    /* Initialize i2c bus */
    return i2c_bus_init(0);
}

/**
 * The callback that is used to inform the caller of the response from the
 * AWS IoT Shadow service. This is passed while calling API aws_iot_shadow_update()
 */
static void ShadowUpdateStatusCallback(const char *pThingName,
        ShadowActions_t action, Shadow_Ack_Status_t status,
        const char *pReceivedJsonDocument, void *pContextData) {

    if (SHADOW_ACK_TIMEOUT == status) {
        os_printf("Update Timeout--\n");
    }

    else if (SHADOW_ACK_REJECTED == status) {
        os_printf("Update Rejected\n");
    }

    else if (SHADOW_ACK_ACCEPTED == status) {
        os_printf("Update Accepted !!\n");
    }

    shadowUpdateInProgress = false;
}

/**
 * Callback to listen on the delta topic registered with the API
 * aws_iot_shadow_register_delta(). Any time a delta is published by AWS IoT Shadow
 * service, the Json document will be delivered via this callback.
 */
static void process_shadow_delta_callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) 
{

    os_printf("Recieved Delta Callback for shadow attribute: %s, ", pContext->pKey);

    if (pContext != NULL) {
        /* this is for debug prints, and limited. It can be populated for other valid shadow JsonPrimitiveTypes */
        if (pContext->type == SHADOW_JSON_INT32) {
            os_printf("with desired state: %i\n", *(int32_t *) (pContext->pData));

        } else if (pContext->type == SHADOW_JSON_UINT32) {
            os_printf("with desired state: %u\n", *(uint32_t *) (pContext->pData));

        } else if (pContext->type == SHADOW_JSON_BOOL) {
            os_printf("with desired state: %s\n", *(bool *) (pContext->pData)? "true" : "false");

        } else if (pContext->type == SHADOW_JSON_FLOAT) {
            os_printf("with desired state: %f\n", *(float *) (pContext->pData));

        } else if (pContext->type == SHADOW_JSON_DOUBLE) {
            os_printf("with desired state: %f\n", *(double *) (pContext->pData));

        } else if (pContext->type == SHADOW_JSON_STRING) {
            os_printf("with desired state: \"%s\" \n", (char *) (pContext->pData));

        } else if (pContext->type == SHADOW_JSON_OBJECT) {
            os_printf("with desired state: %s\n", (char *) (pContext->pData));

        } else {
            os_printf("with desired state (unknown type): %s\n", (char *) (pContext->pData));

        }

        /* check for attribute with pkey 'sensorSwitch' */
        if (strncmp(pContext->pKey, inp301x_shadow_attributes[AWS_SHADOW_ATTRIBUTE_SENSOR_SWITCH].pKey,
                        strlen(inp301x_shadow_attributes[AWS_SHADOW_ATTRIBUTE_SENSOR_SWITCH].pKey)) == 0)
        {
            /* set a flag to indicate a 'reported' JSON to be sent on this delta recieved / accepted */
            sensorSwitch_delta_callback_recieved = true;
        }

        /* check for attribute with pkey 'sensorPollInterval' */
        else if (strncmp(pContext->pKey, inp301x_shadow_attributes[AWS_SHADOW_ATTRIBUTE_SENSOR_POLL_INTERVAL].pKey,
                    strlen(inp301x_shadow_attributes[AWS_SHADOW_ATTRIBUTE_SENSOR_POLL_INTERVAL].pKey)) == 0)
        {
            /* set a flag to indicate a 'reported' JSON to be sent on this delta recieved / accepted */
            sensorPollInterval_delta_callback_recieved = true;
        }
    }
}

/**
 * Calls AWS IoT Shadow service APIs to prepare the JSON document and
 * perform an Update action to a Thing Shadow's sensorSwitch attribute
 * @param shadow_update_type desired or reported
 * @return An IoT Error Type defining successful/failed update action
 */
static IoT_Error_t UpdateSensorSwitchShadowStatus(enum shadow_update_type update_type){
    int ret;

    /* lets wait for ongoing shadow updates to conclude (if any) by yielding, before sending next update */
    while(shadowUpdateInProgress){
        ret = aws_iot_shadow_yield(gpclient, 500);
    }

    ret = aws_iot_shadow_init_json_document(JsonDocumentBuffer,
            MAX_LENGTH_OF_UPDATE_JSON_BUFFER);
    if (SUCCESS == ret) {
        if (update_type == AWS_SHADOW_UPDATE_REPORTED) {
            ret = aws_iot_shadow_add_reported(JsonDocumentBuffer,
                    MAX_LENGTH_OF_UPDATE_JSON_BUFFER, 1, &(inp301x_shadow_attributes[AWS_SHADOW_ATTRIBUTE_SENSOR_SWITCH]));
        }
        else if (update_type == AWS_SHADOW_UPDATE_DESIRED) {
            ret = aws_iot_shadow_add_desired(JsonDocumentBuffer,
                    MAX_LENGTH_OF_UPDATE_JSON_BUFFER, 1, &(inp301x_shadow_attributes[AWS_SHADOW_ATTRIBUTE_SENSOR_SWITCH]));
        }

        if (SUCCESS == ret) {
            ret = aws_iot_finalize_json_document(JsonDocumentBuffer,
                    MAX_LENGTH_OF_UPDATE_JSON_BUFFER);
            if (SUCCESS == ret) {
                os_printf("Update Shadow: %s\n", JsonDocumentBuffer);
                ret = aws_iot_shadow_update(gpclient, AWS_IOT_MY_THING_NAME,
                        JsonDocumentBuffer, ShadowUpdateStatusCallback,
                        NULL, AWS_IOT_SHADOW_ACTION_ACK_TIMEOUT_IN_SEC, true);

                if (shadowUpdateInProgress == true) {
                    /* print just for debug. this situation should not occur! */
                    os_printf("SHADOW_ACK of previous update has not yet arrived..\n");
                }

                shadowUpdateInProgress = true;
            }
        }
    }
    return ret;
}

/**
 * Calls AWS IoT Shadow service APIs to prepare the JSON document and
 * perform an Update action to a Thing Shadow's sensorPollInterval attribute
 * @param shadow_update_type desired or reported
 * @return An IoT Error Type defining successful/failed update action
 */
static IoT_Error_t UpdateSensorPollIntervalShadowStatus(enum shadow_update_type update_type){
    int ret;

    /* lets wait for ongoing shadow updates to conclude (if any) by yielding, before sending next update */
    while(shadowUpdateInProgress){
        ret = aws_iot_shadow_yield(gpclient, 500);
    }

    ret = aws_iot_shadow_init_json_document(JsonDocumentBuffer,
            MAX_LENGTH_OF_UPDATE_JSON_BUFFER);
    if (SUCCESS == ret) {

        if (update_type == AWS_SHADOW_UPDATE_REPORTED) {
            ret = aws_iot_shadow_add_reported(JsonDocumentBuffer,
                    MAX_LENGTH_OF_UPDATE_JSON_BUFFER, 1, &(inp301x_shadow_attributes[AWS_SHADOW_ATTRIBUTE_SENSOR_POLL_INTERVAL]));
        }
        else if (update_type == AWS_SHADOW_UPDATE_DESIRED) {
            ret = aws_iot_shadow_add_desired(JsonDocumentBuffer,
                    MAX_LENGTH_OF_UPDATE_JSON_BUFFER, 1, &(inp301x_shadow_attributes[AWS_SHADOW_ATTRIBUTE_SENSOR_POLL_INTERVAL]));
        }

        if (SUCCESS == ret) {
            ret = aws_iot_finalize_json_document(JsonDocumentBuffer,
                    MAX_LENGTH_OF_UPDATE_JSON_BUFFER);
            if (SUCCESS == ret) {
                os_printf("Update Shadow: %s\n", JsonDocumentBuffer);
                ret = aws_iot_shadow_update(gpclient, AWS_IOT_MY_THING_NAME,
                        JsonDocumentBuffer, ShadowUpdateStatusCallback,
                        NULL, AWS_IOT_SHADOW_ACTION_ACK_TIMEOUT_IN_SEC, true);

                if (shadowUpdateInProgress == true) {
                    /* print just for debug. this situation should not occur! */
                    os_printf("SHADOW_ACK of previous update has not yet arrived..\n");
                }

                shadowUpdateInProgress = true;
            }
        }
    }
    return ret;
}

/**
 * Calls AWS IoT Shadow service APIs to prepare the JSON document and
 * perform an Update 'reported' action to a Thing Shadow's sensor attributes
 * @return An IoT Error Type defining successful/failed update action
 */
static IoT_Error_t UpdateSensorValuesShadowStatus(){
    int ret;

    /* lets wait for ongoing shadow updates to conclude (if any) by yielding, before sending next update */
    while(shadowUpdateInProgress){
        ret = aws_iot_shadow_yield(gpclient, 500);
    }

    /* poll in the loop and print for now */
    poll_sensors(&readings);
    
    // print_sensor_readings(&readings, 1);

    /* Temp BMP */
    inp301x_shadow_params.temperature = readings.temp_bmp;

    /* pressure */
    inp301x_shadow_params.pressure = readings.pressure;

    /* optical */
    inp301x_shadow_params.opticalPower = readings.light.lux;

    /* humidity */
    inp301x_shadow_params.humidity = readings.humidity;

    ret = aws_iot_shadow_init_json_document(JsonDocumentBuffer,
            MAX_LENGTH_OF_UPDATE_JSON_BUFFER);
    if (SUCCESS == ret) {
        ret = aws_iot_shadow_add_reported(JsonDocumentBuffer,
                MAX_LENGTH_OF_UPDATE_JSON_BUFFER, 4,
                &(inp301x_shadow_attributes[AWS_SHADOW_ATTRIBUTE_TEMPERATURE]),
                &(inp301x_shadow_attributes[AWS_SHADOW_ATTRIBUTE_PRESSURE]),
                &(inp301x_shadow_attributes[AWS_SHADOW_ATTRIBUTE_OPTICAL_POWER]),
                &(inp301x_shadow_attributes[AWS_SHADOW_ATTRIBUTE_HUMIDITY]));
        if (SUCCESS == ret) {
            ret = aws_iot_finalize_json_document(JsonDocumentBuffer,
                    MAX_LENGTH_OF_UPDATE_JSON_BUFFER);
            if (SUCCESS == ret) {
                os_printf("Update Shadow: %s\n", JsonDocumentBuffer);
                ret = aws_iot_shadow_update(gpclient, AWS_IOT_MY_THING_NAME,
                        JsonDocumentBuffer, ShadowUpdateStatusCallback,
                        NULL, AWS_IOT_SHADOW_ACTION_ACK_TIMEOUT_IN_SEC, true);

                if (shadowUpdateInProgress == true) {
                    /* print just for debug. this situation should not occur! */
                    os_printf("SHADOW_ACK of previous update has not yet arrived..\n");
                }

                shadowUpdateInProgress = true;
            }
        }
    }
    return ret;
}

/* AWS Thing Certs from file system */
char *aws_root_ca;
char *aws_device_pkey;
char *aws_device_cert;

int read_certs()
{
    int root_ca_len = 0;
    int pkey_len = 0;
    int cert_len = 0;

    aws_root_ca = utils_file_get(MOUNT_PATH "certs/sensor2aws/aws_root_ca", &root_ca_len);
    //os_printf("read_certs() : root ca file size is %d bytes\n", root_ca_len);
    if(aws_root_ca == NULL)
    {
        return -1;
    }

    aws_device_pkey = utils_file_get(MOUNT_PATH "certs/sensor2aws/aws_device_pkey", &pkey_len);
    //os_printf("read_certs() : device private key file zize is %d bytes\n", pkey_len);
    if(aws_device_pkey == NULL)
    {
        return -1;
    }

    aws_device_cert = utils_file_get(MOUNT_PATH "certs/sensor2aws/aws_device_cert", &cert_len);
    //os_printf("read_certs() : device cert file size is %d bytes\n", cert_len);
    if(aws_device_cert == NULL)
    {
        return -1;
    }

    return 0;
}

void free_certs()
{
    os_free(aws_root_ca);
    os_free(aws_device_pkey);
    os_free(aws_device_cert);
}

/* Entry Point */
int main() {
    int rc;

    os_printf("Mounting file system\n");

    if(0 != utils_mount_rootfs()) {
        os_printf("Muonting the file system failed.!\n");
        while(1);
    }

    rc = read_certs();
    if(rc < 0) {
        os_printf("Reading the certs and key in file system failed, cant proceed. \n"
                "Please program needed files in file system...\n");
        return rc;
    }

    os_printf("read_certs() success\n");

    struct i2c_bus *bus = NULL;
    sensor_id_t ids = {};

    if ((rc = validate_inputs()) != 0) {
        os_printf(
                "Basic verification failed. ret:%d. Usage: "
                        " %s=<aws url> %s=<aws_port, def 8883> %s=<Thing Name>\n",
                rc, INPUT_PARAMETER_AWS_URL, INPUT_PARAMETER_AWS_PORT,
                INPUT_PARAMETER_AWS_THING_NAME);
        return rc;
    }

    /* Initializing the sensors */

    /* Initialize i2c */
    bus = init_i2c();

    /* Initialize sensors */
    init_sensors(bus);

    /* Read sensor ids */
    get_sensor_ids(&ids);

    /* Print sensor ids */
    print_sensor_ids(&ids);
    os_printf("\n");

    rc = init_platform();
    if (rc) {
        os_printf("init platform failed. ret:%d\n", rc);
        return rc;
    }

    /* Enable device suspend (deep sleep) via boot argument */
    if (os_get_boot_arg_int("suspend", 0) != 0)
        os_suspend_enable();

    no_mcast = os_get_boot_arg_int("no_mcast", 0);
    wifi_SetMulticastRX(no_mcast);

    os_sem_init(&Wifi_Connect, 0);

    if(ap_got_ip == false) {
        sem_wait = true;
        os_sem_wait(&Wifi_Connect);
    }

    /* poll for initing the variables */
    poll_sensors(&readings);
    
    /* print sensor readings */
    print_sensor_readings(&readings, 1);

    /* lets init a timer available from AWS IoT SDK Platform Adaptation Layer for T2
     * refer -- /talaria_two_aws/talaria_two_pal/t2_time.c
     */
    Timer timer;
    init_timer(&timer);

    while(1){

        /* init connection to aws iot service  */
        rc = init_and_connect_aws_iot();

        os_printf("init_and_connect_aws_iot. ret:%d\n", rc);

        if (SUCCESS != rc) {
            os_printf("init_and_connect_aws_iot failed. ret:%d\n", rc);
            os_printf("...will retry connecting again after some time..\n");

            /* retry after some time. for now, one minute wait is used for retying! */
            os_sleep_us(60000000, OS_TIMEOUT_NO_WAKEUP);

            continue;
        }

        /* In this app, force 'sensorSwitch' and 'sensorPollInterval' values as 'desired' after connect.
         *
         * In some other usecases (eg passive listening devices), at connect, after registering for delta
         * we first send a switch 'false' state update as 'reported', just to sync to the shadow attribute
         * state at cloud and get the delta (if any).
         *
         * In both the cases, later we follow the delta Callbacks to change these values at runtime!
         */

        /* Eg -- sensorSwitch as 'true' and sensorPollInterval as boot arg 'sensor_poll_interval' (in seconds) passed */
        inp301x_shadow_params.sensorOn = true;
        inp301x_shadow_params.sensorPollInterval = os_get_boot_arg_int("sensor_poll_interval", 20);

        /* lets set the 'desired' values of shadow attributes after connect
         * after reboot 'SensorSwitch' is set as true and 'SensorPollInterval' is set as passed by bootArg.
         * In runtime, T2 will recieve the delta changes done to these shadow attributes
         * if execution comes here after 'error in loop', then will retain previous values!
         */
        rc = UpdateSensorSwitchShadowStatus(AWS_SHADOW_UPDATE_DESIRED);
        rc = UpdateSensorPollIntervalShadowStatus(AWS_SHADOW_UPDATE_DESIRED);

        /* Register for delta callbacks only after setting the desired values, to avoid some unwanted corner cases */
        for (int i = 0; i < AWS_SHADOW_ATTRIBUTES_MAX_COUNT; i++)
        {
            /* register shadow delta callbacks only for the shadows attributes which have valid callbacks defined */
            if(inp301x_shadow_attributes[i].cb != NULL) {
                os_printf("Registering for Delta callbacks on shadow attributes :%s\n", inp301x_shadow_attributes[i].pKey);
                rc = aws_iot_shadow_register_delta(gpclient, &(inp301x_shadow_attributes[i]));
                if (SUCCESS != rc) {
                    os_printf("Shadow Register Delta Error ret:%d\n", rc);
                }
            }
        }

        /* send the first sensor value (if sensorOn true), and also start timer with 'sensorPollInterval',
         * so that we can use 'has_timer_expired()' to send sensor values in expected interval in future.
         */
        if (inp301x_shadow_params.sensorOn) {
            rc = UpdateSensorValuesShadowStatus();
        }
        countdown_ms(&timer, (inp301x_shadow_params.sensorPollInterval)*1000);

        /* loop and publish a change in sensors */
        while (NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc
                || SUCCESS == rc) {

            /* lets have main loop yield and sleep as 500 ms each */
            rc = aws_iot_shadow_yield(gpclient, 500);
            if (NETWORK_ATTEMPTING_RECONNECT == rc) {
                os_sleep_us(100000, OS_TIMEOUT_NO_WAKEUP);
                attemptingReconnect = true;

                /* If the client is attempting to reconnect, we will skip the rest of the loop */
                continue;
            }

            /* if previous state was NETWORK_ATTEMPTING_RECONNECT and current state is NETWORK_RECONNECTED
               then we should try to do GET shadow document.. and sync the state.
             */

            if ((attemptingReconnect == true) && (NETWORK_RECONNECTED == rc))
            {
                /* this is where we are syncing with the shadow attributes states at cloud after every reconnect attempt success!
                 * to sync, we send previous known state values in an update as 'reported', and recieve the delta (if any).
                 */

                attemptingReconnect = false;
                rc = UpdateSensorSwitchShadowStatus(AWS_SHADOW_UPDATE_REPORTED);
                rc = UpdateSensorPollIntervalShadowStatus(AWS_SHADOW_UPDATE_REPORTED);
            }

            if (sensorSwitch_delta_callback_recieved) {
                rc = UpdateSensorSwitchShadowStatus(AWS_SHADOW_UPDATE_REPORTED);
                sensorSwitch_delta_callback_recieved = false;
            }

            if (sensorPollInterval_delta_callback_recieved) {
                rc = UpdateSensorPollIntervalShadowStatus(AWS_SHADOW_UPDATE_REPORTED);

                /* restart the timer with the new 'sensorPollInterval' value recieved */
                countdown_ms(&timer, (inp301x_shadow_params.sensorPollInterval)*1000);
                sensorPollInterval_delta_callback_recieved = false;
            }

            /* use 'has_timer_expired()' and send sensor values if its time to send and if sensorSwitch is ON
             * (this way, the yield is not blocked in wait_sleep!)
             *
             * reset the timer for next send after 'sensorPollInterval'
             */

            if(has_timer_expired(&timer)) {

                /* 'sensorPollInterval' has been elasped, send sensor values if sensorSwitch is ON */
                if (inp301x_shadow_params.sensorOn) {
                    rc = UpdateSensorValuesShadowStatus();
                }

                /* restart the timer with 'sensorPollInterval', irrespective of sensor values sent or not! */
                countdown_ms(&timer, (inp301x_shadow_params.sensorPollInterval)*1000);
            }

            /* lets have main loop yield and sleep as 500 ms each, so total loop time taken is around 1 sec.
             * sensorPollInterval is in 'Seconds'
             */
            os_sleep_us(500000, OS_TIMEOUT_NO_WAKEUP);

        } /* while() ends here */

        /* if we are here, means aws_iot_shadow_yield() returned somthing other than
            NETWORK_ATTEMPTING_RECONNECT, NETWORK_RECONNECTED or SUCCESS */
        if (SUCCESS != rc) {
            os_printf("An error occurred in the loop %d\n", rc);
        }

        os_printf("Disconnecting\n");
        rc = aws_iot_shadow_disconnect(gpclient);

        if (SUCCESS != rc) {
            os_printf("Disconnect error %d\n", rc);
        }

        attemptingReconnect = false;
        shadowUpdateInProgress = false;
        sensorSwitch_delta_callback_recieved = false;
        sensorPollInterval_delta_callback_recieved = false;

        /* free 'gpclient' as it will be allocated again when init_and_connect_aws_iot() is called */
        os_free(gpclient);

        if(ap_got_ip == false) {
            sem_wait = true;
            os_sem_wait(&Wifi_Connect);
        }
    }
    return rc;
}

/**
 * Initiates the platform
 * @return Returns zero on success, negative error code from 
 * errorno.h in case of an error.
 *  Error     | Possible reason
 *  --------- | ---------------
 * -EBUSY     | A network is already configured
 * -ENOMEM    | Not enough memory
 * -EIBADF    | Badly formatted passphrase
 */
static int init_platform() {
    int ret;

    ret = wifi_main();
    if(ret != 0) {
        os_printf("main -- WiFi Connection Failed due to WCM returning error \n");
        wifi_destroy(0);
    }
    return ret;
}

/**
 * Validates if the required bootargs are present or not
 * returns zero if success OR negative value in case or error.
 */
static int validate_inputs() {
    if (NULL == os_get_boot_arg_str(INPUT_PARAMETER_AWS_URL)) {
        os_printf("[%s] missing\n", INPUT_PARAMETER_AWS_URL);
        return -1;
    }
    if (NULL == os_get_boot_arg_str(INPUT_PARAMETER_AWS_THING_NAME)) {
        os_printf("[%s] missing\n", INPUT_PARAMETER_AWS_THING_NAME);
        return -2;
    }
    return 0;
}

static int init_and_connect_aws_iot() {

    int rc;

    ShadowInitParameters_t *sp = os_zalloc(sizeof(ShadowInitParameters_t));
    sp->pHost = (char *)os_get_boot_arg_str(INPUT_PARAMETER_AWS_URL);
    sp->port = os_get_boot_arg_int(INPUT_PARAMETER_AWS_PORT, 8883);
    sp->pClientCRT = aws_device_cert;
    sp->pClientKey = aws_device_pkey;
    sp->pRootCA = aws_root_ca;

    gpclient = os_alloc(sizeof(AWS_IoT_Client));
    if(gpclient == NULL) {
        os_free(sp);
        return FAILURE;
    }

    rc = aws_iot_shadow_init(gpclient, sp);
    if (SUCCESS != rc) {
        os_free(sp);
        os_free(gpclient);
        os_printf("Shadow Connection Error ret:%d\n", rc);
        return rc;
    }

    ShadowConnectParameters_t *scp = os_zalloc(sizeof(ShadowConnectParameters_t));
    if(scp == NULL) {
        os_free(sp);
        os_free(gpclient);
        return FAILURE;
    }

    scp->pMyThingName = (char *)AWS_IOT_MY_THING_NAME;
    scp->pMqttClientId = (char *)AWS_IOT_MY_THING_NAME;
    scp->mqttClientIdLen = (uint16_t) strlen(AWS_IOT_MY_THING_NAME);

    os_printf("Shadow Connect\n");
    rc = aws_iot_shadow_connect(gpclient, scp);
    if (SUCCESS != rc) {
        os_printf("Shadow Connection Error ret:%d\n", rc);
        os_free(sp);
        os_free(scp);
        os_free(gpclient);
        return rc;
    }
    os_printf("Shadow Connected\n");
    rc = aws_iot_shadow_set_autoreconnect_status(gpclient, true);
    if (SUCCESS != rc) {
        os_printf("Unable to set Auto Reconnect to true - %d\n", rc);
        os_free(sp);
        os_free(scp);
        os_free(gpclient);
        return rc;
    }

    os_free(sp);
    os_free(scp);
    return rc;
}
