/*
 * Copyright 2020 Cypress Semiconductor Corporation
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * \addtogroup group_cy_ota Cypress Over The Air (OTA) API
 * \{
 * \defgroup group_ota_config OTA Configurations
 */
/**
 *
 *  Customer overrides for the OTA library.
 *
 **********************************************************************/

#ifndef CY_OTA_CONFIG_H__
#define CY_OTA_CONFIG_H__ 1

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \addtogroup group_ota_config
 * \{
 */
/**
 * @brief Initial time for checking for OTA updates
 *
 * This is used to start the timer for the initial OTA update check after calling cy_ota_agent_start().
 */
#define CY_OTA_INITIAL_CHECK_SECS           (10)            /* 10 seconds */

/**
 * @brief Next time for checking for OTA updates
 *
 * This is used to re-start the timer after an OTA update check in the OTA Agent.
 */
#define CY_OTA_NEXT_CHECK_INTERVAL_SECS     (24 * 60 * 60)  /* 1 day between checks */

/**
 * @brief Retry time which checking for OTA updates
 *
 * This is used to re-start the timer after failing to contact the server during an OTA update check.
 */
#define CY_OTA_RETRY_INTERVAL_SECS          (5)             /* 5 seconds between retries after an error */

/**
 * @brief Length of time to check for downloads
 *
 * OTA Agent wakes up, connects to server, and waits this much time before disconnecting.
 * This allows the OTA Agent to be inactive for long periods of time, only checking for short periods.
 * Use 0x00 to continue checking once started.
 */
#define CY_OTA_CHECK_TIME_SECS              (10 * 60)       /* 10 minutes */

/**
 * @brief Expected maximum download time between each OTA packet arrival
 *
 * This is used check that the download occurs in a reasonable time frame.
 * Set to 0 to disable this check.
 */
#define CY_OTA_PACKET_INTERVAL_SECS         (0)             /* default disabled */

/**
 * @brief Length of time to check for getting Job Document
 *
 * OTA Agent wakes up, connects to broker/server, and waits this much time before disconnecting.
 * This allows the OTA Agent to be inactive for long periods of time, only checking for short periods.
 * Use 0x00 to continue checking once started.
 */
#define CY_OTA_JOB_CHECK_TIME_SECS           (30)               /* 30 seconds */

/**
 * @brief Length of time to check for getting OTA Image data
 *
 * After getting the Job (or during a Direct download), this is the amount of time we wait before
 * deciding we are not going to get the download.
 * Use 0x00 to disable.
 */
#define CY_OTA_DATA_CHECK_TIME_SECS          (5 * 60)           /* 5 minutes */
/**
 * @brief Number of OTA session retries
 *
 * Retry count for overall OTA session attempts
 */
#define CY_OTA_RETRIES                      (3)             /* retry entire process 3 times */

/**
 * @brief Number of retries when attempting to contact the server
 *
 * This is used to determine # retries when connecting to the server during an OTA update check.
 */
#define CY_OTA_CONNECT_RETRIES              (3)             /* 3 server connect retries  */

/**
 * @brief Number of OTA download retries
 *
 * Retry count for attempts at downloading the OTA Image
 */
#define CY_OTA_MAX_DOWNLOAD_TRIES           (3)             /* 3 download OTA Image retries */

/**********************************************************************
 * Message Defines
 **********************************************************************/

/**
 * @brief Last part of the topic to subscribe
 *
 * Topic for Device to send message to Publisher:
 *  "COMPANY_TOPIC_PREPEND / BOARD_NAME / PUBLISHER_LISTEN_TOPIC"
 *  The combined topic needs to match the Publisher's subscribe topic
 *
 * Override in cy_ota_config.h
 */
#define PUBLISHER_LISTEN_TOPIC              "publish_notify"

/**
 * @brief First part of the topic to subscribe / publish
 *
 * Topic for Device to send message to Publisher:
 *  "COMPANY_TOPIC_PREPEND / BOARD_NAME / PUBLISHER_LISTEN_TOPIC"
 */
#define COMPANY_TOPIC_PREPEND               "anycloud"

/**
 * @brief End of Topic to send message to Publisher for Direct download
 */
#define PUBLISHER_DIRECT_TOPIC               "OTAImage"

/**
 * @brief Update Successful message
 *
 * Used with sprintf() to create RESULT message to Broker / Server
 */
#define CY_OTA_RESULT_SUCCESS               "Success"

/**
* @brief Update Failure message
*
* Used with sprintf() to create RESULT message to Broker / Server
*/
#define CY_OTA_RESULT_FAILURE               "Failure"

/**
 * @brief Device message to Publisher to ask about updates
 * Used with sprintf() to insert the current version and UniqueTopicName at runtime.
 * Override if desired by defining in cy_ota_config.h.
 */
#define CY_OTA_SUBSCRIBE_UPDATES_AVAIL \
"{\
\"Message\":\"Update Availability\", \
\"Manufacturer\": \"Express Widgits Corporation\", \
\"ManufacturerID\": \"EWCO\", \
\"ProductID\": \"Easy Widgit\", \
\"SerialNumber\": \"ABC213450001\", \
\"BoardName\": \"CY8CPROTO_062_4343W\", \
\"Version\": \"%d.%d.%d\", \
\"UniqueTopicName\": \"%s\"\
}"

/**
 * @brief Device message to Publisher to ask for a download
 * *
 * Used with sprintf() to insert the current version and UniqueTopicName at runtime.
 * Override if desired by defining in cy_ota_config.h.
 */
#define CY_OTA_DOWNLOAD_REQUEST \
"{\
\"Message\":\"Request Update\", \
\"Manufacturer\": \"Express Widgits Corporation\", \
\"ManufacturerID\": \"EWCO\", \
\"ProductID\": \"Easy Widgit\", \
\"SerialNumber\": \"ABC213450001\", \
\"BoardName\": \"CY8CPROTO_062_4343W\", \
\"Version\": \"%d.%d.%d\", \
\"UniqueTopicName\": \"%s\"\
}"

/**
 * @brief Device message to Publisher to ask for a download
 * *
 * Used with sprintf() to insert the current version and UniqueTopicName at runtime.
 * Override if desired by defining in cy_ota_config.h.
 */
#define CY_OTA_DOWNLOAD_DIRECT_REQUEST \
"{\
\"Message\":\"Send Direct Update\", \
\"Manufacturer\": \"Express Widgits Corporation\", \
\"ManufacturerID\": \"EWCO\", \
\"ProductID\": \"Easy Widgit\", \
\"SerialNumber\": \"ABC213450001\", \
\"BoardName\": \"CY8CPROTO_062_4343W\", \
\"Version\": \"%d.%d.%d\" \
}"

/**
 * @brief Device JSON doc to respond to MQTT Publisher
 *
 * Used with sprintf() to create the JSON message
 * Override if desired by defining in cy_ota_config.h.
 */
#define CY_OTA_MQTT_RESULT_JSON \
"{\
\"Message\":\"%s\", \
\"UniqueTopicName\": \"%s\"\
}"

/**
 * @brief Device JSON doc to respond to HTTP Server
 *
 * Used with sprintf() to create the JSON message
 * Override if desired by defining in cy_ota_config.h.
 */
#define CY_OTA_HTTP_RESULT_JSON \
"{\
\"Message\":\"%s\", \
\"File\":\"%s\" \
}"

/**
 * @brief HTTP GET template
 *
 * Used with sprintf() to create the GET request for HTTP server
 */
#ifndef CY_OTA_HTTP_GET_TEMPLATE
#define CY_OTA_HTTP_GET_TEMPLATE \
    "GET %s HTTP/1.1\r\n" \
    "Host: %s:%d \r\n" \
    "\r\n"
#endif

/**
 * @brief HTTP POST template
 *
 * Used with sprintf() to create the POST message for HTTP server
 */
#ifndef CY_OTA_HTTP_POST_TEMPLATE
#define CY_OTA_HTTP_POST_TEMPLATE \
    "POST %s HTTP/1.1\r\n" \
    "Content-Length:%ld \r\n" \
    "\r\n%s"
#endif

/**********************************************************************
 * MQTT Defines
 **********************************************************************/

/**
 * @brief The keep-alive interval for MQTT
 * @brief Maximum number of MQTT Topics
 *
 * An MQTT ping request will be sent periodically at this interval.
 * The maximum number of Topics for subscribing.
 */
#define CY_OTA_MQTT_KEEP_ALIVE_SECONDS          (60)                /* 60 second keep-alive */

/**
 * @brief Maximum number of MQTT Topics
 *
 * The maximum number of Topics for subscribing.
 */
#define CY_OTA_MQTT_MAX_TOPICS                  (2)

/**
 * @brief TOPIC prefix
 *
 * Used as prefix for "Will" and "Acknowledgement" Messages
 */
#define CY_OTA_MQTT_TOPIC_PREFIX                "cy_ota_device"

/**
 * @brief The first characters in the client identifier.
 *
 * A timestamp is appended to this prefix to create a unique
 *   client identifer for each connection.
 */
#define CY_OTA_MQTT_CLIENT_ID_PREFIX            "cy_device"


/** \} group_ota_config */

#ifdef __cplusplus
    }
#endif

#endif /* CY_OTA_CONFIG_H__ */

/** \} group_cy_ota */
