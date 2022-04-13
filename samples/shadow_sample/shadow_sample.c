/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/**
 *
 * The goal of this sample application is to demonstrate the capabilities of aws iot
 * thing shadow service. This example takes the parameters from the aws_iot_config.h
 * file and T2 boot arguments and establishes a connection to the AWS IoT Shadow Service.
 *
 * This device acts as 'Connected Window' and periodically reports (once per 3 seconds)
 * the following parameters to the Thing's Classic Shadow :
 *  1. temperature of the room (double).( Note - temperature changes are 'simulated'.)
 *  2. open/close status of the window(bool). (open or close as "windowOpen" true/false.)
 *
 * The device also 'listens' for a shadow state change for "windowOpen" to act on commands
 * from the cloud.
 *
 * Two variables from a device's perspective are, double temperature and bool windowOpen.
 * So, the corresponding 'Shadow Json Document' in the cloud would be --
 *-------------------------
 {
   "reported": {
     "temperature": 32,
     "windowOpen": false
   },
   "desired": {
     "windowOpen": false
   }
 }
 *-------------------------
 * 
 * The device opens or closes the window based on json object "windowOpen" data [true/false]
 * received as part of shadow 'delta' callback. So a jsonStruct_t object 'windowActuator' is
 * createdwith 'pKey = "windowOpen"' of 'type = SHADOW_JSON_BOOL' and a delta callback
 * 'windowActuate_Callback'. The App then registers for a 'Delta callback' for 'windowActuator'
 * and receive a callback whenever a state change happens for this object in Thing Shadow.
 *
 * (For example : Based on temperature reported, a logic running in the aws cloud infra can
 * automatically decide when to open or close the window by changing the 'desired' state of
 * "windowOpen". OR a manual input by the end-user using a web app/ phone app can change the
 * 'desired' state of "windowOpen".)
 *
 * for the Sample App, change in desired section can be done manually as shown below :
 * Assume the reported and desired states of "windowOpen" are 'false' as shown in above JSON.
 * From AWS IoT Console's Thing Shadow, if the 'desired' section is edited /saved as shown below
 *-------------------------
   "desired": {
     "windowOpen": false
   }
 *-------------------------
 * then a delta callback will be received by the App, as now there is a difference between
 * desired vs reported.
 * Received Delta message
 *-------------------------
   "delta": {
     "windowOpen": true
   }
 *-------------------------
 * This delta message means the desired windowOpen variable has changed to "true".
 *
 * The application will 'act' on this delta message and publish back windowOpen as true as part
 * of the 'reported' section of the shadow document from the device, when the next periodic
 * temperature value is reported.
 *-------------------------
   "reported": {
     "temperature": 28,
     "windowOpen": true
   }
 *-------------------------
 *
 * This 'update reported' message will remove the 'delta' that was created, as now the desired
 * and reported states will match. If this delta message is not removed, then the 
 * AWS IoT Thing Shadow is always going to have a 'delta', and will keep sending delta calllback
 * any time an update is applied to the Shadow.
 *
 * @note Ensure the buffer sizes in aws_iot_config.h are big enough to receive the delta message.
 * The delta message will also contain the metadata with the timestamps.
 *
 * The application takes in the host name, port and thing name as must provide bootArgs and
 * suspend as optional bootArgs.
 * Certs and keys are stored in rootFS and read from app specific paths defined in the
 * sample code.
 */
#include <kernel/os.h>
#include "string.h"
#include "errno.h"
#include <wifi/wcm.h>
#include "aws_iot_log.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_shadow_interface.h"
#include "aws_iot_json_utils.h"
#include "aws_iot_version.h"
#include "fs_utils.h"
#include "wifi_utils.h"

#define ROOMTEMPERATURE_UPPERLIMIT 32.0f
#define ROOMTEMPERATURE_LOWERLIMIT 25.0f
#define STARTING_ROOMTEMPERATURE ROOMTEMPERATURE_LOWERLIMIT

#define MAX_LENGTH_OF_UPDATE_JSON_BUFFER 200

#define INPUT_PARAMETER_AWS_URL "aws_host"
#define INPUT_PARAMETER_AWS_PORT "aws_port"
#define INPUT_PARAMETER_AWS_THING_NAME "thing_name"

#define AWS_IOT_MY_THING_NAME os_get_boot_arg_str(INPUT_PARAMETER_AWS_THING_NAME)

static struct wcm_handle *h = NULL;
static bool ap_link_up = false;
static bool ap_got_ip = false;

OS_APPINFO {.stack_size=4096};

static int init_platform();
static AWS_IoT_Client *pmqttClient;

char *aws_root_ca;
char *aws_device_pkey;
char *aws_device_cert;

