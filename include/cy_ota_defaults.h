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

/** @file
 *
 * Cypress OTA API default values
 *
 *  Default Values for OTA library
 *
 *  Over-ride these defines using the cy_ota_gonfig.h file
 *
 **********************************************************************/

/**
 * \addtogroup group_cy_ota
 * \{
 */

#ifndef CY_OTA_DEFAULTS_H__
#define CY_OTA_DEFAULTS_H__ 1


#ifdef __cplusplus
extern "C" {
#endif

#include "cy_result.h"

/**
 * \addtogroup group_ota_typedefs
 * \{
 */

/**
 * @brief Initial OTA check
 *
 * Time from when cy_ota_start() is called to when the OTA Agent first checks for an update.
 * You can over-ride this define in cy_ota_config.h
 *
 * Minimum value is  CY_OTA_INTERVAL_SECS_MIN.
 * Maximum value is  CY_OTA_INTERVAL_SECS_MAX.
 */
#ifndef CY_OTA_INITIAL_CHECK_SECS
#define CY_OTA_INITIAL_CHECK_SECS           (60)           /* 60 seconds */
#endif

/**
 * @brief Next OTA check
 *
 * Time from when after OTA Agent completes or fails an update until the next check.
 * You can over-ride this define in cy_ota_config.h
 *
 * Minimum value is  CY_OTA_INTERVAL_SECS_MIN.
 * Maximum value is  CY_OTA_INTERVAL_SECS_MAX.
 */
#ifndef CY_OTA_NEXT_CHECK_INTERVAL_SECS
#define CY_OTA_NEXT_CHECK_INTERVAL_SECS     (60 * 60 * 24) /* once per day */
#endif

/**
 * @brief Retry Interval
 *
 * Time between failures for connection before we re-try.
 * You can over-ride this define in cy_ota_config.h
 * Minimum value is  CY_OTA_INTERVAL_SECS_MIN.
 * Maximum value is  CY_OTA_INTERVAL_SECS_MAX.
 */
#ifndef CY_OTA_RETRY_INTERVAL_SECS
#define CY_OTA_RETRY_INTERVAL_SECS          (5)            /* 5 seconds */
#endif

/**
 * @brief Length of time to check for downloads
 *
 * OTA Agent wakes up, connects to server, and waits this much time before disconnecting.
 * This allows the OTA Agent to be inactive for long periods of time, only checking for short periods.
 * Use 0x00 to continue checking once started.
 */
#ifndef CY_OTA_CHECK_TIME_SECS
#define CY_OTA_CHECK_TIME_SECS              (10 * 60)       /* 10 minutes */
#endif

/**
* @brief Expected maximum download time between each OTA packet arrival
*
* This is used check that the download occurs in a reasonable time frame.
*/
#ifndef CY_OTA_PACKET_INTERVAL_SECS
#define CY_OTA_PACKET_INTERVAL_SECS         (60)           /* 1 minute */
#endif

/**
 * @brief Length of time to check for getting Job Document
 *
 * OTA Agent wakes up, connects to broker/server, and waits this much time before disconnecting.
 * This allows the OTA Agent to be inactive for long periods of time, only checking for short periods.
 * Use 0x00 to continue checking once started.
 */
#ifndef CY_OTA_JOB_CHECK_TIME_SECS
#define CY_OTA_JOB_CHECK_TIME_SECS           (30)          /* 30 seconds */
#endif

/**
 * @brief Length of time to check for getting OTA Image data
 *
 * After getting the Job (or during a Direct download), this is the amount of time we wait before
 * deciding we are not going to get the download.
 * Use 0x00 to disable.
 */
#ifndef CY_OTA_DATA_CHECK_TIME_SECS
#define CY_OTA_DATA_CHECK_TIME_SECS          (5 * 60)      /* 5 minutes */
#endif

/**
 * @brief Length of time to check for OTA Image downloads
 *
 * OTA Agent wakes up, connects to server, and waits this much time before disconnecting.
 * This allows the OTA Agent to be inactive for long periods of time, only checking for short periods.
 * Use 0x00 to continue checking once started.
 */
#ifndef CY_OTA_CHECK_TIME_SECS
#define CY_OTA_CHECK_TIME_SECS                (60 * 10)    /* 10 minutes */
#endif

/**
 * @brief Number of OTA session retries
 *
 * Retry count for overall OTA session attempts
 */
#ifndef CY_OTA_RETRIES
#define CY_OTA_RETRIES                          (5)        /* 5 overall OTA retries */
#endif

/**
 * @brief Number of OTA Connect to Server/Broker retries
 *
 * Retry count for connection attempts
 */
#ifndef CY_OTA_CONNECT_RETRIES
#define CY_OTA_CONNECT_RETRIES                  (3)        /* 3 server connect retries  */
#endif

/**
 * @brief Number of OTA download retries
 *
 * Retry count for attempts at downloading the OTA Image
 */
#ifndef CY_OTA_MAX_DOWNLOAD_TRIES
#define CY_OTA_MAX_DOWNLOAD_TRIES               (3)         /* 3 download OTA Image retries */
#endif


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
#ifndef PUBLISHER_LISTEN_TOPIC
#define PUBLISHER_LISTEN_TOPIC              "publish_notify"
#endif

/**
 * @brief First part of the topic to subscribe / publish
 *
 * Topic for Device to send message to Publisher:
 *  "COMPANY_TOPIC_PREPEND / BOARD_NAME / PUBLISHER_LISTEN_TOPIC"
 *
 * Override in cy_ota_config.h
 */
#ifndef COMPANY_TOPIC_PREPEND
#define COMPANY_TOPIC_PREPEND               "anycloud"
#endif

/**
 * @brief End of Topic to send message to Publisher for Direct download
 *
 * Override in cy_ota_config.h
 */
#ifndef PUBLISHER_DIRECT_TOPIC
#define PUBLISHER_DIRECT_TOPIC               "OTAImage"
#endif

/**
 * @brief Update Successful message
 *
 * Used with sprintf() to create RESULT message to Broker / Server
 */
#ifndef CY_OTA_RESULT_SUCCESS
#define CY_OTA_RESULT_SUCCESS               "Success"
#endif

/**
* @brief Update Failure message
*
* Used with sprintf() to create RESULT message to Broker / Server
*/
#ifndef CY_OTA_RESULT_FAILURE
#define CY_OTA_RESULT_FAILURE               "Failure"
#endif

/**
 * @brief Device message to Publisher to ask about updates
 * Used with sprintf() to insert the current version and UniqueTopicName at runtime.
 * Override if desired by defining in cy_ota_config.h.
 */
#ifndef CY_OTA_SUBSCRIBE_UPDATES_AVAIL
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
#endif

/**
 * @brief Device message to Publisher to ask for a download
 * *
 * Used with sprintf() to insert the current version and UniqueTopicName at runtime.
 * Override if desired by defining in cy_ota_config.h.
 */
#ifndef CY_OTA_DOWNLOAD_REQUEST
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
#endif

/**
 * @brief Device message to Publisher to ask for a download
 * *
 * Used with sprintf() to insert the current version and UniqueTopicName at runtime.
 * Override if desired by defining in cy_ota_config.h.
 */
#ifndef CY_OTA_DOWNLOAD_DIRECT_REQUEST
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
#endif

/**
 * @brief Device JSON doc to respond to MQTT Publisher
 *
 * Used with sprintf() to create the JSON message
 * Override if desired by defining in cy_ota_config.h.
 */
#ifndef CY_OTA_MQTT_RESULT_JSON
#define CY_OTA_MQTT_RESULT_JSON \
"{\
\"Message\":\"%s\", \
\"UniqueTopicName\": \"%s\"\
}"
#endif

/**
 * @brief Device JSON doc to respond to HTTP Server
 *
 * Used with sprintf() to create the JSON message
 * Override if desired by defining in cy_ota_config.h.
 */
#ifndef CY_OTA_HTTP_RESULT_JSON
#define CY_OTA_HTTP_RESULT_JSON \
"{\
\"Message\":\"%s\", \
\"File\":\"%s\" \
}"
#endif

/**
 * @brief HTTP GET template
 *
 * Used with sprintf() to create the GET request for HTTP server
 * Override if desired by defining in cy_ota_config.h.
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
 * Override if desired by defining in cy_ota_config.h.
 */
#ifndef CY_OTA_HTTP_POST_TEMPLATE
#define CY_OTA_HTTP_POST_TEMPLATE \
    "POST %s HTTP/1.1\r\n" \
    "Content-Length:%ld \r\n" \
    "\r\n%s"
#endif

/***********************************************************************
 *
 * MQTT Specific defines
 *
 **********************************************************************/

/**
 * @brief The keep-alive interval for MQTT
 * @brief Maximum number of MQTT Topics
 *
 * An MQTT ping request will be sent periodically at this interval.
 * The maximum number of Topics for subscribing.
 */
#ifndef CY_OTA_MQTT_KEEP_ALIVE_SECONDS
#define CY_OTA_MQTT_KEEP_ALIVE_SECONDS      (60)                /* 60 second keep-alive */
#endif

/**
 * @brief Maximum number of MQTT Topics
 *
 * The maximum number of Topics for subscribing.
 */
#ifndef CY_OTA_MQTT_MAX_TOPICS
#define CY_OTA_MQTT_MAX_TOPICS                  (2)
#endif

/**
 * @brief TOPIC prefix
 *
 * Used as prefix for "Will" and "Acknowledgement" Messages
 */
#ifndef CY_OTA_MQTT_TOPIC_PREFIX
#define CY_OTA_MQTT_TOPIC_PREFIX                   "cy_ota_device"
#endif

/**
 * @brief The first characters in the client identifier.
 *
 * A timestamp is appended to this prefix to create a unique
 *   client identifer for each connection.
 */
#ifndef CY_OTA_MQTT_CLIENT_ID_PREFIX
#define CY_OTA_MQTT_CLIENT_ID_PREFIX                "cy_device"
#endif

/** \} group_ota_typedefs */

#ifdef __cplusplus
    }
#endif

#endif /* CY_OTA_DEFAULTS_H__ */

/** \} group_cy_ota */
