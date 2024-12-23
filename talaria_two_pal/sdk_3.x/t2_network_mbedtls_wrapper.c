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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <string.h>

#include "aws_iot_config.h"


#include <timer_platform.h>
#include <network_interface.h>

#include "aws_iot_error.h"
#include "aws_iot_log.h"
#include "network_interface.h"
#include "network_platform.h"

#include "osal.h"

/* This is the value used for ssl read timeout */
#ifndef IOT_SSL_READ_TIMEOUT_MS
	#define IOT_SSL_READ_TIMEOUT_MS 10
#endif

/* When this much time has elapsed after receiving MBEDTLS_ERR_SSL_WANT_READ
 * or MBEDTLS_ERR_SSL_WANT_WRITE, then iot_tls_write will return
 * NETWORK_SSL_WRITE_TIMEOUT_ERROR. */
#ifndef IOT_SSL_WRITE_RETRY_TIMEOUT_MS
	#define IOT_SSL_WRITE_RETRY_TIMEOUT_MS 5000
#endif

/* When this much time has elapsed after receiving MBEDTLS_ERR_SSL_WANT_READ,
 * MBEDTLS_ERR_SSL_WANT_WRITE, or MBEDTLS_ERR_SSL_TIMEOUT, then iot_tls_read
 * will return NETWORK_SSL_READ_TIMEOUT_ERROR. */
#ifndef IOT_SSL_READ_RETRY_TIMEOUT_MS
	#define IOT_SSL_READ_RETRY_TIMEOUT_MS 5000
#endif

/* This defines the value of the debug buffer that gets allocated.
 * The value can be altered based on memory constraints
 */
#ifdef ENABLE_IOT_DEBUG
#define MBEDTLS_DEBUG_BUFFER_SIZE 512
#endif

/*
 * This is a function to do further verification if needed on the cert received
 */

static int _iot_tls_verify_cert(void *data, mbedtls_x509_crt *crt, int depth, uint32_t *flags) {
	char *buf = osal_alloc(1024);
	((void) data);

	os_printf("  Verify requested for (Depth %d):\n", depth);
	mbedtls_x509_crt_info(buf, sizeof(buf) - 1, "", crt);

	if((*flags) == 0) {
		os_printf("    This certificate has no flags\n");
	} else {
		os_printf(buf, sizeof(buf), "  ! ", *flags);
		os_printf("%s\n", buf);
	}
	osal_free(buf);
	return 0;
}

void _iot_tls_set_connect_params(Network *pNetwork, char *pRootCALocation, char *pDeviceCertLocation,
								 char *pDevicePrivateKeyLocation, char *pDestinationURL,
								 uint16_t destinationPort, uint32_t timeout_ms, bool ServerVerificationFlag) {
	pNetwork->tlsConnectParams.DestinationPort = destinationPort;
	pNetwork->tlsConnectParams.pDestinationURL = pDestinationURL;
	pNetwork->tlsConnectParams.pDeviceCertLocation = pDeviceCertLocation;
	pNetwork->tlsConnectParams.pDevicePrivateKeyLocation = pDevicePrivateKeyLocation;
	pNetwork->tlsConnectParams.pRootCALocation = pRootCALocation;
	pNetwork->tlsConnectParams.timeout_ms = timeout_ms;
	pNetwork->tlsConnectParams.ServerVerificationFlag = ServerVerificationFlag;
}

IoT_Error_t iot_tls_init(Network *pNetwork, char *pRootCALocation, char *pDeviceCertLocation,
						 char *pDevicePrivateKeyLocation, char *pDestinationURL,
						 uint16_t destinationPort, uint32_t timeout_ms, bool ServerVerificationFlag) {
	_iot_tls_set_connect_params(pNetwork, pRootCALocation, pDeviceCertLocation, pDevicePrivateKeyLocation,
								pDestinationURL, destinationPort, timeout_ms, ServerVerificationFlag);

	pNetwork->connect = iot_tls_connect;
	pNetwork->read = iot_tls_read;
	pNetwork->write = iot_tls_write;
	pNetwork->disconnect = iot_tls_disconnect;
	pNetwork->isConnected = iot_tls_is_connected;
	pNetwork->destroy = iot_tls_destroy;

	pNetwork->tlsDataParams.flags = 0;

	return SUCCESS;
}