static void simulateRoomTemperature(float *pRoomTemperature) {
	static float deltaChange;

	if(*pRoomTemperature >= ROOMTEMPERATURE_UPPERLIMIT) {
		deltaChange = -0.5f;
	} else if(*pRoomTemperature <= ROOMTEMPERATURE_LOWERLIMIT) {
		deltaChange = 0.5f;
	}

	*pRoomTemperature += deltaChange;
}

static void ShadowUpdateStatusCallback(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
								const char *pReceivedJsonDocument, void *pContextData) {
	IOT_UNUSED(pThingName);
	IOT_UNUSED(action);
	IOT_UNUSED(pReceivedJsonDocument);
	IOT_UNUSED(pContextData);

	if(SHADOW_ACK_TIMEOUT == status) {
		os_printf("Update Timeout--\n");
	} else if(SHADOW_ACK_REJECTED == status) {
		os_printf("Update RejectedXX\n");
	} else if(SHADOW_ACK_ACCEPTED == status) {
		os_printf("Update Accepted !!\n");
	}
}

static void windowActuate_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
	IOT_UNUSED(pJsonString);
	IOT_UNUSED(JsonStringDataLen);

	if(pContext != NULL) {
		os_printf("Delta - Window state changed to %d\n", *(bool *) (pContext->pData));
	}
}

