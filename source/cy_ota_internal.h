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

#ifndef CY_OTA_INTERNAL_H__
#define CY_OTA_INTERNAL_H__ 1


#ifdef __cplusplus
extern "C" {
#endif

#include "cyabs_rtos.h"

#include "iot_mqtt.h"
#include "iot_mqtt_types.h"

/* This is so that Eclipse doesn't complain about the Logging messages */
#ifndef NULL
#define NULL    0
#endif

// change to IOT_LOG_DEBUG for lots of output
#define LIBRARY_LOG_LEVEL   IOT_LOG_INFO
#define LIBRARY_LOG_NAME    "OTA"

#include "iot_logging_setup.h"

// We are not ready yet !
//#define OTA_SIGNING_SUPPORT   1

// Not supported yet
//#define OTA_HTTP_SUPPORT    1
/***********************************************************************
 *
 * defines & enums
 *
 **********************************************************************/
#define SECS_TO_MILLISECS(secs)         (secs * 1000)

/**
 * @brief Time to wait for queue to be free / data available
 */
#define CY_OTA_RECV_QUEUE_TIMEOUT_MS        (500)

/**
 * @brief Tag value used to validate the OTA context
 */
#define CY_OTA_TAG      0x0ad38f41

/**
 * @brief Maximum size of signature scheme descriptive string
 *
 *  ex: "sig-sha256-ecdsa"
 */
#define CY_OTA_MAX_SIGN_LENGTH                      (32)


/* OTA events */
typedef enum
{
    OTA_EVENT_AGENT_RUNNING                 = (1 <<  0),    /**< Notifies cy_ota_agent_start() it is running    */
    OTA_EVENT_AGENT_EXITING                 = (1 <<  1),    /**< Notifies cy_ota_agent_stop() it is exiting     */

    OTA_EVENT_AGENT_SHUTDOWN_NOW            = (1 <<  2),    /**< Notifies Agent to shutdown                     */

    OTA_EVENT_AGENT_START_INITIAL_TIMER     = (1 <<  3),    /**< Start initial OTA check timer                  */
    OTA_EVENT_AGENT_START_NEXT_TIMER        = (1 <<  4),    /**< Start next OTA check timer                     */
    OTA_EVENT_AGENT_START_RETRY_TIMER       = (1 <<  5),    /**< Start retry OTA check timer                    */

    OTA_EVENT_AGENT_START_UPDATE            = (1 <<  6),    /**< Start update process                           */
    OTA_EVENT_AGENT_CONNECT                 = (1 <<  7),    /**< Connect to server                              */
    OTA_EVENT_AGENT_DOWNLOAD                = (1 <<  8),    /**< Get OTA update                                 */
    OTA_EVENT_AGENT_DISCONNECT              = (1 <<  9),    /**< Disconnect from server                         */
    OTA_EVENT_AGENT_VERIFY                  = (1 << 10),    /**< Verify download                                */

    OTA_EVENT_AGENT_PACKET_TIMEOUT          = (1 << 11),    /**< Packet download timed out                      */
    OTA_EVENT_AGENT_DOWNLOAD_TIMEOUT        = (1 << 12),    /**< Timed out waiting for download job             */

    OTA_EVENT_AGENT_STORAGE_ERROR           = (1 << 13),    /**< Notify cy_ota_get() to exit - STORAGE - ERROR  */
    OTA_EVENT_INVALID_VERSION               = (1 << 14),    /**< Notify cy_ota_get() to exit - INVALID VERSION  */

    /* MQTT specific */
    OTA_EVENT_MQTT_GOT_DATA                 = (1 << 15),    /**< Notify cy_ota_mqtt_get() that we got some data */
    OTA_EVENT_MQTT_DATA_DONE                = (1 << 16),    /**< Notify cy_ota_get() that we got all the data   */
    OTA_EVENT_MQTT_DATA_FAIL                = (1 << 17),    /**< Data failure (could be sign / decrypt fail)    */
    OTA_EVENT_MQTT_REDIRECT                 = (1 << 18),    /**< MQTT re-directed to HTTP for OTA download      */
    OTA_EVENT_MQTT_DROPPED_US               = (1 << 19),    /**< MQTT Broker did not respond to ping            */

} ota_events_t;

/* OTA Agent main loop events to wait look for */
#define OTA_EVENT_AGENT_THREAD_EVENTS   ( OTA_EVENT_AGENT_SHUTDOWN_NOW | \
                                           OTA_EVENT_AGENT_START_INITIAL_TIMER | \
                                           OTA_EVENT_AGENT_START_NEXT_TIMER | \
                                           OTA_EVENT_AGENT_START_RETRY_TIMER | \
                                           OTA_EVENT_AGENT_START_UPDATE | \
                                           OTA_EVENT_AGENT_CONNECT | \
                                           OTA_EVENT_AGENT_DOWNLOAD_TIMEOUT | \
                                           OTA_EVENT_AGENT_DOWNLOAD | \
                                           OTA_EVENT_AGENT_DISCONNECT | \
                                           OTA_EVENT_AGENT_VERIFY )
