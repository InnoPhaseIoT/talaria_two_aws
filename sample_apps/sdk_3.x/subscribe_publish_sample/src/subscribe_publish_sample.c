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
 * This example takes the parameters from the aws_iot_config.h file and T2 boot arguments
 * and establishes a connection to the AWS IoT MQTT Platform. Then it subscribes and
 * publishes to the topics provided as bootArgs subscribe_topic and publish_topic.
 * If these topic bootArgs are not provided, then it defaults to 'inno_test/ctrl' as the
 * 'subscribe_topic' and 'inno_test/data' as the 'publish_topic'.
 *
 * If all the certs/keys are correct, in the T2 Console you should see alternate QoS0 and
 * QoS1 messages being published to 'publish_topic' by the application in a loop.
 *
 * if 'publishCount' in code is given a non-zero value, then it defines the number of times
 * the publish should happen. With 'publishCount' as 0, it keeps publishing in a loop.
 *
 * AWS IoT Console->Test page can be used to subscribe to 'inno_test/data' (or T2's
 * 'publish_topic' provided as the bootArg to App) to observe the messages published
 * by the App.
 *
 * AWS IoT Console->Test page can be used to publish the message to 'inno_test/ctrl'
 * (or T2's 'subscribe_topic' provided as the bootArg to App), and T2 App will receive the
 * messages and they will be seen on T2 Console.
 * JSON formatted text as shown below should be used for publishing to T2.
 *------------------------------------
 {
 "from": "AWS IoT console"
 "to": "T2"
 "msg": "Hello from AWS IoT console"
 }
 *------------------------------------

 * The application takes in the host name, port and thing name (as client-id) as must
 * provide bootArgs and publish_topic, subscribe_topic and suspend as optional bootArgs.
 * Certs and keys are stored in file system and read from app specific paths defined in the
 * sample code.
 */
#include <kernel/os.h>
#include "string.h"
#include "errno.h"
#include <wifi/wcm.h>
#include "aws_iot_log.h"
#include "aws_iot_config.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_version.h"
#include "aws_iot_json_utils.h"
#include "fs_utils.h"
#include "wifi_utils.h"
#include "osal.h"

#define HOST_ADDRESS_SIZE 255

#define INPUT_PARAMETER_AWS_URL "aws_host"
#define INPUT_PARAMETER_AWS_PORT "aws_port"
#define INPUT_PARAMETER_AWS_THING_NAME "thing_name"
#define INPUT_PARAMETER_AWS_PUBLISH_TOPIC "publish_topic"
#define INPUT_PARAMETER_AWS_SUBSCRIBE_TOPIC "subscribe_topic"

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
static int parse_received_message(char *msg_received, int payloadLen);

/**
 * @brief This parameter will avoid infinite loop of publish and exit the program after certain number of publishes
 */
static uint32_t publishCount = 0;

static void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen,
									IoT_Publish_Message_Params *params, void *pData) {
	IOT_UNUSED(pData);
	IOT_UNUSED(pClient);
	os_printf("\n<--- Message Received on Subscribed Topic [%.*s]\n", topicNameLen, topicName);

	char *msg_received;
	msg_received = osal_alloc(params->payloadLen + 1);
	memcpy(msg_received, params->payload, params->payloadLen);
	parse_received_message(msg_received, params->payloadLen);
	osal_free(msg_received);
}

static void disconnectCallbackHandler(AWS_IoT_Client *pClient, void *data) {
	os_printf("MQTT Disconnect\n");
	IoT_Error_t rc = FAILURE;

	if(NULL == pClient) {
		return;
	}

	IOT_UNUSED(data);

	if(aws_iot_is_autoreconnect_enabled(pClient)) {
		os_printf("Auto Reconnect is enabled, Reconnecting attempt will start now\n");
	} else {
		os_printf("Auto Reconnect not enabled. Starting manual reconnect...\n");
		rc = aws_iot_mqtt_attempt_reconnect(pClient);
		if(NETWORK_RECONNECTED == rc) {
			os_printf("Manual Reconnect Successful\n");
		} else {
			os_printf("Manual Reconnect Failed - %d\n", rc);
		}
	}
}


