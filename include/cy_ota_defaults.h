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
* @brief Expected maximum download time between each OTA packet arrival
*
* This is used check that the download occurs in a reasonable time frame.
*/
#ifndef CY_OTA_PACKET_INTERVAL_SECS
#define CY_OTA_PACKET_INTERVAL_SECS       (60)             /* 1 minute */
#endif

/**
 * @brief Length of time to check for downloads
 *
 * OTA Agent wakes up, connects to server, and waits this much time before disconnecting.
 * This allows the OTA Agent to be inactive for long periods of time, only checking for short periods.
 * Use 0x00 to continue checking once started.
 */
#ifndef CY_OTA_CHECK_TIME_SECS
#define CY_OTA_CHECK_TIME_SECS                (60 * 10)    /* 10 minutes */
#endif

/**
 * @brief Number of OTA retries
 *
 * Retry count for overall OTA attempts
 * You can over-ride this define in cy_ota_config.h
 * Minimum value is  CY_OTA_INTERVAL_SECS_MIN.
 * Maximum value is  CY_OTA_INTERVAL_SECS_MAX.
 */
#ifndef CY_OTA_RETRIES
#define CY_OTA_RETRIES                          (5)        /* 5 overall OTA retries */
#endif

/**
 * @brief Number of OTA Connect to Server/Broker retries
 *
 * Retry count for connection attempts
 * You can over-ride this define in cy_ota_config.h
 * Minimum value is  CY_OTA_INTERVAL_SECS_MIN.
 * Maximum value is  CY_OTA_INTERVAL_SECS_MAX.
 */
#ifndef CY_OTA_CONNECT_RETRIES
#define CY_OTA_CONNECT_RETRIES                  (3)         /* 3 server connect retries  */
#endif

/**
 * @brief Number of OTA download retries
 *
 * Retry count for attempts at downloading the OA Image
 * You can over-ride this define in cy_ota_config.h
 * Minimum value is  CY_OTA_INTERVAL_SECS_MIN.
 * Maximum value is  CY_OTA_INTERVAL_SECS_MAX.
 */
#ifndef CY_OTA_MAX_DOWNLOAD_TRIES
#define CY_OTA_MAX_DOWNLOAD_TRIES               (3)         /* 3 download OTA Image retries */
#endif

/**
 * @brief Maximum number of MQTT Topics
 *
 * The maximum number of Topics for subscribing.
 * You can over-ride this define in cy_ota_config.h
 * Minimum value is  CY_OTA_INTERVAL_SECS_MIN.
 * Maximum value is  CY_OTA_INTERVAL_SECS_MAX.
 */
#ifndef CY_OTA_MQTT_MAX_TOPICS
#define CY_OTA_MQTT_MAX_TOPICS                  (2)
#endif

/***********************************************************************
 *
 * MQTT Specific defines
 *
 **********************************************************************/

/**
 * @brief TOPIC prefix
 *
 * Used as prefix for "Will" and :"Acknowledgement" Messages
 */
#ifndef IOT_MQTT_TOPIC_PREFIX
#define IOT_MQTT_TOPIC_PREFIX                   "iotdevice"
#endif

/**
 * @brief The first characters in the client identifier.
 *
 * A timestamp is appended to this prefix to create a unique
 *   client identifer for each connection.
 */
#ifndef CLIENT_IDENTIFIER_PREFIX
#define CLIENT_IDENTIFIER_PREFIX                "iotdevice"
#endif

/**
 * @brief MQTT keep-alive interval.
 *
 * An MQTT ping request will be sent periodically at this interval.
 */
#ifndef MQTT_KEEP_ALIVE_SECONDS
#define MQTT_KEEP_ALIVE_SECONDS                 (60)
#endif

/**
 * @brief The timeout for MQTT operations.
 */
#ifndef MQTT_TIMEOUT_MS
#define MQTT_TIMEOUT_MS                         (5000)
#endif

/** \} group_ota_typedefs */

#ifdef __cplusplus
    }
#endif

#endif /* CY_OTA_DEFAULTS_H__ */

/** \} group_cy_ota */
