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
 *  Cypress OTA Agent network abstraction for HTTP
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <FreeRTOS.h>

/* lwIP header files */
#include <lwip/tcpip.h>
#include <lwip/api.h>

#include "cy_ota_api.h"
#include "cy_ota_internal.h"
#include "ip4_addr.h"

#include "iot_platform_types.h"
#include "cyabs_rtos.h"
#include "cy_iot_network_secured_socket.h"

/* Uncomment to print data as we get it from the network */
//#define PRINT_DATA  1

/***********************************************************************
 *
 * HTTP network Functions
 *
 **********************************************************************/


/***********************************************************************
 *
 * defines & enums
 *
 **********************************************************************/

typedef enum
{
    HTTP_CONTINUE                        = 100,
    HTTP_SWITCHING_PROTOCOLS             = 101,
    HTTP_RESPONSE_OK                     = 200,
    HTTP_CREATED                         = 201,
    HTTP_ACCEPTED                        = 202,
    HTTP_NONAUTHORITATIVE                = 203,
    HTTP_NO_CONTENT                      = 204,
    HTTP_RESET_CONTENT                   = 205,
    HTTP_PARTIAL_CONTENT                 = 206,
    HTTP_MULTIPLE_CHOICES                = 300,
    HTTP_MOVED_PERMANENTLY               = 301,
    HTTP_FOUND                           = 302,
    HTTP_SEE_OTHER                       = 303,
    HTTP_NOT_MODIFIED                    = 304,
    HTTP_USEPROXY                        = 305,
    HTTP_TEMPORARY_REDIRECT              = 307,
    HTTP_BAD_REQUEST                     = 400,
    HTTP_UNAUTHORIZED                    = 401,
    HTTP_PAYMENT_REQUIRED                = 402,
    HTTP_FORBIDDEN                       = 403,
    HTTP_NOT_FOUND                       = 404,
    HTTP_METHOD_NOT_ALLOWED              = 405,
    HTTP_NOT_ACCEPTABLE                  = 406,
    HTTP_PROXY_AUTHENTICATION_REQUIRED   = 407,
    HTTP_REQUEST_TIMEOUT                 = 408,
    HTTP_CONFLICT                        = 409,
    HTTP_GONE                            = 410,
    HTTP_LENGTH_REQUIRED                 = 411,
    HTTP_PRECONDITION_FAILED             = 412,
    HTTP_REQUESTENTITYTOOLARGE           = 413,
    HTTP_REQUESTURITOOLONG               = 414,
    HTTP_UNSUPPORTEDMEDIATYPE            = 415,
    HTTP_REQUESTED_RANGE_NOT_SATISFIABLE = 416,
    HTTP_EXPECTATION_FAILED              = 417,
    HTTP_INTERNAL_SERVER_ERROR           = 500,
    HTTP_NOT_IMPLEMENTED                 = 501,
    HTTP_BAD_GATEWAY                     = 502,
    HTTP_SERVICE_UNAVAILABLE             = 503,
    HTTP_GATEWAY_TIMEOUT                 = 504,
    HTTP_VERSION_NOT_SUPPORTED           = 505,
} http_status_code_t;

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

/***********************************************************************
 *
 * Data & Variables
 *
 **********************************************************************/

#define HTTP_HEADER_STR                 "HTTP/"
#define CONTENT_STRING                  "Content-Length:"
#define HTTP_HEADERS_BODY_SEPARATOR     "\r\n\r\n"

/***********************************************************************
 *
 * Forward declarations
 *
 **********************************************************************/

/***********************************************************************
 *
 * Functions
 *
 **********************************************************************/

/**
 * Length limited version of strstr. Ported from wiced_lib.c
 *
 * @param s[in]             : The string to be searched.
 * @param s_len[in]         : The length of the string to be searched.
 * @param substr[in]        : The string to be found.
 * @param substr_len[in]    : The length of the string to be found.
 *
 * @return    pointer to the found string if search successful, otherwise NULL
 */
char* strnstrn(const char *s, uint16_t s_len, const char *substr, uint16_t substr_len)
{
    for (; s_len >= substr_len; s++, s_len--)
    {
        if (strncmp(s, substr, substr_len) == 0)
        {
            return (char*)s;
        }
    }

    return NULL;
}

void cy_ota_http_timer_callback(cy_timer_callback_arg_t arg)
{
    cy_ota_context_t *ctx = (cy_ota_context_t *)arg;
    CY_OTA_CONTEXT_ASSERT(ctx);

    IotLogDebug("%s() new event:%d\n", __func__, ctx->http.http_timer_event);
    /* yes, we set the ota_event as the http get() function uses the same event var */
    cy_rtos_setbits_event(&ctx->ota_event, ctx->http.http_timer_event, 0);
}

cy_rslt_t cy_ota_stop_http_timer(cy_ota_context_t *ctx)
{
    CY_OTA_CONTEXT_ASSERT(ctx);
    return cy_rtos_stop_timer(&ctx->http.http_timer);
}

cy_rslt_t cy_ota_start_http_timer(cy_ota_context_t *ctx, uint32_t secs, ota_events_t event)
{
    cy_rslt_t result;
    uint32_t    num_ms = SECS_TO_MILLISECS(secs);

    CY_OTA_CONTEXT_ASSERT(ctx);

    cy_ota_stop_http_timer(ctx);
    ctx->http.http_timer_event = event;
    result = cy_rtos_start_timer(&ctx->http.http_timer, num_ms);
    return result;
}


