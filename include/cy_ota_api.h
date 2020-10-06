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
 * Cypress OTA API abstracts underlying network and
 * platform support for Over The Air updates.
 */

/**
 * \addtogroup group_cy_ota Cypress Over The Air (OTA) API
 * \{
 * OTA support for downloading and installing firmware updates.
 *
 * \defgroup group_ota_macros OTA Macros
 * Macros used to define OTA Agent behavior.
 *
 * \defgroup group_ota_typedefs OTA Typedefs
 * Typedefs used by the OTA Agent.
 *
 * \defgroup group_ota_callback OTA Agent Callback
 * An application can get callbacks from the OTA Agent.
 *
 * \defgroup group_ota_structures OTA Structures
 * Structures used for passing information to and from OTA Agent.
 *
 * \defgroup group_ota_functions OTA Functions
 * Functions to start/stop and query the OTA Agent.
 *
 */

/**
 *
 *
 **********************************************************************/

#ifndef CY_OTA_API_H__
#define CY_OTA_API_H__ 1

#ifdef __cplusplus
extern "C" {
#endif

#include "cy_ota_config.h"          /* Customer OTA overrides   */
#include "cy_ota_defaults.h"        /* Defaults for OTA         */

#include "cyhal.h"
#include "cybsp.h"
#include "cybsp_wifi.h"

#include "iot_network.h"

#include "iot_mqtt_types.h"

#include "cy_result_mw.h"

 /**
  * \addtogroup group_ota_macros
  * \{
  */

/***********************************************************************
 * Error Defines - move to cy_result_mw.h
 ***********************************************************************/
#ifndef CY_RSLT_MODULE_OTA_UPDATE_BASE
/* Will be defined in cy_result_mw.h - if not merged yet, we have this here. */
#define CY_RSLT_MODULE_OTA_UPDATE_BASE  (CY_RSLT_MODULE_MIDDLEWARE_BASE + 13) /**< OTA Update module base */

#endif


#define CY_RSLT_OTA_ERROR_BASE    CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_OTA_UPDATE_BASE, 0) /**< OTA Update error code base */

#define CY_RSLT_OTA_ERROR_UNSUPPORTED           (CY_RSLT_OTA_ERROR_BASE +  1) /**< Unsupported feature                  */
#define CY_RSLT_OTA_ERROR_GENERAL               (CY_RSLT_OTA_ERROR_BASE +  2) /**< Generic error                        */
#define CY_RSLT_OTA_ERROR_BADARG                (CY_RSLT_OTA_ERROR_BASE +  3) /**< Bad argument                         */
#define CY_RSLT_OTA_ERROR_OUT_OF_MEMORY         (CY_RSLT_OTA_ERROR_BASE +  4) /**< Out of Memory Error                  */
#define CY_RSLT_OTA_ERROR_ALREADY_STARTED       (CY_RSLT_OTA_ERROR_BASE +  5) /**< OTA Already started                  */
#define CY_RSLT_OTA_ERROR_MQTT_INIT             (CY_RSLT_OTA_ERROR_BASE +  6) /**< MQTT init failed                     */
#define CY_RSLT_OTA_ERROR_OPEN_STORAGE          (CY_RSLT_OTA_ERROR_BASE +  7) /**< Local storage open error             */
#define CY_RSLT_OTA_ERROR_WRITE_STORAGE         (CY_RSLT_OTA_ERROR_BASE +  8) /**< Write to local storage error         */
#define CY_RSLT_OTA_ERROR_CLOSE_STORAGE         (CY_RSLT_OTA_ERROR_BASE +  9) /**< Close local storage error            */
#define CY_RSLT_OTA_ERROR_CONNECT               (CY_RSLT_OTA_ERROR_BASE + 10) /**< Server connect Failed                */
#define CY_RSLT_OTA_ERROR_DISCONNECT            (CY_RSLT_OTA_ERROR_BASE + 11) /**< Server disconnect error              */
#define CY_RSLT_OTA_ERROR_REDIRECT              (CY_RSLT_OTA_ERROR_BASE + 12) /**< Redirection failure                  */
#define CY_RSLT_OTA_ERROR_SERVER_DROPPED        (CY_RSLT_OTA_ERROR_BASE + 13) /**< Broker/Server disconnect             */
#define CY_RSLT_OTA_ERROR_MQTT_SUBSCRIBE        (CY_RSLT_OTA_ERROR_BASE + 14) /**< Broker/Server Subscribe error        */
#define CY_RSLT_OTA_ERROR_MQTT_PUBLISH          (CY_RSLT_OTA_ERROR_BASE + 15) /**< Broker/Server Publish error          */
#define CY_RSLT_OTA_ERROR_GET_JOB               (CY_RSLT_OTA_ERROR_BASE + 16) /**< Failed to get OTA Image              */
#define CY_RSLT_OTA_ERROR_GET_DATA              (CY_RSLT_OTA_ERROR_BASE + 17) /**< Failed to get OTA Image              */
#define CY_RSLT_OTA_ERROR_NOT_A_HEADER          (CY_RSLT_OTA_ERROR_BASE + 18) /**< No header in payload                 */
#define CY_RSLT_OTA_ERROR_NOT_A_JOB_DOC         (CY_RSLT_OTA_ERROR_BASE + 19) /**< Job document is invalid              */
#define CY_RSLT_OTA_ERROR_MALFORMED_JOB_DOC     (CY_RSLT_OTA_ERROR_BASE + 20) /**< Malformed Job document               */
#define CY_RSLT_OTA_ERROR_WRONG_BOARD           (CY_RSLT_OTA_ERROR_BASE + 21) /**< Board in Job document does not match */
#define CY_RSLT_OTA_ERROR_INVALID_VERSION       (CY_RSLT_OTA_ERROR_BASE + 22) /**< Invalid version in Job or Packet     */
#define CY_RSLT_OTA_ERROR_VERIFY                (CY_RSLT_OTA_ERROR_BASE + 23) /**< Verify image failure                 */
#define CY_RSLT_OTA_ERROR_SENDING_RESULT        (CY_RSLT_OTA_ERROR_BASE + 24) /**< Sending result failed                */
#define CY_RSLT_OTA_ERROR_APP_RETURNED_STOP     (CY_RSLT_OTA_ERROR_BASE + 25) /**< Callback returned Stop OTA download  */

#define CY_RSLT_OTA_INFO_BASE    CY_RSLT_CREATE(CY_RSLT_TYPE_INFO, CY_RSLT_MODULE_OTA_UPDATE_BASE, 0)   /**< Base for informational results */
#define CY_RSLT_OTA_EXITING                     (CY_RSLT_OTA_INFO_BASE + 1) /**< OTA Agent exiting                       */
#define CY_RSLT_OTA_ALREADY_CONNECTED           (CY_RSLT_OTA_INFO_BASE + 2) /**< OTA Already Connected                */
#define CY_RSLT_OTA_CHANGING_SERVER             (CY_RSLT_OTA_INFO_BASE + 3) /**< Data broker/server different from job   */

#define CY_RSLT_OTA_USE_JOB_FLOW                (CY_RSLT_SUCCESS          ) /**< Use Job Flow for update                 */
#define CY_RSLT_OTA_USE_DIRECT_FLOW             (CY_RSLT_OTA_INFO_BASE + 4) /**< Use Direct Flow for update              */
#define CY_RSLT_OTA_NO_UPDATE_AVAILABLE         (CY_RSLT_OTA_INFO_BASE + 5) /**< No OTA update on Server                 */

/** \} group_ota_macros */

/***********************************************************************
 * Define min / max values
 ***********************************************************************/

/**
  * \addtogroup group_ota_typedefs
  * \{
  */

/**
 * @brief Macro combo to add quotes around a define
 * Used to define a string value for CY_TARGET_BOARD
 * */
#define ADD_QUOTES(str) #str
/**
 * @brief Macro combo to add quotes around a define
 * Used to define a string value for CY_TARGET_BOARD
 * */
#define EXPAND_AND_ADD_QUOTES(str) ADD_QUOTES(str)
/**
 * @brief Macro combo to add quotes around a define
 * Used to define a string value for CY_TARGET_BOARD
 * */
#define CY_TARGET_BOARD_STRING EXPAND_AND_ADD_QUOTES(CY_TARGET_BOARD)

/**
 * @brief Size for HTTP filename to get OTA Image
 */
#define CY_OTA_MQTT_FILENAME_BUFF_SIZE          (256)


/**
 * @brief Size for Unique MQTT Topic to get OTA Image
 */
#define CY_OTA_MQTT_UNIQUE_TOPIC_BUFF_SIZE      (256)

/**
 * @brief Size for MQTT Message to request JOB
 */
#define CY_OTA_MQTT_MESSAGE_BUFF_SIZE           (1024)

/**
 * @brief Size of string for HTTP filename
 */
#define CY_OTA_HTTP_FILENAME_SIZE               (256)

/**
 * @brief First part of the topic to subscribe / publish
 *
 * Topic for Device to send message to Publisher:
 *  "COMPANY_TOPIC_PREPEND / BOARD_NAME / PUBLISHER_LISTEN_TOPIC"
 *
 * Override in cy_ota_config.h
 */
#ifndef COMPANY_TOPIC_PREPEND
#define COMPANY_TOPIC_PREPEND                   "anycloud"
#endif

/**
 * @brief Last part of the topic to subscribe
 *
 * Topic for Device to send message to Publisher:
 *  "COMPANY_TOPIC_PREPEND / BOARD_NAME / PUBLISHER_LISTEN_TOPIC"
 *
 * Override in cy_ota_config.h
 */
#ifndef PUBLISHER_LISTEN_TOPIC
#define PUBLISHER_LISTEN_TOPIC                  "publish_notify"
#endif

/**
 * @brief "Magic" value placed in MQTT Data Payload header
 *
 * Override in cy_ota_config.h
 */
#ifndef CY_OTA_MQTT_MAGIC
#define CY_OTA_MQTT_MAGIC                       "OTAImage"
#endif

/**
 * @brief End of Topic to send message to Publisher for Direct download
 *
 * Override in cy_ota_config.h
 */
#ifndef PUBLISHER_DIRECT_TOPIC
#define PUBLISHER_DIRECT_TOPIC                  "OTAImage"
#endif

/**
 * @brief Publisher Response when NO Update is available
 */
#define NOTIFICATION_RESPONSE_NO_UPDATES        "No Update Available"

/**
 * @brief Publisher Response when Update is available
 */
#define NOTIFICATION_RESPONSE_UPDATES           "Update Available"

/**
 * @brief Publisher Response when receiving results
 */
#define NOTIFICATION_RESPONSE_RESULT_RECEIVED   "Result Received"

/**
 * @brief Topic to receive messages from the publisher
 */
#define CY_OTA_SUBSCRIBE_AVAIL_TOPIC        COMPANY_TOPIC_PREPEND "/" CY_TARGET_BOARD_STRING "/" DEVICE_LISTEN_TOPIC

/**
 * @brief Topic to send messages to the publisher
 */
#define SUBSCRIBER_PUBLISH_TOPIC            COMPANY_TOPIC_PREPEND "/" CY_TARGET_BOARD_STRING "/" PUBLISHER_LISTEN_TOPIC

/**
 * @brief Minimum interval check time
 *
 * Minimum interval time for any timing specification in seconds.
 */
#define CY_OTA_INTERVAL_SECS_MIN        (5)

/**
 * @brief Maximum interval check time
 *
 * This applies to:
 *  Initial wait after call to cy_ota_agent_start() before contacting server.
 *  Wait value after failing or completing an OTA download.
 */
#define CY_OTA_INTERVAL_SECS_MAX        (60 * 60 * 24 * 365)     /* one year */

/*
 * Check for valid define values
 */
#if (CY_OTA_INITIAL_CHECK_SECS < CY_OTA_INTERVAL_SECS_MIN)
    #error  "CY_OTA_INITIAL_CHECK_SECS must be greater or equal to CY_OTA_INTERVAL_SECS_MIN."
#endif
#if (CY_OTA_INITIAL_CHECK_SECS > CY_OTA_INTERVAL_SECS_MAX)
    #error  "CY_OTA_INITIAL_CHECK_SECS must be less than CY_OTA_INTERVAL_SECS_MAX."
#endif

#if (CY_OTA_NEXT_CHECK_INTERVAL_SECS < CY_OTA_INTERVAL_SECS_MIN)
    #error  "CY_OTA_NEXT_CHECK_INTERVAL_SECS must be greater or equal to CY_OTA_INTERVAL_SECS_MIN."
#endif
#if (CY_OTA_NEXT_CHECK_INTERVAL_SECS > CY_OTA_INTERVAL_SECS_MAX)
    #error  "CY_OTA_NEXT_CHECK_INTERVAL_SECS must be less than CY_OTA_INTERVAL_SECS_MAX."
#endif

/* we don't check minimum as this number can be 0 */
#if (CY_OTA_CHECK_TIME_SECS > CY_OTA_INTERVAL_SECS_MAX)
    #error  "CY_OTA_CHECK_TIME_SECS must be less than CY_OTA_INTERVAL_SECS_MAX."
#endif

#if (CY_OTA_RETRY_INTERVAL_SECS < CY_OTA_INTERVAL_SECS_MIN)
    #error  "CY_OTA_RETRY_INTERVAL_SECS must be greater or equal to CY_OTA_INTERVAL_SECS_MIN."
#endif
#if (CY_OTA_RETRY_INTERVAL_SECS > CY_OTA_INTERVAL_SECS_MAX)
    #error  "CY_OTA_RETRY_INTERVAL_SECS must be less than CY_OTA_INTERVAL_SECS_MAX."
#endif

#if (CY_OTA_PACKET_INTERVAL_SECS > CY_OTA_INTERVAL_SECS_MAX)
    #error  "CY_OTA_PACKET_INTERVAL_SECS must be less than CY_OTA_INTERVAL_SECS_MAX."
#endif

/***********************************************************************
 *
 * defines & enums
 *
 **********************************************************************/

/**
 * @brief Message name field in a JSON Job Document
 *
 * The message field indicates the type of request / info is contained in the Job document.
 */
#define CY_OTA_MESSAGE_FIELD                "Message"

/**
 * @brief Manufacturer name field in a JSON Job Document
 *
 * The Manufacturer name can be used to determine if the OTA Image available is correct.
 */
#define CY_OTA_MANUF_FIELD                  "Manufacturer"

/**
 * @brief Manufacturer ID field in a JSON Job Document
 *
 * The Manufacturer name can be used to determine if the OTA Image available is correct.
 */
#define CY_OTA_MANUF_ID_FIELD               "ManufacturerID"

/**
 * @brief Product ID field in a JSON Job Document
 *
 * The Product name can be used to determine if the OTA Image available is correct.
 */
#define CY_OTA_PRODUCT_ID_FIELD             "ProductID"

/**
 * @brief Serial Number field in a JSON Job Document
 *
 * The Serial Number can be used to determine if the OTA Image available is correct.
 */
#define CY_OTA_SERIAL_NUMBER_FIELD          "SerialNumber"

/**
 * @brief Version field in a JSON Job Document
 *
 * The Version can be used to determine if the OTA Image available is an upgrade (ex: "1.6.3").
 */
#define CY_OTA_VERSION_FIELD                "Version"

/**
 * @brief Board field in a JSON Job Document
 *
 * The Board can be used to determine if the OTA Image available is correct.
 */
#define CY_OTA_BOARD_FIELD                  "Board"

/**
 * @brief Connection field in a JSON Job Document
 *
 * The Connection defines the connection type to use to obtain the OTA Image ("HTTP" or "MQTT").
 */
#define CY_OTA_CONNECTION_FIELD             "Connection"

/**
 * @brief Broker field for MQTT Connection in a JSON Job Document
 *
 * The MQTT is the MQTT Broker used to obtain the OTA Image.
 */
#define CY_OTA_BROKER_FIELD                 "Broker"

/**
 * @brief Port field in a JSON Job Document
 *
 * The Port is used for HTTP or MQTT Connection to obtain the OTA Image.
 */
#define CY_OTA_PORT_FIELD                   "Port"

/**
 * @brief Server field is for HTTP Connection in a JSON Job Document
 *
 * The Server is the HTTP server used to obtain the OTA Image. See also @ref CY_OTA_PORT_FIELD @ref CY_OTA_FILE_FIELD
 */
#define CY_OTA_SERVER_FIELD                 "Server"

/**
 * @brief File field is for HTTP Connection in a JSON Job Document
 *
 * The File is the name of the OTA Image on the HTTP server. See also @ref CY_OTA_PORT_FIELD
 */
#define CY_OTA_FILE_FIELD                   "File"

/**
 * @brief Unique Topic field is the name of the topic used for receipt of job/data from the Publisher
 *
 * The Unique Topic Name used for receipt of job / data from Publisher
 */
#define CY_OTA_UNIQUE_TOPIC_FIELD           "UniqueTopicName"

/**
 * @brief MQTT Connection Type used in a JSON Job Document
 *
 * We test for this string in a JSON Job Document for MQTT type Connection. @ref CY_OTA_CONNECTION_FIELD
 */
#define CY_OTA_MQTT_STRING                  "MQTT"
/**
 * @brief HTTP Connection Type used in a JSON Job Document
 *
 * We test for this string in a JSON Job Document for HTTP type Connection. @ref CY_OTA_CONNECTION_FIELD
 */
#define CY_OTA_HTTP_STRING                  "HTTP"
/**
 * @brief HTTPS Connection Type used in a JSON Job Document
 *
 * We test for this string in a JSON Job Document for HTTPS type Connection. @ref CY_OTA_CONNECTION_FIELD
 */
#define CY_OTA_HTTPS_STRING                 "HTTPS"

/**
 *  @brief Max length of "Message" field in a JSON Job Document
 */
#define CY_OTA_MESSAGE_LEN                  (32)

/**
 *  @brief Max length of "Manufacturer" field in a JSON Job Document
 */
#define CY_OTA_JOB_MANUF_LEN                (64)

/**
 *  @brief Max length of "ManufacturerID" field in a JSON Job Document
 */
#define CY_OTA_JOB_MANUF_ID_LEN             (16)

/**
 *  @brief Max length of "ProductID" field in a JSON Job Document
 */
#define CY_OTA_JOB_PRODUCT_ID_LEN           (64)

/**
 *  @brief Max length of "SerialNumber" field in a JSON Job Document
 */
#define CY_OTA_JOB_SERIAL_NUMBER_LEN        (32)

/**
 *  @brief Max length of "Version" field in a JSON Job Document
 */
#define CY_OTA_JOB_VERSION_LEN              (16)

/**
 *  @brief Max length of "Board" field in a JSON Job Document
 */
#define CY_OTA_JOB_BOARD_LEN                (32)

/**
 *  @brief Max length of "Broker" and "Server" fields in a JSON Job Document
 */
#define CY_OTA_JOB_URL_BROKER_LEN           (256)

/**
 * @brief MQTT Broker Port for non-TLS connection
 */
#define CY_OTA_MQTT_BROKER_PORT             (1883)

/**
 * @brief MQTT Broker Port for TLS connection
 */
#define CY_OTA_MQTT_BROKER_PORT_TLS         (8883)

/**
 * @brief MQTT Broker Port for TLS connection with certificates
 */
#define CY_OTA_MQTT_BROKER_PORT_TLS_CERT    (8884)


/**
 * @brief HTTP connection port for non-TLS connection
 */
#define CY_OTA_HTTP_SERVER_PORT             (80)

/**
 * @brief HTTP connection port for TLS connection
 */
#define CY_OTA_HTTP_SERVER_PORT_TLS         (443)


/**
 * @brief Type of OTA update flow
 * Direct Flow: The Application knows the location of the update and directly downloads it.
 * Job Flow: The Application loads a Job document which details where the update is located.
 */
typedef enum
{
    CY_OTA_JOB_FLOW = 0,                /**< Use the Job flow to get the OTA Job document and Data  */
    CY_OTA_DIRECT_FLOW                  /**< Get the OTA Image Data Directly                        */
} cy_ota_update_flow_t;
/**
 * @brief Connection type to use
 */
typedef enum
{
    CY_OTA_CONNECTION_UNKNOWN = 0,      /**< Unknown connection type        */
    CY_OTA_CONNECTION_MQTT,             /**< Use MQTT Connection            */
    CY_OTA_CONNECTION_HTTP,             /**< Use HTTP Connection            */
    CY_OTA_CONNECTION_HTTPS,            /**< Use HTTPS Connection           */
} cy_ota_connection_t;

/**
 * @brief MQTT Session clean flag
 *
 * This flag signals the MQTT broker to start a new session or restart an existing session
 */
typedef enum
{
    CY_OTA_MQTT_SESSION_CLEAN = 0,      /**< Start a clean MQTT session with the broker         */
    CY_OTA_MQTT_SESSION_RESTART         /**< Restart an existing MQTT session with the broker   */
} cy_ota_mqtt_session_type_t;


/**
 * @brief Reasons OTA callbacks to Application
 */
typedef enum
{
    CY_OTA_REASON_STATE_CHANGE = 0,     /**< OTA Agent State changed, see cb_data->state    */
    CY_OTA_REASON_SUCCESS,              /**< State function Succeeded                       */
    CY_OTA_REASON_FAILURE,              /**< State function Failed                          */

    CY_OTA_LAST_REASON                  /**< Placeholder, Do not use                        */
} cy_ota_cb_reason_t;

/**
 * @brief OTA state
 */
typedef enum
{
    CY_OTA_STATE_NOT_INITIALIZED= 0,    /**< OTA system is not initialized                          */
    CY_OTA_STATE_EXITING,               /**< OTA system is exiting                                  */
    CY_OTA_STATE_INITIALIZING,          /**< OTA system is initializing                             */
    CY_OTA_STATE_AGENT_STARTED,         /**< OTA Agent has started                                  */
    CY_OTA_STATE_AGENT_WAITING,         /**< OTA Agent is waiting for timer to start                */

    CY_OTA_STATE_STORAGE_OPEN,          /**< OTA Agent will call cy_storage_open                    */
    CY_OTA_STATE_STORAGE_WRITE,         /**< OTA Agent will call cy_storage_write                   */
    CY_OTA_STATE_STORAGE_CLOSE,         /**< OTA Agent will call cy_storage_close                   */

    CY_OTA_STATE_START_UPDATE,          /**< OTA Agent will determine Job or Direct Flow            */

    CY_OTA_STATE_JOB_CONNECT,           /**< OTA Agent will connect to Job Broker / Server          */
    CY_OTA_STATE_JOB_DOWNLOAD,          /**< OTA Agent will get Job from Broker / Server            */
    CY_OTA_STATE_JOB_DISCONNECT,        /**< OTA Agent will disconnect from Job Broker / Server     */

    CY_OTA_STATE_JOB_PARSE,             /**< OTA Agent will parse Job                               */
    CY_OTA_STATE_JOB_REDIRECT,          /**< OTA Agent will use Job to change Broker / Server       */

    CY_OTA_STATE_DATA_CONNECT,          /**< OTA Agent will connect to Data Broker / Server         */
    CY_OTA_STATE_DATA_DOWNLOAD,         /**< OTA Agent will download Data                           */
    CY_OTA_STATE_DATA_DISCONNECT,       /**< OTA Agent will disconnect from Data Broker / Server    */

    CY_OTA_STATE_VERIFY,                /**< OTA Agent will verify download                         */

    CY_OTA_STATE_RESULT_REDIRECT,       /**< OTA Agent will redirect back to initial connection     */

    CY_OTA_STATE_RESULT_CONNECT,        /**< OTA Agent will connecting to Result Broker / Server    */
    CY_OTA_STATE_RESULT_SEND,           /**< OTA Agent will Send Result                             */
    CY_OTA_STATE_RESULT_RESPONSE,       /**< OTA Agent will wait for Result response                */
    CY_OTA_STATE_RESULT_DISCONNECT,     /**< OTA Agent will disconnect from Result Broker / Server  */

    CY_OTA_STATE_OTA_COMPLETE,          /**< OTA Agent is done with current session,
                                             OTA Agent will reboot or OTA Agent will wait           */

    CY_OTA_NUM_STATES                   /**< Not used, placeholder */
} cy_ota_agent_state_t;

/**
 *  OTA Context pointer
 *
 * Returned from cy_ota_agent_start() and used for subsequent calls
 */
typedef void *cy_ota_context_ptr;

/**
 * @brief Results from registered OTA Callback
 *
 * These are the return values from the Application to the OTA Agent at the end of a callback.
 *
 * Of the callback reasons, \ref cy_ota_cb_reason_t, the OTA Agent only checks the Application
 * callback return values when the reason is CY_OTA_REASON_STATE_CHANGE. The other callback reasons
 * are informational for the Application.
 *
 * When the callback reason is CY_OTA_REASON_STATE_CHANGE, the OTA Agent is about to call the function
 * to act on the new state. The Application callback function can return a value that affects the
 * OTA Agent.
 *
 * There are a few STATE changes that will pick up changes made by the Application and use those changes.
 *
 *      cb_data->reason             - Reason for the callback
 *      cb_data->cb_arg             - Argument provided to cy_ota_agent_start() params
 *      cb_data->state              - CY_OTA_REASON_STATE_CHANGE
 *                                      - Start of cy_ota_agent_state_t, about to call default function
 *                                   - CY_OTA_REASON_SUCCESS
 *                                      - Error function succeeded
 *                                   - CY_OTA_REASON_FAILURE
 *                                      - Error function failed
 *      cb_data->error              - Same as cy_ota_get_last_error()
 *      cb_data->storage            - For CY_OTA_STATE_STORAGE_WRITE callback, points to storage info
 *      cb_data->total_size         - Total # bytes to be downloaded
 *      cb_data->bytes_written      - Total # bytes written
 *      cb_data->percentage         - Percentage of data downloaded
 *      cb_data->connection_type    - Current connection - MQTT or HTTP
 *      cb_data->broker_server      - Current server URL and port
 *      cb_data->credentials        - Pointer to current credentials being used
 *      cb_data->mqtt_connection    - Current MQTT connection (NULL if not connected)
 *                                      - For CY_OTA_REASON_STATE_CHANGE
 *                                        - CY_OTA_STATE_JOB_CONNECT or
 *                                          CY_OTA_STATE_DATA_CONNECT or
 *                                          CY_OTA_STATE_RESULT_CONNECT
 *                                      - OTA Agent about to connect, if Application fills this in
 *                                        then the OTA Agent will use the provided connection instance
 *      cb_data->http_connection    - Current HTTP connection (NULL if not conencted)
 *                                      - For CY_OTA_REASON_STATE_CHANGE
 *                                        - CY_OTA_STATE_JOB_CONNECT or
 *                                          CY_OTA_STATE_DATA_CONNECT or
 *                                          CY_OTA_STATE_RESULT_CONNECT
 *                                      - OTA Agent about to connect, if Application fills this in
 *                                        then the OTA Agent will use the provided connection instance
 *      cb_data->file               - Filename for HTTP GET command
 *      cb_data->unique_topic       - Unique Topic Name to receive data from MQTT Broker / Publisher
 *      cb_data->json_doc           - JSON doc used for:
 *                                      MQTT
 *                                      - Send to request JOB from MQTT Broker
 *                                      - Received JOB from MQTT Broker
 *                                      - Send to request Data from MQTT Broker
 *                                      - Send to report Result to MQTT Broker
 *                                      HTTP
 *                                      - HTTP Get command for Job
 *                                      - Received JOB from HTTP Server
 *                                      - HTTP Get command for Data
 *                                      - HTTP PUT command to report Result to HTTP Server (Not implemented yet)
 */

typedef enum
{
    CY_OTA_CB_RSLT_OTA_CONTINUE = 0,  /**< OTA Agent to continue with function, using modified data from Application  */
    CY_OTA_CB_RSLT_OTA_STOP,          /**< OTA Agent to End current update session (do not quit OTA Agent).           */
    CY_OTA_CB_RSLT_APP_SUCCESS,       /**< Application completed task, OTA Agent uses success.                        */
    CY_OTA_CB_RSLT_APP_FAILED,        /**< Application failed task, OTA Agent uses failure.                           */

    CY_OTA_CB_NUM_RESULTS             /**< Placeholder, do not use */
} cy_ota_callback_results_t;

/** \} group_ota_typedefs */

/***********************************************************************
 *
 * Structures
 *
 **********************************************************************/

/**
 * \addtogroup group_ota_structures
 * \{
 *  Structures used for Network connection and OTA Agent behavior.
 */


/**
 * @brief Struct to hold information on where to write data
 * Information on where to store the down loaded chunk of OTA image.
 * \struct cy_ota_storage_write_info_t
 */
typedef struct
{
    uint32_t        total_size;     /**< Pass total size to storage module; 0 = disregard   */

    uint32_t        offset;         /**< Offset into file/area where data belongs           */
    uint8_t         *buffer;        /**< Pointer to buffer with chunk of data               */
    uint32_t        size;           /**< Size of data in buffer                             */

                                    /* When using MQTT connection  */
    uint16_t        packet_number;  /**< MQTT: The packet number of this chunk              */
    uint16_t        total_packets;  /**< MQTT: Total packets (chunks) in OTA Image          */
} cy_ota_storage_write_info_t;

/**
 * @brief OTA Server/Broker information
 *
 * Describes a Host name (URL) and a port for connections.
 * \struct cy_ota_server_info_t
 */
typedef struct
{
        const char * pHostName;                 /**< Server host name. Must be NULL-terminated.     */
        uint16_t port;                          /**< Server port                                    */
} cy_ota_server_info_t;

/**
 * @brief OTA HTTP specific connection parameters
 * \struct cy_ota_http_params_t
 */
typedef struct
{
    cy_ota_server_info_t        server;         /**< HTTP Server to get Job or OTA Image
                                                 *    set use_get_job_flow in cy_ota_network_params_t
                                                 *    to determine use of job document.
                                                 */

    const char                  *file;          /**< filename for Job or OTA Image
                                                 *    set use_get_job_flow in cy_ota_network_params_t
                                                 *    to determine OTA update flow.
                                                 */
    IotNetworkCredentials_t     credentials;     /**< Setting credentials uses TLS (NULL == non-TLS) */
} cy_ota_http_params_t;

/**
 *  OTA MQTT specific Connection parameters
 *  \struct cy_ota_mqtt_params_t
 */
typedef struct
{
    bool                        awsIotMqttMode;    /**< 0 = normal MQTT, 1 = Special Amazon mode    */
    const char                  *pIdentifier;      /**< Ptr to device ID                            */
    uint8_t                     numTopicFilters;   /**< Number of Topics for MQTT Subscribe         */
    const char                  **pTopicFilters;   /**< Topic Filter text                           */
    cy_ota_mqtt_session_type_t  session_type;      /**< @ref cy_ota_mqtt_session_type_t             */

    cy_ota_server_info_t        broker;            /**< Broker to get Job or OTA Image.
                                                    *    set use_get_job_flow in cy_ota_network_params_t
                                                    *    to determine OTA update flow.
                                                    */
    IotNetworkCredentials_t     credentials;        /**< Setting credentials uses TLS (NULL == non-TLS) */
} cy_ota_mqtt_params_t;

/**
 * @brief Structure passed to callback
 *
 * This holds read/write data for the Application to adjust various things in the callback.
 * The callback is not in an interrupt context, but try to keep it quick!
 * After your function returns, this structure is not available.
 * \struct cy_ota_cb_struct_t
 */
typedef struct
{
    cy_ota_cb_reason_t          reason;         /**< reason for the callback                                */
    void                        *cb_arg;        /**< Argument passed in when registering the callback       */

    cy_ota_agent_state_t        state;          /**< Current OTA Agent state                                */
    cy_rslt_t                   error;          /**< Current OTA Agent error status                         */

    cy_ota_storage_write_info_t *storage;       /**< pointer to a chunk of data to write                    */
    uint32_t                    total_size;     /**< Total # bytes to be downloaded                         */
    uint32_t                    bytes_written;  /**< Total # bytes downloaded                               */
    uint32_t                    percentage;     /**< % bytes downloaded                                     */

    cy_ota_connection_t         connection_type; /**< Connection Type @ref cy_ota_connection_t              */
    cy_ota_server_info_t        broker_server;   /**< MQTT Broker (or HTTP server) for connection           */
    IotNetworkCredentials_t     credentials;     /**< Setting credentials uses TLS (NULL == non-TLS)        */

    IotNetworkConnection_t      http_connection; /**< For Passing HTTP connection instance                  */
    IotMqttConnection_t         mqtt_connection; /**< For Passing MQTT connection instance                  */

    char                        file[CY_OTA_MQTT_FILENAME_BUFF_SIZE]; /**< Filename to request OTA data     */

    /* For MQTT Get Job Message */
    char                        unique_topic[CY_OTA_MQTT_UNIQUE_TOPIC_BUFF_SIZE];   /**< Topic for receiving OTA data */
    char                        json_doc[CY_OTA_MQTT_MESSAGE_BUFF_SIZE];            /**< Message to request OTA data  */
} cy_ota_cb_struct_t;

/** \} group_ota_structures */


/**
 * \addtogroup group_ota_callback
 * \{
 */

/**
 * @brief OTA Agent callback to the Application
 *
 * The Application can use the callback to override the default OTA functions.
 *
 * @param[in] [out]   cb_data   current information that Application callback can use/modify
 *
 * @return
 */
typedef cy_ota_callback_results_t (*cy_ota_callback_t)(cy_ota_cb_struct_t *cb_data);

/** \} group_ota_callback */

/**
 * \addtogroup group_ota_structures
 * \{
 */
/**
 * @brief OTA Connection parameters structure
 *
 *  This information is used for MQTT and HTTP connections.
 *  \struct cy_ota_network_params_t
 */
typedef struct
{
    cy_ota_connection_t         initial_connection; /**< Initial Connection Type @ref cy_ota_connection_t   */

    /* MQTT and HTTP  settings */
    cy_ota_mqtt_params_t        mqtt;               /**< MQTT Connection information                        */
    cy_ota_http_params_t        http;               /**< HTTP Connection information                        */

    const void                  *network_interface; /**< Application network interface for use by OTA       */

    cy_ota_update_flow_t        use_get_job_flow;   /**< Job Flow (CY_OTA_JOB_FLOW or CY_OTA_DIRECT_FLOW)   */
} cy_ota_network_params_t;

/**
 * @brief OTA Agent parameters structure
 *
 * These parameters are for describing some aspects of the OTA Agent.
 * \struct cy_ota_agent_params_t
 */
typedef struct
{
    uint8_t     reboot_upon_completion;     /**< 1 = Automatically reboot upon download completion and verify   */
    uint8_t     validate_after_reboot;      /**< 0 = OTA will set mcuboot to permanent before reboot
                                             *   1 = Application must call cy_ota_validated() after reboot
                                             */
    cy_ota_callback_t   cb_func;            /**< Notification callback function                                 */
    void                *cb_arg;            /**< Opaque arg passed to Notification callback function            */
} cy_ota_agent_params_t;

/** \} group_ota_structures */


/***********************************************************************
 *
 * Functions
 *
 **********************************************************************/

/**
 * \addtogroup group_ota_functions
 * \{
 * OTA functions for starting, stopping, and getting information about the OTA Agent.
 */

/**
 * @brief Start OTA Background Agent
 *
 * Start thread to monitor for OTA updates.
 * This consumes resources until cy_ota_agent_stop() is called.
 *
 * @param[in]   network_params   pointer to cy_ota_network_params_t
 * @param[in]   agent_params     pointer to cy_ota_agent_params_t
 * @param[out]  ota_ptr          Handle to store OTA Agent context structure pointer
 *                               Which is used for other OTA calls.
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_OTA_ERROR_BADARG
 *          CY_RSLT_OTA_ERROR_ALREADY_STARTED
 *          CY_RSLT_OTA_ERROR
 */
cy_rslt_t cy_ota_agent_start(cy_ota_network_params_t *network_params, cy_ota_agent_params_t *agent_params, cy_ota_context_ptr *ota_ptr);

/**
 * @brief Stop OTA Background Agent
 *
 *  Stop thread to monitor OTA updates and release resources.
 *
 * @param[in]   ota_ptr         pointer to OTA Agent context storage returned from @ref cy_ota_agent_start();
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_OTA_ERROR_BADARG
 */
cy_rslt_t cy_ota_agent_stop(cy_ota_context_ptr *ota_ptr);

/**
 * @brief Check for OTA update availability and download update now.
 *
 * Once cy_ota_start() has returned CY_RSLT_SUCCESS, you can initiate an OTA update availability check
 * whenever you desire, this supercede the timing values. It does not change the timer values, and will
 * set the timer to the current values after an OTA download cycle.
 *
 *
 * @param[in]   ota_ptr         pointer to OTA Agent context storage returned from @ref cy_ota_agent_start();
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_OTA_ERROR_BADARG
 *          CY_RSLT_OTA_ERROR_ALREADY_STARTED
 */
cy_rslt_t cy_ota_get_update_now(cy_ota_context_ptr ota_ptr);

/**
 * @brief Application calls to validate the OTA Image
 *
 *  If validate_after_reboot is set to 1 in OTA start parameters,
 *  this must be called after MCUBoot copies Secondary slot to
 *  Primary slot and updated Application is running.
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_OTA_ERROR_GENERAL
 */
cy_rslt_t cy_ota_validated(void);

/**
 * @brief Get OTA Agent State.
 *
 * Use this function to determine what state the OTA Agent is in. You may not want to check if the
 * OTA Agent is in CY_OTA_STATE_AGENT_WAITING so you don't interrupt an OTA cycle.
 *
 * @param[in]  ota_ptr          pointer to OTA Agent context returned from @ref cy_ota_agent_start();
 * @param[out] state            Current OTA State
 *
 * @result  CY_RSLT_SUCCESS     sets @ref cy_ota_agent_state_t
 *          CY_RSLT_OTA_ERROR_GENERAL
 */
cy_rslt_t cy_ota_get_state(cy_ota_context_ptr ota_ptr, cy_ota_agent_state_t *state);

/**
 * @brief Get Last OTA Error
 *
 * The last error value is persistent, and can be queried after @ref cy_ota_agent_stop().
 *
 * @result  @ref cy_rslt_t
 */
cy_rslt_t cy_ota_get_last_error(void);

/**
 * @brief Get String representation of error
 *
 * This can be called at any time.
 *
 * @param[in]   error      value returned from cy_ota_get_last_error()
 *
 * @result pointer to a string (NULL for unknown error)
 */
const char *cy_ota_get_error_string(cy_rslt_t error);

/**
 * @brief Get String Representation of OTA state
 *
 * This can be called at any time.
 *
 * @param[in]   state      @ref cy_ota_agent_state_t
 *
 * @result  pointer to string (NULL for unknown state)
 */
const char *cy_ota_get_state_string(cy_ota_agent_state_t state);

/**
 * @brief Get String representation for callback reason
 *
 * This can be called at any time.
 *
 * @param[in]   reason      @ref cy_ota_cb_reason_t
 *
 * @result  pointer to string (NULL for unknown reason)
 */
const char *cy_ota_get_callback_reason_string(cy_ota_cb_reason_t reason);

/** \} group_ota_functions */

#ifdef __cplusplus
    }
#endif

#endif /* CY_OTA_API_H__ */

 /** \} group_cy_ota */
