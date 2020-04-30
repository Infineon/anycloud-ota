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
 * \defgroup group_ota_callback OTA Agent Callback
 * \defgroup group_ota_functions OTA Functions
 * \defgroup group_ota_structures OTA Structures
 * \defgroup group_ota_macros OTA Macros
 * \defgroup group_ota_typedefs OTA Typedefs
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

 /**
  * \addtogroup group_ota_macros
  * \{
  */

/***********************************************************************
 * Error Defines - move to cy_result_mw.h
 ***********************************************************************/
/** Base module identifier for Middleware Libraries (0x0a00 - 0x0aFF) */
#define CY_RSLT_MODULE_OTA_BASE              (0x0a00U)

#define CY_RSLT_MODULE_OTA_ERROR                (CY_RSLT_MODULE_OTA_BASE +  1) /**< Generic error            */
#define CY_RSLT_MODULE_OTA_BADARG               (CY_RSLT_MODULE_OTA_BASE +  2) /**< Bad argument             */
#define CY_RSLT_MODULE_OTA_UNSUPPORTED          (CY_RSLT_MODULE_OTA_BASE +  3) /**< Unsupported feature      */
#define CY_RSLT_MODULE_OTA_ALREADY_STARTED      (CY_RSLT_MODULE_OTA_BASE +  4) /**< OTA Already started      */
#define CY_RSLT_MODULE_NO_UPDATE_AVAILABLE      (CY_RSLT_MODULE_OTA_BASE +  5) /**< No OTA update on Server  */
#define CY_RSLT_MODULE_OTA_MQTT_INIT_ERROR      (CY_RSLT_MODULE_OTA_BASE +  6) /**< MQTT init failed         */
#define CY_RSLT_MODULE_OTA_TCP_INIT_ERROR       (CY_RSLT_MODULE_OTA_BASE +  7) /**< TCP init failed          */
#define CY_RSLT_MODULE_OTA_CONNECT_ERROR        (CY_RSLT_MODULE_OTA_BASE +  8) /**< Server connect Failed    */
#define CY_RSLT_MODULE_OTA_MQTT_DROPPED_CNCT    (CY_RSLT_MODULE_OTA_BASE +  9) /**< MQTT Broker no ping resp */
#define CY_RSLT_MODULE_OTA_GET_ERROR            (CY_RSLT_MODULE_OTA_BASE + 10) /**< Failed to get OTA Image  */
#define CY_RSLT_MODULE_OTA_NOT_A_REDIRECT       (CY_RSLT_MODULE_OTA_BASE + 11) /**< Not Supported Yet        */
#define CY_RSLT_MODULE_OTA_NOT_A_HEADER         (CY_RSLT_MODULE_OTA_BASE + 12) /**< No header in payload     */
#define CY_RSLT_MODULE_OTA_DISCONNECT_ERROR     (CY_RSLT_MODULE_OTA_BASE + 13) /**< Server disconnect error  */
#define CY_RSLT_MODULE_OTA_OPEN_STORAGE_ERROR   (CY_RSLT_MODULE_OTA_BASE + 14) /**< Local storage open error */
#define CY_RSLT_MODULE_OTA_WRITE_STORAGE_ERROR  (CY_RSLT_MODULE_OTA_BASE + 15) /**< Write to local storage error */
#define CY_RSLT_MODULE_OTA_CLOSE_STORAGE_ERROR  (CY_RSLT_MODULE_OTA_BASE + 16) /**< Close local storage error    */
#define CY_RSLT_MODULE_OTA_VERIFY_ERROR         (CY_RSLT_MODULE_OTA_BASE + 17) /**< Verify image failure     */
#define CY_RSLT_MODULE_OTA_OUT_OF_MEMORY_ERROR  (CY_RSLT_MODULE_OTA_BASE + 18) /**< Out of Memory Error      */
#define CY_RSLT_MODULE_OTA_INVALID_VERSION      (CY_RSLT_MODULE_OTA_BASE + 19) /**< Payload invalid version  */

 /** \} group_ota_macros */

 /***********************************************************************
  * Define min / max values
  ***********************************************************************/

