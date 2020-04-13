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

/*
 *  Cypress OTA Agent network abstraction for MQTT
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "cy_ota_api.h"
#include "cy_ota_internal.h"
#include "cyabs_rtos.h"

#include "iot_config.h"

#include "iot_init.h"
#include "iot_network.h"

#include "iot_mqtt.h"
#include "iot_mqtt_types.h"

#include "cy_iot_network_secured_socket.h"

/***********************************************************************
 *
 * defines & enums
 *
 **********************************************************************/

/**
 * @brief The longest client identifier that an MQTT server must accept (as defined
 * by the MQTT 3.1.1 spec) is 23 characters. Add 1 to include the length of the NULL
 * terminator.
 */
#define CLIENT_IDENTIFIER_MAX_LENGTH                ( 24 )

/**
 * @brief Number of decimal digits for a 16 bit number
 * Used to size the Client ID string to be able to add a unique number to it.
 */
#define UINT16_DECIMAL_LENGTH                       ( 5 )

/**
 * @brief The Last Will and Testament topic name in this demo.
 *
 * The MQTT server will publish a message to this topic name if this client is
 * unexpectedly disconnected.
 */
#define WILL_TOPIC_NAME                             IOT_MQTT_TOPIC_PREFIX "/will"

/**
 * @brief The length of #WILL_TOPIC_NAME.
 */
#define WILL_TOPIC_NAME_LENGTH                      ( ( uint16_t ) ( sizeof( WILL_TOPIC_NAME ) - 1 ) )

/**
 * @brief The message to publish to #WILL_TOPIC_NAME.
 */
#define WILL_MESSAGE                                "MQTT demo unexpectedly disconnected."

/**
 * @brief The length of #WILL_MESSAGE.
 */
#define WILL_MESSAGE_LENGTH                         ( ( size_t ) ( sizeof( WILL_MESSAGE ) - 1 ) )

/**
 * @brief The maximum number of times each PUBLISH in this demo will be retried.
 */
#define PUBLISH_RETRY_LIMIT                         ( 10 )

/**
 * @brief A PUBLISH message is retried if no response is received within this
 * time.
 */
#define PUBLISH_RETRY_MS                            ( 1000 )

/**
 * @brief The topic name on which acknowledgement messages for incoming publishes
 * should be published.
 */
#define ACKNOWLEDGEMENT_TOPIC_NAME                  IOT_MQTT_TOPIC_PREFIX "/acknowledge"

/**
 * @brief The length of #ACKNOWLEDGEMENT_TOPIC_NAME.
 */
#define ACKNOWLEDGEMENT_TOPIC_NAME_LENGTH           ( ( uint16_t ) ( sizeof( ACKNOWLEDGEMENT_TOPIC_NAME ) - 1 ) )

/**
 * @brief Format string of PUBLISH acknowledgement messages in this demo.
 */
#define ACKNOWLEDGEMENT_MESSAGE_FORMAT              "Client has received PUBLISH %.*s from server."

/**
 * @brief Size of the buffers that hold acknowledgement messages in this demo.
 */
#define ACKNOWLEDGEMENT_MESSAGE_BUFFER_LENGTH       ( sizeof( ACKNOWLEDGEMENT_MESSAGE_FORMAT ) + 2 )

/***********************************************************************
 *
 * Macros
 *
 **********************************************************************/

/***********************************************************************
 *
 * Structures
 *
 **********************************************************************/
/**
 * @brief Payload chunk data structure
 *
 * This is used to copy the payload data and allow usage in mainline
 */
typedef struct cy_ota_mqtt_chunk_for_queue_s {
    bool                            in_use;
    cy_ota_storage_write_info_t     chunk_info;
    uint8_t                         chunk_buffer[CY_OTA_MQTT_BUFFER_SIZE_MAX];
} cy_ota_mqtt_chunk_for_queue_t;

/**
 * @brief "Magic" value placed in MQTT Payload header
 */
#define CY_OTA_MQTT_MAGIC                   "OTAImage"

/**
 * @brief MQTT Payload type
 */
typedef enum {
    CY_OTA_MQTT_HEADER_TYPE_ONE_FILE = 0    /**< OTA Image with single file (main application)  */
} cy_ota_mqtt_header_ota_type_t;

/**
 * @brief MQTT Payload header (not the MQTT header, this is our data header)
 */
#pragma pack(push,1)
typedef struct cy_ota_mqtt_chunk_payload_header_s {
    const char      magic[sizeof(CY_OTA_MQTT_MAGIC) - 1];       /**< "OTAImage" @ref CY_OTA_MQTT_MAGIC                  */
    const uint16_t  offset_to_data;                             /**< offset within this payload to start of data        */
    const uint16_t  ota_image_type;                             /**< 0 = single application OTA Image  @ref cy_ota_mqtt_header_ota_type_t */
    const uint16_t  update_version_major;                       /**< OTAImage Major version number                      */
    const uint16_t  update_version_minor;                       /**< OTAImage minor version number                      */
    const uint16_t  update_version_build;                       /**< OTAImage build version number                      */
    const uint32_t  total_size;                                 /* total size of OTA Image (all chunk data concatenated)*/
    const uint32_t  image_offset;                               /* offset within the final OTA Image of THIS chunk data */
    const uint16_t  data_size;                                  /* Size of chunk data in THIS payload                   */
    const uint16_t  total_num_payloads;                         /* Total number of payloads                             */
    const uint16_t  this_payload_index;                         /* THIS payload index                                   */
} cy_ota_mqtt_chunk_payload_header_t;
#pragma pack(pop)


/***********************************************************************
 *
 * Variables
 *
 **********************************************************************/
/**
 *@brief List of chunks used by the queue mechanism
 */
cy_ota_mqtt_chunk_for_queue_t *cy_ota_queue_chunks;

/***********************************************************************
 *
 * Functions
 *
 **********************************************************************/

void _print_connect_info( IotMqttConnectInfo_t *connect)
{

    IotLogInfo("IotMqttConnectInfo_t:");
    IotLogInfo("   AWS mode : %d", connect->awsIotMqttMode);
    IotLogInfo("   ID       : %.*s", connect->clientIdentifierLength, connect->pClientIdentifier);
    IotLogInfo("   user     : %.*s", connect->userNameLength, connect->pUserName);
    IotLogInfo("   pass     : %.*s", connect->passwordLength, connect->pPassword);
    IotLogInfo("   clean    : %d", connect->cleanSession);
    IotLogInfo("prev sub    : %p", connect->pPreviousSubscriptions);
    IotLogInfo("prev sub cnt: %d", connect->previousSubscriptionCount);
    IotLogInfo("   WILL     : %p", connect->pWillInfo);
    if (connect->pWillInfo != NULL)
    {
        IotLogInfo("          topic : %.*s", connect->pWillInfo->topicNameLength, connect->pWillInfo->pTopicName);
        IotLogInfo("          qos   : %d", connect->pWillInfo->qos);
        IotLogInfo("          retry : %ld", connect->pWillInfo->retryLimit);
    }
    IotLogInfo(" keep alive : %d", connect->keepAliveSeconds);

}

