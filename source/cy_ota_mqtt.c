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

#include "cy_json_parser.h"

#define IOT_PUBLISH_RETRY_LIMIT                        (10)
#define IOT_PUBLISH_RETRY_MS                           (1000)
#define IOT_TOPIC_FILTER_COUNT                         (1)

/***********************************************************************
 *
 * defines & enums
 *
 **********************************************************************/
/* For debugging */
//#define PRINT_CONNECT_INFO       1
//#define PRINT_NETWORK_INFO      1
//#define PRINT_MQTT_DATA         1
//#define DEBUG_PACKET_RECEIPT_TIME_DIFF   1

/**
 * @brief The longest client identifier that an MQTT server must accept (as defined
 * by the MQTT 3.1.1 spec) is 23 characters. Add 1 to include the length of the NULL
 * terminator.
 */
#define CLIENT_IDENTIFIER_MAX_LENGTH                (24)

/**
 * @brief Number of decimal digits for a 16 bit number
 * Used to size the Client ID string to be able to add a unique number to it.
 */
#define UINT16_DECIMAL_LENGTH                       (5)

/**
 * @brief The Last Will and Testament topic name in this demo.
 *
 * The MQTT server will publish a message to this topic name if this client is
 * unexpectedly disconnected.
 */
#define WILL_TOPIC_NAME                             CY_OTA_MQTT_TOPIC_PREFIX "/will"

/**
 * @brief The length of #WILL_TOPIC_NAME.
 */
#define WILL_TOPIC_NAME_LENGTH                      ( (uint16_t)(sizeof(WILL_TOPIC_NAME) - 1) )

/**
 * @brief The message to publish to #WILL_TOPIC_NAME.
 */
#define WILL_MESSAGE                                "MQTT demo unexpectedly disconnected."

/**
 * @brief The length of #WILL_MESSAGE.
 */
#define WILL_MESSAGE_LENGTH                         ( ( size_t ) (sizeof(WILL_MESSAGE) - 1) )

/**
 * @brief The maximum number of times each PUBLISH in this demo will be retried.
 */
#define PUBLISH_RETRY_LIMIT                         (10)

/**
 * @brief A PUBLISH message is retried if no response is received within this
 * time.
 */
#define PUBLISH_RETRY_MS                            (1000)

/**
 * @brief The topic name on which acknowledgement messages for incoming publishes
 * should be published.
 */
#define ACKNOWLEDGEMENT_TOPIC_NAME                  CY_OTA_MQTT_TOPIC_PREFIX "/acknowledge"

/**
 * @brief The length of #ACKNOWLEDGEMENT_TOPIC_NAME.
 */
#define ACKNOWLEDGEMENT_TOPIC_NAME_LENGTH           ( ( uint16_t ) (sizeof(ACKNOWLEDGEMENT_TOPIC_NAME) - 1) )

/**
 * @brief Format string of PUBLISH acknowledgement messages in this demo.
 */
#define ACKNOWLEDGEMENT_MESSAGE_FORMAT              "Client has received PUBLISH %.*s from server."

/**
 * @brief Size of the buffers that hold acknowledgement messages in this demo.
 */
#define ACKNOWLEDGEMENT_MESSAGE_BUFFER_LENGTH       (sizeof(ACKNOWLEDGEMENT_MESSAGE_FORMAT) + 2)

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

/***********************************************************************
 *
 * Function declarations
 *
 **********************************************************************/
static cy_rslt_t  cy_ota_modify_subscriptions(IotMqttConnection_t mqttConnection,
                                              IotMqttOperationType_t operation,
                                              uint8_t numTopicFilters,
                                              const char ** pTopicFilters,
                                              void * pCallbackParameter);

/***********************************************************************
 *
 * Functions
 *
 **********************************************************************/

#ifdef PRINT_CONNECT_INFO
void _print_connect_info(IotMqttConnectInfo_t *connect)
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
#endif

#ifdef PRINT_NETWORK_INFO
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
    IotLogInfo("   netiface : %p", network->u.pNetworkConnection);

}
#endif

void cy_ota_mqtt_timer_callback(cy_timer_callback_arg_t arg)
{
    cy_ota_context_t *ctx = (cy_ota_context_t *)arg;
    CY_OTA_CONTEXT_ASSERT(ctx);

    IotLogDebug("%s() new event:%d\n", __func__, ctx->mqtt.mqtt_timer_event);
    /* yes, we set the ota_event as the mqtt get() function uses the same event var */
    cy_rtos_setbits_event(&ctx->ota_event, ctx->mqtt.mqtt_timer_event, 0);
}

cy_rslt_t cy_ota_stop_mqtt_timer(cy_ota_context_t *ctx)
{
    CY_OTA_CONTEXT_ASSERT(ctx);
    return cy_rtos_stop_timer(&ctx->mqtt.mqtt_timer);
}
cy_rslt_t cy_ota_start_mqtt_timer(cy_ota_context_t *ctx, uint32_t secs, ota_events_t event)
{
    cy_rslt_t result;
    uint32_t    num_ms = SECS_TO_MILLISECS(secs);

    CY_OTA_CONTEXT_ASSERT(ctx);

    cy_ota_stop_mqtt_timer(ctx);
    ctx->mqtt.mqtt_timer_event = event;
    result = cy_rtos_start_timer(&ctx->mqtt.mqtt_timer, num_ms);
    return result;
}

cy_rslt_t cy_ota_mqtt_publish_request(cy_ota_context_t *ctx, char *mqtt_topic, char *mqtt_message)
{

    /*Publish for the topic */
    IotMqttPublishInfo_t publishInfo = IOT_MQTT_PUBLISH_INFO_INITIALIZER;
    IotMqttError_t publishStatus = IOT_MQTT_STATUS_PENDING;
    publishInfo.pPayload = NULL;

    CY_OTA_CONTEXT_ASSERT(ctx);

    /* Set the common members of the publish info. */
    publishInfo.qos = IOT_MQTT_QOS_1;
    publishInfo.pTopicName = mqtt_topic;
    publishInfo.topicNameLength = strlen(publishInfo.pTopicName);
    publishInfo.retryMs = IOT_PUBLISH_RETRY_MS;
    publishInfo.retryLimit = IOT_PUBLISH_RETRY_LIMIT;

    publishInfo.pPayload = mqtt_message;
    publishInfo.payloadLength = strlen(publishInfo.pPayload);

    /* Publish the message. */
    IotLogDebug("Publish to %.*s:\n>%.*s< ", publishInfo.topicNameLength, publishInfo.pTopicName, publishInfo.payloadLength, publishInfo.pPayload);
    publishStatus = IotMqtt_PublishSync(ctx->mqtt.mqtt_connection,
                                        &publishInfo,
                                        0,
                                        0);

    if (publishStatus == IOT_MQTT_SUCCESS || publishStatus == IOT_MQTT_STATUS_PENDING )
    {
        IotLogWarn("Timed out waiting for PUBLISH to complete.");
        return CY_RSLT_OTA_ERROR_MQTT_PUBLISH;
    }
    else
    {
         IotLogDebug("PUBLISH completed successfully.");
    }

    return CY_RSLT_SUCCESS;
}

void cy_ota_mqtt_create_unique_topic(cy_ota_context_t *ctx)
{
    cy_time_t                   tval;

    CY_OTA_CONTEXT_ASSERT(ctx);

    /* create unique topic name */
    cy_rtos_get_time(&tval);
    if (strlen(ctx->mqtt.unique_topic) == 0)
    {
        memset(ctx->mqtt.unique_topic, 0x00, sizeof(ctx->mqtt.unique_topic) );
        sprintf(ctx->mqtt.unique_topic, "%s/%s/%s/%d",
                COMPANY_TOPIC_PREPEND, CY_TARGET_BOARD_STRING, CY_OTA_MQTT_MAGIC, (uint16_t)(tval & 0x0000FFFF) );
    }
}