cy_rslt_t cy_ota_http_parse_header(uint8_t **ptr, uint16_t *data_len, uint32_t *file_len, http_status_code_t *response_code)
{
    char    *response_status;
    uint8_t *header_end;

    if ( (ptr == NULL) || (*ptr == NULL) ||
         (data_len == NULL) || (*data_len == 0) ||
         (file_len == NULL) ||
         (response_code == NULL) )
    {
        return CY_RSLT_OTA_ERROR_BADARG;
    }
    *response_code = HTTP_FORBIDDEN;

    /* example:
        "HTTP/1.1 200 Ok\r\n"
        "Server: mini_httpd/1.23 28Dec2015\r\n"
        "Date: Tue, 03 Mar 2020 18:49:23 GMT\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: 830544\r\n"
        "\r\n\r\n"
     */

    /* sanity check */
    if (*data_len < 12)
    {
        return CY_RSLT_OTA_ERROR_NOT_A_HEADER;
    }

    /* Find the HTTP/x.x part*/
    response_status = strnstrn( (char *)*ptr, *data_len, HTTP_HEADER_STR, sizeof(HTTP_HEADER_STR) - 1);
    if (response_status == NULL)
    {
        return CY_RSLT_OTA_ERROR_NOT_A_HEADER;
    }
    /* skip to next ' ' space character */
    response_status = strchr(response_status, ' ');
    if (response_status == NULL)
    {
        return CY_RSLT_OTA_ERROR_NOT_A_HEADER;
    }
    *response_code = (http_status_code_t)atoi(response_status + 1);

    /* Find Content-Length part*/
    response_status = strnstrn( (char *)*ptr, *data_len, CONTENT_STRING, sizeof(CONTENT_STRING) - 1);
    if (response_status == NULL)
    {
        return CY_RSLT_OTA_ERROR_NOT_A_HEADER;
    }
    response_status += sizeof(CONTENT_STRING);
    *file_len = atoi(response_status);

    /* find end of header */
    header_end = (uint8_t *)strnstrn( (char *)*ptr, *data_len, HTTP_HEADERS_BODY_SEPARATOR, sizeof(HTTP_HEADERS_BODY_SEPARATOR) - 1);
    if (header_end == NULL)
    {
        return CY_RSLT_OTA_ERROR_NOT_A_HEADER;
    }
    header_end += sizeof(HTTP_HEADERS_BODY_SEPARATOR) - 1;
    *data_len -= (header_end - *ptr);
    IotLogDebug("Move ptr from %p to %p skipping %d new_len:%d first_byte:0x%x\n", *ptr, header_end, (header_end - *ptr), *data_len, *header_end);

    *ptr = header_end;
    return CY_RSLT_SUCCESS;
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
cy_rslt_t cy_ota_http_validate_network_params(cy_ota_network_params_t *network_params)
{
    if (network_params == NULL)
    {
        return CY_RSLT_OTA_ERROR_BADARG;
    }

    if ( (network_params->http.server.pHostName == NULL) ||
         (network_params->http.server.port == 0)  ||
         (network_params->http.file == NULL)  )
    {
        return CY_RSLT_OTA_ERROR_BADARG;
    }
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
cy_rslt_t cy_ota_http_write_chunk_to_flash(cy_ota_context_t *ctx, cy_ota_storage_write_info_t *chunk_info)
{
    cy_ota_callback_results_t cb_result;

    IotLogDebug("%s()\n", __func__);

    if ( (ctx == NULL) || (chunk_info == NULL) )
    {
        IotLogError("%s() Bad args\n", __func__);
        return CY_RSLT_OTA_ERROR_BADARG;
    }

    ctx->num_packets_received++;    /* this is so we don't have a false failure with the per packet timer */
    chunk_info->packet_number = ctx->num_packets_received;

    /* store the chunk */
    ctx->storage = chunk_info;
    cb_result = cy_ota_internal_call_cb(ctx, CY_OTA_REASON_STATE_CHANGE, CY_OTA_STATE_STORAGE_WRITE);
    switch( cb_result )
    {
    default:
    case CY_OTA_CB_RSLT_OTA_CONTINUE:
        if (cy_ota_storage_write(ctx, chunk_info) != CY_RSLT_SUCCESS)
        {
            IotLogError("%s() Write failed\n", __func__);
            cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_DATA_FAIL, 0);
            return CY_RSLT_OTA_ERROR_WRITE_STORAGE;
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
    }

    /* update the stats */
    ctx->total_bytes_written   += chunk_info->size;
    ctx->last_offset            = chunk_info->offset;
    ctx->last_size              = chunk_info->size;
    ctx->last_packet_received   = chunk_info->packet_number;
    ctx->total_packets          = chunk_info->total_packets;

    IotLogDebug("Written to offset:%ld  %ld of %ld (%ld remaining)",
                ctx->last_offset, ctx->total_bytes_written, ctx->total_image_size,
                (ctx->total_image_size - ctx->total_bytes_written) );

    return CY_RSLT_SUCCESS;
}

/**
 * Provide an asynchronous notification of incoming network data.
 *
 * A function with this signature may be set with platform_network_function_setreceivecallback
 * to be invoked when data is available on the network.
 *
 * param[in] pConnection The connection on which data is available, defined by
 * the network stack.
 * param[in] pContext The third argument passed to @ref platform_network_function_setreceivecallback.
 */
void cy_ota_http_receive_callback(IotNetworkConnection_t pConnection, void * pContext )
{
    cy_rslt_t           result = CY_RSLT_SUCCESS;
    uint32_t            bytes_received;
    uint16_t            data_len;
    uint32_t            file_len = 0;
    uint8_t             *ptr;
    cy_ota_context_t    *ctx = (cy_ota_context_t *)pContext;
    CY_OTA_CONTEXT_ASSERT(ctx);

    if (ctx->curr_state == CY_OTA_STATE_JOB_DOWNLOAD)
    {
        IotLogDebug("%() Received Job packet.\n", __func__);
    }
    else if ( (ctx->curr_state != CY_OTA_STATE_DATA_DOWNLOAD) ||
              (ctx->sub_callback_mutex_inited != 1) )
    {
        IotLogWarn("%s() Received packet outside of downloading.\n", __func__);
        goto _callback_exit;
    }

    result = cy_rtos_get_mutex(&ctx->sub_callback_mutex, CY_OTA_WAIT_HTTP_MUTEX_MS);
    if (result != CY_RSLT_SUCCESS)
    {
        /* we didn't get the mutex - something is wrong! */
        IotLogError("%() Mutex timeout!\n", __func__);
        return;
    }

    if (pConnection != NULL)
    {
        uint32_t data_to_receive = CY_OTA_HTTP_SIZE_OF_RECV_BUFFER;

        if (ctx->curr_state == CY_OTA_STATE_JOB_DOWNLOAD)
        {
            data_to_receive = CY_OTA_HTTP_TYPICAL_HEADER_SIZE;
        }
        else if (ctx->curr_state == CY_OTA_STATE_DATA_DOWNLOAD)
        {
            /* reduce the expected bytes to the remainder of the image size, so we don't get a timeout */
            if ( (ctx->total_image_size > 0) && ( (ctx->total_image_size - ctx->total_bytes_written) < data_to_receive) )
            {
                data_to_receive = (ctx->total_image_size - ctx->total_bytes_written);
            }
        }
        else
        {
            IotLogError("%s() Bad state !\n", __func__);
            return;
        }

        ptr = (uint8_t *)ctx->http.data_buffer;
        bytes_received = IotNetworkSecureSockets_Receive(ctx->http.connection, ptr, data_to_receive);
        if (bytes_received == 0)
        {
            IotLogError("%s() IotNetworkSecureSockets_Receive() received %ld\n", __func__, bytes_received);
        }
        else
        {
            cy_ota_storage_write_info_t chunk_info = { 0 };
            data_len = bytes_received;

#ifdef PRINT_DATA
            cy_ota_print_data( (const char *)ptr, 32);
#endif

            if (ctx->total_bytes_written == 0)
            {
                http_status_code_t response_code;
                /* first block here - check the HTTP header */

                /* cy_ota_http_parse_header() moves ptr will be pointed past the header.   */
                result = cy_ota_http_parse_header(&ptr, &data_len, &file_len, &response_code);
                if (result != CY_RSLT_SUCCESS)
                {
                    /* couldn't parse the header */
                    IotLogError("HTTP parse header fail: 0x%lx !\r\n ", result);
                    if (ctx->curr_state == CY_OTA_STATE_JOB_DOWNLOAD)
                    {
                        result = CY_RSLT_OTA_ERROR_GET_JOB;
                    }
                    else
                    {
                        result = CY_RSLT_OTA_ERROR_GET_DATA;
                    }
                    goto _callback_exit;
                }
                else if (response_code < 100)
                {
                    /* do nothing here */
                }
                else if (response_code < 200 )
                {
                    /* 1xx (Informational): The request was received, continuing process */
                }
                else if (response_code < 300 )
                {
                    /* 2xx (Successful): The request was successfully received, understood, and accepted */
                    chunk_info.total_size = file_len;
                    ctx->total_image_size = file_len;
                    IotLogDebug("%s() HTTP File Length: 0x%lx (%ld)\n", __func__, chunk_info.total_size, chunk_info.total_size);
                }
                else if (response_code < 400 )
                {
                    /* 3xx (Redirection): Further action needs to be taken in order to complete the request */
                    IotLogError("HTTP response code: %d, redirection - code needed to handle this!\r\n ", response_code);
                    if (ctx->curr_state == CY_OTA_STATE_JOB_DOWNLOAD)
                    {
                        result = CY_RSLT_OTA_ERROR_GET_JOB;
                    }
                    else
                    {
                        result = CY_RSLT_OTA_ERROR_GET_DATA;
                    }
                    goto _callback_exit;
                }
                else
                {
                    /* 4xx (Client Error): The request contains bad syntax or cannot be fulfilled */
                    IotLogError("HTTP response code: %d, ERROR!\r\n ", response_code);
                    if (ctx->curr_state == CY_OTA_STATE_JOB_DOWNLOAD)
                    {
                        result = CY_RSLT_OTA_ERROR_GET_JOB;
                    }
                    else
                    {
                        result = CY_RSLT_OTA_ERROR_GET_DATA;
                    }
                    goto _callback_exit;
                }
            }

            if (result == CY_RSLT_SUCCESS)
            {
                if (ctx->curr_state == CY_OTA_STATE_JOB_DOWNLOAD)
                {
                    /* Determine if we need to read more to get the full Job document
                     * ptr is pointed to after the header by cy_ota_http_parse_header()
                     */
                    uint8_t  *end_of_header = ptr;
                    uint32_t header_len = (uint32_t)ptr - (uint32_t)ctx->http.data_buffer;
                    uint32_t read_past_file_start = (bytes_received - header_len);
                    uint32_t remainder = 0;
                    if (file_len > read_past_file_start)
                    {
                        remainder = file_len - read_past_file_start;
                    }

                    /* make sure we fit in the document buffer */
                    if (file_len > sizeof(ctx->job_doc) )
                    {
                        IotLogError("HTTP: Job doc too long! %d bytes! Change CY_OTA_JOB_MAX_LEN (%d)!",
                                    file_len, CY_OTA_MQTT_MESSAGE_BUFF_SIZE);
                        result = CY_RSLT_OTA_ERROR_MALFORMED_JOB_DOC;
                        goto _callback_exit;
                    }

                    if ( (remainder > 0) && (remainder < file_len ) )
                    {
                        /* we need to read a bit more */
                        bytes_received = IotNetworkSecureSockets_Receive(ctx->http.connection, &end_of_header[read_past_file_start], remainder);
                        if (bytes_received == 0)
                        {
                            IotLogWarn("%s() IotNetworkSecureSockets_Receive() received %ld\n", __func__, bytes_received);
                            result = CY_RSLT_OTA_ERROR_MALFORMED_JOB_DOC;
                            goto _callback_exit;
                        }
                    }
                    if (remainder != bytes_received)
                    {
                        IotLogError("%d:%s() did not get enough data ! received %ld wanted %ld\n", __LINE__, __func__, bytes_received, remainder);
                        result = CY_RSLT_OTA_ERROR_MALFORMED_JOB_DOC;
                        goto _callback_exit;
                    }

                    IotLogDebug("HTTP: Got Job doc ! %d bytes! \n>%.*s<", file_len, file_len, ptr);

                    /* Copy the Job document into the buffer. We will parse the job document in cy_ota_agent.c */
                    memset(ctx->job_doc, 0x00, sizeof(ctx->job_doc) );
                    memcpy(ctx->job_doc, end_of_header, file_len);
                    result = CY_RSLT_SUCCESS;
                    goto _callback_exit;
                }
                else
                {
                    /* set parameters for writing */
                    chunk_info.offset     = ctx->total_bytes_written;
                    chunk_info.buffer     = ptr;
                    chunk_info.size       = data_len;
                    chunk_info.total_size = ctx->total_image_size;
                    IotLogDebug("call cy_ota_http_write_chunk_to_flash(%p %d)\n", ptr, data_len);
                    result = cy_ota_http_write_chunk_to_flash(ctx, &chunk_info);
                    if (result == CY_RSLT_OTA_ERROR_APP_RETURNED_STOP)
                    {
                        IotLogWarn("%s() cy_ota_storage_write() returned OTA_STOP 0x%lx\n", __func__, result);
                        goto _callback_exit;
                    }
                    else if (result != CY_RSLT_SUCCESS)
                    {
                        IotLogError("%s() cy_ota_storage_write() failed 0x%lx\n", __func__, result);
                        result = CY_RSLT_OTA_ERROR_WRITE_STORAGE;
                        goto _callback_exit;
                    }
                }
            }
        }
    }
    else
    {
        IotLogWarn("%s() No active connection!", __func__);
        goto _callback_exit_no_events;
    }

_callback_exit:

    if (result == CY_RSLT_OTA_ERROR_MALFORMED_JOB_DOC)
    {
        IotLogWarn(" HTTP: CY_OTA_EVENT_MALFORMED_JOB_DOC !");
        cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_MALFORMED_JOB_DOC, 0);
    }
    else if (result == CY_RSLT_OTA_ERROR_SERVER_DROPPED)
    {
        IotLogWarn(" HTTP recv callback: CY_OTA_EVENT_DROPPED_US !");
        cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_DROPPED_US, 0);
    }
    else if (result == CY_RSLT_OTA_ERROR_WRITE_STORAGE)
    {
        IotLogWarn(" CY_OTA_EVENT_STORAGE_ERROR !");
        cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_STORAGE_ERROR, 0);
    }
    else if (result == CY_RSLT_OTA_ERROR_APP_RETURNED_STOP)
    {
        IotLogWarn(" CY_OTA_EVENT_APP_STOPPED_OTA !");
        cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_APP_STOPPED_OTA, 0);
    }
    else if (result != CY_RSLT_SUCCESS)
    {
        IotLogWarn(" CY_OTA_EVENT_DATA_FAIL ! 0x%lx", result);
        cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_DATA_FAIL, 0);
    }
    else
    {
        IotLogDebug(" CY_OTA_EVENT_GOT_DATA!");
        cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_GOT_DATA, 0);
    }