void _print_network_info(IotMqttNetworkInfo_t *network)
{
    IotLogInfo("IotMqttNetworkInfo_t:");
    IotLogInfo("   create   : %d", network->createNetworkConnection);
    if (network->createNetworkConnection)
    {
        IotLogInfo("     serverInfo : %p", network->u.setup.pNetworkServerInfo);
        if (network->u.setup.pNetworkServerInfo != NULL)
        {
            IotNetworkServerInfo_t pServerInfo = network->u.setup.pNetworkServerInfo;
            (void)pServerInfo;
            IotLogInfo("             server : %s", pServerInfo->pHostName);
            IotLogInfo("             port   : %d", pServerInfo->port);
        }
        IotLogInfo("     credential : %p", network->u.setup.pNetworkCredentialInfo);
        if (network->u.setup.pNetworkCredentialInfo !=  NULL)
        {
            IotNetworkCredentials_t pCredentials = network->u.setup.pNetworkCredentialInfo;
            (void)pCredentials;
            IotLogInfo("         AlpnProtos : %s", pCredentials->pAlpnProtos);
            IotLogInfo("         disableSni : %d", pCredentials->disableSni);
            IotLogInfo("  maxFragmentLength : %d", pCredentials->maxFragmentLength);
            IotLogInfo("            pRootCa : %p", pCredentials->pRootCa);
            IotLogInfo("         RootCaSize : %d", pCredentials->rootCaSize);
            IotLogInfo("        pClientCert : %p", pCredentials->pClientCert);
            IotLogInfo("     clientCertSize : %d", pCredentials->clientCertSize);
            IotLogInfo("        pPrivateKey : %p", pCredentials->pPrivateKey);
            IotLogInfo("     privateKeySize : %d", pCredentials->privateKeySize);
            IotLogInfo("     UserName       : %.*s (%d)", pCredentials->userNameSize, pCredentials->pUserName, pCredentials->userNameSize);
            IotLogInfo("     Password       : %.*s (%d)", pCredentials->passwordSize, pCredentials->pPassword, pCredentials->passwordSize);
        }
    }
    else
    {
        IotLogInfo("     Connection : %p", network->u.pNetworkConnection);
    }
    IotLogInfo("   netiface : %p", network->pNetworkInterface);

}

void cy_ota_mqtt_print_data( const char *buffer, uint32_t length)
{
    uint32_t i, j;
    for (i = 0 ; i < length; i+=16)
    {
        IotLogDebug("0x%04lx ", i);
        for (j = 0 ; j < 16; j++)
        {
            if ((i + j) < length)
            {
                IotLogDebug("0x%02x ", buffer[ i + j]);
            }
            else
            {
                IotLogDebug("     ");
            }
        }
        IotLogDebug("    ");
        for (j = 0 ; j < 16; j++)
        {
            if ((i + j) < length)
            {
                IotLogDebug("%c ", (isprint(buffer[ i + j]) ? buffer[ i + j] : '.'));
            }
            else
            {
                break;
            }
        }
        IotLogDebug("\n");
    }
}

void cy_ota_mqtt_timer_callback(cy_timer_callback_arg_t arg)
{
    cy_ota_context_t *ctx = (cy_ota_context_t *)arg;
    CY_OTA_CONTEXT_ASSERT(ctx);

    IotLogDebug("%s() new event:%d\n", __func__, ctx->u.mqtt.mqtt_timer_event);
    /* yes, we set the ota_event as the mqtt get() function uses the same event var */
    cy_rtos_setbits_event(&ctx->ota_event, ctx->u.mqtt.mqtt_timer_event, 0);
}

cy_rslt_t cy_ota_stop_mqtt_timer(cy_ota_context_t *ctx)
{
    CY_OTA_CONTEXT_ASSERT(ctx);
    return cy_rtos_stop_timer(&ctx->u.mqtt.mqtt_timer);
}
cy_rslt_t cy_ota_start_mqtt_timer(cy_ota_context_t *ctx, uint32_t secs, ota_events_t event)
{
    cy_rslt_t result;
    uint32_t    num_ms = SECS_TO_MILLISECS(secs);

    CY_OTA_CONTEXT_ASSERT(ctx);

    cy_ota_stop_mqtt_timer(ctx);
    ctx->u.mqtt.mqtt_timer_event = event;
    result = cy_rtos_start_timer(&ctx->u.mqtt.mqtt_timer, num_ms);
    return result;
}

/*-----------------------------------------------------------*/
/**
 * @brief Called by _mqttSubscriptionCallback - parse to see if there is a re-direct
 *
 * The re-direct will tell us where to get the OTA Image data using HTTP GET
 *
 * @param[in]   ctx         - The OTA context @ref cy_ota_context_t
 * @param[in]   buffer      - payload from MQTT callback
 * @param[in]   length      - length of the buffer
 *
 * @return      CY_RSLT_SUCCESS - use chunk_info to write to FLASH
 *              CY_RSLT_MODULE_OTA_ERROR
 *              CY_RSLT_MODULE_OTA_BADARG
 *              CY_RSLT_MODULE_OTA_NOT_A_REDIRECT
 */
cy_rslt_t cy_ota_parse_check_for_redirect(cy_ota_context_t *ctx, const uint8_t *buffer, uint32_t length)
{
    CY_OTA_CONTEXT_ASSERT(ctx);

    if ( (buffer == NULL) || (length == 0) )
    {
        return CY_RSLT_MODULE_OTA_BADARG;
    }

    /* need to specify the format of the payload */
    if (0)
    {
        /* we got a good redirect payload */
//        memset(redirect_url, 0x00, sizeof(redirect_url));
//        strncpy(redirect_url, new_url, sizeof(redirect_url) -1 );
//        ctx->new_server.pHostName = &redirect_url;
//        ctx->new_server.port = new_port;
//        ctx->redirect_to_http = true;
//        Create a signal for telling cy_ota_agent main loop to redirect ?
    }

    return CY_RSLT_MODULE_OTA_NOT_A_REDIRECT;
}

/*-----------------------------------------------------------*/
/**
 * @brief Called by _mqttSubscriptionCallback - parse the data chunk header
 *
 * @param[in]   buffer      - payload from MQTT callback
 * @param[in]   length      - length of the buffer
 * @param[out]  chunk_info  - Filled for writing data to FLASH @ref cy_ota_storage_write_info_t
 *
 * @return      CY_RSLT_SUCCESS - use chunk_info to write to FLASH
 *              CY_RSLT_MODULE_OTA_ERROR
 *              CY_RSLT_MODULE_OTA_BADARG
 *              CY_RSLT_MODULE_OTA_NOT_A_HEADER
 */