/**
  * \addtogroup group_ota_typedefs
  * \{
  */

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
 * @brief Transport type to use
 */
typedef enum {
    CY_OTA_TRANSPORT_MQTT   =   0,              /**< Use MQTT Transport             */
#ifdef OTA_HTTP_SUPPORT
    CY_OTA_TRANSPORT_HTTP,                      /**< Use HTTP Transport  (FUTURE)   */
#endif
} cy_ota_transport_t;

/**
 * @brief MQTT Session clean flag
 *
 * This flag signals the MQTT broker to start a new session or restart an existing session
 */
typedef enum {
    CY_OTA_MQTT_SESSION_CLEAN = 0,  /**< Start a clean MQTT session with the broker         */
    CY_OTA_MQTT_SESSION_RESTART     /**< Restart an existing MQTT session with the broker   */
} cy_ota_mqtt_session_type_t;


/**
 * @brief OTA callback reasons
 */
typedef enum {
    CY_OTA_REASON_AGENT_STARTED = 0,            /**< Agent started successfully     */

    CY_OTA_REASON_EXCEEDED_RETRIES,             /**< Connection failed > retries    */
    CY_OTA_REASON_SERVER_CONNECT_FAILED,        /**< Server connection failed       */
    CY_OTA_REASON_DOWNLOAD_FAILED,              /**< OTA Download failed            */
    CY_OTA_REASON_SERVER_DROPPED_US,            /**< Broker did not respond to ping *
                                                 *   or disconnected. If App        *
                                                 *   provided the MQTT connection   *
                                                 *   the OTA Agent will exit        */
    CY_OTA_REASON_OTA_FLASH_WRITE_ERROR,        /**< OTA writing to FLASH failed    */
    CY_OTA_REASON_OTA_VERIFY_FAILED,            /**< OTA download verify failed     */
    CY_OTA_REASON_OTA_INVALID_VERSION,          /**< OTA download version failed    */

    CY_OTA_REASON_CONNECTING,                   /**< Agent Start server connection  */
    CY_OTA_REASON_CONENCTED_TO_SERVER,          /**< Agent connected to server      */
    CY_OTA_REASON_DOWNLOAD_STARTED,             /**< Download started               */
    CY_OTA_REASON_DOWNLOAD_PERCENT,             /**< Download percentage            */
    CY_OTA_REASON_DOWNLOAD_COMPLETED,           /**< Download completed             */
    CY_OTA_REASON_DISCONNECTED,                 /**< Agent disconnected from server */
    CY_OTA_REASON_OTA_VERIFIED,                 /**< OTA download verify succeeded  */
    CY_OTA_REASON_OTA_COMPLETED,                /**< OTA download completed         */
    CY_OTA_REASON_NO_UPDATE,                    /**< OTA No update at this time     */

    CY_OTA_LAST_REASON                          /**< Do not use                     */
} cy_ota_cb_reason_t;

/**
 * @brief OTA state
 */
typedef enum {
    CY_OTA_STATE_NOT_INITIALIZED= 0,    /**< OTA system not initialized             */
    CY_OTA_STATE_EXITING,               /**< OTA system exiting                     */
    CY_OTA_STATE_INITIALIZING,          /**< OTA system initializing                */
    CY_OTA_STATE_AGENT_WAITING,         /**< OTA Agent waiting for instruction      */
    CY_OTA_STATE_CONNECTING,            /**< OTA Agent Connecting to Server         */
    CY_OTA_STATE_DOWNLOADING,           /**< Download of update file in progress    */
    CY_OTA_STATE_DONWLOAD_COMPLETE,     /**< All data has been received             */
    CY_OTA_STATE_DISCONNECTING,         /**< OTA Agent disconnecting from server    */
    CY_OTA_STATE_VERIFYING,             /**< OTA Agent verifying download           */

    CY_OTA_LAST_STATE                   /**< Not used                               */
} cy_ota_agent_state_t;