cy_rslt_t cy_ota_subscribe_and_publish_unique_topic(cy_ota_context_t *ctx, const char *message_doc)
{
    char                        *unique_topics[1];
    cy_rslt_t                   result = CY_RSLT_SUCCESS;
    cy_ota_callback_results_t   cb_result;

    CY_OTA_CONTEXT_ASSERT(ctx);

    /* create unique topic name if not done yet*/
    cy_ota_mqtt_create_unique_topic(ctx);
    memset(ctx->mqtt.json_doc, 0x00, sizeof(ctx->mqtt.json_doc) );
    sprintf(ctx->mqtt.json_doc, message_doc,
            APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD, ctx->mqtt.unique_topic);

    /* callback to app */
    cb_result = cy_ota_internal_call_cb(ctx, CY_OTA_REASON_STATE_CHANGE, ctx->curr_state);


    IotLogDebug("%s() Topic: %s", __func__, ctx->mqtt.unique_topic);
    IotLogDebug("%s() Messg: %s", __func__, ctx->mqtt.json_doc);

    switch( cb_result )
    {

    case CY_OTA_CB_RSLT_OTA_CONTINUE:
        unique_topics[0] = ctx->mqtt.unique_topic;
        /* Add the topic filter subscriptions */
        IotLogInfo("MQTT Subscribe to UNIQUE TOPIC '%s'\n",  ctx->mqtt.unique_topic);
        result = cy_ota_modify_subscriptions(ctx->mqtt.mqtt_connection,
                                             IOT_MQTT_SUBSCRIBE,
                                             1,
                                             (const char **)&unique_topics[0],
                                             ctx);
        break;
    case CY_OTA_CB_RSLT_OTA_STOP:
        IotLogError("%s() App returned OTA Stop for STATE_CHANGE for JOB_DOWNLOAD", __func__);
        return CY_RSLT_OTA_ERROR_APP_RETURNED_STOP;
    case CY_OTA_CB_RSLT_APP_SUCCESS:      /* TODO are we assuming app subscribed here ? */
        IotLogInfo("%s() App returned APP_SUCCESS for STATE_CHANGE for JOB_DOWNLOAD", __func__);
        return CY_RSLT_SUCCESS;
    case CY_OTA_CB_RSLT_APP_FAILED:
        IotLogError("%s() App returned APP_FAILED for STATE_CHANGE for JOB_DOWNLOAD", __func__);
        return CY_RSLT_OTA_ERROR_MQTT_PUBLISH;
    case CY_OTA_CB_NUM_RESULTS:
        return CY_RSLT_OTA_ERROR_MQTT_PUBLISH;
    }

    if (result != CY_RSLT_SUCCESS)
    {
        IotLogWarn("%s() cy_ota_modify_subscriptions() failed\n", __func__);
    }

    if (result == CY_RSLT_SUCCESS)
    {
        IotLogInfo("\nPublish on %s : %s\n", SUBSCRIBER_PUBLISH_TOPIC, ctx->mqtt.json_doc);
        result = cy_ota_mqtt_publish_request(ctx, SUBSCRIBER_PUBLISH_TOPIC, ctx->mqtt.json_doc);
        if (result != CY_RSLT_SUCCESS)
        {
            IotLogWarn("%s() cy_ota_mqtt_publish_request() failed\n", __func__);
            result = CY_RSLT_OTA_ERROR_MQTT_PUBLISH;
        }
    }
    return result;
}

/*-----------------------------------------------------------*/
/**
 * @brief Called by MQTT callback - parse the data chunk header
 *
 * @param[in]   buffer      - payload from MQTT callback
 * @param[in]   length      - length of the buffer
 * @param[out]  chunk_info  - Filled for writing data to FLASH @ref cy_ota_storage_write_info_t
 *
 * @return      CY_RSLT_SUCCESS - use chunk_info to write to FLASH
 *              CY_RSLT_OTA_ERROR_GENERAL
 *              CY_RSLT_OTA_ERROR_BADARG
 *              CY_RSLT_OTA_ERROR_NOT_A_HEADER
 */
cy_rslt_t cy_ota_mqtt_parse_chunk(const uint8_t *buffer, uint32_t length, cy_ota_storage_write_info_t *chunk_info)
{
    cy_ota_mqtt_chunk_payload_header_t  *header = (cy_ota_mqtt_chunk_payload_header_t *)buffer;

    if ( (header == NULL) || (length == 0) || (chunk_info == NULL) )
    {
        return CY_RSLT_OTA_ERROR_BADARG;
    }

    /* start with clean slate */
    memset(chunk_info, 0x00, sizeof(cy_ota_storage_write_info_t) );

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

    /* test for magic */
    if (memcmp(header->magic, CY_OTA_MQTT_MAGIC, sizeof(CY_OTA_MQTT_MAGIC) != 0) )
    {
        return CY_RSLT_OTA_ERROR_NOT_A_HEADER;
    }
    /* test for some other validity measures */
    if ( (header->offset_to_data > length) ||
         (header->ota_image_type != CY_OTA_MQTT_HEADER_TYPE_ONE_FILE) ||
         (header->data_size > header->total_size) ||
         (header->this_payload_index > header->total_num_payloads) )
    {
        return CY_RSLT_OTA_ERROR_NOT_A_HEADER;
    }

    /* validate version is higher than current application */
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
        return CY_RSLT_OTA_ERROR_INVALID_VERSION;
    }

    chunk_info->total_size      = header->total_size;
    chunk_info->offset          = header->image_offset;
    chunk_info->buffer          = (uint8_t *)&buffer[header->offset_to_data];
    chunk_info->size            = header->data_size;
    chunk_info->packet_number   = header->this_payload_index;
    chunk_info->total_packets   = header->total_num_payloads;

    return CY_RSLT_SUCCESS;
}

/**
 * @brief Write a chunk of OTA data to FLASH
 *
 * @param[in]   ctx         - ptr to OTA context
 * @param[in]   chunk_info  - ptr to a chunk_info structure
 *
 * @return      CY_RSLT_SUCCESS
 *              CY_RSLT_OTA_ERROR_BADARG
 *              CY_RSLT_OTA_ERROR_WRITE_STORAGE
 */