cy_rslt_t cy_ota_mqtt_parse_chunk(const uint8_t *buffer, uint32_t length, cy_ota_storage_write_info_t *chunk_info)
{
    cy_ota_mqtt_chunk_payload_header_t  *header = (cy_ota_mqtt_chunk_payload_header_t *)buffer;

    if ( (header == NULL) || (length == 0) || (chunk_info == NULL) )
    {
        return CY_RSLT_MODULE_OTA_BADARG;
    }

    /* start with clean slate */
    memset(chunk_info, 0x00, sizeof(cy_ota_storage_write_info_t));

    IotLogDebug ("Magic: %c%c%c%c%c%c%c%c\n", header->magic[0], header->magic[1], header->magic[2], header->magic[3],
                                            header->magic[4], header->magic[5], header->magic[6], header->magic[7]);
    IotLogDebug ("header->offset_to_data     off:%d : %d\n", offsetof(cy_ota_mqtt_chunk_payload_header_t,offset_to_data), header->offset_to_data);
    IotLogDebug ("header->ota_image_type     off:%d : %d\n", offsetof(cy_ota_mqtt_chunk_payload_header_t,ota_image_type), header->ota_image_type);
    IotLogDebug ("header->version                   : %d.%d.%d\n", header->update_version_major, header->update_version_minor, header->update_version_build);
    IotLogDebug ("header->total_size         off:%d : %ld\n", offsetof(cy_ota_mqtt_chunk_payload_header_t,total_size), header->total_size);
    IotLogDebug ("header->image_offset       off:%d : %ld\n", offsetof(cy_ota_mqtt_chunk_payload_header_t,image_offset), header->image_offset);
    IotLogDebug ("header->data_size          off:%d : %d\n", offsetof(cy_ota_mqtt_chunk_payload_header_t,data_size), header->data_size);
    IotLogDebug ("header->total_num_payloads off:%d : %d\n", offsetof(cy_ota_mqtt_chunk_payload_header_t,total_num_payloads), header->total_num_payloads);
    IotLogDebug ("header->this_payload_index off:%d : %d\n", offsetof(cy_ota_mqtt_chunk_payload_header_t,this_payload_index), header->this_payload_index);
    /* debugging */
    {
        cy_thread_t thread;
        cy_rtos_get_thread_handle(&thread);
        IotLogInfo ("Rec'd %d of %d th:0x%lx\n", header->this_payload_index, header->total_num_payloads, thread);
    }

    /* test for magic */
    if (memcmp(header->magic, CY_OTA_MQTT_MAGIC, sizeof(CY_OTA_MQTT_MAGIC) != 0))
    {
        return CY_RSLT_MODULE_OTA_NOT_A_HEADER;
    }
    /* test for some other validity measures */
    if ( (header->offset_to_data > length) ||
         (header->ota_image_type != CY_OTA_MQTT_HEADER_TYPE_ONE_FILE) ||
         (header->data_size > header->total_size) ||
         (header->this_payload_index > header->total_num_payloads) )
    {
        return CY_RSLT_MODULE_OTA_NOT_A_HEADER;
    }

    /* test for version */
    if ( (APP_VERSION_MAJOR > header->update_version_major) ||
         ( (APP_VERSION_MAJOR == header->update_version_major) &&
           ( ( ( (uint32_t)(APP_VERSION_MINOR + 1) ) >    /* fix Coverity 238370 when APP_VERSION_MINOR == 0 */
               ( (uint32_t)(header->update_version_minor + 1) ) ) ) ) ||
         ( (APP_VERSION_MAJOR == header->update_version_major) &&
           (APP_VERSION_MINOR == header->update_version_minor) &&
           (APP_VERSION_BUILD >= header->update_version_build) ) )
    {
        IotLogError("Current Application version %d.%d.%d update %d.%d.%d. Fail.",
                    APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD,
                    header->update_version_major, header->update_version_minor, header->update_version_build);
        return CY_RSLT_MODULE_OTA_INVALID_VERSION;
    }

    chunk_info->total_size      = header->total_size;
    chunk_info->offset          = header->image_offset;
    chunk_info->buffer          = (uint8_t *)&buffer[header->offset_to_data];
    chunk_info->size            = header->data_size;
    chunk_info->packet_number   = header->this_payload_index;
    chunk_info->total_packets   = header->total_num_payloads;

#ifdef OTA_SIGNING_SUPPORT
    /* only mark these fields if there is a signature in this payload */
    if (header->signature_scheme[0] != 0x00)
    {
        memcpy(chunk_info->signature_scheme, header->signature_scheme, sizeof(chunk_info->signature_scheme));
        chunk_info->signature_ptr  = (uint8_t *)&buffer[header->offset_to_data + header->data_size];
    }
#endif

    return CY_RSLT_SUCCESS;
}

/**
 * @brief get a pre-allocated chunk_info buffer
 *
 * Get a pre-allocated Queue chunk structure.
 *
 * Allocate a buffer to hold the payload data as it is not guaranteed
 * after the callback returns.
 *
 * Copy the payload into new buffer.
 *
 * @param[in]   ctx         - pointer to OTA agent context @ref cy_ota_context_t
 * @param[in]   chunk_info  - pointer to chunk information
 *
 * @return  PTR to chunk info
 *          NULL on error
 */
cy_ota_mqtt_chunk_for_queue_t *cy_ota_mqtt_get_queue_chunk(cy_ota_context_t *ctx, cy_ota_storage_write_info_t *chunk_info)
{
    int i;
    cy_ota_mqtt_chunk_for_queue_t *queue_chunk;

    if (chunk_info == NULL)
    {
        IotLogError("%s() Bad arg\n", __func__);
        return NULL;
    }
    if (chunk_info->size > CY_OTA_MQTT_BUFFER_SIZE_MAX)
    {
        IotLogError("%s() chunk copy size %d > %d! Increase CY_OTA_MQTT_BUFFER_SIZE_MAX",
                     __func__, chunk_info->size, CY_OTA_MQTT_BUFFER_SIZE_MAX);
        return NULL;
    }

    CY_OTA_CONTEXT_ASSERT(ctx);

    queue_chunk = NULL;
    while (queue_chunk == NULL)
    {
        cy_rtos_get_mutex(&ctx->recv_mutex, CY_OTA_WAIT_MQTT_MUTEX_MS);

        /* find a free one in our list */
        queue_chunk =  NULL;
        for (i = 0; i < CY_OTA_RECV_QUEUE_LEN; i++)
        {
            if (cy_ota_queue_chunks[i].in_use == 0)
            {
                cy_ota_queue_chunks[i].in_use = 1;
                queue_chunk = &cy_ota_queue_chunks[i];
                break;
            }
        }
        cy_rtos_set_mutex(&ctx->recv_mutex);

        if (queue_chunk != NULL)
        {
            /* copy over chunk info */
            queue_chunk->chunk_info = *chunk_info;

            /* Set chunk_info buffer pointer with the buffer in the queue_chunk
             * We need to check for a NULL pointer before we use the queue_cunk->chunk_info.buffer
             */
            queue_chunk->chunk_info.buffer = &queue_chunk->chunk_buffer[0];

            /* clear the buffer so there are no errors with old data */
            memset(queue_chunk->chunk_info.buffer, 0x00, CY_OTA_MQTT_BUFFER_SIZE_MAX);

            /* copy over the buffer from the chunk_info to the queue_chunk
             * MQTT owns the chunk_info->buffer and we can't transfer ownership
             */
            memcpy(queue_chunk->chunk_info.buffer, chunk_info->buffer, chunk_info->size);

            IotLogDebug("%s() New Queue chunk :%p", __func__, queue_chunk);
            break;
        }
    }

    return queue_chunk;

}

