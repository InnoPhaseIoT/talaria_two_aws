/******************************************************************************
  * @file   aws_iot_config.h
  *
  * 
       
  ******************************************************************************
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
  ******************************************************************************
  */

/**
 * @file aws_iot_config.h
 * @brief AWS IoT specific configuration file
 */

#ifndef AWS_IOT_CONFIG_H_
#define AWS_IOT_CONFIG_H_

// MQTT PubSub
#ifndef DISABLE_IOT_JOBS
#define AWS_IOT_MQTT_RX_BUF_LEN 2048 ///< Any message that comes into the device should be less than this buffer size. If a received message is bigger than this buffer size the message will be dropped.
#else
#define AWS_IOT_MQTT_RX_BUF_LEN 2048
#endif
#define AWS_IOT_MQTT_TX_BUF_LEN 512 ///< Any time a message is sent out through the MQTT layer. The message is copied into this buffer anytime a publish is done. This will also be used in the case of Thing Shadow
#define AWS_IOT_MQTT_NUM_SUBSCRIBE_HANDLERS 8 ///< Maximum number of topic filters the MQTT client can handle at any given time. This should be increased appropriately when using Thing Shadow

// Shadow and Job common configs
#define MAX_SIZE_OF_UNIQUE_CLIENT_ID_BYTES 80  ///< Maximum size of the Unique Client Id. For More info on the Client Id refer \ref response "Acknowledgments"
#define MAX_SIZE_CLIENT_ID_WITH_SEQUENCE MAX_SIZE_OF_UNIQUE_CLIENT_ID_BYTES + 10 ///< This is size of the extra sequence number that will be appended to the Unique client Id
#define MAX_SIZE_CLIENT_TOKEN_CLIENT_SEQUENCE MAX_SIZE_CLIENT_ID_WITH_SEQUENCE + 20 ///< This is size of the the total clientToken key and value pair in the JSON
#define MAX_SIZE_OF_THING_NAME 32 ///< The Thing Name should not be bigger than this value. Modify this if the Thing Name needs to be bigger

// Thing Shadow specific configs
#define SHADOW_MAX_SIZE_OF_RX_BUFFER (AWS_IOT_MQTT_RX_BUF_LEN+1) ///< Maximum size of the SHADOW buffer to store the received Shadow message, including terminating NULL byte.
#define MAX_ACKS_TO_COMEIN_AT_ANY_GIVEN_TIME 10 ///< At Any given time we will wait for this many responses. This will correlate to the rate at which the shadow actions are requested
#define MAX_THINGNAME_HANDLED_AT_ANY_GIVEN_TIME 10 ///< We could perform shadow action on any thing Name and this is maximum Thing Names we can act on at any given time
#define MAX_JSON_TOKEN_EXPECTED 120 ///< These are the max tokens that is expected to be in the Shadow JSON document. Include the metadata that gets published
#define MAX_SHADOW_TOPIC_LENGTH_WITHOUT_THINGNAME 60 ///< All shadow actions have to be published or subscribed to a topic which is of the format $aws/things/{thingName}/shadow/update/accepted. This refers to the size of the topic without the Thing Name
#define MAX_SHADOW_TOPIC_LENGTH_BYTES MAX_SHADOW_TOPIC_LENGTH_WITHOUT_THINGNAME + MAX_SIZE_OF_THING_NAME ///< This size includes the length of topic with Thing Name

// Job specific configs
#ifndef DISABLE_IOT_JOBS
#define MAX_SIZE_OF_JOB_ID 64
#define MAX_JOB_JSON_TOKEN_EXPECTED 120
#define MAX_SIZE_OF_JOB_REQUEST AWS_IOT_MQTT_TX_BUF_LEN

#define MAX_JOB_TOPIC_LENGTH_WITHOUT_JOB_ID_OR_THING_NAME 40
#define MAX_JOB_TOPIC_LENGTH_BYTES MAX_JOB_TOPIC_LENGTH_WITHOUT_JOB_ID_OR_THING_NAME + MAX_SIZE_OF_THING_NAME + MAX_SIZE_OF_JOB_ID + 2
#endif

// Auto Reconnect specific config
#define AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL 1000 ///< Minimum time before the First reconnect attempt is made as part of the exponential back-off algorithm
#define AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL 128000 ///< Maximum time interval after which exponential back-off will stop attempting to reconnect.

#define DISABLE_METRICS true ///< Disable the collection of metrics by setting this to true

// TLS configs
#define IOT_SSL_READ_TIMEOUT_MS 10 ///< Timeout associated with underlying socket of TLS connection (set by mbedtls_ssl_conf_read_timeout)
#define IOT_SSL_READ_RETRY_TIMEOUT_MS 5000 ///< Minimum elapsed time before returning from iot_tls_read when pending data has not yet been received
#define IOT_SSL_WRITE_RETRY_TIMEOUT_MS 5000 ///< Minimum elapsed time before returning from iot_tls_write when pending data has not yet been written

#endif /* AWS_IOT_CONFIG_H_ */