cy_rslt_t cy_ota_mqtt_write_chunk_to_flash(cy_ota_context_t *ctx, cy_ota_storage_write_info_t *chunk_info)
{
    cy_rslt_t                 result;
    cy_ota_callback_results_t cb_result;

    IotLogDebug("%s()\n", __func__);

    if ( (ctx == NULL) || (chunk_info == NULL) )
    {
        IotLogError("%s() Bad args\n", __func__);
        return CY_RSLT_OTA_ERROR_BADARG;
    }

    ctx->num_packets_received++;    /* this is so we don't have a false failure with the per packet timer */

    /* check for receipt of duplicate packets - do not write twice */
    if (chunk_info->packet_number >= CY_OTA_MAX_PACKETS)
    {
        IotLogError("MQTT PACKET index %d too large. increase  CY_OTA_MAX_PACKETS (%d)\n", chunk_info->packet_number, CY_OTA_MAX_PACKETS);
    }
    else
    {
        ctx->mqtt.received_packets[chunk_info->packet_number]++;
        if (ctx->mqtt.received_packets[chunk_info->packet_number] > 1)
        {
            IotLogDebug("DEBUG PACKET index %d Duplicate - not written\n", chunk_info->packet_number, CY_OTA_MAX_PACKETS);
            return CY_RSLT_SUCCESS;
        }
    }

    /* store the chunk */
    ctx->storage = chunk_info;
    cb_result = cy_ota_internal_call_cb(ctx, CY_OTA_REASON_STATE_CHANGE, CY_OTA_STATE_STORAGE_WRITE);
    switch( cb_result )
    {
    case CY_OTA_CB_RSLT_OTA_CONTINUE:
        result = cy_ota_storage_write(ctx, chunk_info);
        if (result != CY_RSLT_SUCCESS)
        {
            IotLogError("%s() Write failed\n", __func__);
            cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_DATA_FAIL, 0);
            return result;
        }
        break;
    case CY_OTA_CB_RSLT_OTA_STOP:
        IotLogError("%s() App returned OTA Stop for STATE_CHANGE for STORAGE_WRITE", __func__);
        return CY_RSLT_OTA_ERROR_APP_RETURNED_STOP;
    case CY_OTA_CB_RSLT_APP_SUCCESS:
        IotLogInfo("%s() App returned APP_SUCCESS for STATE_CHANGE for STORAGE_WRITE", __func__);
        break;
    case CY_OTA_CB_RSLT_APP_FAILED:
        IotLogError("%s() App returned APP_FAILED for STATE_CHANGE for STORAGE_WRITE", __func__);
        return CY_RSLT_OTA_ERROR_WRITE_STORAGE;
    case CY_OTA_CB_NUM_RESULTS:
        return CY_RSLT_OTA_ERROR_WRITE_STORAGE;
    }

    /* Test for out-of-order chunks
     * Out of order will be a problem when using
     * TAR archives for multi-file downloads.
     * Tar archives must use HTTP for Connection.
     */
    if ( (chunk_info->packet_number > 0) &&
         (chunk_info->packet_number != (ctx->last_packet_received + 1) ) )
    {
        IotLogDebug("OUT OF ORDER last:%d current:%d\n",
                    ctx->last_packet_received, chunk_info->packet_number);
    }

    /* update the stats */
    ctx->total_bytes_written   += chunk_info->size;
    ctx->last_offset            = chunk_info->offset;
    ctx->last_size              = chunk_info->size;
    ctx->last_packet_received   = chunk_info->packet_number;
    ctx->total_packets          = chunk_info->total_packets;

    IotLogDebug("Written packet %d of %d to offset:%ld  %ld of %ld\n",
                ctx->last_packet_received, ctx->total_packets,
                ctx->last_offset, ctx->total_bytes_written, ctx->total_image_size);

    cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_GOT_DATA, 0);

    return CY_RSLT_SUCCESS;
}

static void disconnectCallback(void * pCallbackContext,
                               IotMqttCallbackParam_t * pCallbackParam )
{
    if ( (pCallbackContext == NULL) || (pCallbackParam == NULL) )
    {
        /* bad callback - no info! */
        return;
    }

    IotLogDebug("Network disconnected..........reason: %d\n", pCallbackParam->u.disconnectReason);
    if ( ( (pCallbackParam->u.disconnectReason == IOT_MQTT_DISCONNECT_CALLED) ||
           (pCallbackParam->u.disconnectReason ==  IOT_MQTT_BAD_PACKET_RECEIVED) ||
           (pCallbackParam->u.disconnectReason == IOT_MQTT_KEEP_ALIVE_TIMEOUT) ) &&
         (pCallbackParam->mqttConnection != NULL) )
    {
        cy_ota_context_t *ctx = (cy_ota_context_t *)pCallbackContext;
        CY_OTA_CONTEXT_ASSERT(ctx);

        /* Only report disconnect if we are downloading */
        if ( (ctx->curr_state == CY_OTA_STATE_JOB_DOWNLOAD) ||
             (ctx->curr_state == CY_OTA_STATE_DATA_DOWNLOAD) ||
             (ctx->curr_state == CY_OTA_STATE_RESULT_SEND) ||
             (ctx->curr_state == CY_OTA_STATE_RESULT_RESPONSE) )
        {
            IotLogWarn("%s() CY_OTA_EVENT_DROPPED_US Network MQTT disconnect reason:%d state:%d %s!",
                    __func__,  pCallbackParam->u.disconnectReason,
                    ctx->curr_state, cy_ota_get_state_string(ctx->curr_state));

            cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_DROPPED_US, 0);
        }
    }
}

/**
 * @brief Called by the MQTT library when an incoming PUBLISH message is received.
 *
 * The demo uses this callback to handle incoming PUBLISH messages. This callback
 * prints the contents of an incoming message and publishes an acknowledgment
 * to the MQTT server.
 * @param[in] param1 Counts the total number of received PUBLISH messages. This
 * callback will increment this counter.
 * @param[in] pPublish Information about the incoming PUBLISH message passed by
 * the MQTT library.
 */
