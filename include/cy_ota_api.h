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
 * \defgroup group_ota_functions Functions
 * \defgroup group_ota_structures Structures
 * \defgroup group_ota_macros Macros
 * \defgroup group_ota_typedefs Typedefs
 */

/**
 *  \mainpage
 *
 * Basic concept
 * =============
 *  MCUBoot runs the current application in Primary Slot.
 *  A New application is downloaded and stored in the Secondary Slot.
 *  The Secondary Slot is marked so that MCUBoot will copy it over to the Primary Slot.\n
 *  If validate_after_reboot == 0 then Secondary Slot will be tagged as "perm"\n
 *  If validate_after_reboot == 1 then Secondary Slot will be tagged as "test"\n
 *  If  reboot_upon_completion == 1, the system will reboot automatically.\n
 *  On the next system reboot, MCUBoot sees that there is a New Application in the Secondary Slot.
 *  MCUBoot then copies the New Application from Secondary Slot to Primary Slot.
 *  MCUBoot runs New Application in Primary Slot.
 *  If validate_after_reboot == 1 then the New Application must validate itself and call cy_ota_validated() to complete the process.
 *
 * API
 * ===
 *
 * Start background agent to check for updates
 * -------------------------------------------
 * This is a non-blocking call.\n
 * This consumes resources while running.\n
 *   Thread\n
 *   RAM\n
 *   socket\n
 *   timer\n
 *   events\n
 * The start function returns a context pointer that is used in subsequent calls.\n
 * The OTA Agent uses callbacks to signal application when events happen events.\n
 * `cy_rslt_t cy_ota_agent_start(cy_ota_network_params_t *network_params, cy_ota_agent_params_t *agent_params, cy_ota_context_ptr *ota_ptr);`
 * \n
 * These defines determine when the checks happen. Between checks, the OTA Agent is not checking for updates.\n
 * CY_OTA_INITIAL_CHECK_SECS
 * CY_OTA_NEXT_CHECK_INTERVAL_SECS
 * CY_OTA_RETRY_INTERVAL_SECS
 * CY_OTA_CHECK_TIME_SECS
 *
 *
 * Stop OTA agent
 * --------------
 * When you want to stop the OTA agent from checking for updates.\n
 * `cy_rslt_t cy_ota_agent_stop(cy_ota_context_ptr *ota_ptr);`
 *
 * Trigger a check right now.
 * ----------------------------------------------
 * Use this when you want to trigger a check for an OTA update earlier than
 * is currently scheduled. The OTA agent must have been started already.\n
 * `cy_rslt_t cy_ota_get_update_now(cy_ota_context_ptr ota_ptr);`
 *
 *
 * Set up cy_ota_config.h for application-specific over-riding defines
 * ===================================================================
 * You must create a file called <b>"cy_ota_config.h"</b> to contain over-riding defines for:
 *
 * \code
 * // Initial time for checking for OTA updates
 * // This is used to start the timer for the initial OTA update check after calling cy_ota_agent_start().
 * #define CY_OTA_INITIAL_CHECK_SECS           (10)                // 10 seconds
 *
 * // Next time for checking for OTA updates
 * // This is used to re-start the timer after an OTA update check in the OTA Agent.
 * #define CY_OTA_NEXT_CHECK_INTERVAL_SECS     (24 * 60 * 60)      // one day between checks
 *
 * // Retry time which checking for OTA updates
 * // This is used to re-start the timer after failing to contact the server during an OTA update check.
 * #define CY_OTA_RETRY_INTERVAL_SECS          (5)                 // seconds between retries after an error
 *
 * // Length of time to check for downloads
 * // OTA Agent wakes up, connects to server, and waits this much time before stopping checks.
 * // This allows the OTA Agent to be inactive for long periods of time, only checking at the interval.
 * // Use 0x00 to continue checking once started.
 * #define CY_OTA_CHECK_TIME_SECS                (60)              // 1 minute
 *
 * // Expected maximum download time between each OTA packet arrival.
 * // This is used check that the download occurs in a reasonable time frame.
 * #define CY_OTA_PACKET_INTERVAL_SECS       (60)                // 1 minute
 *
 * // Number of retries when attempting OTA update
 * // This is used to determine # retries when attempting an OTA update.
 * #define CY_OTA_RETRIES                       (5)                // retry entire process 5 times
 *
 * // Number of retries when attempting to contact the server
 * // This is used to determine # retries when connecting to the server during an OTA update check.
 * #define CY_OTA_CONNECT_RETRIES              (3)                 // 3 server connect retries
 *
 * // The number of MQTT Topics to subscribe to
 * #define CY_OTA_MQTT_MAX_TOPICS               (2)                 // 2 topics
 *
 * // The keep-alive interval for MQTT
 * // An MQTT ping request will be sent periodically at this interval.
 * #define CY_OTA_MQTT_KEEP_ALIVE_SECONDS           ( 60 )         // 60 second keep-alive
 *
 * // The timeout for MQTT operations.
 * #define CY_OTA_MQTT_TIMEOUT_MS                   ( 5000 )       // 5 second timeout waiting for MQTT response
 *
 * // This queue allows us to always access FLASH from the same thread, and release
 * // the MQTT packet as soon as possible.
 * // This is the number of queue elements in the queue.
 * // Total queue size is a bit more than (CY_OTA_RECV_QUEUE_LEN * CY_OTA_MQTT_BUFFER_SIZE_MAX)
 * // Increasing the buffer size allows more data to be transferred per packet, but also consumes RAM.
 * #define CY_OTA_RECV_QUEUE_LEN                   (4)
 *
 * // Max OTA payload data size (not MQTT payload size)
 * #define CY_OTA_MQTT_BUFFER_SIZE_MAX             (8 * 1024)
 * \endcode
 *
 * Example of call sequence to start OTA agent
 * ===========================================
 * \code
 *
 * #define OTA_SERVER_URL                      "test.mosquitto.org";
 *
 * #define OTA_MQTT_SERVER_PORT                (1883)
 * #define OTA_MQTT_SECURE_SERVER_PORT         (8883)
 *
 * // MQTT identifier
 * #define OTA_MQTT_ID                         "CY8CP_062_4343W"
 *
 * // MQTT topics
 * const char * my_topics[ CY_OTA_MQTT_MAX_TOPICS ] =
 * {
 *        "anycloud/ota/image"
 * };
 *
 * // MQTT Credentials for OTA
 * struct IotNetworkCredentials    credentials = { 0 };
 *
 * // network parameters for OTA
 * cy_ota_network_params_t     ota_test_network_params = { 0 };
 *
 * //brief aAgent parameters for OTA
 * cy_ota_agent_params_t     ota_test_agent_params = { 0 };
 *
 * //forward declaration for OTA callback function
 * static void ota_callback(cy_ota_cb_reason_t reason, uint32_t value, void *cb_arg );
 *
 * //OTA context
 * cy_ota_context_ptr ota_context;
 *
 * // OTA callback function
 * static void ota_callback(cy_ota_cb_reason_t reason, uint32_t value, void *cb_arg )
 * {
 *    cy_ota_agent_state_t ota_state;
 *    cy_ota_context_ptr ctx = *((cy_ota_context_ptr *)cb_arg);
 *    cy_ota_get_state(ctx, &ota_state);
 *    printf("Application OTA callback ctx:%p reason:%d %s value:%ld state:%d %s", ctx,
 *            reason, cy_ota_get_callback_reason_string(reason), value,
 *            ota_state, cy_ota_get_state_string(ota_state) );
 * }
 *
 *  main ()
 * {
 *    // Initialize the Wifi and connect to an AP here
 *    // Do what is appropriate for your board
 *
 *    // Initialize underlying support code that is needed for OTA and MQTT
 *    if (IotSdk_Init() != 1)
 *    {
 *        printf("IotSdk_Init Failed.");
 *        while(true)
 *        {
 *            cy_rtos_delay_milliseconds(10);
 *        }
 *    }
 *
 *    // Call the Network Secured Sockets initialization function.
 *    IotNetworkError_t networkInitStatus = IOT_NETWORK_SUCCESS;
 *    networkInitStatus = IotNetworkSecureSockets_Init();
 *    if( networkInitStatus != IOT_NETWORK_SUCCESS )
 *    {
 *        printf("IotNetworkSecureSockets_Init Failed.");
 *        while(true)
 *        {
 *            cy_rtos_delay_milliseconds(10);
 *        }
 *    }
 *
 *    // Initialize the MQTT subsystem
 *    if( IotMqtt_Init() != IOT_MQTT_SUCCESS )
 *    {
 *        printf("IotMqtt_Init Failed.");
 *        while(true)
 *        {
 *            cy_rtos_delay_milliseconds(10);
 *        }
 *    }
 *
 *    // Prepare structures for initializing OTA Agent
 *    ota_test_network_params.server.pHostName = OTA_SERVER_URL;    // URL must not be a local variable
 *    ota_test_network_params.server.port = OTA_MQTT_SERVER_PORT;
 *    // For MQTT
 *    ota_test_network_params.transport = CY_OTA_TRANSPORT_MQTT;
 *    ota_test_network_params.u.mqtt.numTopicFilters = 1;
 *    ota_test_network_params.u.mqtt.pTopicFilters = my_topics;
 *    ota_test_network_params.u.mqtt.pIdentifier = OTA_MQTT_ID;
 *    ota_test_network_params.network_interface = (void *)IOT_NETWORK_INTERFACE_CY_SECURE_SOCKETS;
 *
 *    - Set up the credentials information
 * #if (OTA_MQTT_USE_TLS == 1)
 *    credentials.pAlpnProtos = NULL;
 *    credentials.maxFragmentLength = 0;
 *    credentials.disableSni = 0;
 *    credentials.pRootCa = (const char *) \&root_ca_certificate;
 *    credentials.rootCaSize = sizeof(root_ca_certificate) - 1;
 *    credentials.pClientCert = (const char *) \&client_cert;
 *    credentials.clientCertSize = sizeof(client_cert) - 1;
 *    credentials.pPrivateKey = (const char *) \&client_key;
 *    credentials.privateKeySize = sizeof(client_key) - 1;
 *    credentials.pUserName = "Test";
 *    credentials.userNameSize = strlen("Test");
 *    credentials.pPassword = "";
 *    credentials.passwordSize = 0;
 *    ota_test_network_params.credentials = \&credentials;
 *    ota_test_network_params.server.port = OTA_MQTT_SECURE_SERVER_PORT;
 * #else
 *    ota_test_network_params.credentials = NULL;
 *    ota_test_network_params.server.port = OTA_MQTT_SERVER_PORT;
 * #endif
 *    ota_test_network_params.u.mqtt.awsIotMqttMode = 0;
 *
 *    // OTA Agent parameters
 *    ota_test_agent_params.validate_after_reboot = 0;
 *    ota_test_agent_params.reboot_upon_completion = 1;
 *    ota_test_agent_params.cb_func = ota_callback;
 *    ota_test_agent_params.cb_arg = \&ota_context;
 *
 *    result = cy_ota_agent_start(\&ota_test_network_params, \&ota_test_agent_params, \&ota_context);
 *    if (result != CY_RSLT_SUCCESS)
 *    {
 *        printf("cy_ota_agent_start() Failed - result: 0x%lx", result);
 *        while(true)
 *        {
 *            cy_rtos_delay_milliseconds(10);
 *        }
 *    }
 *
 *   // OTA Agent is running in another thread
 *   // The rest of the code can run here
 *    while(true)
 *    {
 *        cy_rtos_delay_milliseconds(10);
 *    }
 * }
 * \endcode
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

#if (CY_OTA_PACKET_INTERVAL_SECS < CY_OTA_INTERVAL_SECS_MIN)
    #error  "CY_OTA_PACKET_INTERVAL_SECS must be greater or equal to CY_OTA_INTERVAL_SECS_MIN."
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
 * @brief OTA callback reasons
 */