_callback_exit_no_events:

    cy_rtos_set_mutex(&ctx->sub_callback_mutex);
}

/**
 * brief Provide an asynchronous notification of network closing
 *
 * A function with this signature may be set with platform_network_function_setclosecallback
 * to be invoked when the network connection is closed.
 *
 * param[in] pConnection The connection that was closed, defined by
 * the network stack.
 * param[in] reason The reason the connection was closed
 * param[in] pContext The third argument passed to @ref platform_network_function_setclosecallback.
 */
void cy_ota_http_close_callback(IotNetworkConnection_t pConnection,
                                IotNetworkCloseReason_t reason,
                                void * pContext )
{
    cy_ota_context_t    *ctx = (cy_ota_context_t *)pContext;
    CY_OTA_CONTEXT_ASSERT(ctx);

    /* only report disconnection for HTTP connection */
    if ( ( (ctx->curr_connect_type == CY_OTA_CONNECTION_HTTP) ||
           (ctx->curr_connect_type == CY_OTA_CONNECTION_HTTPS) ) &&
           (pConnection != NULL) )
    {
        switch (reason)
        {
        case IOT_NETWORK_NOT_CLOSED:
            break;
        case IOT_NETWORK_SERVER_CLOSED:
        case IOT_NETWORK_TRANSPORT_FAILURE:
        case IOT_NETWORK_CLIENT_CLOSED:
        case IOT_NETWORK_UNKNOWN_CLOSED:
            /* Only report disconnect if we are downloading */
            if ( (ctx->curr_state == CY_OTA_STATE_JOB_DOWNLOAD) ||
                 (ctx->curr_state == CY_OTA_STATE_DATA_DOWNLOAD) ||
                 (ctx->curr_state == CY_OTA_STATE_RESULT_SEND) ||
                 (ctx->curr_state == CY_OTA_STATE_RESULT_RESPONSE) )
            {
                IotLogWarn("%s() CY_OTA_EVENT_DROPPED_US Network reason:%d state:%d !",
                        __func__,  reason, ctx->curr_state, cy_ota_get_state_string(ctx->curr_state));
                cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_DROPPED_US, 0);
            }
            break;
        }
    }
}