static void cy_ota_MQTT_subscription_callback(void * param1,
                                              IotMqttCallbackParam_t * const pPublish )
{
    cy_rslt_t       result = CY_RSLT_SUCCESS;
    cy_ota_context_t *ctx = (cy_ota_context_t *)param1;
    cy_ota_storage_write_info_t chunk_info = { 0 };
    IotMqttPublishInfo_t  *info;
    const char * pPayload;

    CY_OTA_CONTEXT_ASSERT(ctx);
    CY_ASSERT(pPublish != NULL);


     /* Make sure the callback we get is when we are expecting it
     * Do this before grabbing the Mutex !
     */
    if (ctx->curr_state == CY_OTA_STATE_JOB_DOWNLOAD)
    {
        IotLogDebug("%() Received Job packet.\n", __func__);
    }
    else if (ctx->curr_state == CY_OTA_STATE_RESULT_SEND)
    {
        IotLogDebug("%() Received Result response.\n", __func__);
    }
    else if ( (ctx->curr_state != CY_OTA_STATE_DATA_DOWNLOAD) || (ctx->sub_callback_mutex_inited != 1) )
    {
        cy_ota_mqtt_chunk_payload_header_t *header;
        info = &pPublish->u.message.info;
        pPayload = pPublish->u.message.info.pPayload;
        CY_ASSERT(pPayload != NULL);

        header = (cy_ota_mqtt_chunk_payload_header_t *)pPayload;
        if (memcmp(header->magic, CY_OTA_MQTT_MAGIC, sizeof(CY_OTA_MQTT_MAGIC) != 0) )
        {
            IotLogWarn("%s() Received packet outside of downloading on topic %.*s.\n", __func__
                    , pPublish->u.message.info.topicNameLength, pPublish->u.message.info.pTopicName);
        }

        /* return before we get the mutex */
        return;
    }

    result = cy_rtos_get_mutex(&ctx->sub_callback_mutex, CY_OTA_WAIT_MQTT_MUTEX_MS);
    if (result != CY_RSLT_SUCCESS)
    {
        /* we didn't get the mutex - something is wrong! */
        IotLogError("%() Mutex timeout!\n", __func__);
        return;
    }

#ifdef DEBUG_PACKET_RECEIPT_TIME_DIFF
    {
        static cy_time_t last_packet_time, curr_packet_time;
        cy_rtos_get_time(&curr_packet_time);
        if (last_packet_time != 0)
        {
            cy_time_t diff = curr_packet_time - last_packet_time;
            IotLogInfo("Time difference from last packet rec'd: %ld", diff);
        }
        last_packet_time = curr_packet_time;
    }
#endif

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

#ifdef PRINT_MQTT_DATA
    cy_ota_print_data(pPayload, 32);
#endif

    if (ctx->curr_state == CY_OTA_STATE_JOB_DOWNLOAD)
    {
        if ( (pPayload == NULL) || (info->payloadLength == 0) )
        {
            IotLogWarn("Payload from Publisher Error!");
            result = CY_RSLT_OTA_ERROR_BADARG;
            goto _callback_exit;
        }

        /* Copy the Job document. We send it back to the Broker when we send the unique topic name
         * We parse the job document in cy_ota_agent.c
         */
        if (info->payloadLength > sizeof(ctx->job_doc) )
        {
            IotLogWarn("MQTT: Job doc too long! %d bytes! Change CY_OTA_JOB_MAX_LEN!", info->payloadLength);
            result = CY_RSLT_OTA_ERROR_MALFORMED_JOB_DOC;
            goto _callback_exit;
        }
        memset(ctx->job_doc, 0x00, sizeof(ctx->job_doc) );
        memcpy(ctx->job_doc, pPayload, info->payloadLength);
        result = CY_RSLT_SUCCESS;
    }
    if (ctx->curr_state == CY_OTA_STATE_RESULT_SEND)
    {
        /* TODO: STDE - Do we need to do anything? maybe just acknowledge for OTA Agent? */
        result = CY_RSLT_SUCCESS;
    }
    else if (ctx->curr_state == CY_OTA_STATE_DATA_DOWNLOAD)
    {
        result = cy_ota_mqtt_parse_chunk( (const uint8_t *)pPayload, (uint32_t)info->payloadLength, &chunk_info);

        if (result == CY_RSLT_SUCCESS)
        {
            IotLogDebug("Received packet %d of %d", chunk_info.packet_number, chunk_info.total_packets);

            /* update ctx with appropriate sizes */
            if (ctx->total_image_size == 0)
            {
                ctx->total_image_size = chunk_info.total_size;
            }

            /* write the data */
            result = cy_ota_mqtt_write_chunk_to_flash(ctx, &chunk_info);
            /* Errors handled below */
        }
        else
        {
            IotLogDebug("Packet %d had errors in header", chunk_info.packet_number);
        }
    }

_callback_exit:

    /* Handle Job and Data error conditions */
    if (result == CY_RSLT_OTA_ERROR_MALFORMED_JOB_DOC)
    {
        IotLogWarn(" CY_RSLT_OTA_ERROR_MALFORMED_JOB_DOC !");
        cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_MALFORMED_JOB_DOC, 0);
    }
    else if (result == CY_RSLT_OTA_ERROR_WRITE_STORAGE)
    {
        IotLogWarn(" CY_OTA_EVENT_STORAGE_ERROR !");
        cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_STORAGE_ERROR, 0);
    }
    else if(result == CY_RSLT_OTA_ERROR_APP_RETURNED_STOP)
    {
        IotLogWarn(" CY_OTA_EVENT_APP_STOPPED_OTA !");
        cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_APP_STOPPED_OTA, 0);
    }
    else if (result == CY_RSLT_OTA_ERROR_INVALID_VERSION)
    {
        IotLogWarn(" CY_OTA_EVENT_INVALID_VERSION !");
        cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_INVALID_VERSION, 0);
    }
    else if (result != CY_RSLT_SUCCESS)
    {
        IotLogWarn(" CY_OTA_EVENT_DATA_FAIL !");
        cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_DATA_FAIL, 0);
    }
    else
    {
        IotLogDebug(" CY_OTA_EVENT_GOT_DATA!");
        cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_GOT_DATA, 0);
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
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_OTA_ERROR_GET_JOB
 *
 */