typedef enum {
    CY_OTA_REASON_AGENT_STARTED = 0,            /**< Agent started successfully     */

    CY_OTA_REASON_EXCEEDED_RETRIES,             /**< Connection failed > retries    */
    CY_OTA_REASON_SERVER_CONNECT_FAILED,        /**< Server connection failed       */
    CY_OTA_REASON_DOWNLOAD_FAILED,              /**< OTA Download failed            */
    CY_OTA_REASON_SERVER_DROPPED_US,            /**< Broker did not respond to ping */
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
 * \addtogroup group_ota_functions
 * \{
 */

/**
 *  @brief Application informational callback from OTA Agent
 *
 * @param[in]  reason       - Callback reason
 * @param[in]  value        - used for %
 * @param[in]  cb_arg       - user supplied data ptr
 *
 * @return - N/A
 */
typedef void (*cy_ota_callback_t)(cy_ota_cb_reason_t reason, uint32_t value, void *cb_arg );

/** \} group_ota_functions */

/***********************************************************************
 *
 * Structures
 *
 **********************************************************************/

/**
 * \addtogroup group_ota_structures
 * \{
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
    bool                    awsIotMqttMode;    /**< 1 = AFR AWS mode                        */
    const char              *pIdentifier;      /**< Ptr to device ID                        */
    uint8_t                 numTopicFilters;   /**< Number of Topics for MQTT Subscribe     */
    const char              **pTopicFilters;   /**< Topic Filter text                       */

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
 */

/**
 * @brief Start OTA Background Agent
 *
 * Start thread to monitor for OTA updates.
 * This consumes resources until cy_ota_agent_stop() is called.
 *
 * @param[in]  network_params   pointer to Network parameter structure
 * @param[in]  agent_params     pointer to Agent timing parameter structure
 * @param[out] ota_ptr          Handle to OTA Agent context structure pointer
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
 * @param[in] ota_ptr         pointer to OTA Agent context storage returned from cy_ota_agent_start();
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_MODULE_OTA_ERROR
 */
cy_rslt_t cy_ota_agent_stop(cy_ota_context_ptr *ota_ptr);

/**
 * @brief Check for OTA update availability and download update now.
 *
 * @param[in] ota_ptr         pointer to OTA Agent context storage returned from cy_ota_agent_start();
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_MODULE_OTA_ERROR
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
 * @param[in]  ota_ptr          pointer to OTA Agent context returned from cy_ota_agent_start();
 * @param[out] state            Current OTA State
 *
 * @result @ref cy_ota_agent_state_t
 */
cy_rslt_t cy_ota_get_state(cy_ota_context_ptr ota_ptr, cy_ota_agent_state_t *state);

/**
 * @brief Get Last OTA Error
 *
 * This can be called after cy_ota_agent_stop(), the value is persistent.
 *
 * @result Last error reported
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