/**
 * @brief Connect to OTA Update server
 *
 * NOTE: Individual Network Connection type will do whatever is necessary
 *      ex: MQTT
 *          - connect
 *          HTTP
 *          - connect
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_OTA_ERROR_GENERAL
 */
cy_rslt_t cy_ota_http_connect(cy_ota_context_t *ctx)
{
    IotNetworkError_t       err;
    IotNetworkCredentials_t credentials = NULL;
    IotNetworkServerInfo_t  server;

    CY_OTA_CONTEXT_ASSERT(ctx);

    if (ctx->http.connection_from_app == true)
    {
        IotLogInfo("%s() App provided connection.\n", __func__);
        return CY_RSLT_SUCCESS;
    }

    if (ctx->http.connection != NULL)
    {
        IotLogError("%s() Already connected.\n", __func__);
        return CY_RSLT_OTA_ERROR_GENERAL;
    }

    /* determine server info and credential info */
    server = (IotNetworkServerInfo_t)&ctx->network_params.http.server;
    credentials = ctx->network_params.http.credentials;

    /* If application changed job_doc we need to use the parsed info */
    if ( (ctx->curr_state == CY_OTA_STATE_DATA_CONNECT) &&
         (ctx->network_params.use_get_job_flow == CY_OTA_JOB_FLOW)  &&
         (ctx->parsed_job.parse_result == CY_RSLT_OTA_CHANGING_SERVER) )
    {
        server = (IotNetworkServerInfo_t)&ctx->parsed_job.broker_server;
        if (ctx->callback_data.credentials != NULL)
        {
            credentials = ctx->callback_data.credentials;
        }
    }

    /*
     * IotNetworkSecureSockets_Create() assumes that you are setting up a TLS connection
     * if credentials are passed in. So we need to make sure that we are only passing
     * in credentials when we really want a TLS connection.
     */

    if (ctx->network_params.use_get_job_flow == CY_OTA_JOB_FLOW && ctx->curr_state == CY_OTA_STATE_DATA_CONNECT &&
        ctx->parsed_job.connect_type != CY_OTA_CONNECTION_HTTPS)
    {
        credentials = NULL;
    }

    if ((ctx->network_params.use_get_job_flow == CY_OTA_DIRECT_FLOW || ctx->curr_state != CY_OTA_STATE_DATA_CONNECT) &&
        (ctx->network_params.initial_connection != CY_OTA_CONNECTION_HTTPS))
    {
            credentials = NULL;
    }

    /* create the secure socket and connect to the server - this is a blocking call */
    IotLogDebug("Connecting to HTTP Server credentials:%p server:%s:%d",
               credentials, (server->pHostName == NULL) ? "None" : server->pHostName, server->port);
    err = IotNetworkSecureSockets_Create(server,
                                         credentials,
                                         &ctx->http.connection);
    if (err != IOT_NETWORK_SUCCESS)
    {
        IotLogError("%s() socket create failed %d.\n", __func__, err);
        return CY_RSLT_OTA_ERROR_CONNECT;
    }
    IotLogDebug("Connected to HTTP Server %s:%d\n", ctx->network_params.http.server.pHostName, ctx->network_params.http.server.port);

    /* set up receive data callback */
    err =  IotNetworkSecureSockets_SetReceiveCallback(ctx->http.connection, cy_ota_http_receive_callback, ctx);
    if (err != IOT_NETWORK_SUCCESS)
    {
        IotLogError("%s() SetReceiveCallback() failed %d.\n", __func__, err);
        return CY_RSLT_OTA_ERROR_CONNECT;
    }

    /* set up socket close callback */
    err = IotNetworkSecureSockets_SetCloseCallback(ctx->http.connection, cy_ota_http_close_callback, ctx);
    if (err != IOT_NETWORK_SUCCESS)
    {
        IotLogError("%s() SetCloseCallback() failed %d.\n", __func__, err);
        return CY_RSLT_OTA_ERROR_CONNECT;
    }

    return CY_RSLT_SUCCESS;
}