/**
 * @brief free a pre-allocated chunk_info buffer
 *
 */
cy_rslt_t cy_ota_mqtt_free_queue_chunk(cy_ota_context_t *ctx, cy_ota_mqtt_chunk_for_queue_t *queue_chunk)
{
    if (queue_chunk == NULL)
    {
        IotLogError("%s() Bad arg\n", __func__);
        return CY_RSLT_MODULE_OTA_BADARG;
    }

    cy_rtos_get_mutex(&ctx->recv_mutex, CY_OTA_WAIT_MQTT_MUTEX_MS);

    /* clear out the queue_chunk so it can be re-used */
    memset(queue_chunk, 0x00, sizeof(cy_ota_mqtt_chunk_for_queue_t));
    cy_rtos_set_mutex(&ctx->recv_mutex);

    return CY_RSLT_SUCCESS;
}

/**
 * @brief Queue data to be written in cy_ota_mqtt_get() thread
 *
 * @param[in]   ctx         - pointer to OTA agent context @ref cy_ota_context_t
 * @param[in]   chunk_info  - pointer to chunk information
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_MODULE_OTA_ERROR
 */
cy_rslt_t cy_ota_mqtt_queue_data_to_write(cy_ota_context_t *ctx, cy_ota_storage_write_info_t *chunk_info)
{
    cy_ota_mqtt_chunk_for_queue_t *queue_chunk;
    int tries;

    if (chunk_info == NULL)
    {
        IotLogError("%s() Bad arg\n", __func__);
        return CY_RSLT_MODULE_OTA_BADARG;
    }

    CY_OTA_CONTEXT_ASSERT(ctx);

    tries = 10;
    queue_chunk = NULL;
    while (queue_chunk == NULL)
    {
        if (tries-- > 0)
        {
            queue_chunk = cy_ota_mqtt_get_queue_chunk(ctx, chunk_info);
            cy_rtos_delay_milliseconds(10);
        }
        else
        {
            /* something is wrong if we can't get a queue entry in 1 second. */
            IotLogError("%s() failed to queue the data! increase CY_OTA_RECV_QUEUE_LEN!", __func__);
            IotLogError("%s() Or something is holding off the OTA Agent Thread!", __func__);
            return CY_RSLT_MODULE_OTA_OUT_OF_MEMORY_ERROR;
        }
    }

    if (cy_rtos_put_queue(&ctx->recv_data_queue, (void *)&queue_chunk, CY_OTA_RECV_QUEUE_TIMEOUT_MS, 0) == CY_RSLT_SUCCESS)
    {
        size_t num_waiting;
        cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_MQTT_GOT_DATA, 0);
        cy_rtos_count_queue(&ctx->recv_data_queue, &num_waiting);
        IotLogInfo("Queued %d of %d depth: %d of %d",
                     chunk_info->packet_number, chunk_info->total_packets,
                     (uint16_t)num_waiting, CY_OTA_RECV_QUEUE_LEN);
        return CY_RSLT_SUCCESS;
    }

    IotLogError("%s() failed to queue the data! increase CY_OTA_RECV_QUEUE_LEN!", __func__);
    return CY_RSLT_MODULE_OTA_OUT_OF_MEMORY_ERROR;
}

static void disconnectCallback( void * pCallbackContext,
                                 IotMqttCallbackParam_t * pCallbackParam )
{
    if ((pCallbackContext == NULL) || (pCallbackParam == NULL))
    {
        /* bad callback - no info! */
        return;
    }

    if( pCallbackParam->u.disconnectReason == IOT_MQTT_KEEP_ALIVE_TIMEOUT )
    {
        IotLogInfo( "Network disconnected..........\n" );
        if (pCallbackParam->mqttConnection != NULL)
        {
            cy_ota_context_t *ctx = (cy_ota_context_t *)pCallbackContext;
            CY_OTA_CONTEXT_ASSERT(ctx);

            cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_MQTT_DROPPED_US, 0);
        }
    }
}

/**
 * @brief Called by the MQTT library when an incoming PUBLISH message is received.
 *
 * The demo uses this callback to handle incoming PUBLISH messages. This callback
 * prints the contents of an incoming message and publishes an acknowledgement
 * to the MQTT server.
 * @param[in] param1 Counts the total number of received PUBLISH messages. This
 * callback will increment this counter.
 * @param[in] pPublish Information about the incoming PUBLISH message passed by
 * the MQTT library.
 */