/**
 * @brief OTA errors
 */
typedef enum
{
    OTA_ERROR_NONE = 0,
    OTA_ERROR_CONNECTING,           /**< Error connecting to update server or Broker    */
    OTA_ERROR_DOWNLOADING,          /**< Error downloading update                       */
    OTA_ERROR_NO_UPDATE,            /**< Error no update available                      */
    OTA_ERROR_WRITING_TO_FLASH,     /**< Error Writing to FLASH                         */
    OTA_ERROR_VERIFY_FAILED,        /**< Error Verify Step failed                       */
    OTA_ERROR_INVALID_VERSION,      /**< Error Invalid version                          */
    OTA_ERROR_SERVER_DROPPED,       /**< Error Server did not answer keep-alive ping    */

    /* MQTT specific */
    OTA_ERROR_SUBSCRIBING,          /**< Error Subscribing to MQTT Broker               */

    CY_OTA_LAST_ERROR               /**< Not used                               */
} cy_ota_error_t;

/**
 *  OTA Context pointer
 *
 * Returned from cy_ota_agent_start() and used for subsequent calls
 */
typedef void *cy_ota_context_ptr;

/** \} group_ota_typedefs */

/**
 * \addtogroup group_ota_callback
 * \{
 *  OTA Agent callback function.
 *
 */

/**
 *  @brief Application informational callback from OTA Agent
 *
 * @param[in]  reason       - Callback reason @ref cy_ota_cb_reason_t
 * @param[in]  value        - When callback reason is CY_OTA_REASON_DOWNLOAD_PERCENT\n
 *                            The value is the percentage of the download completed
 * @param[in]  cb_arg       - user supplied data ptr
 *
 * @return - N/A
 */
typedef void (*cy_ota_callback_t)(cy_ota_cb_reason_t reason, uint32_t value, void *cb_arg );

/** \} group_ota_callback */

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
 * OTA Server/Broker information
 * \struct cy_ota_server_into_s
 */
typedef struct cy_ota_server_into_s {
        const char * pHostName;             /**< Server host name. Must be NULL-terminated. */
        uint16_t port;                      /**< Server port                                */
} cy_ota_server_into_t;

#ifdef OTA_HTTP_SUPPORT
/**
 * @brief OTA HTTP specific Connection parameters
 */
typedef struct cy_ota_http_params_s {
    const char              *job_file;                  /**< filename for update job    */
} cy_ota_http_params_t;
#endif

/**
 *  OTA MQTT specific Connection parameters
 * \struct cy_ota_mqtt_params_s
 */
typedef struct cy_ota_mqtt_params_s {
    /* MQTT info */
    bool                        awsIotMqttMode;    /**< 0 = normal MQTT, 1 = Special Amazon mode    */
    const char                  *pIdentifier;      /**< Ptr to device ID                            */
    uint8_t                     numTopicFilters;   /**< Number of Topics for MQTT Subscribe         */
    const char                  **pTopicFilters;   /**< Topic Filter text                           */
    cy_ota_mqtt_session_type_t  session_type;      /**< @ref cy_ota_mqtt_session_type_t             */

    /* If application already has an MQTT connection, set true and provide connection pointer */
    IotMqttConnection_t         app_mqtt_connection;   /**< Application MQTT connection information    */

} cy_ota_mqtt_params_t;

/**
 *  OTA Connection parameters structure
 *  \struct cy_ota_network_params_s
 */
typedef struct cy_ota_network_params_s {

    cy_ota_transport_t          transport;              /**< Transport Type @ref cy_ota_transport_t  */

    /* MQTT or HTTP */
    union {
        cy_ota_mqtt_params_t    mqtt;                   /**< MQTT transport information                 */
#ifdef OTA_HTTP_SUPPORT
        cy_ota_http_params_t    http;                   /**< HTTP transport information                 */
#endif
    } u;                                                /**< Union for different transport (FUTURE)     */

    /* Common parameters */
    cy_ota_server_into_t        server;                 /**< Server/MQTT Broker                         */

    IotNetworkCredentials_t     credentials;            /**< Setting the credentials pointer uses TlS   */

    const void                  *network_interface;     /**< network interface                          */

} cy_ota_network_params_t;