/**
 * @brief get the OTA job
 *
 * NOTE: Individual Network Connection type will do whatever is necessary
 *       HTTP
 *          - pull the data from the server
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_OTA_ERROR_GENERAL
 */
cy_rslt_t cy_ota_http_get_job(cy_ota_context_t *ctx)
{
    cy_rslt_t       result;
    uint32_t        req_buff_len;
    uint32_t        bytes_sent;
    uint32_t        waitfor_clear;
    cy_ota_callback_results_t   cb_result;

    CY_OTA_CONTEXT_ASSERT(ctx);

    if (cy_rtos_init_mutex(&ctx->sub_callback_mutex) != CY_RSLT_SUCCESS)
    {
        IotLogWarn("%s() sub_callback_mutex init failed\n", __func__);
        return CY_RSLT_OTA_ERROR_GET_JOB;
    }
    ctx->sub_callback_mutex_inited = 1;

    /* clear any lingering events */
    waitfor_clear = CY_OTA_EVENT_HTTP_EVENTS;
    result = cy_rtos_waitbits_event(&ctx->ota_event, &waitfor_clear, 1, 0, 1);
    if (waitfor_clear != 0)
    {
        IotLogDebug("%s() Clearing waitfor: 0x%lx", __func__, waitfor_clear);
    }

    /* Form GET request for JOB */
    memset(ctx->http.file, 0x00, sizeof(ctx->http.file));
    strncpy(ctx->http.file, ctx->network_params.http.file, (sizeof(ctx->http.file) - 1) );
    memset(ctx->http.json_doc, 0x00, sizeof(ctx->http.json_doc));
    snprintf(ctx->http.json_doc, sizeof(ctx->http.json_doc), CY_OTA_HTTP_GET_TEMPLATE,
             ctx->http.file, ctx->curr_server->pHostName, ctx->curr_server->port);

    IotLogDebug("%d : %s() CALLING CB STATE_CHANGE %s stop_OTA_session:%d ", __LINE__, __func__,
            cy_ota_get_state_string(ctx->curr_state), ctx->stop_OTA_session);

    cb_result = cy_ota_internal_call_cb(ctx, CY_OTA_REASON_STATE_CHANGE, ctx->curr_state);

    /* json_doc size may have changed during callback */
    req_buff_len = strlen(ctx->http.json_doc);

    IotLogDebug("HTTP Get Job     File After cb: %s", ctx->http.file);
    IotLogDebug("HTTP Get Job json_doc After cb: %d:%s", req_buff_len, ctx->http.json_doc);

    switch( cb_result )
    {
        default:
        case CY_OTA_CB_RSLT_OTA_CONTINUE:
            bytes_sent = IotNetworkSecureSockets_Send(ctx->http.connection, (uint8 *)ctx->http.json_doc, req_buff_len);
            if (bytes_sent != req_buff_len)
            {
                IotLogError("%s() IotNetworkSecureSockets_Send(len:0x%x) sent 0x%lx\n", __func__, req_buff_len, bytes_sent);
                result = CY_RSLT_OTA_ERROR_GET_JOB;
                goto cleanup_and_exit;
            }
            break;

        case CY_OTA_CB_RSLT_OTA_STOP:
            IotLogError("%s() App returned OTA Stop for STATE_CHANGE for JOB_DOWNLOAD", __func__);
            result = CY_RSLT_OTA_ERROR_GET_JOB;
            goto cleanup_and_exit;

        case CY_OTA_CB_RSLT_APP_SUCCESS:
            IotLogInfo("%s() App returned APP_SUCCESS for STATE_CHANGE for JOB_DOWNLOAD", __func__);
            result = CY_RSLT_SUCCESS;
            goto cleanup_and_exit;

        case CY_OTA_CB_RSLT_APP_FAILED:
            IotLogError("%s() App returned APP_FAILED for STATE_CHANGE for JOB_DOWNLOAD", __func__);
            result = CY_RSLT_OTA_ERROR_GET_JOB;
            goto cleanup_and_exit;
    }

    while (1)
    {
        uint32_t waitfor;

        /* get event */
        waitfor = CY_OTA_EVENT_HTTP_EVENTS;

        result = cy_rtos_waitbits_event(&ctx->ota_event, &waitfor, 1, 0, CY_OTA_WAIT_HTTP_EVENTS_MS);
        IotLogDebug("%s() HTTP cy_rtos_waitbits_event: 0x%lx type:%d mod:0x%lx code:%d\n", __func__, waitfor, CY_RSLT_GET_TYPE(result), CY_RSLT_GET_MODULE(result), CY_RSLT_GET_CODE(result) );

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
            IotLogDebug("%d:%s() result = CY_RSLT_OTA_ERROR_NO_UPDATE_AVAILABLE", __LINE__, __func__);
            result = CY_RSLT_OTA_NO_UPDATE_AVAILABLE;
            break;
        }

        if (waitfor & CY_OTA_EVENT_GOT_DATA)
        {
            /* If we get malformed (short) job doc, look into using
             * DATA_DONE instead of GOT_DATA
             */
            result = CY_RSLT_SUCCESS;
            break;
        }

        if (waitfor & CY_OTA_EVENT_INVALID_VERSION)
        {
            result = CY_RSLT_OTA_ERROR_INVALID_VERSION;
            break;
        }

        if (waitfor & CY_OTA_EVENT_DROPPED_US)
        {
            IotLogWarn(" HTTP JOB loop: CY_OTA_EVENT_DROPPED_US waitfor:0x%lx!", waitfor);
            result = CY_RSLT_OTA_ERROR_SERVER_DROPPED;
            break;
        }
    }   /* While 1 */

    IotLogDebug("%s() HTTP GET JOB DONE result: 0x%lx\n", __func__, result);

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
 *          HTTP
 *          - pull the data from the server
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_OTA_ERROR_GENERAL
 */