int main(int argc, char **argv) {
	bool infinitePublishFlag = true;
	char cPayload[100];
	IoT_Error_t rc = FAILURE;

	IoT_Publish_Message_Params paramsQOS0;
	IoT_Publish_Message_Params paramsQOS1;

	os_printf("\nAWS IoT SDK Version %d.%d.%d-%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

	/* takes care of platform specific initializations
	 * either returns error or blocks until we get ip from AP
	 */
	rc = init_platform();
	if (rc) {
		os_printf("init platform failed. ret:%d\n", rc);
		return rc;
	}

	IoT_Client_Init_Params *mqttInitParams;

	mqttInitParams = osal_alloc(sizeof(IoT_Client_Init_Params));
	mqttInitParams->enableAutoReconnect = false; // We enable this later below
	mqttInitParams->pHostURL = (char *)os_get_boot_arg_str(INPUT_PARAMETER_AWS_URL);
	mqttInitParams->port = os_get_boot_arg_int(INPUT_PARAMETER_AWS_PORT, 8883);
	mqttInitParams->pRootCALocation = aws_root_ca;
	mqttInitParams->pDeviceCertLocation = aws_device_cert;
	mqttInitParams->pDevicePrivateKeyLocation = aws_device_pkey;
	mqttInitParams->mqttCommandTimeout_ms = 20000;
	mqttInitParams->tlsHandshakeTimeout_ms = 5000;
	mqttInitParams->isSSLHostnameVerify = true;
	mqttInitParams->disconnectHandler = disconnectCallbackHandler;
	mqttInitParams->disconnectHandlerData = NULL;

	pmqttClient = osal_alloc(sizeof(AWS_IoT_Client));
	rc = aws_iot_mqtt_init(pmqttClient, mqttInitParams);
	if(SUCCESS != rc) {
		IOT_ERROR("aws_iot_mqtt_init returned error : %d ", rc);
		return rc;
	}

	IoT_Client_Connect_Params *connectParams = osal_alloc(sizeof(IoT_Client_Connect_Params));

	connectParams->keepAliveIntervalInSec = 600;
	connectParams->isCleanSession = true;
	connectParams->MQTTVersion = MQTT_3_1_1;
	/* we use thing-name as client-id, just to have a unique name */
	connectParams->pClientID = (char *)AWS_IOT_MY_THING_NAME;
	connectParams->clientIDLen = (uint16_t) strlen(AWS_IOT_MY_THING_NAME);
	connectParams->isWillMsgPresent = false;
	connectParams->usernameLen = 0;
	connectParams->passwordLen = 0;

	os_printf("Connecting...\n");
	os_printf("heap[%d] max contentlen[%d] sizeof IoT_Publish_Message_Params (%d)\n",
			os_avail_heap(), MBEDTLS_SSL_MAX_CONTENT_LEN, sizeof(IoT_Publish_Message_Params));

	rc = aws_iot_mqtt_connect(pmqttClient, connectParams);
	if(SUCCESS != rc) {
		IOT_ERROR("Error(%d) connecting to %s:%d", rc, mqttInitParams->pHostURL, mqttInitParams->port);
		return rc;
	}

	/*
	 * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
	 *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
	 *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
	 */
	rc = aws_iot_mqtt_autoreconnect_set_status(pmqttClient, true);
	if(SUCCESS != rc) {
		IOT_ERROR("Unable to set Auto Reconnect to true - %d", rc);
		return rc;
	}

	/* subscribe-publish-sample specific logic */
	char *subscribe_topic = (char *)os_get_boot_arg_str(INPUT_PARAMETER_AWS_SUBSCRIBE_TOPIC);
	if (!subscribe_topic) {
		subscribe_topic = "inno_test/ctrl";
	}

	os_printf("Subscribing...\n");
	rc = aws_iot_mqtt_subscribe(pmqttClient, subscribe_topic,
			strlen(subscribe_topic), QOS0, iot_subscribe_callback_handler, NULL);
	if(SUCCESS != rc) {
		IOT_ERROR("Error subscribing : %d ", rc);
		return rc;
	}

	os_printf("Subscribed to topic [%s] ret[%d] qos[%d]\n", subscribe_topic, rc, QOS0);

	int message_id = 0;
	char *publish_topic = (char *)os_get_boot_arg_str(INPUT_PARAMETER_AWS_PUBLISH_TOPIC);
	if (!publish_topic) {
		publish_topic = "inno_test/data";
	}

	paramsQOS0.qos = QOS0;
	paramsQOS0.payload = (void *) cPayload;
	paramsQOS0.isRetained = 0;

	paramsQOS1.qos = QOS1;
	paramsQOS1.payload = (void *) cPayload;
	paramsQOS1.isRetained = 0;

	if(publishCount != 0) {
		infinitePublishFlag = false;
	}

	while((NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc)
		  && (publishCount > 0 || infinitePublishFlag)) {

		//Max time the yield function will wait for read messages
		rc = aws_iot_mqtt_yield(pmqttClient, 100);
		if(NETWORK_ATTEMPTING_RECONNECT == rc) {
            vTaskDelay(100);

			// If the client is attempting to reconnect we will skip the rest of the loop.
			continue;
		}

		os_printf("sleep\n");
        vTaskDelay(10000);

		paramsQOS0.payloadLen = snprintf(cPayload, sizeof(cPayload),
										"{\"from\":\"Talaria T2\",\"to\":\"AWS\",\"msg\":\"Howdy Ho\",\"msg_id\":%d}",
										++message_id);

		os_printf("\n---> Publishing with 'Message QoS0' to Topic [%s]\n", publish_topic);
		os_printf("msg[%s]\n", cPayload);
		rc = aws_iot_mqtt_publish(pmqttClient, publish_topic, strlen(publish_topic), &paramsQOS0);

		os_printf("\nQoS0 Message Publish %s for \"msg_id\":%d. Return Status [%d]\n",
						rc ? "Failure" : "Successful", message_id, rc);

		if(publishCount > 0) {
			publishCount--;
		}

		if(publishCount == 0 && !infinitePublishFlag) {
			break;
		}

		paramsQOS1.payloadLen = snprintf(cPayload, sizeof(cPayload),
										"{\"from\":\"Talaria T2\",\"to\":\"AWS\",\"msg\":\"Howdy Ho\",\"msg_id\":%d}",
										++message_id);

		os_printf("\n---> Publishing with 'Message QoS1' to Topic [%s]\n", publish_topic);
		os_printf("msg[%s]\n", cPayload);
		rc = aws_iot_mqtt_publish(pmqttClient, publish_topic, strlen(publish_topic), &paramsQOS1);

		os_printf("\nQoS1 Message Publish %s for \"msg_id\":%d. Return Status [%d]\n",
						rc ? "Failure" : "Successful", message_id, rc);
		if (rc == MQTT_REQUEST_TIMEOUT_ERROR) {
			// if PUBACK is not recieved for a QOS1 msg before a timeout, the stack will re-publish it again. 
			// so, lets not treat 'MQTT_REQUEST_TIMEOUT_ERROR' as a critical error which breaks the loop.
			os_printf("QOS1 publish ack not received. \n");
			rc = SUCCESS;
		}

		if(publishCount > 0) {
			publishCount--;
		}
	}

	// Wait for all the messages to be received
	aws_iot_mqtt_yield(pmqttClient, 100);

	if(SUCCESS != rc) {
		IOT_ERROR("An error occurred in the loop %d", rc);
	} else {
		os_printf("Publish done\n");
	}

	return rc;
}

static int parse_received_message(char *msg_received, int payloadLen) {
	int i, r;
	jsmn_parser p;
	jsmntok_t t[16];

	jsmn_init(&p);
	r = jsmn_parse(&p, msg_received, payloadLen, t, sizeof(t) / sizeof(t[0]));
	if (r < 0) {
		os_printf("Failed to parse JSON: %d\n", r);
		return 1;
	}
	/* Assume the top-level element is an object */
	if (r < 1 || t[0].type != JSMN_OBJECT) {
		os_printf("Object expected\n");
		return 1;
	}
	for (i = 1; i < r; i++) {
		
		if (jsoneq(msg_received, &t[i], "from") == 0) {
			os_printf("- from: %.*s\n", t[i + 1].end - t[i + 1].start,
					msg_received + t[i + 1].start);
			i++;
		} else if (jsoneq(msg_received, &t[i], "to") == 0) {
			os_printf("- to: %.*s\n", t[i + 1].end - t[i + 1].start,
					msg_received + t[i + 1].start);
			i++;
		} else if (jsoneq(msg_received, &t[i], "msg") == 0) {
			os_printf("- message: %.*s\n", t[i + 1].end - t[i + 1].start,
					msg_received + t[i + 1].start);
			i++;
		} else
			i++;
	}
	return 0;
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
    vTaskDelay(2000);

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

	aws_root_ca = utils_file_get(MOUNT_PATH "certs/aws/app/aws_root_ca", &root_ca_len);
	//os_printf("read_certs() : root ca file size is %d bytes\n", root_ca_len);
	if(aws_root_ca == NULL)
	{
		return -1;
	}

	aws_device_pkey = utils_file_get(MOUNT_PATH "certs/aws/app/aws_device_pkey", &pkey_len);
	//os_printf("read_certs() : device private key file zize is %d bytes\n", pkey_len);
	if(aws_device_pkey == NULL)
	{
		return -1;
	}

	aws_device_cert = utils_file_get(MOUNT_PATH "certs/aws/app/aws_device_cert", &cert_len);
	//os_printf("read_certs() : device cert file size is %d bytes\n", cert_len);
	if(aws_device_cert == NULL)
	{
		return -1;
	}

	return 0;
}

void free_certs()
{
	osal_free(aws_root_ca);
	osal_free(aws_device_pkey);
	osal_free(aws_device_cert);
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
		os_printf("Reading the certs and key in file system failed, cant proceed. \n"
				"Please program needed files in file system...\n");
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
        vTaskDelay(1000);
	}

	return rc;
}