IoT_Error_t iot_tls_is_connected(Network *pNetwork) {
	/* Use this to add implementation which can check for physical layer disconnect */
	return NETWORK_PHYSICAL_LAYER_CONNECTED;
}

IoT_Error_t iot_tls_connect(Network *pNetwork, TLSConnectParams *params) {
	int ret = 0;
	const char *pers = "aws_iot_tls_wrapper";
	TLSDataParams *tlsDataParams = NULL;
	char portBuffer[6];

#if 1
	const char *alpnProtocols[] = { "x-amzn-mqtt-ca", NULL };
#endif

#ifdef ENABLE_IOT_DEBUG
	unsigned char buf[MBEDTLS_DEBUG_BUFFER_SIZE];
#endif

	if(NULL == pNetwork) {
		return NULL_VALUE_ERROR;
	}

	if(NULL != params) {
		_iot_tls_set_connect_params(pNetwork, params->pRootCALocation, params->pDeviceCertLocation,
									params->pDevicePrivateKeyLocation, params->pDestinationURL,
									params->DestinationPort, params->timeout_ms, params->ServerVerificationFlag);
	}

	tlsDataParams = &(pNetwork->tlsDataParams);

	mbedtls_net_init(&(tlsDataParams->server_fd));
	mbedtls_ssl_init(&(tlsDataParams->ssl));
	mbedtls_ssl_config_init(&(tlsDataParams->conf));
	mbedtls_ctr_drbg_init(&(tlsDataParams->ctr_drbg));
	mbedtls_x509_crt_init(&(tlsDataParams->cacert));
	mbedtls_x509_crt_init(&(tlsDataParams->clicert));
	mbedtls_pk_init(&(tlsDataParams->pkey));

	os_printf("\n  . Seeding the random number generator...\n");
	mbedtls_entropy_init(&(tlsDataParams->entropy));
	if((ret = mbedtls_ctr_drbg_seed(&(tlsDataParams->ctr_drbg), mbedtls_entropy_func, &(tlsDataParams->entropy),
									(const unsigned char *) pers, strlen(pers))) != 0) {
		IOT_ERROR(" failed\n  ! mbedtls_ctr_drbg_seed returned -0x%x\n", -ret);
		return NETWORK_MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;
	}

	os_printf("  . Loading the CA root certificate...\n");
	ret = mbedtls_x509_crt_parse(&(tlsDataParams->cacert), (const unsigned char*) pNetwork->tlsConnectParams.pRootCALocation,
									strlen(pNetwork->tlsConnectParams.pRootCALocation) + 1);
	if(ret < 0) {
		IOT_ERROR(" failed\n  !  mbedtls_x509_crt_parse returned -0x%x while parsing root cert\n\n", -ret);
		return NETWORK_X509_ROOT_CRT_PARSE_ERROR;
	}
	os_printf("  Root Done (%d skipped)\n", ret);

	os_printf("  . Loading the client cert and key. size TLSDataParams:%d\n", sizeof(TLSDataParams));

	ret = mbedtls_x509_crt_parse(&(tlsDataParams->clicert), (const unsigned char*) pNetwork->tlsConnectParams.pDeviceCertLocation,
									strlen(pNetwork->tlsConnectParams.pDeviceCertLocation) + 1);
	if(ret != 0) {
		IOT_ERROR(" failed\n  !  mbedtls_x509_crt_parse returned -0x%x while parsing device cert\n\n", -ret);
		return NETWORK_X509_DEVICE_CRT_PARSE_ERROR;
	}

	os_printf("  Loading the client cert done.... ret[%d]\n", ret);

	ret = mbedtls_pk_parse_key(&(tlsDataParams->pkey), (const unsigned char*) pNetwork->tlsConnectParams.pDevicePrivateKeyLocation,
								strlen(pNetwork->tlsConnectParams.pDevicePrivateKeyLocation) + 1, NULL, 0);
	if(ret != 0) {
		IOT_ERROR(" failed\n  !  mbedtls_pk_parse_key returned -0x%x while parsing private key\n\n", -ret);
		return NETWORK_PK_PRIVATE_KEY_PARSE_ERROR;
	}

	os_printf("  Loading the client pkey done.... ret[%d]\n", ret);
    os_printf("  ok\n");

	snprintf(portBuffer, 6, "%d", pNetwork->tlsConnectParams.DestinationPort);
	os_printf("  . Connecting to %s/%s...\n", pNetwork->tlsConnectParams.pDestinationURL, portBuffer);
	if((ret = mbedtls_net_connect(&(tlsDataParams->server_fd), pNetwork->tlsConnectParams.pDestinationURL,
								  portBuffer, MBEDTLS_NET_PROTO_TCP)) != 0) {
		IOT_ERROR(" failed\n  ! mbedtls_net_connect returned [-0x%x] uRL[%s] port[%s]\n\n",
				    -ret, pNetwork->tlsConnectParams.pDestinationURL, portBuffer);
		switch(ret) {
			case MBEDTLS_ERR_NET_SOCKET_FAILED:
				return NETWORK_ERR_NET_SOCKET_FAILED;
			case MBEDTLS_ERR_NET_UNKNOWN_HOST:
				return NETWORK_ERR_NET_UNKNOWN_HOST;
			case MBEDTLS_ERR_NET_CONNECT_FAILED:
			default:
				return NETWORK_ERR_NET_CONNECT_FAILED;
		};
	}

	ret = mbedtls_net_set_block(&(tlsDataParams->server_fd));
	if(ret != 0) {
		IOT_ERROR(" failed\n  ! net_set_(non)block() returned -0x%x\n\n", -ret);
		return SSL_CONNECTION_ERROR;
	}

	os_printf("  ok\n");

	os_printf("  . Setting up the SSL/TLS structure...\n");
	if((ret = mbedtls_ssl_config_defaults(&(tlsDataParams->conf), MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
										  MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
		IOT_ERROR(" failed\n  ! mbedtls_ssl_config_defaults returned -0x%x\n\n", -ret);
		return SSL_CONNECTION_ERROR;
	}

	mbedtls_ssl_conf_verify(&(tlsDataParams->conf), _iot_tls_verify_cert, NULL);
	if(pNetwork->tlsConnectParams.ServerVerificationFlag == true) {
		mbedtls_ssl_conf_authmode(&(tlsDataParams->conf), MBEDTLS_SSL_VERIFY_REQUIRED);
	} else {
		os_printf("  verification is optional\n");
		mbedtls_ssl_conf_authmode(&(tlsDataParams->conf), MBEDTLS_SSL_VERIFY_OPTIONAL);
	}
	mbedtls_ssl_conf_rng(&(tlsDataParams->conf), mbedtls_ctr_drbg_random, &(tlsDataParams->ctr_drbg));

	mbedtls_ssl_conf_ca_chain(&(tlsDataParams->conf), &(tlsDataParams->cacert), NULL);
	if((ret = mbedtls_ssl_conf_own_cert(&(tlsDataParams->conf), &(tlsDataParams->clicert), &(tlsDataParams->pkey))) != 0) {
		IOT_ERROR(" failed\n  ! mbedtls_ssl_conf_own_cert returned %d\n\n", ret);
		return SSL_CONNECTION_ERROR;
	}

	mbedtls_ssl_conf_read_timeout(&(tlsDataParams->conf), 2 * pNetwork->tlsConnectParams.timeout_ms);

#if 1
	/* Use the AWS IoT ALPN extension for MQTT if port 443 is requested. */

	if(443 == pNetwork->tlsConnectParams.DestinationPort) {
		if((ret = mbedtls_ssl_conf_alpn_protocols(&(tlsDataParams->conf), alpnProtocols)) != 0) {
			IOT_ERROR(" failed\n  ! mbedtls_ssl_conf_alpn_protocols returned -0x%x\n\n", -ret);
			return SSL_CONNECTION_ERROR;
		}
	}
#endif

	/* Assign the resulting configuration to the SSL context. */
	if((ret = mbedtls_ssl_setup(&(tlsDataParams->ssl), &(tlsDataParams->conf))) != 0) {
		IOT_ERROR(" failed\n  ! mbedtls_ssl_setup returned -0x%x\n\n", -ret);
		return SSL_CONNECTION_ERROR;
	}
	if((ret = mbedtls_ssl_set_hostname(&(tlsDataParams->ssl), pNetwork->tlsConnectParams.pDestinationURL)) != 0) {
		IOT_ERROR(" failed\n  ! mbedtls_ssl_set_hostname returned %d\n\n", ret);
		return SSL_CONNECTION_ERROR;
	}
	os_printf("  SSL state connect : %d \n", tlsDataParams->ssl.state);
	mbedtls_ssl_set_bio(&(tlsDataParams->ssl), &(tlsDataParams->server_fd), mbedtls_net_send, NULL,
						mbedtls_net_recv_timeout);
	os_printf("  ok\n");

	os_printf("  SSL state connect : %d \n", tlsDataParams->ssl.state);
	os_printf("  . Performing the SSL/TLS handshake... \n");

	while((ret = mbedtls_ssl_handshake(&(tlsDataParams->ssl))) != 0) {
		if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			IOT_ERROR(" failed\n  ! mbedtls_ssl_handshake returned -0x%x\n", -ret);
			if(ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
				IOT_ERROR("    Unable to verify the server's certificate. "
							  "Either it is invalid,\n"
							  "    or you didn't set ca_file or ca_path "
							  "to an appropriate value.\n"
							  "    Alternatively, you may want to use "
							  "auth_mode=optional for testing purposes.\n");
			}
			return SSL_CONNECTION_ERROR;
		}
	}

	os_printf("  SSL/TLS Handshake DONE.. ret:%d\n", ret);
	os_printf("  ok\n    [ Protocol is %s ]\n    [ Ciphersuite is %s ]\n", mbedtls_ssl_get_version(&(tlsDataParams->ssl)),
		  		mbedtls_ssl_get_ciphersuite(&(tlsDataParams->ssl)));
	if((ret = mbedtls_ssl_get_record_expansion(&(tlsDataParams->ssl))) >= 0) {
		os_printf("    [ Record expansion is %d ]\n", ret);
	} else {
		os_printf("    [ Record expansion is unknown (compression) ]\n");
	}

	os_printf("  . Verifying peer X.509 certificate...\n");

	if(pNetwork->tlsConnectParams.ServerVerificationFlag == true) {
		char *vrfy_buf = osal_alloc(512);
		if((tlsDataParams->flags = mbedtls_ssl_get_verify_result(&(tlsDataParams->ssl))) != 0) {
			IOT_ERROR(" failed\n");
			mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", tlsDataParams->flags);
			IOT_ERROR("%s\n", vrfy_buf);
			ret = SSL_CONNECTION_ERROR;
		} else {
			os_printf("  ok\n");
			ret = SUCCESS;
		}
		osal_free(vrfy_buf);
	} else {
		os_printf("  Server Verification skipped\n");
		ret = SUCCESS;
	}

#ifdef ENABLE_IOT_DEBUG
	if(mbedtls_ssl_get_peer_cert(&(tlsDataParams->ssl)) != NULL) {
		IOT_DEBUG("  . Peer certificate information    ...\n");
		mbedtls_x509_crt_info((char *) buf, sizeof(buf) - 1, "      ", mbedtls_ssl_get_peer_cert(&(tlsDataParams->ssl)));
		IOT_DEBUG("%s\n", buf);
	}
#endif

	mbedtls_ssl_conf_read_timeout(&(tlsDataParams->conf), IOT_SSL_READ_TIMEOUT_MS);

#ifdef IOT_SSL_SOCKET_NON_BLOCKING
	mbedtls_net_set_nonblock(&(tlsDataParams->server_fd));
#endif

	return (IoT_Error_t) ret;
}

IoT_Error_t iot_tls_write(Network *pNetwork, unsigned char *pMsg, size_t len, Timer *timer, size_t *written_len) {
	mbedtls_ssl_context *pSsl = &(pNetwork->tlsDataParams.ssl);
	size_t txLen = 0U;
	int ret = 0;
	/* This timer checks for a timeout whenever MBEDTLS_ERR_SSL_WANT_READ
	 * or MBEDTLS_ERR_SSL_WANT_WRITE are returned by mbedtls_ssl_write.
	 * Timeout is specified by IOT_SSL_WRITE_RETRY_TIMEOUT_MS. */
	Timer writeTimer;

	/* This variable is unused */
	(void) timer;

	/* The timer must be started in case no bytes are written on the first try */
	init_timer(&writeTimer);
	countdown_ms(&writeTimer, IOT_SSL_WRITE_RETRY_TIMEOUT_MS);

	while(len > 0U) {
		ret = mbedtls_ssl_write(pSsl, pMsg, len);

		if(ret > 0) {
			if((size_t) ret > len) {
				IOT_ERROR("More bytes written than requested\n\n");
				return NETWORK_SSL_WRITE_ERROR;
			}

			/* Successfully sent data, so reset the timeout */
			init_timer(&writeTimer);
			countdown_ms(&writeTimer, IOT_SSL_WRITE_RETRY_TIMEOUT_MS);

			txLen += ret;
			pMsg += ret;
			len -= ret;
		} else if(ret == MBEDTLS_ERR_SSL_WANT_READ ||
				ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
			if(has_timer_expired(&writeTimer)) {
				*written_len = txLen;
				return NETWORK_SSL_WRITE_TIMEOUT_ERROR;
			}
		} else {
			IOT_ERROR(" failed\n  ! mbedtls_ssl_write returned -0x%x\n\n", (unsigned int) -ret);
			/* All other negative return values indicate connection needs to be reset.
			 * Will be caught in ping request so ignored here */
			return NETWORK_SSL_WRITE_ERROR;
		}
	}

	*written_len = txLen;
	return SUCCESS;
}

IoT_Error_t iot_tls_read(Network *pNetwork, unsigned char *pMsg, size_t len, Timer *timer, size_t *read_len) {
	mbedtls_ssl_context *pSsl = &(pNetwork->tlsDataParams.ssl);
	size_t rxLen = 0U;
	int ret;
	/* This timer checks for a timeout whenever MBEDTLS_ERR_SSL_WANT_READ,
	 * MBEDTLS_ERR_SSL_WANT_WRITE, or MBEDTLS_ERR_SSL_TIMEOUT are returned by
	 * mbedtls_ssl_read. Timeout is specified by IOT_SSL_READ_RETRY_TIMEOUT_MS. */
	Timer readTimer;

	/* This variable is unused */
	(void) timer;

	/* The timer must be started in case no bytes are read on the first try */
	init_timer(&readTimer);
	countdown_ms(&readTimer, IOT_SSL_READ_RETRY_TIMEOUT_MS);

	while(len > 0U) {
		/* This read will timeout after IOT_SSL_READ_TIMEOUT_MS if there's no data to be read */
		ret = mbedtls_ssl_read(pSsl, pMsg, len);

		if(ret > 0) {
			if((size_t) ret > len) {
				IOT_ERROR("More bytes read than requested\n\n");
				return NETWORK_SSL_WRITE_ERROR;
			}

			/* Successfully received data, so reset the timeout */
			init_timer(&readTimer);
			countdown_ms(&readTimer, IOT_SSL_READ_RETRY_TIMEOUT_MS);

			rxLen += ret;
			pMsg += ret;
			len -= ret;
		} else if(ret == MBEDTLS_ERR_SSL_WANT_READ || 
				ret == MBEDTLS_ERR_SSL_WANT_WRITE ||
				ret == MBEDTLS_ERR_SSL_TIMEOUT) {

			if(rxLen == 0U) {
				return NETWORK_SSL_NOTHING_TO_READ;
			} else {

    			if(has_timer_expired(&readTimer)) {
					return NETWORK_SSL_READ_TIMEOUT_ERROR;
				}
			}
		} else {
			IOT_ERROR("Failed\n  ! mbedtls_ssl_read returned -0x%x\n\n", (unsigned int) -ret);
			return NETWORK_SSL_READ_ERROR;
		}
	}

	*read_len = rxLen;
	return SUCCESS;
}

IoT_Error_t iot_tls_disconnect(Network *pNetwork) {
	mbedtls_ssl_context *ssl = &(pNetwork->tlsDataParams.ssl);
	int ret = 0;
	do {
		ret = mbedtls_ssl_close_notify(ssl);
	} while(ret == MBEDTLS_ERR_SSL_WANT_WRITE);

	/* All other negative return values indicate connection needs to be reset.
	 * No further action required since this is disconnect call */

	return SUCCESS;
}

IoT_Error_t iot_tls_destroy(Network *pNetwork) {
	TLSDataParams *tlsDataParams = &(pNetwork->tlsDataParams);

	mbedtls_net_free(&(tlsDataParams->server_fd));

	mbedtls_x509_crt_free(&(tlsDataParams->clicert));
	mbedtls_x509_crt_free(&(tlsDataParams->cacert));
	mbedtls_pk_free(&(tlsDataParams->pkey));
	mbedtls_ssl_free(&(tlsDataParams->ssl));
	mbedtls_ssl_config_free(&(tlsDataParams->conf));
	mbedtls_ctr_drbg_free(&(tlsDataParams->ctr_drbg));
	mbedtls_entropy_free(&(tlsDataParams->entropy));

	return SUCCESS;
}

#ifdef __cplusplus
}
#endif