cy_rslt_t cy_ota_http_get_data(cy_ota_context_t *ctx)
{
    cy_rslt_t       result;
    uint32_t        req_buff_len;
    uint32_t        bytes_sent;
    uint32_t        waitfor_clear;
    cy_ota_callback_results_t   cb_result;

    CY_OTA_CONTEXT_ASSERT(ctx);

    IotLogDebug("%s()\n", __func__);

    if (cy_rtos_init_mutex(&ctx->sub_callback_mutex) != CY_RSLT_SUCCESS)
    {
        IotLogError("%s() sub_callback_mutex init failed\n", __func__);
        return CY_RSLT_OTA_ERROR_GET_DATA;
    }
    ctx->sub_callback_mutex_inited = 1;

    /* clear any lingering events */
    waitfor_clear = CY_OTA_EVENT_HTTP_EVENTS;
    result = cy_rtos_waitbits_event(&ctx->ota_event, &waitfor_clear, 1, 0, 1);
    if (waitfor_clear != 0)
    {
        IotLogDebug("%s() Clearing waitfor: 0x%lx", __func__, waitfor_clear);
    }

    result = cy_rtos_init_timer(&ctx->http.http_timer, CY_TIMER_TYPE_ONCE,
                        cy_ota_http_timer_callback, (cy_timer_callback_arg_t)ctx);
    if (result != CY_RSLT_SUCCESS)
    {
        /* Timer init failed */
        IotLogError("%s() Timer Create Failed!\n", __func__);
        ctx->sub_callback_mutex_inited = 0;
        cy_rtos_deinit_mutex(&ctx->sub_callback_mutex);
        return CY_RSLT_OTA_ERROR_GET_DATA;
    }

    /* Form GET request - re-use data buffer to save some RAM */
    memset(ctx->http.file, 0x00, sizeof(ctx->http.file));
    memset(ctx->http.json_doc, 0x00, sizeof(ctx->http.json_doc));
    if (ctx->network_params.use_get_job_flow == CY_OTA_DIRECT_FLOW)
    {
        /* Caller gave us the file name directly - use what is params */
        strncpy(ctx->http.file, ctx->network_params.http.file, (sizeof(ctx->http.file) - 1) );
        snprintf(ctx->http.json_doc, sizeof(ctx->http.json_doc), CY_OTA_HTTP_GET_TEMPLATE,
                ctx->http.file, ctx->curr_server->pHostName, ctx->curr_server->port);
    }
    else
    {
        /* We got the file name from the Job file.
         * The Job redirect will have already changed the current server */
        strncpy(ctx->http.file, ctx->parsed_job.file, (sizeof(ctx->http.file) - 1) );
        snprintf(ctx->http.json_doc, sizeof(ctx->http.json_doc), CY_OTA_HTTP_GET_TEMPLATE,
                ctx->parsed_job.file, ctx->curr_server->pHostName, ctx->curr_server->port);
    }
    req_buff_len = strlen(ctx->http.json_doc);

    IotLogDebug("Get Data: %d:%s:", req_buff_len, ctx->http.json_doc);

    IotLogDebug("%d : %s() CALLING CB STATE_CHANGE %s stop_OTA_session:%d ", __LINE__, __func__,
            cy_ota_get_state_string(ctx->curr_state), ctx->stop_OTA_session);

    cb_result = cy_ota_internal_call_cb(ctx, CY_OTA_REASON_STATE_CHANGE, ctx->curr_state);
    req_buff_len = strlen(ctx->http.json_doc);
    switch( cb_result )
    {
    default:
    case CY_OTA_CB_RSLT_OTA_CONTINUE:
        IotLogDebug("HTTP Data: send GET request %d: %s",req_buff_len,  ctx->http.json_doc);
        bytes_sent = IotNetworkSecureSockets_Send(ctx->http.connection, (uint8 *)ctx->http.json_doc, req_buff_len);
        if (bytes_sent != req_buff_len)
        {
            IotLogError("%s() IotNetworkSecureSockets_Send(len:0x%x) sent 0x%lx\n", __func__, req_buff_len, bytes_sent);
            result = CY_RSLT_OTA_ERROR_GET_DATA;
            goto cleanup_and_exit;
        }
        break;

    case CY_OTA_CB_RSLT_OTA_STOP:
        IotLogError("%s() App returned OTA Stop for STATE_CHANGE for DATA_DOWNLOAD", __func__);
        result = CY_RSLT_OTA_ERROR_APP_RETURNED_STOP;
        goto cleanup_and_exit;

    case CY_OTA_CB_RSLT_APP_SUCCESS:
        IotLogDebug("%s() App returned APP_SUCCESS for STATE_CHANGE for DATA_DOWNLOAD", __func__);
        result = CY_RSLT_SUCCESS;
        goto cleanup_and_exit;

    case CY_OTA_CB_RSLT_APP_FAILED:
        IotLogError("%s() App returned APP_FAILED for STATE_CHANGE for DATA_DOWNLOAD", __func__);
        result = CY_RSLT_OTA_ERROR_GET_DATA;
        goto cleanup_and_exit;
    }

    while (1)
    {
        uint32_t waitfor;

        /* get event */
        waitfor = CY_OTA_EVENT_HTTP_EVENTS;
        result = cy_rtos_waitbits_event(&ctx->ota_event, &waitfor, 1, 0, CY_OTA_WAIT_HTTP_EVENTS_MS);
        IotLogDebug("%s() HTTP cy_rtos_waitbits_event: 0x%lx type:%d mod:0x%lx code:%d\n", __func__, waitfor, CY_RSLT_GET_TYPE(result), CY_RSLT_GET_MODULE(result), CY_RSLT_GET_CODE(result) );

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
            IotLogDebug("%d:%s() result = CY_RSLT_OTA_ERROR_NO_UPDATE_AVAILABLE", __LINE__, __func__);
            result = CY_RSLT_OTA_NO_UPDATE_AVAILABLE;
            break;
        }

        if (waitfor & CY_OTA_EVENT_STORAGE_ERROR)
        {
            result = CY_RSLT_OTA_ERROR_WRITE_STORAGE;
            break;
        }

        if (waitfor & CY_OTA_EVENT_GOT_DATA)
        {
            if (ctx->packet_timeout_sec > 0 )
            {
                /* got some data - restart the download interval timer */
                IotLogDebug("%s() RESTART PACKET TIMER %ld secs\n", __func__, ctx->packet_timeout_sec);
                cy_ota_start_http_timer(ctx, ctx->packet_timeout_sec, CY_OTA_EVENT_PACKET_TIMEOUT);
            }

            if (ctx->total_bytes_written > 0 && ctx->total_bytes_written >= ctx->total_image_size)
            {
                IotLogDebug("Done writing all data! %ld of %ld\n", ctx->total_bytes_written, ctx->total_image_size);
                cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_DATA_DONE, 0);
                /* stop timer asap */
                cy_ota_stop_http_timer(ctx);
            }
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
                cy_ota_start_http_timer(ctx, ctx->packet_timeout_sec, CY_OTA_EVENT_PACKET_TIMEOUT);

                /* update our variable */
                ctx->last_num_packets_received = ctx->num_packets_received;

                continue;
            }
            IotLogWarn("OTA Timeout waiting for a packet (%d seconds), fail\n", ctx->packet_timeout_sec);
            cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_DATA_FAIL, 0);
        }

        if (waitfor & CY_OTA_EVENT_DATA_DONE)
        {
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

        if (waitfor & CY_OTA_EVENT_APP_STOPPED_OTA)
        {
            result = CY_RSLT_OTA_ERROR_APP_RETURNED_STOP;
            break;
        }

        if (waitfor & CY_OTA_EVENT_DROPPED_US)
        {
            IotLogWarn(" HTTP Data loop: CY_OTA_EVENT_DROPPED_US !");
            result = CY_RSLT_OTA_ERROR_SERVER_DROPPED;
            break;
        }
    }   /* While 1 */

    IotLogDebug("%s() HTTP GET DATA DONE result: 0x%lx\n", __func__, result);

  cleanup_and_exit:
    ctx->sub_callback_mutex_inited = 0;
    cy_rtos_deinit_mutex(&ctx->sub_callback_mutex);

    /* we completed the download, stop the timer */
    cy_ota_stop_http_timer(ctx);
    cy_rtos_deinit_timer(&ctx->http.http_timer);

    return result;
}