/* Main OTA Agent loop event wait time */
#define CY_OTA_WAIT_FOR_EVENTS_MS       (CY_RTOS_NEVER_TIMEOUT)

/* OTA MQTT main loop events to wait look for */
#define OTA_EVENT_MQTT_EVENTS  ( OTA_EVENT_AGENT_SHUTDOWN_NOW | \
                                    OTA_EVENT_AGENT_PACKET_TIMEOUT | \
                                    OTA_EVENT_AGENT_DOWNLOAD_TIMEOUT | \
                                    OTA_EVENT_AGENT_STORAGE_ERROR | \
                                    OTA_EVENT_AGENT_DISCONNECT | \
                                    OTA_EVENT_INVALID_VERSION | \
                                    OTA_EVENT_MQTT_GOT_DATA |  \
                                    OTA_EVENT_MQTT_DATA_DONE | \
                                    OTA_EVENT_MQTT_DATA_FAIL | \
                                    OTA_EVENT_MQTT_REDIRECT | \
                                    OTA_EVENT_MQTT_DROPPED_US )

/* Inner MQTT event loop - time to wait for events */
#define CY_OTA_WAIT_MQTT_EVENTS_MS      (CY_RTOS_NEVER_TIMEOUT)

/* Time to wait for a free Queue entry to pass buffer to MQTT event loop */
#define CY_OTA_WAIT_MQTT_MUTEX_MS       (SECS_TO_MILLISECS(20))

/***********************************************************************
 *
 * Macros
 *
 **********************************************************************/

/**
 * @brief Verify that the OTA Context is valid
 */
#define CY_OTA_CONTEXT_ASSERT(ctx)  CY_ASSERT((ctx!=0) && (ctx->tag==CY_OTA_TAG))

/***********************************************************************
 *
 * Structures
 *
 **********************************************************************/

/**
 * @brief Extra MQTT context data
 */
typedef struct cy_ota_mqtt_context_s {

    bool                connectionEstablished;          /**< true if MQTT connection established    */

    IotMqttConnection_t mqttConnection;                 /**< MQTT connection information            */

    cy_timer_t          mqtt_timer;                     /**< for detecting early end of download    */
    ota_events_t        mqtt_timer_event;               /**< event to trigger when timer goes off   */
} cy_ota_mqtt_context_t;

#ifdef OTA_HTTP_SUPPORT
/**
 * @brief Extra HTTP context data
 */
typedef struct cy_ota_http_context_s {
    struct netconn          *tcp_conn;
} cy_ota_http_context_t;
#endif

/**
 * @brief internal OTA Context structure
 */