static void _mqttSubscriptionCallback( void * param1,
                                       IotMqttCallbackParam_t * const pPublish )
{
    cy_rslt_t       result;
    cy_ota_context_t *ctx = (cy_ota_context_t *)param1;
    cy_ota_storage_write_info_t chunk_info = { 0 };
    IotMqttPublishInfo_t  *info;
    const char * pPayload;

    CY_OTA_CONTEXT_ASSERT(ctx);
    CY_ASSERT(pPublish != NULL);

    cy_rtos_get_mutex(&ctx->sub_callback_mutex, CY_OTA_WAIT_MQTT_MUTEX_MS);

    info = &pPublish->u.message.info;
    pPayload = pPublish->u.message.info.pPayload;

    CY_ASSERT(pPayload != NULL);

    IotLogDebug("\n\n====================================\n");
    IotLogDebug("IotMqttPublishInfo: \n");
    IotLogDebug("               qos: %d\n", info->qos); // IOT_MQTT_QOS_0, IOT_MQTT_QOS_1 (IOT_MQTT_QOS_2 not supported)
    IotLogDebug("            retain: %d\n", info->retain);
    IotLogDebug("             Topic: %.*s\n", info->topicNameLength, info->pTopicName);
    IotLogDebug("           retryMs: %ld\n", info->retryMs);
    IotLogDebug("        retryLimit: %ld\n", info->retryLimit);
    IotLogDebug("    payload length: %d\n", info->payloadLength);
    IotLogDebug("           payload: %p\n", pPayload);

//    cy_ota_mqtt_print_data( pPayload, 32);

    /* see if the message is directing us to get the data through http: */
    result = cy_ota_parse_check_for_redirect(ctx, (const uint8_t *)pPayload, (uint32_t)info->payloadLength);
    if (result == CY_RSLT_SUCCESS)
    {
        /* TODO; Determine how to force a cy_ota_http_disconnect(), cy_ota_http_connect(), cy_ota_http_get() */
        IotLogError("%s() MQTT -> HTTP re-direct functionality not implemented\n", __func__);
//        TODO: cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_MQTT_REDIRECT, 0);
        return;
    }

    result = cy_ota_mqtt_parse_chunk((const uint8_t *)pPayload, (uint32_t)info->payloadLength, &chunk_info);
    if (result == CY_RSLT_MODULE_OTA_NOT_A_HEADER)
    {
        IotLogWarn("%s() Received unknown chunk from MQTT\n", __func__);
        cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_MQTT_DATA_FAIL, 0);
        return;
    }
    else if (result == CY_RSLT_MODULE_OTA_INVALID_VERSION)
    {
        cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_INVALID_VERSION, 0);
        return;
    }
    else if (result != CY_RSLT_SUCCESS)
    {
        IotLogError("%s() Received chunk with bad header from MQTT\n", __func__);
        cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_MQTT_DATA_FAIL, 0);
        return;
    }

    /* update ctx with appropriate sizes */
    if (ctx->total_image_size == 0)
    {
        ctx->total_image_size = chunk_info.total_size;
    }

    /* queue data write */
    if (cy_ota_mqtt_queue_data_to_write(ctx, &chunk_info) != CY_RSLT_SUCCESS)
    {
        IotLogWarn("%s() cy_ota_mqtt_queue_data_to_write() failed !\n", __func__);
    }

    cy_rtos_set_mutex(&ctx->sub_callback_mutex);

}

/*-----------------------------------------------------------*/

/**
 * @brief Establish a new connection to the MQTT server.
 *
 * @param[in] awsIotMqttMode Specify if this demo is running with the AWS IoT
 * MQTT server. Set this to `false` if using another MQTT server.
 * @param[in] pIdentifier NULL-terminated MQTT client identifier.
 * @param[in] pNetworkServerInfo Passed to the MQTT connect function when
 * establishing the MQTT connection.
 * @param[in] pNetworkCredentialInfo Passed to the MQTT connect function when
 * establishing the MQTT connection.
 * @param[in] pNetworkInterface The network interface to use for this demo.
 * @param[out] pMqttConnection Set to the handle to the new MQTT connection.
 *
 * @return `EXIT_SUCCESS` if the connection is successfully established; `EXIT_FAILURE`
 * otherwise.
 */
static int _establishMqttConnection( cy_ota_context_t *ctx,
                                     bool awsIotMqttMode,
                                     char * pIdentifier,
                                     void * pNetworkServerInfo,
                                     void * pNetworkCredentialInfo,
                                     const IotNetworkInterface_t * pNetworkInterface,
                                     IotMqttConnection_t * pMqttConnection )
{
    cy_time_t tval;
    int status = EXIT_SUCCESS;
    IotMqttError_t connectStatus = IOT_MQTT_STATUS_PENDING;
    IotMqttNetworkInfo_t networkInfo = IOT_MQTT_NETWORK_INFO_INITIALIZER;
    IotMqttConnectInfo_t connectInfo = IOT_MQTT_CONNECT_INFO_INITIALIZER;
    IotMqttPublishInfo_t willInfo = IOT_MQTT_PUBLISH_INFO_INITIALIZER;
    char temp_identifier[CLIENT_IDENTIFIER_MAX_LENGTH ] = { 0 };
    char pClientIdentifierBuffer[ CLIENT_IDENTIFIER_MAX_LENGTH ] = { 0 };

    /* Set the members of the network info not set by the initializer. This
     * struct provides information on the transport layer to the MQTT connection. */
    networkInfo.createNetworkConnection = true;
    networkInfo.u.setup.pNetworkServerInfo = pNetworkServerInfo;
    networkInfo.u.setup.pNetworkCredentialInfo = pNetworkCredentialInfo;
    networkInfo.pNetworkInterface = pNetworkInterface;
    networkInfo.disconnectCallback.function = disconnectCallback;
    networkInfo.disconnectCallback.pCallbackContext = (void *)ctx;

    #if ( IOT_MQTT_ENABLE_SERIALIZER_OVERRIDES == 1 ) && defined( IOT_DEMO_MQTT_SERIALIZER )
        networkInfo.pMqttSerializer = IOT_DEMO_MQTT_SERIALIZER;
    #endif

    /* Set the members of the connection info not set by the initializer. */
    connectInfo.awsIotMqttMode = awsIotMqttMode;
    connectInfo.cleanSession = true;
    connectInfo.keepAliveSeconds = MQTT_KEEP_ALIVE_SECONDS;
    connectInfo.pWillInfo = &willInfo;

    /* Set the members of the Last Will and Testament (LWT) message info. The
     * MQTT server will publish the LWT message if this client disconnects
     * unexpectedly. */
    willInfo.pTopicName = WILL_TOPIC_NAME;
    willInfo.topicNameLength = WILL_TOPIC_NAME_LENGTH;
    willInfo.pPayload = WILL_MESSAGE;
    willInfo.payloadLength = WILL_MESSAGE_LENGTH;

    /* Use the parameter client identifier if provided. Otherwise, generate a
     * unique client identifier. */
    cy_rtos_get_time(&tval);
    memset(temp_identifier, 0x00, sizeof(temp_identifier));
    if( ( pIdentifier == NULL ) || ( pIdentifier[ 0 ] == '\0' ) )
    {
        strncpy(temp_identifier, CLIENT_IDENTIFIER_PREFIX, CLIENT_IDENTIFIER_MAX_LENGTH - UINT16_DECIMAL_LENGTH);
    }
    else
    {
        strncpy(temp_identifier, pIdentifier, CLIENT_IDENTIFIER_MAX_LENGTH - UINT16_DECIMAL_LENGTH);
    }

    /* Every active MQTT connection must have a unique client identifier. The demos
     * generate this unique client identifier by appending a timestamp to a common
     * prefix. */
    status = snprintf( pClientIdentifierBuffer,
                       CLIENT_IDENTIFIER_MAX_LENGTH,
                       "%s%d", temp_identifier, (uint16_t)(tval & 0xFFFF) );

    /* Check for errors from snprintf. */
    if( status >=  CLIENT_IDENTIFIER_MAX_LENGTH)
    {
        IotLogError( "Failed to generate unique MQTT client identifier. Using partial");
        status = snprintf( pClientIdentifierBuffer,
                           CLIENT_IDENTIFIER_MAX_LENGTH,
                           "Unique%d", (uint16_t)(tval & 0xFFFF) );
        if( status >=  CLIENT_IDENTIFIER_MAX_LENGTH)
        {
            IotLogError( "Failed to generate unique MQTT client identifier. Fail");
            status = EXIT_FAILURE;
        }
    }
    else
    {
        status = EXIT_SUCCESS;
    }

    /* Set the client identifier buffer and length. */
    connectInfo.pClientIdentifier = pClientIdentifierBuffer;
    connectInfo.clientIdentifierLength = ( uint16_t ) strlen(pClientIdentifierBuffer);
    if ( networkInfo.u.setup.pNetworkCredentialInfo != NULL)
    {
        connectInfo.pUserName = networkInfo.u.setup.pNetworkCredentialInfo->pUserName;
        connectInfo.userNameLength = networkInfo.u.setup.pNetworkCredentialInfo->userNameSize;
        connectInfo.pPassword = networkInfo.u.setup.pNetworkCredentialInfo->pPassword;
        connectInfo.passwordLength = networkInfo.u.setup.pNetworkCredentialInfo->passwordSize;
    }
//    _print_connect_info(&connectInfo);
//    _print_network_info(&networkInfo);

    /* Establish the MQTT connection. */
    if( status == EXIT_SUCCESS )
    {
        IotLogInfo( "MQTT unique client identifier is %.*s (length %hu).\n",
                    connectInfo.clientIdentifierLength,
                    connectInfo.pClientIdentifier,
                    connectInfo.clientIdentifierLength);

        connectStatus = IotMqtt_Connect( &networkInfo,
                                         &connectInfo,
                                         MQTT_TIMEOUT_MS,
                                         pMqttConnection);

        if( connectStatus != IOT_MQTT_SUCCESS )
        {
            IotLogError( "MQTT CONNECT returned error %s.\n",
                         IotMqtt_strerror( connectStatus ) );

            status = EXIT_FAILURE;
        }
    }

    return status;
}