/**
 * @brief Open Storage area for download
 *
 * NOTE: Typically, this erases Secondary Slot
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_OTA_ERROR_GENERAL
 */
cy_rslt_t cy_ota_http_disconnect(cy_ota_context_t *ctx)
{
    cy_rslt_t           result = CY_RSLT_SUCCESS;
    IotNetworkError_t   err = IOT_NETWORK_SUCCESS;

    CY_OTA_CONTEXT_ASSERT(ctx);

    /* Only disconnect if the Application did not pass in the connection */
    if (ctx->http.connection_from_app == false)
    {
        IotNetworkConnection_t  old_conn;
        old_conn = ctx->http.connection;
        ctx->http.connection = NULL;

        /* Only disconnect if we had connected before */
        if (old_conn != NULL)
        {
            err = IotNetworkSecureSockets_Close(old_conn);
            if (err != IOT_NETWORK_SUCCESS)
            {
                IotLogError("%s() IotNetworkSecureSockets_Close() returned Error %d", __func__, result);
                result = CY_RSLT_OTA_ERROR_DISCONNECT;
            }

            err = IotNetworkSecureSockets_Destroy(old_conn);
            if (err != IOT_NETWORK_SUCCESS)
            {
                IotLogError("%s() IotNetworkSecureSockets_Destroy() returned Error %d", __func__, result);
                result = CY_RSLT_OTA_ERROR_DISCONNECT;
            }
        }
    }
    return result;
}