typedef struct cy_ota_context_s {

    uint32_t                tag;                    /**< Must be CY_OTA_TAG to be valid         */

    cy_ota_network_params_t network_params;         /**< copy of initial connection parameters  */
    cy_ota_agent_params_t   agent_params;           /**< copy of initial agent parameters       */

    cy_event_t              ota_event;              /**< Event signaling @ref ota_events_t      */
    cy_thread_t             ota_agent_thread;       /**< OTA Agent Thread                       */

    cy_ota_agent_state_t    curr_state;             /**< current OTA system state               */

    uint32_t                initial_timer_sec;      /**< Seconds before attempting connect after cy_ota_agent_start()   */
    uint32_t                next_timer_sec;         /**< Seconds between connect after successful download              */
    uint32_t                retry_timer_sec;        /**< Seconds between connect retry after failed download            */
    uint32_t                check_timeout_sec;      /**< Seconds to wait for server to start download                   */
    uint32_t                packet_timeout_sec;     /**< Seconds to wait between packets for download                   */
    uint16_t                ota_retries;            /**< count # retries between initial or next intervals              */

    cy_timer_t              ota_timer;              /**< for delaying start of connections      */
    ota_events_t            ota_timer_event;        /**< event to trigger when timer goes off   */

    /* Storage and progress info */
    void                    *storage_loc;           /**< can be cast as flash_area or FILE as needed                    */
    uint32_t                total_image_size;       /**< Total size of OTA Image                                        */
    uint32_t                total_bytes_written;    /**< Number of bytes written to FLASH                               */
    uint32_t                last_offset;            /**< Last offset written to from cy_ota_storage_write()             */
    uint32_t                last_size;              /**< last size of data written from cy_ota_storage_write()          */
    uint16_t                last_packet_received;   /**< Last Packet of data we have received                           */
    uint16_t                total_packets;          /**< Total number of Packets of data for the OTA Image              */

    cy_mutex_t              sub_callback_mutex;     /**< Keep subscription callbacks from being timesliced              */
    cy_mutex_t              recv_mutex;             /**< Keep data_queue from being accessed by multi threads           */
    cy_queue_t              recv_data_queue;        /**< Pass data from callbacks to cy_ota_get() loop                  */

    /* Network connection */
    int                     contact_server_retry_count; /**< Keep count of # tries to contact server                    */
    int                     download_retry_count;       /**< Keep count of # tries to download update                   */

    cy_ota_transport_t      curr_transport;         /**< Transport Type @ref cy_ota_transport_t                         */
    cy_ota_server_into_t    curr_server;            /**< current server information (may have changed by a redirect)    */
    union {
        cy_ota_mqtt_context_t   mqtt;               /**< MQTT specific context data             */
#ifdef OTA_HTTP_SUPPORT
        cy_ota_http_context_t   http;               /**< HTTP specific context data             */
#endif
        }u;

#ifdef OTA_HTTP_SUPPORT
    /* For Update Model where MQTT re-directs us to get data from HTTP server. */
    bool                    redirect_to_http;       /**< MQTT packet told us to go get the OTA Image using HTTP         */
    IotNetworkServerInfo_t  new_server;             /**< Server to switch to                                            */
#endif
} cy_ota_context_t;

/**
 * @brief Struct to hold information on where to write data
 *
 * Information on where to store the downloaded chunk of OTA image.
 */

typedef struct cy_ota_storage_write_info_s {

    uint32_t        total_size; /**< Pass total size to storage module; 0 = disregard   */

    uint32_t        offset;     /**< Offset into file/area where data belongs           */
    uint8_t         *buffer;    /**< Pointer to buffer with chunk of data
                                     Replaced with malloc() buffer in
                                     cy_ota_mqtt_queue_data_to_write()
                                     Buffer free() when OTA_EVENT_MQTT_GOT_DATA event   */
    uint32_t        size;       /**< Size of data in buffer                             */

    uint16_t        packet_number;  /**< This packet number                             */
    uint16_t        total_packets;  /**< Total packets in OTA Image                     */
#ifdef OTA_SIGNING_SUPPORT
                    /* we are always base64 */
    char            signature_scheme[CY_OTA_MAX_SIGN_LENGTH];   /* eg: “sig-ecdsa-sha256” empty indicates not signed    */
    uint8_t         *signature_ptr;  /* Points to the signature length, Only valid if signature scheme is not empty */
#endif
} cy_ota_storage_write_info_t;

/***********************************************************************
 *
 * Variables
 *
 **********************************************************************/
/**@ brief Callback Reason Strings
 * For printing debug
 */
extern const char *cy_ota_reason_strings[CY_OTA_LAST_REASON];

/**@ brief OTA State Strings
 * For printing debug
 */
extern const char *cy_ota_state_strings[CY_OTA_LAST_STATE];

/***********************************************************************
 *
 * Functions
 *
 **********************************************************************/
/* --------------------------------------------------------------- */

/**
 * @brief OTA internal Callback to User
 *
 * @param   ctx     - OTA context
 * @param   reason  - OTA Callback reason
 * @param   value   - DOWNLOAD %
 *
 * @return  N/A
 */