/*-----------------------------------------------------------*/

/**
 * @brief Add or remove subscriptions by either subscribing or unsubscribing.
 *
 * @param[in] mqttConnection The MQTT connection to use for subscriptions.
 * @param[in] operation Either #IOT_MQTT_SUBSCRIBE or #IOT_MQTT_UNSUBSCRIBE.
 * @param[in] pTopicFilters Array of topic filters for subscriptions.
 * @param[in] pCallbackParameter The parameter to pass to the subscription
 * callback.
 *
 * @return `EXIT_SUCCESS` if the subscription operation succeeded; `EXIT_FAILURE`
 * otherwise.
 */
static int _modifySubscriptions( IotMqttConnection_t mqttConnection,
                                 IotMqttOperationType_t operation,
                                 uint8_t numTopicFilters,
                                 const char ** pTopicFilters,
                                 void * pCallbackParameter )
{
    int status = EXIT_SUCCESS;
    int32_t i = 0;
    IotMqttError_t subscriptionStatus = IOT_MQTT_STATUS_PENDING;
    IotMqttSubscription_t pSubscriptions[ CY_OTA_MQTT_MAX_TOPICS ] = { IOT_MQTT_SUBSCRIPTION_INITIALIZER };

    /* Set the members of the subscription list. */
    IotLogDebug("%s() numTopicFilters: %d\n", __func__, numTopicFilters);
    for( i = 0; i < numTopicFilters; i++ )
    {
        IotLogDebug("%s() index: %ld: filter: %s\n", __func__, i, pTopicFilters[ i ]);
        pSubscriptions[ i ].qos = IOT_MQTT_QOS_1;
        pSubscriptions[ i ].pTopicFilter = pTopicFilters[ i ];
        pSubscriptions[ i ].topicFilterLength = strlen(pTopicFilters[ i ]);
        pSubscriptions[ i ].callback.pCallbackContext = pCallbackParameter;
        pSubscriptions[ i ].callback.function = _mqttSubscriptionCallback;
    }

    /* Modify subscriptions by either subscribing or unsubscribing. */
    if( operation == IOT_MQTT_SUBSCRIBE )
    {
        subscriptionStatus = IotMqtt_TimedSubscribe( mqttConnection,
                                                     pSubscriptions,
                                                     numTopicFilters,
                                                     0,
                                                     MQTT_TIMEOUT_MS );

        /* Check the status of SUBSCRIBE. */
        switch( subscriptionStatus )
        {
            case IOT_MQTT_SUCCESS:
                IotLogInfo( "\nAll topic filter subscriptions accepted.......\n");
                status = EXIT_SUCCESS;
                break;

            case IOT_MQTT_SERVER_REFUSED:

                /* Check which subscriptions were rejected before exiting the demo. */
                for( i = 0; i < numTopicFilters; i++ )
                {
                    if( IotMqtt_IsSubscribed( mqttConnection,
                                              pSubscriptions[ i ].pTopicFilter,
                                              pSubscriptions[ i ].topicFilterLength,
                                              NULL ) == true )
                    {
                        IotLogInfo( "Topic filter %.*s was accepted.",
                                    pSubscriptions[ i ].topicFilterLength,
                                    pSubscriptions[ i ].pTopicFilter);
                    }
                    else
                    {
                        IotLogError( "Topic filter %.*s was rejected.",
                                     pSubscriptions[ i ].topicFilterLength,
                                     pSubscriptions[ i ].pTopicFilter);
                    }
                }

                status = EXIT_FAILURE;
                break;

            default:

                status = EXIT_FAILURE;
                break;
        }
    }
    else if( operation == IOT_MQTT_UNSUBSCRIBE )
    {
        subscriptionStatus = IotMqtt_TimedUnsubscribe( mqttConnection,
                                                       pSubscriptions,
                                                       numTopicFilters,
                                                       0,
                                                       MQTT_TIMEOUT_MS );

        /* Check the status of UNSUBSCRIBE. */
        if( subscriptionStatus != IOT_MQTT_SUCCESS )
        {
            status = EXIT_FAILURE;
        }
    }
    else
    {
        /* Only SUBSCRIBE and UNSUBSCRIBE are valid for modifying subscriptions. */
        IotLogError( "MQTT operation %s is not valid for modifying subscriptions.",
                     IotMqtt_OperationType( operation ) );

        status = EXIT_FAILURE;
    }

    return status;
}

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
cy_rslt_t cy_ota_mqtt_validate_network_params(cy_ota_network_params_t *network_params)
{
    IotLogDebug("%s()\n", __func__);
    if(network_params == NULL)
    {
        return CY_RSLT_MODULE_OTA_BADARG;
    }

    if ( (network_params->u.mqtt.pIdentifier == NULL) ||
         (network_params->server.pHostName == NULL) ||
         (network_params->network_interface == NULL) )
    {
        IotLogError("%s() BAD ARGS\n", __func__);
        IotLogError("    Identifier:%s host:%s net:%p \n", network_params->u.mqtt.pIdentifier, network_params->server.pHostName, network_params->network_interface);
        return CY_RSLT_MODULE_OTA_BADARG;
    }

    return CY_RSLT_SUCCESS;
}