cy_rslt_t cy_ota_http_report_result(cy_ota_context_t *ctx, cy_rslt_t last_error)
{
    cy_rslt_t                   result = CY_RSLT_SUCCESS;
    cy_ota_callback_results_t   cb_result;
    uint32_t                    buff_len;
    uint32_t                    bytes_sent;

    sprintf(ctx->http.json_doc, CY_OTA_HTTP_RESULT_JSON,
            ( (last_error == CY_RSLT_SUCCESS) ? CY_OTA_RESULT_SUCCESS : CY_OTA_RESULT_FAILURE),
            ctx->http.file);

    IotLogDebug("%d : %s() CALLING CB STATE_CHANGE %s stop_OTA_session:%d ", __LINE__, __func__,
            cy_ota_get_state_string(ctx->curr_state), ctx->stop_OTA_session);

    cb_result = cy_ota_internal_call_cb(ctx, CY_OTA_REASON_STATE_CHANGE, ctx->curr_state);
    buff_len = strlen(ctx->http.json_doc);

    IotLogDebug("HTTP POST result     File After cb: %s", ctx->http.file);
    IotLogDebug("HTTP POST result json_doc After cb: %ld:%s",buff_len, ctx->http.json_doc);

    /* Form POST message for HTTP Server */

    sprintf((char *)ctx->http.data_buffer, CY_OTA_HTTP_POST_TEMPLATE,
            ( (last_error == CY_RSLT_SUCCESS) ? CY_OTA_RESULT_SUCCESS : CY_OTA_RESULT_FAILURE),
            buff_len, ctx->http.json_doc);
    buff_len = strlen((char *)ctx->http.data_buffer);

    switch( cb_result )
    {
    default:
    case CY_OTA_CB_RSLT_OTA_CONTINUE:
        bytes_sent = IotNetworkSecureSockets_Send(ctx->http.connection, (uint8 *)ctx->http.data_buffer, buff_len);
        if (bytes_sent != buff_len)
        {
            IotLogError("%s() IotNetworkSecureSockets_Send(len:0x%x) sent 0x%lx\n", __func__, buff_len, bytes_sent);
            result = CY_RSLT_OTA_ERROR_SENDING_RESULT;
        }
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
    }

    // TODO: STDE wait for response ?

    return result;
}