void cy_ota_internal_call_cb( cy_ota_context_t *ctx, cy_ota_cb_reason_t reason, uint32_t value);

/***********************************************************************
 *
 * OTA Network abstraction
 *
 * in cy_ota_mqtt.c
 * in cy_ota_tcp.c
 *
 **********************************************************************/
/**
 * @brief Validate network parameters
 *
 * NOTE: Individual Network Transport type will test appropriate fields
 *
 * @param[in]  network_params   pointer to Network parameter structure
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_MODULE_OTA_ERROR
 */
#ifdef OTA_HTTP_SUPPORT
cy_rslt_t cy_ota_http_validate_network_params(cy_ota_network_params_t *network_params);
#endif
cy_rslt_t cy_ota_mqtt_validate_network_params(cy_ota_network_params_t *network_params);

/**
 * @brief Connect to OTA Update server
 *
 * NOTE: Individual Network Transport type will do whatever is necessary
 *      ex: MQTT
 *          - connect
 *          TCP
 *          - connect
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_MODULE_OTA_ERROR
 */
#ifdef OTA_HTTP_SUPPORT
cy_rslt_t cy_ota_http_connect(cy_ota_context_t *ctx);
#endif
cy_rslt_t cy_ota_mqtt_connect(cy_ota_context_t *ctx);

/**
 * @brief get the OTA download
 *
 * NOTE: Individual Network Transport type will do whatever is necessary
 *      ex: MQTT
 *          - subscribe to start data transfer
 *          TCP
 *          - pull the data from the server
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_MODULE_OTA_ERROR
 */
#ifdef OTA_HTTP_SUPPORT
cy_rslt_t cy_ota_http_get(cy_ota_context_t *ctx);
#endif
cy_rslt_t cy_ota_mqtt_get(cy_ota_context_t *ctx);

/**
 * @brief Open Storage area for download
 *
 * NOTE: Typically, this erases Secondary Slot
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_MODULE_OTA_ERROR
 */
#ifdef OTA_HTTP_SUPPORT
cy_rslt_t cy_ota_http_disconnect(cy_ota_context_t *ctx);
#endif
cy_rslt_t cy_ota_mqtt_disconnect(cy_ota_context_t *ctx);


/***********************************************************************
 *
 * OTA Storage abstraction
 *
 * in cy_ota_storage.c
 *
 **********************************************************************/

/**
 * @brief Open Storage area for download
 *
 * NOTE: Typically, this erases Secondary Slot
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_MODULE_OTA_ERROR
 */
cy_rslt_t cy_ota_storage_open(cy_ota_context_t *ctx);

/**
 * @brief Open Storage area for download
 *
 * @param[in]   ctx         - pointer to OTA agent context @ref cy_ota_context_t
 * @param[in]   chunk_info  - pointer to chunk information
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_MODULE_OTA_ERROR
 */
cy_rslt_t cy_ota_storage_write(cy_ota_context_t *ctx, cy_ota_storage_write_info_t *chunk_info);

/**
 * @brief Close Storage area for download
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_MODULE_OTA_ERROR
 */
cy_rslt_t cy_ota_storage_close(cy_ota_context_t *ctx);

/**
 * @brief Verify download signature on whole OTA Image
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_MODULE_OTA_ERROR
 */
cy_rslt_t cy_ota_storage_verify(cy_ota_context_t *ctx);

/**
 * @brief App has validated the new OTA Image
 *
 * This call needs to be after reboot and MCUBoot has copied the new Application
 *      to the Primary Slot.
 *
 * @param   N/A
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_MODULE_OTA_ERROR
 */
cy_rslt_t cy_ota_storage_validated(void);

#ifdef OTA_SIGNING_SUPPORT
/**
 * @brief Check that the OTA chunk is signed properly
 *
 * @param  chunk_info   ptr to info structure of the chunk
 *
 * @return  CY_RSLT_SUCCESS
 *         CY_RSLT_MODULE_OTA_BADARG
 *         CY_RSLT_MODULE_OTA_VERIFY_ERROR
 *         CY_RSLT_MODULE_OTA_OUT_OF_MEMORY_ERROR
 */
cy_rslt_t cy_ota_sign_check_chunk( cy_ota_storage_write_info_t *chunk_info);
#endif

#ifdef __cplusplus
    }
#endif

#endif /* CY_OTA_INTERNAL_H__ */