/**
 * @brief Connect to OTA Update server
 *
 * NOTE: Individual Network Transport type will do whatever is necessary
 *       This function must have it's own timeout for connection failure
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_MODULE_OTA_ERROR
 */
cy_rslt_t cy_ota_mqtt_connect(cy_ota_context_t *ctx)
{
    int status;
    CY_OTA_CONTEXT_ASSERT(ctx);

    IotLogInfo("%s()\n", __func__);

    /* allocate the memory for the queue so we don't use static RAM */
    if (cy_ota_queue_chunks != NULL)
    {
        IotLogError("%s() Queue Not NULL - possible memory leak?\n", __func__);
        return CY_RSLT_MODULE_OTA_OUT_OF_MEMORY_ERROR;
    }
    cy_ota_queue_chunks = (cy_ota_mqtt_chunk_for_queue_t *)Iot_DefaultMalloc((CY_OTA_RECV_QUEUE_LEN * sizeof(cy_ota_mqtt_chunk_for_queue_t)));
    if (cy_ota_queue_chunks == NULL)
    {
        IotLogDebug("%s() Queue Alloc failed\n", __func__);
        return CY_RSLT_MODULE_OTA_OUT_OF_MEMORY_ERROR;
    }

    /* clear the queue entries  */
    memset(cy_ota_queue_chunks, 0x00, (CY_OTA_RECV_QUEUE_LEN * sizeof(cy_ota_mqtt_chunk_for_queue_t)));

    /* Initialize the Queue */
    if (cy_rtos_init_queue(&ctx->recv_data_queue, CY_OTA_RECV_QUEUE_LEN, sizeof(cy_ota_mqtt_chunk_for_queue_t *)) != CY_RSLT_SUCCESS)
    {
        IotLogDebug("%s() Queue init failed\n", __func__);
        return CY_RSLT_MODULE_OTA_GET_ERROR;
    }


    /* Handle of the MQTT connection used in this demo. */
    ctx->u.mqtt.mqttConnection = IOT_MQTT_CONNECTION_INITIALIZER;

    /* Establish a new MQTT connection. called function has a timeout */
    IotLogInfo("Establishing MQTT Connection for %s to %s port:%d...\n", ctx->network_params.u.mqtt.pIdentifier, ctx->curr_server.pHostName, ctx->curr_server.port);
    status = _establishMqttConnection( ctx,
                                       ctx->network_params.u.mqtt.awsIotMqttMode,
                                       (char *)ctx->network_params.u.mqtt.pIdentifier,
                                       &ctx->curr_server,
                                       ctx->network_params.credentials,
                                       ctx->network_params.network_interface,
                                       &ctx->u.mqtt.mqttConnection );

    if( status == EXIT_SUCCESS )
    {
        /* Mark the MQTT connection as established. */
        ctx->u.mqtt.connectionEstablished = true;
        return CY_RSLT_SUCCESS;
    }

    IotLogWarn("MQTT Connection failed\n");
    return CY_RSLT_MODULE_OTA_MQTT_INIT_ERROR;
}

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
cy_rslt_t cy_ota_mqtt_get(cy_ota_context_t *ctx)
{
    int         status;
    cy_rslt_t   result = CY_RSLT_SUCCESS;
    IotLogDebug("%s()\n", __func__);

    CY_OTA_CONTEXT_ASSERT(ctx);

    if (ctx->u.mqtt.connectionEstablished != true)
    {
        IotLogWarn("%s() connection not established\n", __func__);
        return CY_RSLT_MODULE_OTA_GET_ERROR;
    }

    if (cy_rtos_init_mutex(&ctx->recv_mutex) != CY_RSLT_SUCCESS)
    {
        IotLogWarn("%s() recv_mutex init failed\n", __func__);
        return CY_RSLT_MODULE_OTA_GET_ERROR;
    }

    if (cy_rtos_init_mutex(&ctx->sub_callback_mutex) != CY_RSLT_SUCCESS)
    {
        IotLogWarn("%s() sub_callback_mutex init failed\n", __func__);
        return CY_RSLT_MODULE_OTA_GET_ERROR;
    }

    IotLogInfo("\nMQTT Subscribe for Messages..............\n");
    /* Add the topic filter subscriptions used in this demo. */
    status = _modifySubscriptions( ctx->u.mqtt.mqttConnection,
                                   IOT_MQTT_SUBSCRIBE,
                                   ctx->network_params.u.mqtt.numTopicFilters,
                                   ctx->network_params.u.mqtt.pTopicFilters,
                                   ctx );
    if (status != EXIT_SUCCESS)
    {
        IotLogWarn("%s() _modifySubscriptions(0 failed\n", __func__);
        return CY_RSLT_MODULE_OTA_GET_ERROR;
    }

    /* Create download interval timer */
   result = cy_rtos_init_timer(&ctx->u.mqtt.mqtt_timer, CY_TIMER_TYPE_ONCE,
                               cy_ota_mqtt_timer_callback, (cy_timer_callback_arg_t)ctx);
   if (result != CY_RSLT_SUCCESS)
   {
       /* Event create failed */
       IotLogWarn( "%s() Timer Create Failed!\n", __func__);
       return CY_RSLT_MODULE_OTA_GET_ERROR;
   }

    while (1)
    {
        uint32_t    waitfor;

        /* get event */
        waitfor = OTA_EVENT_MQTT_EVENTS;
        result = cy_rtos_waitbits_event(&ctx->ota_event, &waitfor, 1, 0, CY_OTA_WAIT_MQTT_EVENTS_MS);
        IotLogInfo("%s() MQTT cy_rtos_waitbits_event: 0x%lx type:%d mod:0x%lx code:%d\n", __func__, waitfor, CY_RSLT_GET_TYPE(result), CY_RSLT_GET_MODULE(result), CY_RSLT_GET_CODE(result) );

        /* We only want to act on events we are waiting on.
         * For timeouts, just loop around.
         */
        if (waitfor == 0)
        {
            continue;
        }

        if (waitfor & OTA_EVENT_AGENT_SHUTDOWN_NOW)
        {
            /* Pass along to Agent thread */
            cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_SHUTDOWN_NOW, 0);
            result = CY_RSLT_SUCCESS;
            break;
        }

        if (waitfor & OTA_EVENT_AGENT_DOWNLOAD_TIMEOUT)
        {
//            /* Pass along to Agent thread */
//            cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_DOWNLOAD_TIMEOUT, 0);
            result = CY_RSLT_MODULE_NO_UPDATE_AVAILABLE;
            break;
        }

        if (waitfor & OTA_EVENT_AGENT_STORAGE_ERROR)
        {
            result = CY_RSLT_MODULE_OTA_WRITE_STORAGE_ERROR;
            break;
        }

        if (waitfor & OTA_EVENT_MQTT_REDIRECT)
        {
            result = CY_RSLT_SUCCESS;
            break;
        }

        if (waitfor & OTA_EVENT_MQTT_GOT_DATA)
        {
            cy_ota_mqtt_chunk_for_queue_t *queue_chunk;

            /* got some data - restart the download interval timer */
            IotLogDebug("%s() RESTART PACKET TIMER %ld secs\n", __func__, ctx->packet_timeout_sec);
            cy_ota_start_mqtt_timer(ctx, ctx->packet_timeout_sec, OTA_EVENT_AGENT_PACKET_TIMEOUT);

            queue_chunk = NULL;
            result = cy_rtos_get_queue(&ctx->recv_data_queue, &queue_chunk, CY_OTA_RECV_QUEUE_TIMEOUT_MS, 0);
            IotLogDebug("%d:%s() cy_rtos_get_queue() returned queue_chunk: %p result:0x%lx\n", __LINE__, __func__, queue_chunk, result);

            while ( ( result == CY_RSLT_SUCCESS) && (queue_chunk != NULL) )
            {

#ifdef OTA_SIGNING_SUPPORT
                /* only check if there is a signature in this payload */
                if (&queue_chunk->chunk_info.signature_ptr != NULL)
                {
                    /* check that the signature is correct (If this is a signed chunk) */
                    if (cy_ota_sign_check_chunk(&queue_chunk->chunk_info) != CY_RSLT_SUCCESS)
                    {
                        cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_MQTT_DATA_FAIL, 0);
                        break;
                    }
                }
#endif

                /* store the chunk */
                if (cy_ota_storage_write(ctx, &queue_chunk->chunk_info) != CY_RSLT_SUCCESS)
                {
                    cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_MQTT_DATA_FAIL, 0);
                    break;
                }

                /* test for in-sequence chunks */
                if ( (queue_chunk->chunk_info.packet_number > 0) &&
                     (queue_chunk->chunk_info.packet_number != (ctx->last_packet_received + 1)) )
                {
                    IotLogWarn("OUT OF ORDER last:%d current:%d\n",
                                ctx->last_packet_received, queue_chunk->chunk_info.packet_number);
                }

                /* update the stats */
                ctx->total_bytes_written   += queue_chunk->chunk_info.size;
                ctx->last_offset            = queue_chunk->chunk_info.offset;
                ctx->last_size              = queue_chunk->chunk_info.size;
                ctx->last_packet_received   = queue_chunk->chunk_info.packet_number;
                ctx->total_packets          = queue_chunk->chunk_info.total_packets;

                /* free the queue chunk  */
                cy_ota_mqtt_free_queue_chunk(ctx, queue_chunk);
                queue_chunk = NULL;

                if (ctx->total_image_size > 0)
                {
                    uint32_t percent = (ctx->total_bytes_written * 100) / ctx->total_image_size;
                    cy_ota_internal_call_cb(ctx, CY_OTA_REASON_DOWNLOAD_PERCENT, percent);
                }

                IotLogDebug("Written packet %d of %d to offset:%ld of %ld\n",
                            ctx->last_packet_received, ctx->total_packets, ctx->last_offset, ctx->total_image_size);
                if (ctx->total_bytes_written >= ctx->total_image_size)
                {
                    IotLogInfo("Done writing all packets! %ld of %ld\n", (ctx->last_packet_received +1), ctx->total_packets);
                    cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_MQTT_DATA_DONE, 0);
                    /* stop timer asap */
                    cy_ota_stop_mqtt_timer(ctx);
                }

                result = cy_rtos_get_queue(&ctx->recv_data_queue, &queue_chunk, CY_OTA_RECV_QUEUE_TIMEOUT_MS, 0);
                IotLogDebug("%d:%s() cy_rtos_get_queue() returned queue_chunk: %p result:0x%lx\n", __LINE__, __func__, queue_chunk, result);
            }
        }

        if (waitfor & OTA_EVENT_AGENT_PACKET_TIMEOUT)
        {
            IotLogWarn("Timeout waiting for a packet - fail\n");
            cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_MQTT_DATA_FAIL, 0);
        }

        if (waitfor & OTA_EVENT_MQTT_DATA_DONE)
        {
            result = CY_RSLT_SUCCESS;
            break;
        }

        if (waitfor & OTA_EVENT_INVALID_VERSION)
        {
            result = CY_RSLT_MODULE_OTA_INVALID_VERSION;
            break;
        }

        if (waitfor & OTA_EVENT_MQTT_DATA_FAIL)
        {
            result = CY_RSLT_MODULE_OTA_GET_ERROR;
            break;
        }

        if (waitfor & OTA_EVENT_MQTT_DROPPED_US)
        {
            result = CY_RSLT_MODULE_OTA_MQTT_DROPPED_CNCT;
            break;
        }
    }   /* While 1 */

    /* we completed the download, stop the timer */
    cy_ota_stop_mqtt_timer(ctx);

    /* Add the topic filter subscriptions used in this demo. */
    status = _modifySubscriptions( ctx->u.mqtt.mqttConnection,
                                   IOT_MQTT_UNSUBSCRIBE,
                                   ctx->network_params.u.mqtt.numTopicFilters,
                                   ctx->network_params.u.mqtt.pTopicFilters,
                                   ctx );

    if (status != EXIT_SUCCESS)
    {
        IotLogDebug("%s() _modifySubscriptions(UNSUBSCRIBE) failed\n", __func__);
    }

    cy_rtos_deinit_timer(&ctx->u.mqtt.mqtt_timer);

    cy_rtos_deinit_mutex(&ctx->sub_callback_mutex);
    cy_rtos_deinit_mutex(&ctx->recv_mutex);

    IotLogDebug("%s() MQTT DONE result: 0x%lx\n", __func__, result);
    return result;
}

/**
 * @brief Disconnect from MQTT broker
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_MODULE_OTA_ERROR
 */
cy_rslt_t cy_ota_mqtt_disconnect(cy_ota_context_t *ctx)
{
    IotLogInfo("%s()\n", __func__);
    CY_OTA_CONTEXT_ASSERT(ctx);

    /* De init the queue and free the RAM   */
    cy_rtos_deinit_queue(&ctx->recv_data_queue);

    if (cy_ota_queue_chunks != NULL)
    {
        Iot_DefaultFree(cy_ota_queue_chunks);
    }
    cy_ota_queue_chunks = NULL;

    if (ctx->u.mqtt.connectionEstablished)
    {
        IotMqtt_Disconnect( ctx->u.mqtt.mqttConnection, 0 );
        ctx->u.mqtt.connectionEstablished = false;
    }
    return CY_RSLT_SUCCESS;
}