static cy_rslt_t cy_ota_establish_MQTT_connection(cy_ota_context_t *ctx,
                                                 bool awsIotMqttMode,
                                                 char * pIdentifier,
                                                 void * pNetworkServerInfo,
                                                 void * pNetworkCredentialInfo,
                                                 const IotNetworkInterface_t * pNetworkInterface,
                                                 IotMqttConnection_t * pMqttConnection )
{
    cy_time_t tval;
    int       string_len;
    IotMqttError_t connectStatus = IOT_MQTT_STATUS_PENDING;
    IotMqttNetworkInfo_t networkInfo = IOT_MQTT_NETWORK_INFO_INITIALIZER;
    IotMqttConnectInfo_t connectInfo = IOT_MQTT_CONNECT_INFO_INITIALIZER;
    IotMqttPublishInfo_t willInfo = IOT_MQTT_PUBLISH_INFO_INITIALIZER;
    char temp_identifier[CLIENT_IDENTIFIER_MAX_LENGTH ] = { 0 };
    char pClientIdentifierBuffer[ CLIENT_IDENTIFIER_MAX_LENGTH ] = { 0 };

    CY_OTA_CONTEXT_ASSERT(ctx);

    /* Set the members of the network info not set by the initializer. This
     * struct provides information on the Connection layer to the MQTT connection. */
    networkInfo.createNetworkConnection = true;
    networkInfo.u.setup.pNetworkServerInfo = pNetworkServerInfo;
    networkInfo.u.setup.pNetworkCredentialInfo = pNetworkCredentialInfo;
    networkInfo.pNetworkInterface = pNetworkInterface;
    networkInfo.disconnectCallback.function = disconnectCallback;
    networkInfo.disconnectCallback.pCallbackContext = (void *)ctx;

    #if (IOT_MQTT_ENABLE_SERIALIZER_OVERRIDES == 1) && defined(IOT_DEMO_MQTT_SERIALIZER)
        networkInfo.pMqttSerializer = IOT_DEMO_MQTT_SERIALIZER;
    #endif

    /* Set the members of the connection info not set by the initializer. */
    connectInfo.awsIotMqttMode = awsIotMqttMode;
    connectInfo.cleanSession = (ctx->network_params.mqtt.session_type == CY_OTA_MQTT_SESSION_CLEAN) ? true : false;
    connectInfo.keepAliveSeconds = CY_OTA_MQTT_KEEP_ALIVE_SECONDS;
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
    memset(temp_identifier, 0x00, sizeof(temp_identifier) );

    IotLogDebug("%s()incoming pIdentifier:%p %.*s", __func__,
            pIdentifier, (pIdentifier != NULL) ? strlen(pIdentifier) : 0, pIdentifier);

    if ( (pIdentifier == NULL) || (pIdentifier[ 0 ] == '\0') )
    {
        strncpy(temp_identifier, CY_OTA_MQTT_CLIENT_ID_PREFIX, CLIENT_IDENTIFIER_MAX_LENGTH - UINT16_DECIMAL_LENGTH);
    }
    else
    {
        strncpy(temp_identifier, pIdentifier, CLIENT_IDENTIFIER_MAX_LENGTH - UINT16_DECIMAL_LENGTH);
    }

    /* Every active MQTT connection must have a unique client identifier. The demos
     * generate this unique client identifier by appending a timestamp to a common
     * prefix. */
    string_len = snprintf(pClientIdentifierBuffer,
                          CLIENT_IDENTIFIER_MAX_LENGTH,
                          "%s%d", temp_identifier, (uint16_t)(tval & 0x0000FFFF) );

    /* Check for errors from snprintf. */
    if (string_len >=  CLIENT_IDENTIFIER_MAX_LENGTH)
    {
        IotLogWarn("Failed to generate unique MQTT client identifier. Using partial");
        string_len = snprintf(pClientIdentifierBuffer,
                              CLIENT_IDENTIFIER_MAX_LENGTH,
                              "Unique%d", (uint16_t)(tval & 0xFFFF) );
        if (string_len >=  CLIENT_IDENTIFIER_MAX_LENGTH)
        {
            IotLogError("Failed to generate unique MQTT client identifier. Fail");
            return CY_RSLT_OTA_ERROR_GET_JOB;
        }
    }

    /* Set the client identifier buffer and length. */
    connectInfo.pClientIdentifier = pClientIdentifierBuffer;
    connectInfo.clientIdentifierLength = (uint16_t ) strlen(pClientIdentifierBuffer);
    if (networkInfo.u.setup.pNetworkCredentialInfo != NULL)
    {
        connectInfo.pUserName = networkInfo.u.setup.pNetworkCredentialInfo->pUserName;
        connectInfo.userNameLength = networkInfo.u.setup.pNetworkCredentialInfo->userNameSize;
        connectInfo.pPassword = networkInfo.u.setup.pNetworkCredentialInfo->pPassword;
        connectInfo.passwordLength = networkInfo.u.setup.pNetworkCredentialInfo->passwordSize;
    }
#ifdef PRINT_CONNECT_INFO
    _print_connect_info(&connectInfo);
#endif
#ifdef PRINT_NETWORK_INFO
    _print_network_info(&networkInfo);
#endif

    /* Establish the MQTT connection. */
    IotLogInfo("MQTT unique client identifier is %.*s (length %hu).\n",
                connectInfo.clientIdentifierLength,
                connectInfo.pClientIdentifier,
                connectInfo.clientIdentifierLength);

    connectStatus = IotMqtt_Connect(&networkInfo,
                                    &connectInfo,
                                    IOT_MQTT_RESPONSE_WAIT_MS,
                                    pMqttConnection);

    if (connectStatus != IOT_MQTT_SUCCESS )
    {
        IotLogError("MQTT CONNECT returned error %s.\n", IotMqtt_strerror(connectStatus) );

        return CY_RSLT_OTA_ERROR_GET_JOB;
    }

    return CY_RSLT_SUCCESS;
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
 * @return CY_RSLT_SUCCESS
 *         CY_RSLT_OTA_ERROR_MQTT_SUBSCRIBE
 *
 */
static cy_rslt_t cy_ota_modify_subscriptions(IotMqttConnection_t mqttConnection,
                                             IotMqttOperationType_t operation,
                                             uint8_t numTopicFilters,
                                             const char ** pTopicFilters,
                                             void * pCallbackParameter)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    int32_t i = 0;
    IotMqttError_t subscriptionStatus = IOT_MQTT_STATUS_PENDING;
    IotMqttSubscription_t pSubscriptions[ CY_OTA_MQTT_MAX_TOPICS ] = { IOT_MQTT_SUBSCRIPTION_INITIALIZER };

    /* Set the members of the subscription list. */
    IotLogInfo("%s() numTopicFilters: %d\n", __func__, numTopicFilters);
    for(i = 0; i < numTopicFilters; i++ )
    {
        IotLogInfo("%s() index: %ld: filter: %s\n", __func__, i, pTopicFilters[ i ]);
        pSubscriptions[ i ].qos = IOT_MQTT_QOS_1;
        pSubscriptions[ i ].pTopicFilter = pTopicFilters[ i ];
        pSubscriptions[ i ].topicFilterLength = strlen(pTopicFilters[ i ]);
        pSubscriptions[ i ].callback.pCallbackContext = pCallbackParameter;
        pSubscriptions[ i ].callback.function = cy_ota_MQTT_subscription_callback;
    }

    /* Modify subscriptions by either subscribing or unsubscribing. */
    if (operation == IOT_MQTT_SUBSCRIBE )
    {
        IotLogDebug("%s() mqttConnection: %p \n", __func__, mqttConnection);
        subscriptionStatus = IotMqtt_TimedSubscribe(mqttConnection,
                                                    pSubscriptions,
                                                    numTopicFilters,
                                                    0,
                                                    IOT_MQTT_RESPONSE_WAIT_MS);

        /* Check the status of SUBSCRIBE. */
        switch(subscriptionStatus)
        {
            case IOT_MQTT_SUCCESS:
                IotLogDebug("\nAll topic filter subscriptions accepted.......\n");
                result = CY_RSLT_SUCCESS;
                break;

            case IOT_MQTT_SERVER_REFUSED:
                /* Check which subscriptions were rejected before exiting */
                for(i = 0; i < numTopicFilters; i++ )
                {
                    if (IotMqtt_IsSubscribed(mqttConnection,
                                             pSubscriptions[ i ].pTopicFilter,
                                             pSubscriptions[ i ].topicFilterLength,
                                             NULL) == true )
                    {
                        IotLogWarn("Topic filter %.*s was accepted.",
                                   pSubscriptions[ i ].topicFilterLength,
                                   pSubscriptions[ i ].pTopicFilter);
                    }
                    else
                    {
                        IotLogWarn("Topic filter %.*s was rejected.",
                                    pSubscriptions[ i ].topicFilterLength,
                                    pSubscriptions[ i ].pTopicFilter);
                    }
                }
                result = CY_RSLT_OTA_ERROR_MQTT_SUBSCRIBE;
                break;

            default:
                result = CY_RSLT_OTA_ERROR_MQTT_SUBSCRIBE;
                break;
        }
    }
    else if (operation == IOT_MQTT_UNSUBSCRIBE )
    {
        subscriptionStatus = IotMqtt_TimedUnsubscribe(mqttConnection,
                                                      pSubscriptions,
                                                      numTopicFilters,
                                                      0,
                                                      IOT_MQTT_RESPONSE_WAIT_MS);

        /* Check the status of UNSUBSCRIBE. */
        if (subscriptionStatus != IOT_MQTT_SUCCESS )
        {
            result = CY_RSLT_OTA_ERROR_MQTT_SUBSCRIBE;
        }
    }
    else
    {
        /* Only SUBSCRIBE and UNSUBSCRIBE are valid for modifying subscriptions. */
        IotLogError("MQTT operation %s is not valid for modifying subscriptions.",
                     IotMqtt_OperationType(operation) );

        result = CY_RSLT_OTA_ERROR_MQTT_SUBSCRIBE;
    }

    IotLogDebug("%s() returning: 0x%x\n", __func__, result);
    return result;
}

/**
 * @brief Validate network parameters
 *
 * NOTE: Individual Network Connection type will test appropriate fields
 *
 * @param[in]  network_params   pointer to Network parameter structure
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_OTA_ERROR_GENERAL
 */
cy_rslt_t cy_ota_mqtt_validate_network_params(cy_ota_network_params_t *network_params)
{
    IotLogDebug("%s()\n", __func__);
    if (network_params == NULL)
    {
        return CY_RSLT_OTA_ERROR_BADARG;
    }

    if ( (network_params->mqtt.pIdentifier == NULL) ||
         (network_params->network_interface == NULL) ||
         (network_params->mqtt.broker.pHostName == NULL) )
    {
        IotLogError("%s() BAD ARGS\n", __func__);
        IotLogError("    Identifier:%s Broker:%s net iface:%p \n",
                    network_params->mqtt.pIdentifier,
                    network_params->mqtt.broker.pHostName,
                    network_params->network_interface);
        return CY_RSLT_OTA_ERROR_BADARG;
    }

    return CY_RSLT_SUCCESS;
}

/**
 * @brief Connect to OTA Update server
 *
 * NOTE: Individual Network Connection type will do whatever is necessary
 *       This function must have it's own timeout for connection failure
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_OTA_ERROR_GENERAL
 */