int main() {
	IoT_Error_t rc = FAILURE;

	char *JsonDocumentBuffer = os_alloc(MAX_LENGTH_OF_UPDATE_JSON_BUFFER);
	size_t sizeOfJsonDocumentBuffer = MAX_LENGTH_OF_UPDATE_JSON_BUFFER;

	float temperature = 0.0;

	bool windowOpen = false;
	jsonStruct_t *windowActuator = os_zalloc(sizeof(jsonStruct_t));
	windowActuator->cb = windowActuate_Callback;
	windowActuator->pData = &windowOpen;
	windowActuator->dataLength = sizeof(bool);
	windowActuator->pKey = "windowOpen";
	windowActuator->type = SHADOW_JSON_BOOL;

	jsonStruct_t *temperatureHandler = os_alloc(sizeof(jsonStruct_t));
	temperatureHandler->cb = NULL;
	temperatureHandler->pKey = "temperature";
	temperatureHandler->pData = &temperature;
	temperatureHandler->dataLength = sizeof(float);
	temperatureHandler->type = SHADOW_JSON_FLOAT;

	os_printf("\nAWS IoT SDK Version %d.%d.%d-%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

	/* takes care of platform specific initializations
	 * either returns error or blocks until we get ip from AP
	 */
	rc = init_platform();
	if (rc) {
		os_printf("init platform failed. ret:%d\n", rc);
		return rc;
	}

	// initialize the mqtt client
	pmqttClient = os_alloc(sizeof(AWS_IoT_Client));

	ShadowInitParameters_t *sp = os_zalloc(sizeof(ShadowInitParameters_t));
	sp->pHost = (char *)os_get_boot_arg_str(INPUT_PARAMETER_AWS_URL);
	sp->port = os_get_boot_arg_int(INPUT_PARAMETER_AWS_PORT, 8883);
	sp->pClientCRT = aws_device_cert;
	sp->pClientKey = aws_device_pkey;
	sp->pRootCA = aws_root_ca;
	sp->enableAutoReconnect = false;
	sp->disconnectHandler = NULL;

	os_printf("Shadow Init");
	rc = aws_iot_shadow_init(pmqttClient, sp);
	if(SUCCESS != rc) {
		IOT_ERROR("Shadow Connection Error");
		return rc;
	}

	ShadowConnectParameters_t *scp = os_zalloc(sizeof(ShadowConnectParameters_t));
	scp->pMyThingName = (char *)AWS_IOT_MY_THING_NAME;
	/* we use thing-name as client-id, just to have a unique name */
	scp->pMqttClientId = (char *)AWS_IOT_MY_THING_NAME;
	scp->mqttClientIdLen = (uint16_t) strlen(AWS_IOT_MY_THING_NAME);

	os_printf("Shadow Connect");
	rc = aws_iot_shadow_connect(pmqttClient, scp);
	if(SUCCESS != rc) {
		IOT_ERROR("Shadow Connection Error");
		return rc;
	}

	/*
	 * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
	 *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
	 *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
	 */
	rc = aws_iot_shadow_set_autoreconnect_status(pmqttClient, true);
	if(SUCCESS != rc) {
		IOT_ERROR("Unable to set Auto Reconnect to true - %d", rc);
		return rc;
	}

	rc = aws_iot_shadow_register_delta(pmqttClient, windowActuator);

	if(SUCCESS != rc) {
		IOT_ERROR("Shadow Register Delta Error");
	}

	temperature = STARTING_ROOMTEMPERATURE;

	// loop and publish a change in temperature
	while(NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc) {
		rc = aws_iot_shadow_yield(pmqttClient, 500);
		if(NETWORK_ATTEMPTING_RECONNECT == rc) {
			os_sleep_us(100000, OS_TIMEOUT_NO_WAKEUP);
			// If the client is attempting to reconnect we will skip the rest of the loop.
			continue;
		}

		os_printf("\nOn Device: window state %s\n", windowOpen ? "true" : "false");
		simulateRoomTemperature(&temperature);

		rc = aws_iot_shadow_init_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
		if(SUCCESS == rc) {
			rc = aws_iot_shadow_add_reported(JsonDocumentBuffer, sizeOfJsonDocumentBuffer, 2, temperatureHandler,
											 windowActuator);
			if(SUCCESS == rc) {
				rc = aws_iot_finalize_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
				if(SUCCESS == rc) {
					os_printf("Update Shadow: %s\n", JsonDocumentBuffer);
					rc = aws_iot_shadow_update(pmqttClient, AWS_IOT_MY_THING_NAME, JsonDocumentBuffer,
											   ShadowUpdateStatusCallback, NULL, 10, true);
				}
			}
		}

		os_sleep_us(3000000, OS_TIMEOUT_NO_WAKEUP);
	}

	if(SUCCESS != rc) {
		IOT_ERROR("An error occurred in the loop %d", rc);
	}

	os_printf("Disconnecting\n");
	rc = aws_iot_shadow_disconnect(pmqttClient);

	if(SUCCESS != rc) {
		IOT_ERROR("Disconnect error %d", rc);
	}

	os_free(sp);
	os_free(scp);
	os_free(pmqttClient);

	return rc;
}

/*-------- t2 platform init specific functions ------------*/
/**
 * WCM notification callback function pointer
 */
static void wcm_notify_callback(void *ctx, struct os_msg *msg)
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
            ap_got_ip = true;
            break;

        case(WCM_NOTIFY_MSG_DISCONNECT_DONE):
            os_printf("wcm_notify_cb to App Layer - WCM_NOTIFY_MSG_DISCONNECT_DONE\n");
            ap_got_ip = false;
            break;

        case(WCM_NOTIFY_MSG_CONNECTED):
            os_printf("wcm_notify_cb to App Layer - WCM_NOTIFY_MSG_CONNECTED\n");
            break;

        default:
            break;
    }
    os_msg_release(msg);
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
        os_printf(" wcm_notify_enable failed.\n");
        return -ENOMEM;
    }
    os_sleep_us(2000000, OS_TIMEOUT_NO_WAKEUP);

    wcm_notify_enable(h, wcm_notify_callback, NULL);

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
    if(rval < 0) {
        pr_err("network connection trial Failed, wcm_auto_connect returned %d\n", rval);
        /* can fail due to, already busy, no memory */
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
        rval = wcm_auto_connect(h, 0);
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

int read_certs()
{
	int root_ca_len = 0;
	int pkey_len = 0;
	int cert_len = 0;

	aws_root_ca = utils_file_get("/root/certs/aws/app/aws_root_ca", &root_ca_len);
	//os_printf("read_certs() : root ca file size is %d bytes\n", root_ca_len);
	if(aws_root_ca == NULL)
	{
		return -1;
	}

	aws_device_pkey = utils_file_get("/root/certs/aws/app/aws_device_pkey", &pkey_len);
	//os_printf("read_certs() : device private key file zize is %d bytes\n", pkey_len);
	if(aws_device_pkey == NULL)
	{
		return -1;
	}

	aws_device_cert = utils_file_get("/root/certs/aws/app/aws_device_cert", &cert_len);
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

/**
 * takes care of platform specific initializations
 * blocks until we get ip from AP
 * @return Returns zero on success, negative error code from 
 * errorno.h in case of an error.
 */
int init_platform() {

	int rc = 0;

	os_printf("Mounting file system\n");
	rc = utils_mount_rootfs();

	rc = read_certs();
	if(rc < 0) {
		os_printf("Reading the certs and key in rooFS failed, cant proceed. \n"
				"Please program needed files in rootFS...\n");
		return rc;
	}

	os_printf("read_certs() success\n");

	if ((rc = validate_inputs()) != 0) {
		os_printf("Basic verification failed. ret:%d. Usage: %s=<aws url> %s=<aws_port, def 8883> %s=<Thing Name>\n",
				rc, INPUT_PARAMETER_AWS_URL, INPUT_PARAMETER_AWS_PORT, INPUT_PARAMETER_AWS_THING_NAME);
		return rc;
	}

    rc = wifi_main();
    if(rc != 0) {
        os_printf("main -- WiFi Connection Failed due to WCM returning error \n");
        wifi_destroy(0);
		return rc;
    }

	/* Enable device suspend (deep sleep) via boot argument */
	if (os_get_boot_arg_int("suspend", 0) != 0)
		os_suspend_enable();

	/* wait for wcm callback to confirm we got ip */
	while(!ap_got_ip) {
		os_sleep_us(1000000, OS_TIMEOUT_NO_WAKEUP);
	}

	return rc;
}
