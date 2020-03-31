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

#include "net_sockets.h"
#include "cyabs_rtos.h"

#ifdef  OTA_HTTP_SUPPORT

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

#define OTA_TCP_REQ_BUFFER_LEN  (1024)

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

/* template for HTTP GET */
static char get_request_template[] =
{
    "GET %s HTTP/1.1\r\n"
    "Host: %s:%d \r\n"
    "\r\n"
};

#define HTTP_HEADER_STR                 "HTTP/"
#define CONTENT_STRING                  "Content-Length:"
#define HTTP_HEADERS_BODY_SEPARATOR     "\r\n\r\n"

/* Used to create GET request */
char req_buffer[OTA_TCP_REQ_BUFFER_LEN];

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

cy_rslt_t cy_ota_http_parse_header( char **ptr, uint16_t *data_len, uint32_t *file_len, http_status_code_t *response_code)
{
    char    *response_status;
    char    *header_end;

    if ( (ptr == NULL) || (*ptr == NULL) ||
         (data_len == NULL) || (*data_len == 0) ||
         (file_len == NULL) ||
         (response_code == NULL) )
    {
        return CY_RSLT_MODULE_OTA_BADARG;
    }
    *response_code = HTTP_FORBIDDEN;

    /* example:
HTTP/1.1 200 Ok\r\n
Server: mini_httpd/1.23 28Dec2015\r\n
Date: Tue, 03 Mar 2020 18:49:23 GMT\r\n
Content-Type: application/octet-stream\r\n
Content-Length: 830544\r\n
\r\n\r\n
     */

//    /* sanity check *
    if (*data_len < 12)
    {
        return CY_RSLT_MODULE_OTA_NOT_A_HEADER;
    }

    /* Find the HTTP/x.x part*/
    response_status = strnstrn( *ptr, *data_len, HTTP_HEADER_STR, sizeof(HTTP_HEADER_STR) - 1 );
    if (response_status == NULL)
    {
        return CY_RSLT_MODULE_OTA_NOT_A_HEADER;
    }
    /* skip to next ' ' space character */
    response_status = strchr(response_status, ' ');
    if (response_status == NULL)
    {
        return CY_RSLT_MODULE_OTA_NOT_A_HEADER;
    }
    *response_code = (http_status_code_t)atoi(response_status + 1);

    /* Find Content-Length part*/
    response_status = strnstrn( *ptr, *data_len, CONTENT_STRING, sizeof(CONTENT_STRING) - 1);
    if (response_status == NULL)
    {
        return CY_RSLT_MODULE_OTA_NOT_A_HEADER;
    }
    response_status += sizeof(CONTENT_STRING);
    *file_len = atoi(response_status);

    /* find end of header */
    header_end = strnstrn( *ptr, *data_len, HTTP_HEADERS_BODY_SEPARATOR, sizeof(HTTP_HEADERS_BODY_SEPARATOR) - 1);
    if (header_end == NULL)
    {
        return CY_RSLT_MODULE_OTA_NOT_A_HEADER;
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
 * NOTE: Individual Network Transport type will test appropriate fields
 *
 * @param[in]  network_params   pointer to Network parameter structure
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_MODULE_OTA_ERROR
 */
cy_rslt_t cy_ota_http_validate_network_params(cy_ota_network_params_t *network_params)
{
    IotLogInfo("%s()\n", __func__);
    if(network_params == NULL)
    {
        return CY_RSLT_MODULE_OTA_BADARG;
    }

    if ( (network_params->server.pHostName == NULL) || (network_params->u.http.job_file == NULL) )
    {
        return CY_RSLT_MODULE_OTA_BADARG;
    }
    return CY_RSLT_SUCCESS;
}

/**
 * @brief Connect to OTA Update server
 *
 * NOTE: Individual Network Transport type will do whatever is necessary
 *      ex: MQTT
 *          - connect
 *          HTTP
 *          - connect
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_MODULE_OTA_ERROR
 */
cy_rslt_t cy_ota_http_connect(cy_ota_context_t *ctx)
{
    ip_addr_t server_addr;
    err_t   err;


    IotLogInfo("%s() %p 0x%lx\n", __func__, ctx, ctx->tag);
    CY_OTA_CONTEXT_ASSERT(ctx);

    if (ctx->u.http.tcp_conn != NULL)
    {
        IotLogError("%s() Already connected.\n", __func__);
        return CY_RSLT_MODULE_OTA_ERROR;
    }

    /* Create a HTTP socket */
    ctx->u.http.tcp_conn = netconn_new(NETCONN_TCP);
    if (ctx->u.http.tcp_conn == NULL)
    {
        IotLogError("Failed to create a HTTP socket\n");
        CY_ASSERT(0);
    }

    err = netconn_bind(ctx->u.http.tcp_conn, &((struct netif *)(ctx->network_params.network_interface))->ip_addr, 0);  /* any port */
    if (err != ERR_OK)
    {
        netconn_delete(ctx->u.http.tcp_conn);
        ctx->u.http.tcp_conn = NULL;
        IotLogError("netconn_bind returned error. Error: %d\n", err);
        CY_ASSERT(0);
    }

    /* TODO: DNS Lookup? convert host addr string to ip_addr struct */
    ip_addr_set_ip4_u32_val(server_addr, ipaddr_addr(ctx->curr_server.pHostName));

    err = netconn_connect(ctx->u.http.tcp_conn, &server_addr, ctx->curr_server.port);
    if (err != ERR_OK)
    {
        netconn_delete(ctx->u.http.tcp_conn);
        ctx->u.http.tcp_conn = NULL;
        IotLogError("%s() netconn_connect returned Error %d\n", __func__, err);
        return CY_RSLT_MODULE_OTA_CONNECT_ERROR;
    }

    // TODO: TLS connection

    return CY_RSLT_SUCCESS;
}

/**
 * @brief get the OTA download
 *
 * NOTE: Individual Network Transport type will do whatever is necessary
 *      ex: MQTT
 *          - subscribe to start data transfer
 *          HTTP
 *          - pull the data from the server
 *
 * @param[in]   ctx - pointer to OTA agent context @ref cy_ota_context_t
 *
 * @return  CY_RSLT_SUCCESS
 *          CY_RSLT_MODULE_OTA_ERROR
 */
cy_rslt_t cy_ota_http_get(cy_ota_context_t *ctx)
{
    cy_rslt_t       result;
    struct netbuf   *data_buffer = NULL;
    uint16_t        data_len;
    char            *ptr;
    err_t           err;
    uint8_t         done;
    uint32_t        offset;
    uint32_t        file_len;

    IotLogInfo("%s()\n", __func__);
    CY_OTA_CONTEXT_ASSERT(ctx);

    /* Form GET request */
    memset(&req_buffer, 0x00, sizeof(req_buffer));
    snprintf(req_buffer, sizeof(req_buffer), get_request_template,
            ctx->network_params.u.http.job_file, ctx->curr_server.pHostName, ctx->curr_server.port);

    /* send GET request */
    IotLogDebug("GET req:\n%s\n", req_buffer);
    err = netconn_write(ctx->u.http.tcp_conn, req_buffer, strlen(req_buffer), NETCONN_NOFLAG);
    if(err != ERR_OK)
    {
        IotLogError("%s() netconn_write returned Error %d\n", __func__, err);
        return CY_RSLT_MODULE_OTA_GET_ERROR;
    }

    /* open the storage area */
    result = cy_ota_storage_open(ctx);
    if (result != CY_RSLT_SUCCESS)
    {
        IotLogError("%s() cy_ota_storage_open() returned0x%lx\n", __func__, result);
        return result;
    }

    /* wait for data */
    done = 0;
    offset = 0;
    while (!done)
    {
        /* Read the data from the HTTP client. */
        err = netconn_recv(ctx->u.http.tcp_conn, &data_buffer);
        if (err == ERR_TIMEOUT)
        {
            IotLogError("%s() netconn_recv TIMEOUT Error %d\n", __func__, err);
            continue;
        }
        else if (err != ERR_OK)
        {
            IotLogError("%s() netconn_recv returned Error %d\n", __func__, err);
            result = CY_RSLT_MODULE_OTA_GET_ERROR;
            break;
        }
        else
        {
            cy_ota_storage_write_info_t chunk_info = { 0 };

            /* get info from the data buffer */
            err = netbuf_data(data_buffer, (void *)&ptr, &data_len);
            if ((ptr == NULL) || (data_len == 0))
            {
                IotLogError("%s() netbuf_data returned Error %d\n", __func__, err);
                result = CY_RSLT_MODULE_OTA_GET_ERROR;
                break;
            }
            IotLogDebug("netbuf_data data_buffer %p\n", data_buffer);

            if (offset == 0)
            {
                http_status_code_t response_code;
                /* first block here - check the HTTP header */
                result = cy_ota_http_parse_header(&ptr, &data_len, &file_len, &response_code);
                if (result != CY_RSLT_SUCCESS)
                {
                    /* couldn't parse the header */
                    IotLogError( "HTTP parse header fail: 0x%lx !\r\n ", result);
                    result = CY_RSLT_MODULE_OTA_GET_ERROR;
                    break;
                }
                /* ptr is moved forward, data_len adjusted to skip header */
                if (response_code < 100)
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
                    IotLogError("%s() HTTP File Length: 0x%lx (%ld)\n", __func__, chunk_info.total_size, chunk_info.total_size);
                }
                else if (response_code < 400 )
                {
                    /* 3xx (Redirection): Further action needs to be taken in order to complete the request */
                    IotLogError( "HTTP response code: %d, redirection - code needed to handle this!\r\n ", response_code);
                    result = CY_RSLT_MODULE_OTA_GET_ERROR;
                    break;
                }
                else
                {
                    /* 4xx (Client Error): The request contains bad syntax or cannot be fulfilled */
                    IotLogError( "HTTP response code: %d, ERROR!\r\n ", response_code);
                    result = CY_RSLT_MODULE_OTA_GET_ERROR;
                    break;
                }
            }

            /* set parameters for writing */
            chunk_info.offset = ctx->total_bytes_written;
            chunk_info.buffer = (uint8_t*)ptr;
            chunk_info.size   = data_len;
            result = cy_ota_storage_write(ctx, &chunk_info);
            if (result != CY_RSLT_SUCCESS)
            {
                IotLogError("%s() cy_ota_storage_write() 0x%lx\n", __func__, result);
                result = CY_RSLT_MODULE_OTA_WRITE_STORAGE_ERROR;
                break;
            }

            for(int i = 0; i < 4; i++)
            {
                IotLogDebug(" %0x", ptr[i]);
            }
            IotLogDebug("\n");

            /* free the buffer */
            IotLogDebug("Free data_buffer %p\n", data_buffer);
            netbuf_free(data_buffer);
        }

        offset += data_len;
        if (offset >= file_len)
        {
            IotLogDebug("%s() received all data off:%ld file len:%ld\n", __func__, offset, file_len);
            break;
        }

        cy_rtos_delay_milliseconds(10);
    }


    /* free a dangling buffer */
    if (data_buffer != NULL)
    {
        IotLogDebug("%d:%s() Free data_buffer %p\n", __LINE__, __func__, data_buffer);
        netbuf_free(data_buffer);
    }

    /* Close the storage area */
    result = cy_ota_storage_close(ctx);
    if (result != CY_RSLT_SUCCESS)
    {
        IotLogError(" %s() cy_ota_storage_close failed 0x%lx\n", __func__, result);
    }

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
 *          CY_RSLT_MODULE_OTA_ERROR
 */
cy_rslt_t cy_ota_http_disconnect(cy_ota_context_t *ctx)
{
    err_t err;
    cy_rslt_t       result = CY_RSLT_SUCCESS;
    struct netconn  *old_conn;

    IotLogInfo("%s()\n", __func__);
    CY_OTA_CONTEXT_ASSERT(ctx);

    old_conn = ctx->u.http.tcp_conn;
    ctx->u.http.tcp_conn = NULL;

    /* Only disconnect if we had connected before */
    if (old_conn != NULL)
    {
        err = netconn_disconnect(old_conn);
        if (err != ERR_OK)
        {
            IotLogError("%s() netconn_disconnect returned Error %d", __func__, err);
            result = CY_RSLT_MODULE_OTA_DISCONNECT_ERROR;
        }
        else
        {
            err = netconn_delete(old_conn);
            if (err != ERR_OK)
            {
                IotLogError("%s() netconn_delete returned Error %d", __func__, err);
                result = CY_RSLT_MODULE_OTA_DISCONNECT_ERROR;
            }
        }
    }

    return result;
}

#endif  /*  !OTA_HTTP_SUPPORT */