cy_rslt_t cy_ota_mqtt_connect(cy_ota_context_t *ctx)
{
    cy_rslt_t                   result;
    cy_ota_server_info_t        server;
    IotNetworkCredentials_t     credentials;

    CY_OTA_CONTEXT_ASSERT(ctx);

    credentials = ctx->network_params.mqtt.credentials;

    if (ctx->mqtt.connection_from_app == true)
    {
        IotLogInfo("%s() App provided connection.\n", __func__);
        return CY_RSLT_SUCCESS;
    }

    if (ctx->mqtt.connection_established == true)
    {
        IotLogError("%s() Already connected.\n", __func__);
        return CY_RSLT_OTA_ERROR_GENERAL;
    }


    /* Handle of the MQTT connection used in this library. */
    ctx->mqtt.mqtt_connection = IOT_MQTT_CONNECTION_INITIALIZER;

    server.pHostName = ctx->curr_server->pHostName;
    server.port = ctx->curr_server->port;
    /* If application changed job_doc we need to use the parsed info */
    if ( (ctx->curr_state == CY_OTA_STATE_DATA_CONNECT) &&
         (ctx->network_params.use_get_job_flow == CY_OTA_JOB_FLOW)  &&
         (ctx->parsed_job.parse_result == CY_RSLT_OTA_CHANGING_SERVER) )
    {
        server.pHostName = ctx->parsed_job.broker_server.pHostName;
        server.port = ctx->parsed_job.broker_server.port;
        if (ctx->callback_data.credentials != NULL)
        {
            credentials = ctx->callback_data.credentials;
        }
    }

    if (ctx->curr_server->port == CY_OTA_MQTT_BROKER_PORT)
    {
        credentials = NULL;
    }

    /* Establish a new MQTT connection. called function has a timeout */
    IotLogInfo("\nEstablishing MQTT Connection for %s :: %s:%d...credentials:%p\n",
                ctx->network_params.mqtt.pIdentifier,
                server.pHostName,
                server.port,
                credentials);
    result = cy_ota_establish_MQTT_connection(ctx,
                                              ctx->network_params.mqtt.awsIotMqttMode,
                                              (char *)ctx->network_params.mqtt.pIdentifier,
                                              &server,
                                              credentials,
                                              ctx->network_params.network_interface,
                                              &ctx->mqtt.mqtt_connection);

    if (result != CY_RSLT_SUCCESS)
    {
        IotLogWarn("MQTT Connection failed\n");
        result = CY_RSLT_OTA_ERROR_MQTT_INIT;
    }
    else
    {
        /* Mark the MQTT connection as established. */
        ctx->mqtt.connection_established = true;
    }

    return result;
}

/**
 * @brief get the OTA job
 *
 * NOTE: Individual Network Connection type will do whatever is necessary
 *      ex: MQTT
 *          - subscribe to start data transfer
 *          TCP
 *          - pull the data from the server
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_OTA_ERROR_GENERAL
 */
cy_rslt_t cy_ota_mqtt_get_job(cy_ota_context_t *ctx)
{
    uint32_t    waitfor_clear;
    cy_rslt_t   result = CY_RSLT_SUCCESS;
    IotLogDebug("%s()\n", __func__);

    CY_OTA_CONTEXT_ASSERT(ctx);

    if (ctx->mqtt.connection_established != true)
    {
        IotLogWarn("%s() connection not established\n", __func__);
        return CY_RSLT_OTA_ERROR_GET_JOB;
    }

    if (cy_rtos_init_mutex(&ctx->sub_callback_mutex) != CY_RSLT_SUCCESS)
    {
        IotLogWarn("%s() sub_callback_mutex init failed\n", __func__);
        return CY_RSLT_OTA_ERROR_GET_JOB;
    }
    ctx->sub_callback_mutex_inited = 1;

    /* clear any lingering events */
    waitfor_clear = CY_OTA_EVENT_MQTT_EVENTS;
    result = cy_rtos_waitbits_event(&ctx->ota_event, &waitfor_clear, 1, 0, 1);
    if (waitfor_clear != 0)
    {
        IotLogDebug("%s() Clearing waitfor: 0x%lx", __func__, waitfor_clear);
    }

    IotLogDebug("\nMQTT Subscribe for Job Messages..............\n");
    /* Add the topic filter */
    result = cy_ota_modify_subscriptions(ctx->mqtt.mqtt_connection,
                                         IOT_MQTT_SUBSCRIBE,
                                         ctx->network_params.mqtt.numTopicFilters,
                                         ctx->network_params.mqtt.pTopicFilters,
                                         ctx);

    if (result != CY_RSLT_SUCCESS)
    {
        IotLogWarn("%s() cy_ota_modify_subscriptions() failed\n", __func__);
        goto cleanup_and_exit;
    }
    ctx->mqtt.subscribed = true;

    /* Ask if there is an update available */
    result = cy_ota_subscribe_and_publish_unique_topic(ctx, CY_OTA_SUBSCRIBE_UPDATES_AVAIL);
    if (result != CY_RSLT_SUCCESS)
    {
        IotLogWarn("%s() subscribe/publish () failed result:0x%lx\n", __func__, result);
        goto cleanup_and_exit;
    }

    while (1)
    {
        uint32_t waitfor;

        /* get event */
        waitfor = CY_OTA_EVENT_MQTT_EVENTS;
        result = cy_rtos_waitbits_event(&ctx->ota_event, &waitfor, 1, 0, CY_OTA_WAIT_MQTT_EVENTS_MS);
        IotLogDebug("%s() MQTT cy_rtos_waitbits_event: 0x%lx result:0x%lx\n", __func__, waitfor, result);

        /* We only want to act on events we are waiting on.
         * For timeouts, just loop around.
         */
        if (waitfor == 0)
        {
            continue;
        }
        if (waitfor & CY_OTA_EVENT_SHUTDOWN_NOW)
        {
            /* Pass along to Agent thread */
            cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_SHUTDOWN_NOW, 0);
            result = CY_RSLT_SUCCESS;
            break;
        }

        if (waitfor & CY_OTA_EVENT_DATA_DOWNLOAD_TIMEOUT)
        {
            /* This was generated by a timer in cy_ota_agent.c
             * Pass along to Agent thread.
             */
            IotLogDebug("MQTT: JOB Download Timeout return:0x%lx", CY_RSLT_OTA_NO_UPDATE_AVAILABLE);
            result = CY_RSLT_OTA_NO_UPDATE_AVAILABLE;
            break;
        }

        if (waitfor & CY_OTA_EVENT_GOT_DATA)
        {
            /* If we get malformed (short) job doc, look into using
             * DATA_DONE instead of GOT_DATA
             */
            IotLogDebug("MQTT: JOB Download Got Data");
            result = CY_RSLT_SUCCESS;
            break;
        }

        if (waitfor & CY_OTA_EVENT_MALFORMED_JOB_DOC)
        {
            IotLogDebug("%s() MQTT CY_OTA_EVENT_MALFORMED_JOB_DOC\n", __func__);
            result = CY_RSLT_OTA_ERROR_MALFORMED_JOB_DOC;
            break;
        }

        if (waitfor & CY_OTA_EVENT_INVALID_VERSION)
        {
            IotLogDebug("%s() MQTT CY_OTA_EVENT_INVALID_VERSION\n", __func__);
            result = CY_RSLT_OTA_ERROR_INVALID_VERSION;
            break;
        }

        if (waitfor & CY_OTA_EVENT_DATA_FAIL)
        {
            IotLogDebug("%s() MQTT CY_OTA_EVENT_DATA_FAIL\n", __func__);
            result = CY_RSLT_OTA_ERROR_GET_JOB;
            break;
        }

        if (waitfor & CY_OTA_EVENT_DROPPED_US)
        {
            IotLogDebug("%s() MQTT CY_OTA_EVENT_DROPPED_US state:%d %s\n", __func__, ctx->curr_state, cy_ota_get_state_string(ctx->curr_state));
            result = CY_RSLT_OTA_ERROR_SERVER_DROPPED;
            break;
        }

    }

  cleanup_and_exit:
    ctx->sub_callback_mutex_inited = 0;
    cy_rtos_deinit_mutex(&ctx->sub_callback_mutex);

    return result;
}