/**
 *  OTA Agent parameters structure
 *  \struct cy_ota_agent_params_s
 */
typedef struct cy_ota_agent_params_s {

    uint8_t     reboot_upon_completion;     /**< Automatically reboot upon download completion and verify       */
    uint8_t     validate_after_reboot;      /**< 0 = OTA will set mcuboot to permanent before reboot            *
                                             *   1 = Application must call cy_ota_validated() after reboot      */

    cy_ota_callback_t   cb_func;            /**< Callback notification function                                 */
    void                *cb_arg;            /**< Opaque arg used in callback function                           */
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
 * @param[in]  network_params   pointer to @ref cy_ota_network_params_s
 * @param[in]  agent_params     pointer to @ref cy_ota_agent_params_s
 * @param[out] ota_ptr          Handle to store OTA Agent context structure pointer\n
 *                               which is used in other OTA calls.
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_MODULE_OTA_ERROR
 */
cy_rslt_t cy_ota_agent_start(cy_ota_network_params_t *network_params, cy_ota_agent_params_t *agent_params, cy_ota_context_ptr *ota_ptr);


/**
 * @brief Stop OTA Background Agent
 *
 *  Stop thread to monitor OTA updates and release resources.
 *
 * @param[in] ota_ptr         pointer to OTA Agent context storage returned from @ref cy_ota_agent_start();
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_MODULE_OTA_ERROR
 */
cy_rslt_t cy_ota_agent_stop(cy_ota_context_ptr *ota_ptr);

/**
 * @brief Check for OTA update availability and download update now.
 *
 * @param[in] ota_ptr         pointer to OTA Agent context storage returned from @ref cy_ota_agent_start();
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_MODULE_OTA_BADARG
 *          CY_RSLT_MODULE_OTA_ERROR
 *          CY_RSLT_MODULE_OTA_ALREADY_STARTED
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
 *          CY_RSLT_MODULE_OTA_ERROR
 */
cy_rslt_t cy_ota_validated(void);

/**
 * @brief Get OTA Agent State
 *
 * @param[in]  ota_ptr          pointer to OTA Agent context returned from @ref cy_ota_agent_start();
 * @param[out] state            Current OTA State
 *
 * @result @ref cy_ota_agent_state_t
 */
cy_rslt_t cy_ota_get_state(cy_ota_context_ptr ota_ptr, cy_ota_agent_state_t *state);

/**
 * @brief Get Last OTA Error
 *
 * The last error value is persistent, and can be queried after @ref cy_ota_agent_stop().
 *
 * @result @ref cy_ota_error_t
 */
cy_ota_error_t cy_ota_last_error(void);

/**
 * @brief Get Error String
 *
 * This can be called at any time.
 *
 * @param[in]  error      value returned from cy_ota_last_error()
 *
 * @result pointer to a string
 */
const char *cy_ota_get_error_string(cy_ota_error_t error);

/**
 * @brief Get String Representation of OTA state
 *
 * This can be called at any time.
 *
 * @param   state      @ref cy_ota_agent_state_t
 *
 * @result  pointer to string
 */
const char *cy_ota_get_state_string(cy_ota_agent_state_t state);

/**
 * @brief Get String representation for callback reason
 *
 * This can be called at any time.
 *
 * @param   reason      @ref cy_ota_cb_reason_t
 *
 * @result  pointer to string
 */
const char *cy_ota_get_callback_reason_string(cy_ota_cb_reason_t reason);

/** \} group_ota_functions */

#ifdef __cplusplus
    }
#endif

#endif /* CY_OTA_API_H__ */

 /** \} group_cy_ota */