/**
 * @brief get the OTA download
 *
 * NOTE: Individual Network Connection type will do whatever is necessary
 *      ex: MQTT
 *          - subscribe to start data transfer
 *          TCP
 *          - pull the data from the server
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_OTA_ERROR_GENERAL
 */
cy_rslt_t cy_ota_mqtt_get_data(cy_ota_context_t *ctx)
{
    int                         i;
    uint32_t                    waitfor_clear;
    cy_rslt_t                   result = CY_RSLT_SUCCESS;
    cy_ota_callback_results_t   cb_result;

    IotLogDebug("%s()\n", __func__);

    CY_OTA_CONTEXT_ASSERT(ctx);

    if (ctx->mqtt.connection_established != true)
    {
        IotLogWarn("%s() connection not established\n", __func__);
        return CY_RSLT_OTA_ERROR_GET_DATA;
    }

    if (cy_rtos_init_mutex(&ctx->sub_callback_mutex) != CY_RSLT_SUCCESS)
    {
        IotLogWarn("%s() sub_callback_mutex init failed\n", __func__);
        return CY_RSLT_OTA_ERROR_GET_DATA;
    }
    ctx->sub_callback_mutex_inited = 1;

    /* clear any lingering events */
    waitfor_clear = CY_OTA_EVENT_MQTT_EVENTS;
    result = cy_rtos_waitbits_event(&ctx->ota_event, &waitfor_clear, 1, 0, 1);
    if (waitfor_clear != 0)
    {
        IotLogDebug("%s() Clearing waitfor: 0x%lx", __func__, waitfor_clear);
    }

    /* send request */
    memset(ctx->mqtt.json_doc, 0x00, sizeof(ctx->mqtt.json_doc));
    if (ctx->mqtt.use_unique_topic == 1)
    {
        /* we might have gotten the Job from HTTP, always create unique */
        result = cy_ota_subscribe_and_publish_unique_topic(ctx, CY_OTA_DOWNLOAD_REQUEST);
        if (result != CY_RSLT_SUCCESS)
        {
            IotLogWarn("%s() subscribe/publish () failed result:0x%lx\n", __func__, result);
            goto cleanup_and_exit;
        }
        ctx->mqtt.subscribed = true;
    }
    else
    {
        /* Create the direct request */
        sprintf(ctx->mqtt.json_doc, CY_OTA_DOWNLOAD_DIRECT_REQUEST,
                APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD);

        IotLogDebug("\nMQTT Subscribe for Direct download DATA Messages..............\n");
        /* Add the topic filter subscriptions  */
        result = cy_ota_modify_subscriptions(ctx->mqtt.mqtt_connection,
                                             IOT_MQTT_SUBSCRIBE,
                                             ctx->network_params.mqtt.numTopicFilters,
                                             ctx->network_params.mqtt.pTopicFilters,
                                             ctx);
        if (result != CY_RSLT_SUCCESS)
        {
            IotLogWarn("%s() cy_ota_modify_subscriptions() failed\n", __func__);
            goto cleanup_and_exit;
        }
        ctx->mqtt.subscribed = true;

        IotLogDebug("%d : %s() CALLING CB STATE_CHANGE %s stop_OTA_session:%d ", __LINE__, __func__,
                cy_ota_get_state_string(ctx->curr_state), ctx->stop_OTA_session);

        cb_result = cy_ota_internal_call_cb(ctx, CY_OTA_REASON_STATE_CHANGE, ctx->curr_state);
        switch( cb_result )
        {
        case CY_OTA_CB_RSLT_OTA_CONTINUE:
            result = cy_ota_mqtt_publish_request(ctx, SUBSCRIBER_PUBLISH_TOPIC, ctx->mqtt.json_doc);
            if (result != CY_RSLT_SUCCESS)
            {
                IotLogWarn("%s() cy_ota_mqtt_publish_request() for Data failed\n", __func__);
                goto cleanup_and_exit;
            }
            break;

        case CY_OTA_CB_RSLT_OTA_STOP:
            IotLogError("%s() App returned OTA Stop for STATE_CHANGE for DATA_DOWNLOAD", __func__);
            result = CY_RSLT_OTA_ERROR_APP_RETURNED_STOP;
            goto cleanup_and_exit;

        case CY_OTA_CB_RSLT_APP_SUCCESS:
            IotLogInfo("%s() App returned APP_SUCCESS for STATE_CHANGE for DATA_DOWNLOAD", __func__);
            result = CY_RSLT_SUCCESS;
            goto cleanup_and_exit;

        case CY_OTA_CB_RSLT_APP_FAILED:
            IotLogError("%s() App returned APP_FAILURE for STATE_CHANGE for DATA_DOWNLOAD", __func__);
            result = CY_RSLT_OTA_ERROR_GET_DATA;
            goto cleanup_and_exit;

        case CY_OTA_CB_NUM_RESULTS:
            result = CY_RSLT_OTA_ERROR_GET_DATA;
            goto cleanup_and_exit;
        }
    }

    /* Create download interval timer */
   result = cy_rtos_init_timer(&ctx->mqtt.mqtt_timer, CY_TIMER_TYPE_ONCE,
                               cy_ota_mqtt_timer_callback, (cy_timer_callback_arg_t)ctx);
   if (result != CY_RSLT_SUCCESS)
   {
       /* Event create failed */
       IotLogWarn("%s() Timer Create Failed!\n", __func__);
       result = CY_RSLT_OTA_ERROR_GET_DATA;
       goto cleanup_and_exit;
   }

   if (ctx->packet_timeout_sec > 0 )
   {
       /* Start the download interval timer */
       IotLogDebug("%s() MQTT DATA START PACKET TIMER %ld secs\n", __func__, ctx->packet_timeout_sec);
       cy_ota_start_mqtt_timer(ctx, ctx->packet_timeout_sec, CY_OTA_EVENT_PACKET_TIMEOUT);
   }

   /* clear out tally of received / written packets */
    memset(&ctx->mqtt.received_packets, 0, sizeof(ctx->mqtt.received_packets) );

    while (1)
    {
        uint32_t waitfor;

        /* get event */
        waitfor = CY_OTA_EVENT_MQTT_EVENTS;
        result = cy_rtos_waitbits_event(&ctx->ota_event, &waitfor, 1, 0, CY_OTA_WAIT_MQTT_EVENTS_MS);
        IotLogDebug("%s() MQTT cy_rtos_waitbits_event: 0x%lx type:%d mod:0x%lx code:%d\n", __func__, waitfor, CY_RSLT_GET_TYPE(result), CY_RSLT_GET_MODULE(result), CY_RSLT_GET_CODE(result) );

        /* We only want to act on events we are waiting on.
         * For timeouts, just loop around.
         */
        if (waitfor == 0)
        {
            continue;
        }

        if (waitfor & CY_OTA_EVENT_SHUTDOWN_NOW)
        {
            /* Pass along to Agent thread */
            cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_SHUTDOWN_NOW, 0);
            result = CY_RSLT_SUCCESS;
            break;
        }

        if (waitfor & CY_OTA_EVENT_DATA_DOWNLOAD_TIMEOUT)
        {
            /* This was generated by a timer in cy_ota_agent.c
             * Pass along to Agent thread.
             */
            IotLogDebug("MQTT: Download Timeout ");
            result = CY_RSLT_OTA_NO_UPDATE_AVAILABLE;
            break;
        }

        if (waitfor & CY_OTA_EVENT_STORAGE_ERROR)
        {
            result = CY_RSLT_OTA_ERROR_WRITE_STORAGE;
            break;
        }

        if (waitfor & CY_OTA_EVENT_APP_STOPPED_OTA)
        {
            IotLogDebug("%s() App told us to stop \n", __func__);
            result = CY_RSLT_OTA_ERROR_APP_RETURNED_STOP;
            break;
        }

        if (waitfor & CY_OTA_EVENT_GOT_DATA)
        {
            if (ctx->packet_timeout_sec > 0 )
            {
                /* got some data - restart the download interval timer */
                IotLogDebug("%s() RESTART PACKET TIMER %ld secs\n", __func__, ctx->packet_timeout_sec);
                cy_ota_start_mqtt_timer(ctx, ctx->packet_timeout_sec, CY_OTA_EVENT_PACKET_TIMEOUT);
            }

            if (ctx->total_bytes_written >= ctx->total_image_size)
            {
                /* stop timer asap so we don't get a timeout */
                cy_ota_stop_mqtt_timer(ctx);

                IotLogDebug("Done writing all data! %ld of %ld\n", ctx->total_bytes_written, ctx->total_image_size);
                cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_DATA_DONE, 0);
            }
            continue;
        }

        if (waitfor & CY_OTA_EVENT_PACKET_TIMEOUT)
        {
            /* We set a timer and if packets take too long, we will assume the broker forgot about us.
             * Set with CY_OTA_PACKET_INTERVAL_SECS.
             */
            if (ctx->num_packets_received > ctx->last_num_packets_received)
            {
                /* If we received packets since the last time we were here, just continue.
                 * This thread may be held off for a while, and we don't want a false failure.
                 */
                IotLogDebug("%s() RESTART PACKET TIMER %ld secs\n", __func__, ctx->packet_timeout_sec);
                cy_ota_start_mqtt_timer(ctx, ctx->packet_timeout_sec, CY_OTA_EVENT_PACKET_TIMEOUT);

                /* update our variable */
                ctx->last_num_packets_received = ctx->num_packets_received;
                continue;
            }
            IotLogWarn("OTA Timeout waiting for a packet (%d seconds), fail\n", ctx->packet_timeout_sec);
            cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_DATA_FAIL, 0);
        }

        if (waitfor & CY_OTA_EVENT_DATA_DONE)
        {
            IotLogDebug("Got all the data !");
            result = CY_RSLT_SUCCESS;
            break;
        }

        if (waitfor & CY_OTA_EVENT_INVALID_VERSION)
        {
            result = CY_RSLT_OTA_ERROR_INVALID_VERSION;
            break;
        }

        if (waitfor & CY_OTA_EVENT_DATA_FAIL)
        {
            result = CY_RSLT_OTA_ERROR_GET_DATA;
            break;
        }

        if (waitfor & CY_OTA_EVENT_DROPPED_US)
        {
            IotLogDebug("%s() MQTT CY_OTA_EVENT_DROPPED_US state:%d %s\n", __func__, ctx->curr_state, cy_ota_get_state_string(ctx->curr_state));
            result = CY_RSLT_OTA_ERROR_SERVER_DROPPED;
            break;
        }
    }   /* While 1 */

    IotLogDebug("%s() MQTT DONE result: 0x%lx\n", __func__, result);

    for (i = 0;i < ctx->total_packets; i++)
    {
        if (ctx->mqtt.received_packets[i] == 0 )
        {
            IotLogDebug("PACKET %d missing!\n", i);
        }
        else if (ctx->mqtt.received_packets[i] > 1 )
        {
            IotLogDebug("PACKET %d Duplicate!\n", i);
        }
    }

  cleanup_and_exit:
    if (ctx->mqtt.mqtt_timer != NULL)
    {
        /* we completed the download, stop the timer */
        cy_ota_stop_mqtt_timer(ctx);

        cy_rtos_deinit_timer(&ctx->mqtt.mqtt_timer);
    }

    ctx->sub_callback_mutex_inited = 0;
    cy_rtos_deinit_mutex(&ctx->sub_callback_mutex);

    return result;
}

/**
 * @brief Disconnect from MQTT broker
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_OTA_ERROR_GENERAL
 */
cy_rslt_t cy_ota_mqtt_disconnect(cy_ota_context_t *ctx)
{
    char    *unique_topics[1];
    IotLogDebug("%s()\n", __func__);
    CY_OTA_CONTEXT_ASSERT(ctx);

    if (ctx->mqtt.subscribed == true)
    {
        IotLogDebug("\nMQTT UnSubscribe for Job and Data Messages..............\n");
        /* Remove the topic filters subscriptions */
        cy_ota_modify_subscriptions(ctx->mqtt.mqtt_connection,
                                    IOT_MQTT_UNSUBSCRIBE,
                                    ctx->network_params.mqtt.numTopicFilters,
                                    ctx->network_params.mqtt.pTopicFilters,
                                    ctx);

        /* unsubscribe from unique topic (if we used one) */
        if (strlen(ctx->mqtt.unique_topic) > 0)
        {
            unique_topics[0] = ctx->mqtt.unique_topic;
            cy_ota_modify_subscriptions(ctx->mqtt.mqtt_connection,
                                        IOT_MQTT_UNSUBSCRIBE,
                                        1,
                                        (const char **)&unique_topics[0],
                                        ctx);
        }
        ctx->mqtt.subscribed = false;
    }

    if (ctx->mqtt.connection_from_app == false)
    {

        if (ctx->mqtt.connection_established == true)
        {
            ctx->mqtt.connection_established = false;
            IotMqtt_Disconnect(ctx->mqtt.mqtt_connection, 0);
            ctx->mqtt.mqtt_connection = NULL;
        }
    }
    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_ota_mqtt_report_result(cy_ota_context_t *ctx, cy_rslt_t last_error)
{
    cy_ota_callback_results_t   cb_result;
    cy_rslt_t                   result = CY_RSLT_SUCCESS;

    sprintf(ctx->mqtt.json_doc, CY_OTA_MQTT_RESULT_JSON,
            ( (last_error == CY_RSLT_SUCCESS) ? CY_OTA_RESULT_SUCCESS : CY_OTA_RESULT_FAILURE),
            ctx->mqtt.unique_topic);

    IotLogDebug("%d : %s() CALLING CB STATE_CHANGE %s stop_OTA_session:%d ", __LINE__, __func__,
            cy_ota_get_state_string(ctx->curr_state), ctx->stop_OTA_session);

    cb_result = cy_ota_internal_call_cb(ctx, CY_OTA_REASON_STATE_CHANGE, ctx->curr_state);
    switch( cb_result )
    {
    case CY_OTA_CB_RSLT_OTA_CONTINUE:
        result = cy_ota_mqtt_publish_request(ctx, SUBSCRIBER_PUBLISH_TOPIC, ctx->mqtt.json_doc);
        break;
    case CY_OTA_CB_RSLT_OTA_STOP:
        IotLogError("%s() App returned OTA Stop for STATE_CHANGE for SEND_RESULT", __func__);
        result = CY_RSLT_OTA_ERROR_APP_RETURNED_STOP;
        break;
    case CY_OTA_CB_RSLT_APP_SUCCESS:
        IotLogInfo("%s() App returned APP_SUCCESS for STATE_CHANGE for SEND_RESULT", __func__);
        break;
    case CY_OTA_CB_RSLT_APP_FAILED:
        IotLogError("%s() App returned APP_FAILED for STATE_CHANGE for SEND_RESULT", __func__);
        result = CY_RSLT_OTA_ERROR_SENDING_RESULT;
        break;
    case CY_OTA_CB_NUM_RESULTS:
        result = CY_RSLT_OTA_ERROR_SENDING_RESULT;
        break;
    }

    // TODO: STDE wait for response ?

    return result;
}
