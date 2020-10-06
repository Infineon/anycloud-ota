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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "cy_ota_api.h"
#include "cy_ota_internal.h"
#include "cy_json_parser.h"

/***********************************************************************
 *
 * defines & enums
 *
 **********************************************************************/
/* We have a callback that calls into the application, and we don't want to have a stack overflow */
#define OTA_AGENT_THREAD_STACK_SIZE    (12 * 1024)

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
 * Internal State Function call definition
 */
typedef cy_rslt_t (*cy_ota_state_function_t)(cy_ota_context_t *ctx);

/**
 * Structure for changing states based on events
 */
typedef struct {

    cy_ota_agent_state_t            curr_state;         /* search table for entries of current state        */
    bool                            send_start_cb;      /* Sometimes we need the function to send the cb    */

    cy_ota_state_function_t         state_function;     /* Function to call for this state */

    /* success reporting depends on OTA Agent or App running the function, and the return values            */
    cy_ota_agent_state_t            success_state;      /* change to this state if successful               */

    cy_rslt_t                       failure_result;     /* result value for this state failure              */
    cy_ota_agent_state_t            failure_state;      /* change to this state if failed                   */

    cy_ota_agent_state_t            app_stop_state;     /* change to this state if app requested stop or last_error != NONE      */

} cy_ota_agent_state_table_entry_t;

/***********************************************************************
 *
 * Forward Declarations
 *
 **********************************************************************/
static cy_rslt_t cy_ota_wait_for_start(cy_ota_context_t *ctx);
static cy_rslt_t cy_ota_determine_flow(cy_ota_context_t *ctx);

static cy_rslt_t cy_ota_open_filesystem(cy_ota_context_t *ctx);
static cy_rslt_t cy_ota_close_filesystem(cy_ota_context_t *ctx);

static cy_rslt_t cy_ota_connect(cy_ota_context_t *ctx);
static cy_rslt_t cy_ota_disconnect(cy_ota_context_t *ctx);

static cy_rslt_t cy_ota_job_download(cy_ota_context_t *ctx);
static cy_rslt_t cy_ota_job_parse(cy_ota_context_t *ctx);
static cy_rslt_t cy_ota_job_redirect(cy_ota_context_t *ctx);

static cy_rslt_t cy_ota_data_download(cy_ota_context_t *ctx);

static cy_rslt_t cy_ota_verify_data(cy_ota_context_t *ctx);

static cy_rslt_t cy_ota_result_redirect(cy_ota_context_t *ctx);
static cy_rslt_t cy_ota_result_send(cy_ota_context_t *ctx);

static cy_rslt_t cy_ota_complete(cy_ota_context_t *ctx);

/***********************************************************************
 *
 * Data
 *
 **********************************************************************/

/*
 * NOTE: Some states are not represented in the table, they are used in Application callback
 *       to clarify callback reason
 *       CY_OTA_STATE_NOT_INITIALIZED   - just for reporting to Application
 *       CY_OTA_STATE_INITIALIZING      - just for reporting to Application
 *       CY_OTA_STATE_AGENT_STARTED     - just for reporting to Application
 *       CY_OTA_STATE_STORAGE_WRITE     - Used when we are about to write, not a true state
 *
 */
cy_ota_agent_state_table_entry_t    cy_ota_state_table[] =
{
    /* Current State                                send_start_cb flag
     *      function call                           New State for success
     *      Failure result                          New State for failure
     *      app_stop_state
     */
        /* Wait for timer signal */
    { CY_OTA_STATE_AGENT_WAITING,                   true,
            cy_ota_wait_for_start,                  CY_OTA_STATE_START_UPDATE,
            CY_RSLT_OTA_EXITING,                    CY_OTA_STATE_EXITING,
            CY_OTA_STATE_EXITING
    },

    /* Determine if Job or Direct
     * Being a little tricky here in that cy_ota_determine_flow returns:
     * CY_RSLT_MODULE_USE_JOB_FLOW for Job Flow ( alias for CY_RSLT_SUCCESS)
     * CY_RSLT_OTA_USE_DIRECT_FLOW Direct Flow
     */
    { CY_OTA_STATE_START_UPDATE,                    true,
            cy_ota_determine_flow,                  CY_OTA_STATE_JOB_CONNECT,
            CY_RSLT_OTA_USE_DIRECT_FLOW,            CY_OTA_STATE_STORAGE_OPEN,  /* we use non-success to mean skip Job Flow */
            CY_OTA_STATE_OTA_COMPLETE
    },

    /* Get the Job */
    { CY_OTA_STATE_JOB_CONNECT,                     true,
            cy_ota_connect,                         CY_OTA_STATE_JOB_DOWNLOAD,
            CY_RSLT_OTA_ERROR_CONNECT,              CY_OTA_STATE_AGENT_WAITING, /* if we failed to connect, wait and try again */
            CY_OTA_STATE_OTA_COMPLETE
    },
    { CY_OTA_STATE_JOB_DOWNLOAD,                    false,  /* CY_OTA_REASON_STATE_CHANGE called from cy_ota_xxxx_get_job() */
            cy_ota_job_download,                    CY_OTA_STATE_JOB_DISCONNECT,
            CY_RSLT_OTA_ERROR_GET_JOB,              CY_OTA_STATE_JOB_DISCONNECT,
            CY_OTA_STATE_JOB_DISCONNECT
    },
    { CY_OTA_STATE_JOB_DISCONNECT,                  true,
            cy_ota_disconnect,                      CY_OTA_STATE_JOB_PARSE,
            CY_RSLT_OTA_ERROR_DISCONNECT,           CY_OTA_STATE_OTA_COMPLETE,      /* if error in getting job, no update avail */
            CY_OTA_STATE_OTA_COMPLETE
    },

    /* Parse the Job */
    { CY_OTA_STATE_JOB_PARSE,                           true,
            cy_ota_job_parse,                           CY_OTA_STATE_JOB_REDIRECT,
            CY_RSLT_OTA_ERROR_MALFORMED_JOB_DOC,        CY_OTA_STATE_RESULT_REDIRECT,   /* failed to parse, send result */
            CY_OTA_STATE_OTA_COMPLETE
    },
    { CY_OTA_STATE_JOB_REDIRECT,                    true,
            cy_ota_job_redirect,                    CY_OTA_STATE_STORAGE_OPEN,
            CY_RSLT_OTA_ERROR_REDIRECT,             CY_OTA_STATE_RESULT_REDIRECT,
            CY_OTA_STATE_OTA_COMPLETE
    },

    /* Open the file system */
    { CY_OTA_STATE_STORAGE_OPEN,                    true,
            cy_ota_open_filesystem,                 CY_OTA_STATE_DATA_CONNECT,
            CY_RSLT_OTA_ERROR_OPEN_STORAGE,         CY_OTA_STATE_RESULT_REDIRECT,   /* failed to open storage, send result */
            CY_OTA_STATE_OTA_COMPLETE
    },

    /* Get the Data */
    { CY_OTA_STATE_DATA_CONNECT,                    true,
            cy_ota_connect,                         CY_OTA_STATE_DATA_DOWNLOAD,
            CY_RSLT_OTA_ERROR_CONNECT,              CY_OTA_STATE_RESULT_REDIRECT,
            CY_OTA_STATE_OTA_COMPLETE
    },
    { CY_OTA_STATE_DATA_DOWNLOAD,                   false,  /* CY_OTA_REASON_STATE_CHANGE called from cy_ota_xxxx_get_job() */
            cy_ota_data_download,                   CY_OTA_STATE_DATA_DISCONNECT,
            CY_RSLT_OTA_ERROR_GET_DATA,             CY_OTA_STATE_DATA_DISCONNECT,
            CY_OTA_STATE_DATA_DISCONNECT
    },
    { CY_OTA_STATE_DATA_DISCONNECT,                 true,
            cy_ota_disconnect,                      CY_OTA_STATE_STORAGE_CLOSE,
            CY_RSLT_OTA_ERROR_DISCONNECT,           CY_OTA_STATE_STORAGE_CLOSE,
            CY_OTA_STATE_OTA_COMPLETE
    },

    /* Close the storage area */
    { CY_OTA_STATE_STORAGE_CLOSE,                   true,
            cy_ota_close_filesystem,                CY_OTA_STATE_VERIFY,
            CY_RSLT_OTA_ERROR_CLOSE_STORAGE,        CY_OTA_STATE_RESULT_REDIRECT,
            CY_OTA_STATE_OTA_COMPLETE
    },

    /* Verify the download */
    { CY_OTA_STATE_VERIFY,                          true,
            cy_ota_verify_data,                     CY_OTA_STATE_RESULT_REDIRECT,
            CY_RSLT_OTA_ERROR_VERIFY,               CY_OTA_STATE_RESULT_REDIRECT,   /* always send result */
            CY_OTA_STATE_RESULT_REDIRECT
    },

    /* Redirect for sending result */
    { CY_OTA_STATE_RESULT_REDIRECT,                 true,
            cy_ota_result_redirect,                 CY_OTA_STATE_RESULT_CONNECT,
            CY_RSLT_OTA_USE_DIRECT_FLOW,            CY_OTA_STATE_OTA_COMPLETE,
            CY_OTA_STATE_OTA_COMPLETE
    },

    /* Send the Result */
    { CY_OTA_STATE_RESULT_CONNECT,                  true,
            cy_ota_connect,                         CY_OTA_STATE_RESULT_SEND,
            CY_RSLT_OTA_ERROR_CONNECT,              CY_OTA_STATE_AGENT_WAITING,
            CY_OTA_STATE_OTA_COMPLETE
    },
    { CY_OTA_STATE_RESULT_SEND,                     false,
            cy_ota_result_send,                     CY_OTA_STATE_RESULT_DISCONNECT,
            CY_RSLT_OTA_ERROR_SENDING_RESULT,       CY_OTA_STATE_RESULT_DISCONNECT,
            CY_OTA_STATE_RESULT_DISCONNECT
    },
    { CY_OTA_STATE_RESULT_DISCONNECT,               true,
            cy_ota_disconnect,                      CY_OTA_STATE_OTA_COMPLETE,
            CY_RSLT_OTA_ERROR_DISCONNECT,           CY_OTA_STATE_OTA_COMPLETE,
            CY_OTA_STATE_OTA_COMPLETE
    },

    /* Check for reboot after result sent */
    { CY_OTA_STATE_OTA_COMPLETE,                    true,
            cy_ota_complete,                        CY_OTA_STATE_AGENT_WAITING,
            CY_RSLT_SUCCESS,                        CY_OTA_STATE_AGENT_WAITING,
            CY_OTA_STATE_AGENT_WAITING
    },

    /* exit the OTA Agent */
    { CY_OTA_STATE_EXITING,                         true,
            NULL,                                   CY_OTA_STATE_AGENT_WAITING,
            CY_RSLT_OTA_EXITING,                    CY_OTA_STATE_AGENT_WAITING,
            CY_OTA_STATE_AGENT_WAITING
    },
};
#define CY_OTA_NUM_STATE_TABLE_ENTRIES  ( sizeof(cy_ota_state_table) / sizeof(cy_ota_state_table[0]) )

const char *cy_ota_reason_strings[CY_OTA_LAST_REASON] = {
    "OTA Agent State Change.   ",
    "OTA Agent Function Success",
    "OTA Agent Function Failure",
};

const char *cy_ota_state_strings[CY_OTA_NUM_STATES] = {
    "OTA STATE Not Initialized",
    "OTA STATE Exiting",
    "OTA STATE Initializing",
    "OTA STATE Started",
    "OTA STATE Agent waiting",

    "OTA STATE Storage Open",
    "OTA STATE Storage Write",
    "OTA STATE Storage Close",

    "OTA STATE Start Update",

    "OTA STATE Connecting for Job",
    "OTA STATE Download Job",
    "OTA STATE Disconnect from Job server",

    "OTA STATE parse Job",
    "OTA STATE Job redirection",

    "OTA STATE Connecting for Data",
    "OTA STATE Downloading Data",
    "OTA STATE Disconnecting from Data server",

    "OTA STATE Verifying",

    "OTA STATE Result Redirect to initial connection.",

    "OTA STATE Connecting to send Result",
    "OTA STATE Sending Result",
    "OTA STATE wait for Result response",
    "OTA STATE Disconnect after Result response",

    "OTA STATE Session complete"
};

typedef struct
{
    cy_rslt_t   error;
    const char *string;

} cy_ota_error_string_lookup_t;
cy_ota_error_string_lookup_t cy_ota_error_strings[] =
{
    { CY_RSLT_SUCCESS, "OTA NO Errors" },
    { CY_RSLT_OTA_ERROR_UNSUPPORTED, "OTA Unsupported feature." },
    { CY_RSLT_OTA_ERROR_GENERAL, "OTA Unspecified error" },
    { CY_RSLT_OTA_ERROR_BADARG,  "OTA ERROR Bad Args" },
    { CY_RSLT_OTA_ERROR_OUT_OF_MEMORY, "ORA ERROR Out of memory" },
    { CY_RSLT_OTA_ERROR_ALREADY_STARTED, "ORA ERROR Agent already started" },
    { CY_RSLT_OTA_ERROR_MQTT_INIT, "ORA ERROR MQTT Initialization" },
    { CY_RSLT_OTA_ERROR_OPEN_STORAGE, "OTA ERROR Opening local Storage" },
    { CY_RSLT_OTA_ERROR_WRITE_STORAGE, "OTA ERROR Writing to lcoal Storage" },
    { CY_RSLT_OTA_ERROR_CLOSE_STORAGE, "OTA ERROR Closing local Storage" },
    { CY_RSLT_OTA_ERROR_CONNECT, "OTA ERROR Connecting" },
    { CY_RSLT_OTA_ERROR_DISCONNECT, "OTA ERROR Disconnecting" },
    { CY_RSLT_OTA_ERROR_REDIRECT, "OTA ERROR Redirection was bad" },
    { CY_RSLT_OTA_ERROR_SERVER_DROPPED, "OTA ERROR Server dropped connection" },
    { CY_RSLT_OTA_ERROR_MQTT_SUBSCRIBE, "OTA ERROR MQTT subscribe failed" },
    { CY_RSLT_OTA_ERROR_MQTT_PUBLISH, "OTA ERROR MQTT publish failed" },
    { CY_RSLT_OTA_ERROR_GET_JOB, "OTA ERROR Downloading Job" },
    { CY_RSLT_OTA_ERROR_GET_DATA, "OTA ERROR Downloading Data" },
    { CY_RSLT_OTA_ERROR_NOT_A_HEADER, "OTA ERROR packet does not have proper header" },
    { CY_RSLT_OTA_ERROR_NOT_A_JOB_DOC, "OTA ERROR packet not a Job document" },
    { CY_RSLT_OTA_ERROR_MALFORMED_JOB_DOC, "OTA ERROR Job document malformed" },
    { CY_RSLT_OTA_ERROR_WRONG_BOARD, "OTA ERROR Job for different board" },
    { CY_RSLT_OTA_ERROR_INVALID_VERSION, "OTA ERROR Job has invalid version" },
    { CY_RSLT_OTA_ERROR_VERIFY, "OTA ERROR OTA Image verification failure" },
    { CY_RSLT_OTA_ERROR_SENDING_RESULT, "OTA ERROR Sending Result" },
    { CY_RSLT_OTA_ERROR_APP_RETURNED_STOP, "OTA ERROR Application callback stopped OTA" },

    /* Informational */
    { CY_RSLT_OTA_EXITING, "OTA Agent exiting" },
    { CY_RSLT_OTA_ALREADY_CONNECTED, "ORA ERROR Agent already connected" },
    { CY_RSLT_OTA_CHANGING_SERVER, "OTA Is changing Server connection" },

    { CY_RSLT_OTA_USE_JOB_FLOW,  "OTA Agent use Job download flow" },
    { CY_RSLT_OTA_USE_DIRECT_FLOW, "OTA Agent use Direct data download flow" },

    { CY_RSLT_OTA_NO_UPDATE_AVAILABLE, "OTA ERROR No Update Available" },

};
#define CY_OTA_NUM_ERROR_STRINGS    (sizeof(cy_ota_error_strings)/sizeof(cy_ota_error_strings[0]) )


/***********************************************************************
 *
 * Global Variables
 *
 **********************************************************************/

/* so we only allow one instance of the ota context */
static void *ota_context_only_one;

/* hold last error so app can retrieve after OTA exits */
static cy_rslt_t cy_ota_last_error;        /**< Last OTA error                             */

/***********************************************************************
 *
 * Utility Functions
 *
 **********************************************************************/

void cy_ota_print_data(const char *buffer, uint32_t length)
{
    uint32_t i, j;
    for (i = 0 ; i < length; i+=16)
    {
        printf("0x%04lx ", i);
        for (j = 0 ; j < 16; j++)
        {
            if ( (i + j) < length)
            {
                printf("0x%02x ", buffer[ i + j]);
            }
            else
            {
                printf("     ");
            }
        }
        printf("    ");
        for (j = 0 ; j < 16; j++)
        {
            if ( (i + j) < length)
            {
                printf("%c ", (isprint((int)(buffer[i + j])) ? buffer[i + j] : '.') );
            }
            else
            {
                break;
            }
        }
        printf("\n");
    }
}

/***********************************************************************
 *
 * Callback to Application
 *
 **********************************************************************/

/**
 * @brief OTA internal Callback to Application
 *
 * @param   ctx     - OTA context
 * @param   reason  - OTA Callback reason
 * @param   state   - OTA State to report in callback
 *                      This is usually ctx->curr_state
 *                      When downloading Data and Storage Writing it will be CY_OTA_STATE_STORAGE_WRITE
 *
 * @return  return value from callback
 */
cy_ota_callback_results_t cy_ota_internal_call_cb( cy_ota_context_t *ctx,
                                                   cy_ota_cb_reason_t reason,
                                                   cy_ota_agent_state_t report_state)
{
    /* default result is for OTA Agent to continue with default functionality */
    cy_ota_callback_results_t   cb_result = CY_OTA_CB_RSLT_OTA_CONTINUE;

    CY_OTA_CONTEXT_ASSERT(ctx);
    if (ctx->agent_params.cb_func != NULL)
    {
        IotLogDebug("%s() CB reason:%d", __func__, reason);

        /* set up callback data */
        memset(&ctx->callback_data, 0x00, sizeof(ctx->callback_data));

        ctx->callback_data.reason = reason;
        ctx->callback_data.cb_arg = ctx->agent_params.cb_arg;

        ctx->callback_data.state = report_state;
        ctx->callback_data.error = cy_ota_last_error;

        /* fill in callback structure data for connection info */
        ctx->callback_data.connection_type = ctx->curr_connect_type;
        if (ctx->curr_server != NULL)
        {
            ctx->callback_data.broker_server.pHostName = ctx->curr_server->pHostName;
            ctx->callback_data.broker_server.port = ctx->curr_server->port;
        }

        /* copy connection specific fields */
        memset(ctx->callback_data.file, 0x00, sizeof(ctx->callback_data.file) );
        memset(ctx->callback_data.json_doc, 0x00, sizeof(ctx->callback_data.json_doc));
        if (ctx->callback_data.connection_type == CY_OTA_CONNECTION_MQTT)
        {
            strncpy(ctx->callback_data.json_doc, ctx->mqtt.json_doc, (sizeof(ctx->callback_data.json_doc) - 1) );
            strncpy(ctx->callback_data.unique_topic, ctx->mqtt.unique_topic, (sizeof(ctx->callback_data.unique_topic) - 1) );
            ctx->callback_data.credentials = ctx->network_params.mqtt.credentials;
        }
        else if ( (ctx->callback_data.connection_type == CY_OTA_CONNECTION_HTTP) ||
                  (ctx->callback_data.connection_type == CY_OTA_CONNECTION_HTTPS) )
        {
            strncpy(ctx->callback_data.json_doc, ctx->http.json_doc, (sizeof(ctx->callback_data.json_doc) - 1) );
            strncpy(ctx->callback_data.file, ctx->http.file, (sizeof(ctx->callback_data.file) - 1) );
            IotLogDebug("------------> cb file: '%s'    http.file'%s' params:'%s'", ctx->callback_data.file, ctx->http.file, ctx->network_params.http.file);
            if ( (ctx->curr_state == CY_OTA_STATE_DATA_CONNECT) &&
                 (ctx->network_params.use_get_job_flow == CY_OTA_JOB_FLOW) )
            {
                strncpy(ctx->callback_data.file, ctx->parsed_job.file, (sizeof(ctx->callback_data.file) - 1) );
            }
            ctx->callback_data.credentials = ctx->network_params.http.credentials;
        }

        if (ctx->curr_state == CY_OTA_STATE_JOB_PARSE)
        {
            strncpy(ctx->callback_data.json_doc, ctx->job_doc, (sizeof(ctx->callback_data.json_doc) - 1) );
        }

        /* For STORAGE info */
        ctx->callback_data.storage = ctx->storage;

        /* Total data info */
        ctx->callback_data.total_size    = ctx->total_image_size;
        ctx->callback_data.bytes_written = ctx->total_bytes_written;
        if (ctx->total_image_size > 0)
        {
           ctx->callback_data.percentage = (ctx->total_bytes_written * 100) / ctx->total_image_size;
        }

        /* call the Application Callback function */
        IotLogInfo("%s() calling OTA Callback state: %d\n", __func__, ctx->curr_state);
        cb_result = ctx->agent_params.cb_func(&ctx->callback_data);
        IotLogInfo("%s()\n                         ----> CB returned: %d\n", __func__, cb_result);

        /* copy connection specific fields */
        if (ctx->curr_state == CY_OTA_STATE_JOB_PARSE)
        {
            if (strlen(ctx->callback_data.json_doc) > 0)
            {
                memcpy(ctx->job_doc, ctx->callback_data.json_doc, sizeof(ctx->job_doc));
            }
        }
        else if(  (ctx->curr_state == CY_OTA_STATE_JOB_CONNECT) ||
                  (ctx->curr_state == CY_OTA_STATE_DATA_CONNECT) ||
                  (ctx->curr_state == CY_OTA_STATE_RESULT_CONNECT) ||
                  (ctx->curr_state == CY_OTA_STATE_JOB_DOWNLOAD) )
        {
            if (ctx->callback_data.connection_type == CY_OTA_CONNECTION_MQTT)
            {
                if (ctx->callback_data.mqtt_connection != NULL)
                {
                    /* Application is supplying the MQTT connection.
                     *   Store application's connection in mqtt.mqtt_connection.
                     * NOTE: We are not connected at this time,
                     *       saving the connection is ok.
                     */
                    ctx->mqtt.connection_from_app = true;
                    ctx->mqtt.connection_established = true;
                    ctx->mqtt.mqtt_connection = ctx->callback_data.mqtt_connection;
                }

                if ( (strlen(ctx->callback_data.json_doc) > 0) &&
                     (strcmp(ctx->mqtt.json_doc, ctx->callback_data.json_doc) != 0) )
                {
                    strncpy(ctx->mqtt.json_doc, ctx->callback_data.json_doc, (sizeof(ctx->mqtt.json_doc) - 1));
                    strncpy(ctx->job_doc, ctx->callback_data.json_doc, (sizeof(ctx->job_doc) - 1));
                }
                if ( (strlen(ctx->callback_data.unique_topic) > 0) &&
                     (strcmp(ctx->mqtt.unique_topic, ctx->callback_data.unique_topic) != 0) )
                {
                    strncpy(ctx->mqtt.unique_topic, ctx->callback_data.unique_topic, (sizeof(ctx->mqtt.unique_topic) - 1));
                }
            }
            else if ( (ctx->callback_data.connection_type == CY_OTA_CONNECTION_HTTP) ||
                      (ctx->callback_data.connection_type == CY_OTA_CONNECTION_HTTPS) )
            {
                if (ctx->callback_data.http_connection != NULL)
                {
                    /* Application is supplying the HTTP connection.
                     *   Store application's connection in http.http_connection.
                     * NOTE: We are not connected at this time,
                     *       saving the connection is ok.
                     */
                    ctx->http.connection_from_app = true;
                    ctx->http.connection = ctx->callback_data.http_connection;
                }
                if ( (strlen(ctx->callback_data.json_doc) > 0) &&
                     (strcmp(ctx->http.json_doc, ctx->callback_data.json_doc) != 0) )
                {
                    strncpy(ctx->http.json_doc, ctx->callback_data.json_doc, (sizeof(ctx->http.json_doc) - 1) );
                    strncpy(ctx->job_doc, ctx->callback_data.json_doc, (sizeof(ctx->job_doc) - 1));
                }
                if ( (strlen(ctx->callback_data.file) > 0) &&
                     (strcmp(ctx->http.file, ctx->callback_data.file) != 0) )
                {
                    strncpy(ctx->http.file, ctx->callback_data.file, (sizeof(ctx->http.file) - 1) );
                }
            } /* connection type */
        } /* if starting a connection */

        if ( cb_result == CY_OTA_CB_RSLT_APP_SUCCESS &&
             (ctx->curr_state == CY_OTA_STATE_JOB_DISCONNECT  ||
              ctx->curr_state == CY_OTA_STATE_DATA_DISCONNECT ||
              ctx->curr_state == CY_OTA_STATE_RESULT_DISCONNECT ) )
        {
            /*
             * The app has taken care of the disconnect operation. Clean up
             * our connection information.
             */

            if (ctx->callback_data.connection_type == CY_OTA_CONNECTION_MQTT)
            {
                ctx->mqtt.connection_from_app    = false;
                ctx->mqtt.connection_established = false;
                ctx->mqtt.mqtt_connection        = NULL;
            }
            else
            {
                ctx->http.connection_from_app = false;
                ctx->http.connection          = NULL;
            }
        }
    } /* called callback */

    IotLogInfo("%s(reason:%d) CB returning 0x%lx", __func__, reason, cb_result);
    return cb_result;
}

/***********************************************************************
 *
 * Miscellaneous Functions
 *
 **********************************************************************/
static void cy_ota_set_state(cy_ota_context_t *ctx, cy_ota_agent_state_t state)
{
    CY_OTA_CONTEXT_ASSERT(ctx);

    /* sanity check */
    if (state >= CY_OTA_NUM_STATES )
    {
        IotLogError("%s() BAD STATE: %d\n", __func__, state);
    }
    else
    {
        IotLogDebug("%s() state: %d\n", __func__, state);
        ctx->curr_state = state;
    }
}

static void cy_ota_set_last_error(cy_ota_context_t *ctx, cy_rslt_t error)
{
    CY_OTA_CONTEXT_ASSERT(ctx);

    if (error == CY_RSLT_SUCCESS)
    {
        if ( (ctx->curr_state <= CY_OTA_STATE_AGENT_WAITING) ||
             (ctx->curr_state == CY_OTA_STATE_START_UPDATE) ||
             (ctx->curr_state == CY_OTA_STATE_JOB_CONNECT) ||
             (ctx->curr_state == CY_OTA_STATE_DATA_CONNECT) ||
             (ctx->curr_state == CY_OTA_STATE_DATA_DOWNLOAD) ||
             (ctx->curr_state == CY_OTA_STATE_RESULT_CONNECT) )
        {
            cy_ota_last_error = CY_RSLT_SUCCESS;
        }
    }
    else if (cy_ota_last_error != CY_RSLT_OTA_ERROR_APP_RETURNED_STOP)
    {
        cy_ota_last_error = error;
    }
}

/* --------------------------------------------------------------- *
 * Timer Functions
 * --------------------------------------------------------------- */

void cy_ota_timer_callback(cy_timer_callback_arg_t arg)
{
    cy_ota_context_t *ctx = (cy_ota_context_t *)arg;
    CY_OTA_CONTEXT_ASSERT(ctx);

    if (ctx->curr_state < CY_OTA_STATE_AGENT_WAITING )
    {
        IotLogDebug("%s() Timer event with bad state: %s\n", __func__, cy_ota_get_state_string(ctx->curr_state));
    }
    else
    {
        IotLogDebug("%s() new timer event: 0x%lx\n", __func__, ctx->ota_timer_event);
        cy_rtos_setbits_event(&ctx->ota_event, ctx->ota_timer_event, 0);
    }
}

cy_rslt_t cy_ota_stop_timer(cy_ota_context_t *ctx)
{
    CY_OTA_CONTEXT_ASSERT(ctx);
    return cy_rtos_stop_timer(&ctx->ota_timer);
}

cy_rslt_t cy_ota_start_timer(cy_ota_context_t *ctx, uint32_t secs, ota_events_t event)
{
    cy_rslt_t result;
    uint32_t    num_ms = SECS_TO_MILLISECS(secs);

    CY_OTA_CONTEXT_ASSERT(ctx);

    IotLogDebug("%s() new timer event: 0x%lx\n", __func__, event);

    cy_ota_stop_timer(ctx);
    ctx->ota_timer_event = event;
    result = cy_rtos_start_timer(&ctx->ota_timer, num_ms);
    return result;
}

/* --------------------------------------------------------------- *
 * Misc Functions
 * --------------------------------------------------------------- */
cy_rslt_t cy_ota_setup_connection_type(cy_ota_context_t *ctx)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    if (ctx->curr_connect_type == CY_OTA_CONNECTION_UNKNOWN)
    {
        result = CY_RSLT_OTA_ERROR_REDIRECT;
    }
    else if (ctx->curr_state == CY_OTA_STATE_JOB_REDIRECT)
    {
        IotLogInfo("redirect:   curr: %s : %d", ctx->curr_server->pHostName, ctx->curr_server->port);
        IotLogInfo("redirect: parsed: %s : %d", ctx->parsed_job.broker_server.pHostName, ctx->parsed_job.broker_server.port);
        if ( (strcmp(ctx->curr_server->pHostName, ctx->parsed_job.broker_server.pHostName) != 0 ) ||
             (ctx->curr_server->port != ctx->parsed_job.broker_server.port) )
        {
            ctx->curr_server    = &ctx->parsed_job.broker_server;
            IotLogInfo("%s() Redirect Change to %s %s : %d", __func__,
                    (ctx->curr_connect_type == CY_OTA_CONNECTION_MQTT) ? "MQTT Broker" : "HTTP Server",
                    ctx->curr_server->pHostName, ctx->curr_server->port);
            result = CY_RSLT_OTA_CHANGING_SERVER;
        }
    }
    else
    {
        IotLogInfo("%s() connection:%d state:%s!", __func__,
                    ctx->curr_connect_type, cy_ota_get_state_string(ctx->curr_state));
        switch (ctx->curr_connect_type)
        {
        case CY_OTA_CONNECTION_UNKNOWN:
            result = CY_RSLT_OTA_ERROR_REDIRECT;
            break;

        case CY_OTA_CONNECTION_MQTT:
            if (ctx->curr_server != &ctx->network_params.mqtt.broker)
            {
                ctx->curr_server    = &ctx->network_params.mqtt.broker;
                IotLogDebug("%s() Set to MQTT Broker %s : %d", __func__,
                        ctx->curr_server->pHostName, ctx->curr_server->port);
                result = CY_RSLT_OTA_CHANGING_SERVER;
            }
            break;

        case CY_OTA_CONNECTION_HTTP:
        case CY_OTA_CONNECTION_HTTPS:
            if (ctx->curr_server != &ctx->network_params.http.server )
            {
                ctx->curr_server    = &ctx->network_params.http.server;
                IotLogDebug("%s() Set to HTTP Server %s : %d", __func__,
                        ctx->curr_server->pHostName, ctx->curr_server->port);
                result = CY_RSLT_OTA_CHANGING_SERVER;
            }
            break;
        } /* switch */
    }

    return result;
}

/** Callback function used JSON parse
 *
 * @param[in] json_object : JSON object which contains the key=value pair parsed by the JSON parser
 *
 * @return cy_rslt_t
 */
cy_rslt_t cy_OTA_JSON_callback(cy_JSON_object_t* json_object, void *arg)
{
    cy_ota_context_t *ctx = (cy_ota_context_t *)arg;
    char   * obj;
    uint8_t  obj_len;
    char     *val;
    uint16_t val_len;

    CY_OTA_CONTEXT_ASSERT(ctx);
    obj = (char *)json_object->object_string;
    obj_len = json_object->object_string_length;
    val = json_object->value;
    val_len = json_object->value_length;
    IotLogDebug("%s() name : %.*s", __func__, obj_len, obj);
    IotLogDebug("%s() value: %.*s", __func__, val_len, val);

    /* Note - memcpy is used to limit the length of the copy.
     * The whole job_info struct is cleared to 0x00 before
     * parsing.
     */
    switch (json_object->value_type)
    {
        case JSON_STRING_TYPE:
        {
            if (strncmp(obj, CY_OTA_MESSAGE_FIELD, obj_len) == 0)
            {
                if (val_len > sizeof(ctx->parsed_job.message) )
                {
                    IotLogWarn("Job parse: Message text too long!");
                    val_len = sizeof(ctx->parsed_job.message) - 1;
                }
                memcpy(ctx->parsed_job.message, val, val_len);
            }
            else if (strncmp(obj, CY_OTA_MANUF_FIELD, obj_len) == 0)
            {
                if (val_len > sizeof(ctx->parsed_job.manuf) )
                {
                    IotLogWarn("Job parse: Manufacturer name too long!");
                    val_len = sizeof(ctx->parsed_job.manuf) - 1;
                }
                memcpy(ctx->parsed_job.manuf, val, val_len);
            }
            else if (strncmp(obj, CY_OTA_MANUF_ID_FIELD, obj_len) == 0)
            {
                if (val_len > sizeof(ctx->parsed_job.manuf_id) )
                {
                    IotLogWarn("Job parse: Manufacturer ID name too long!");
                    val_len = sizeof(ctx->parsed_job.manuf_id) - 1;
                }
                memcpy(ctx->parsed_job.manuf_id, val, val_len);
            }
            else if (strncmp(obj, CY_OTA_PRODUCT_ID_FIELD, obj_len) == 0)
            {
                if (val_len > sizeof(ctx->parsed_job.product) )
                {
                    IotLogWarn("Job parse: Product Name too long!");
                    val_len = sizeof(ctx->parsed_job.product) - 1;
                }
                memcpy(ctx->parsed_job.product, val, val_len);
            }
            else if (strncmp(obj, CY_OTA_SERIAL_NUMBER_FIELD, obj_len) == 0)
            {
                if (val_len > sizeof(ctx->parsed_job.serial) )
                {
                    IotLogWarn("Job parse: Serial Number text too long!");
                    val_len = sizeof(ctx->parsed_job.serial) - 1;
                }
                memcpy(ctx->parsed_job.serial, val, val_len);
            }
            else if (strncmp(obj, CY_OTA_VERSION_FIELD, obj_len) == 0)
            {
                /* copy version string, and split into parts */
                char        *dot;
                if (val_len > sizeof(ctx->parsed_job.version) )
                {
                    IotLogWarn("Job parse: Version Number text too long!");
                    val_len = sizeof(ctx->parsed_job.version) - 1;
                }
                memcpy(ctx->parsed_job.version, val, val_len);
                /* split into parts */
                ctx->parsed_job.ver_major = atoi(ctx->parsed_job.version);
                dot = strchr(ctx->parsed_job.version, '.');
                if (dot == NULL)
                {
                    IotLogWarn("%s() OTA Job Bad Version field %.*s", __func__, val_len, val);
                    return CY_RSLT_OTA_ERROR_MALFORMED_JOB_DOC;
                }
                dot++;
                ctx->parsed_job.ver_minor = atoi(dot);
                dot = strchr(dot, '.');
                if (dot == NULL)
                {
                    IotLogWarn("%s() OTA Job Bad Version field %.*s", __func__, val_len, val);
                    return CY_RSLT_OTA_ERROR_MALFORMED_JOB_DOC;
                }
                dot++;
                ctx->parsed_job.ver_build = atoi(dot);

            }
            else if (strncmp(obj, CY_OTA_BOARD_FIELD, obj_len) == 0)
            {
                if (val_len > sizeof(ctx->parsed_job.board) )
                {
                    IotLogWarn("Job parse: Board Name too long!");
                    val_len = sizeof(ctx->parsed_job.board) - 1;
                }
                memcpy(ctx->parsed_job.board, val, val_len);
            }
            else if (strncmp(obj, CY_OTA_CONNECTION_FIELD, obj_len) == 0)
            {
                /* determine Connection type */
                if (strncmp(val, CY_OTA_MQTT_STRING, val_len) == 0)
                {
                    ctx->parsed_job.connect_type = CY_OTA_CONNECTION_MQTT;
                }
                else if (strncmp(val, CY_OTA_HTTP_STRING, val_len) == 0)
                {
                    ctx->parsed_job.connect_type = CY_OTA_CONNECTION_HTTP;
                }
                else if (strncmp(val, CY_OTA_HTTPS_STRING, val_len) == 0)
                {
                    ctx->parsed_job.connect_type = CY_OTA_CONNECTION_HTTPS;
                }
                else
                {
                    IotLogWarn("%s() OTA Job Unknown Connection Type %.*s", __func__, val_len, val);
                    return CY_RSLT_OTA_ERROR_MALFORMED_JOB_DOC;
                }
            }
            else if ( (strncmp(obj, CY_OTA_SERVER_FIELD, obj_len) == 0) ||
                      (strncmp(obj, CY_OTA_BROKER_FIELD, obj_len) == 0) )
            {
                /* only copy over new broker / server name if there is one! */
                if (val_len > 0)
                {
                    if (val_len > sizeof(ctx->parsed_job.new_host_name) )
                    {
                        IotLogWarn("Job parse: Broker / Server text too long. Increase CY_OTA_JOB_URL_BROKER_LEN!");
                        val_len = sizeof(ctx->parsed_job.new_host_name) - 1;
                    }
                    memset(ctx->parsed_job.new_host_name, 0x00, sizeof(ctx->parsed_job.new_host_name));
                    memcpy(ctx->parsed_job.new_host_name, val, val_len);
                }
            }
            else if (strncmp(obj, CY_OTA_PORT_FIELD, obj_len) == 0)
            {
                ctx->parsed_job.broker_server.port = atoi(val);
            }
            else if (strncmp(obj, CY_OTA_FILE_FIELD, obj_len) == 0)
            {
                if (val_len > sizeof(ctx->parsed_job.file) )
                {
                    IotLogWarn("Job parse: File name too long!");
                    val_len = sizeof(ctx->parsed_job.file) - 1;
                }
                memcpy(ctx->parsed_job.file, val, val_len);
            }
            else if (strncmp(obj, CY_OTA_UNIQUE_TOPIC_FIELD, obj_len) == 0)
            {
                if (val_len > sizeof(ctx->parsed_job.topic) )
                {
                    IotLogWarn("Job parse: Topic name too long!");
                    val_len = sizeof(ctx->parsed_job.topic) - 1;
                }
                memcpy(ctx->parsed_job.topic, val, val_len);
            }
        }
        break;

    case JSON_NUMBER_TYPE:
    case JSON_VALUE_TYPE:
    case JSON_ARRAY_TYPE:
    case JSON_OBJECT_TYPE:
    case JSON_BOOLEAN_TYPE:
    case JSON_NULL_TYPE:
    case UNKNOWN_JSON_TYPE:
    default:
        IotLogWarn("%s() unknown JSON value type: %d", __func__, json_object->value_type);
        break;
    }

    return CY_RSLT_SUCCESS;
}

/*-----------------------------------------------------------*/
/**
 * @brief Parse Job to find info on getting the update
 *
 * The job will tell us where to get the OTA Image data using HTTP GET
 *
 * @param[in]   ctx         - The OTA context @ref cy_ota_context_t
 * @param[in]   buffer      - payload from MQTT callback
 * @param[in]   length      - length of the buffer
 *
 * @return      CY_RSLT_SUCCESS                     - Use current broker/server - go to download
 *              CY_RSLT_OTA_CHANGING_SERVER    - Change broker/server - not an error
 *
 *              CY_RSLT_OTA_ERROR_GENERAL
 *              CY_RSLT_OTA_ERROR_BADARG
 *              CY_RSLT_OTA_ERROR_MALFORMED_JOB_DOC
 *              CY_RSLT_OTA_ERROR_INVALID_VERSION
 *              CY_RSLT_OTA_ERROR_WRONG_BOARD
 */
cy_rslt_t cy_ota_parse_job_info(cy_ota_context_t *ctx, const char *buffer, uint32_t length)
{
    cy_rslt_t   result;
    CY_OTA_CONTEXT_ASSERT(ctx);

    if ( (buffer == NULL) || (length == 0) )
    {
        return CY_RSLT_OTA_ERROR_BADARG;
    }

    /* start with clean job_info, but not the doc we just received! */
    memset(&ctx->parsed_job, 0x00, sizeof(ctx->parsed_job) );
    /* If broker / server info is "", we want to use current values, so fill in before the parse */
    memset(ctx->parsed_job.new_host_name, 0x00, sizeof(ctx->parsed_job.new_host_name));
    strncpy(ctx->parsed_job.new_host_name, ctx->curr_server->pHostName, (sizeof(ctx->parsed_job.new_host_name) - 1) );
    ctx->parsed_job.broker_server.port = ctx->curr_server->port;

    /* parse the OTA Job */
    cy_JSON_parser_register_callback( cy_OTA_JSON_callback, (void *)ctx);
    result = cy_JSON_parser( buffer, length);
    if (result != CY_RSLT_SUCCESS)
    {
        IotLogWarn("OTA Could not parse the Job JSON document! 0x%lx", result);
        cy_ota_print_data(buffer, length);
        return CY_RSLT_OTA_ERROR_MALFORMED_JOB_DOC;
    }

    /* set up our pointer into our parsed data for new "broker_server" info */
    ctx->parsed_job.broker_server.pHostName = ctx->parsed_job.new_host_name;

    IotLogInfo("\n\n Parsed OTA JSON Job document");
    IotLogInfo("  Message  : %s", ctx->parsed_job.message);
    IotLogInfo("  Manuf    : %s", ctx->parsed_job.manuf);
    IotLogInfo("  Manuf ID : %s", ctx->parsed_job.manuf_id);
    IotLogInfo("  Product  : %s", ctx->parsed_job.product);
    IotLogInfo("  Serial # : %s", ctx->parsed_job.serial);
    IotLogInfo("  Version  : %s (%d.%d.%d)", ctx->parsed_job.version, ctx->parsed_job.ver_major,
                                             ctx->parsed_job.ver_minor, ctx->parsed_job.ver_build);
    IotLogInfo("  Board    : %s", ctx->parsed_job.board);
    IotLogInfo(" Connection: %s", (ctx->parsed_job.connect_type == CY_OTA_CONNECTION_MQTT) ? CY_OTA_MQTT_STRING :
                                   (ctx->parsed_job.connect_type == CY_OTA_CONNECTION_HTTP) ? CY_OTA_HTTP_STRING :
                                   (ctx->parsed_job.connect_type == CY_OTA_CONNECTION_HTTPS) ? CY_OTA_HTTPS_STRING :
                                   "Unknown");
    if (ctx->parsed_job.connect_type == CY_OTA_CONNECTION_MQTT)
    {
        IotLogInfo("  Broker   : %s", ctx->parsed_job.broker_server.pHostName);
        IotLogInfo("  Port     : %d", ctx->parsed_job.broker_server.port);

    }
    else if ( (ctx->parsed_job.connect_type == CY_OTA_CONNECTION_HTTP) ||
              (ctx->parsed_job.connect_type == CY_OTA_CONNECTION_HTTPS) )
    {
        IotLogInfo("  Server   : %s", ctx->parsed_job.broker_server.pHostName);
        IotLogInfo("  Port     : %d", ctx->parsed_job.broker_server.port);
        IotLogInfo("  FILE     : %s", ctx->parsed_job.file);
    }
    else
    {
        IotLogError("Bad Connection Type in Job Doc : %s", ctx->parsed_job.connect_type);
        return CY_RSLT_OTA_ERROR_MALFORMED_JOB_DOC;
    }
    IotLogInfo("  Unique Topic : %s", ctx->parsed_job.topic);

    /* validate version is higher than current application */
    if ( (APP_VERSION_MAJOR > ctx->parsed_job.ver_major) ||
         ( (APP_VERSION_MAJOR == ctx->parsed_job.ver_major) &&
           ( ( ( (uint32_t)(APP_VERSION_MINOR + 1) ) >    /* fix Coverity 238370 when APP_VERSION_MINOR == 0 */
               ( (uint32_t)(ctx->parsed_job.ver_minor + 1) ) ) ) ) ||
         ( (APP_VERSION_MAJOR == ctx->parsed_job.ver_major) &&
           (APP_VERSION_MINOR == ctx->parsed_job.ver_minor) &&
           ( (uint32_t)(APP_VERSION_BUILD + 1) >=     /* fix Coverity 238370 when APP_VERSION_BUILD == 0 */
             (uint32_t)(ctx->parsed_job.ver_build + 1) ) ) )
    {
        IotLogWarn("OTA Job - Current Application version %d.%d.%d update version %d.%d.%d. Fail.",
                    APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD,
                    ctx->parsed_job.ver_major, ctx->parsed_job.ver_minor, ctx->parsed_job.ver_build);
        return CY_RSLT_OTA_ERROR_INVALID_VERSION;
    }

    /* validate kit type */
    if (strcmp(ctx->parsed_job.board, CY_TARGET_BOARD_STRING) != 0)
    {
        IotLogWarn("OTA Job - board %s does not match this kit %s.", ctx->parsed_job.board, CY_TARGET_BOARD_STRING);
        return CY_RSLT_OTA_ERROR_WRONG_BOARD;
    }

    if ( (ctx->parsed_job.connect_type == ctx->curr_connect_type) &&
         (ctx->parsed_job.broker_server.port != 0) &&
         (ctx->parsed_job.broker_server.port != ctx->curr_server->port) )
    {
        IotLogWarn("OTA Job - Switching ports from %d to %d.", ctx->curr_server->port, ctx->parsed_job.broker_server.port);
    }

    /* do we need to change Broker/Server ? */
    if ( ( (ctx->parsed_job.connect_type == ctx->curr_connect_type) &&
           (ctx->parsed_job.broker_server.pHostName != NULL) ) &&
         ( (ctx->parsed_job.broker_server.pHostName[0] == 0) ||
           (strcmp(ctx->parsed_job.broker_server.pHostName, ctx->curr_server->pHostName) == 0) ) &&
         ( (ctx->parsed_job.broker_server.port == 0) ||
           (ctx->parsed_job.broker_server.port == ctx->curr_server->port) ) )
    {
        strcpy(ctx->parsed_job.new_host_name, ctx->curr_server->pHostName);
        ctx->parsed_job.broker_server.port = ctx->curr_server->port;
        IotLogDebug("%s Use same server '%s:%d'", __func__, ctx->parsed_job.broker_server.pHostName, ctx->parsed_job.broker_server.port);
    }
    else
    {
        IotLogInfo("%s Switch server was: %s:%d", __func__, ctx->curr_server->pHostName, ctx->curr_server->port);
        IotLogInfo("%s Switch server new: %s:%d", __func__, ctx->parsed_job.broker_server.pHostName, ctx->parsed_job.broker_server.port);
        result = CY_RSLT_OTA_CHANGING_SERVER;
    }


    /* Warn if Port is unexpected for connection type */
    if (ctx->parsed_job.connect_type == CY_OTA_CONNECTION_MQTT)
    {
        if ( (ctx->parsed_job.broker_server.port != CY_OTA_MQTT_BROKER_PORT) &&
             (ctx->parsed_job.broker_server.port != CY_OTA_MQTT_BROKER_PORT_TLS) &&
             (ctx->parsed_job.broker_server.port != CY_OTA_MQTT_BROKER_PORT_TLS_CERT) )
        {
            IotLogWarn("  Check Job Doc for correct MQTT Port: %d", ctx->parsed_job.broker_server.port);
        }

    }
    else if ( (ctx->parsed_job.connect_type == CY_OTA_CONNECTION_HTTP) ||
              (ctx->parsed_job.connect_type == CY_OTA_CONNECTION_HTTPS) )
    {
        if ( (ctx->parsed_job.broker_server.port != CY_OTA_HTTP_SERVER_PORT) &&
             (ctx->parsed_job.broker_server.port != CY_OTA_HTTP_SERVER_PORT_TLS) )
        {
            IotLogWarn("  Check Job Doc for correct HTTP Port: %d", ctx->parsed_job.broker_server.port);
        }
    }

    return result;
}

/* --------------------------------------------------------------- */
static cy_rslt_t cy_ota_validate_agent_params(cy_ota_agent_params_t *agent_params)
{
        /* nothing to test at this time
         *
         * timer values tested at compile time
         *
         */

    return CY_RSLT_SUCCESS;
}

/* --------------------------------------------------------------- */
cy_rslt_t cy_ota_clear_curr_connection_info(cy_ota_context_t *ctx)
{
    CY_OTA_CONTEXT_ASSERT(ctx);

    memset(&ctx->job_doc, 0x00, sizeof(ctx->job_doc));
    memset(&ctx->parsed_job, 0x00, sizeof(ctx->parsed_job));
    ctx->mqtt.unique_topic[0] = 0;

    return CY_RSLT_SUCCESS;
}

/* --------------------------------------------------------------- */
cy_rslt_t cy_ota_clear_received_stats(cy_ota_context_t *ctx)
{
    /* sanity check */
    CY_OTA_CONTEXT_ASSERT(ctx);

    ctx->last_offset = 0;
    ctx->last_packet_received = 0;
    ctx->last_size = 0;
    ctx->total_bytes_written = 0;
    ctx->total_image_size = 0;
    ctx->total_packets = 0;

    return CY_RSLT_SUCCESS;
}

void cy_ota_start_initial_timer(cy_ota_context_t *ctx)
{
    uint32_t    secs;

    /* Use CY_OTA_INITIAL_CHECK_SECS to set timer */
    secs = ctx->initial_timer_sec;
    if (secs == 0)
    {
        secs = 1;
    }

    IotLogDebug("%s() START INITIAL TIMER %ld secs\n", __func__, ctx->initial_timer_sec);
    cy_ota_start_timer(ctx, ctx->initial_timer_sec, CY_OTA_EVENT_START_UPDATE);
}

void cy_ota_start_next_timer(cy_ota_context_t *ctx)
{
    /* Use CY_OTA_NEXT_CHECK_SECS to set timer */
    if (ctx->next_timer_sec > 0 )
    {
        IotLogDebug("%s() START NEXT TIMER %ld secs\n", __func__, ctx->next_timer_sec);
        cy_ota_start_timer(ctx, ctx->next_timer_sec, CY_OTA_EVENT_START_UPDATE);
    }
}

void cy_ota_start_retry_timer(cy_ota_context_t *ctx)
{
    if (ctx->retry_timer_sec > 0)
    {
        IotLogDebug("%s() START RETRY TIMER %ld secs\n", __func__, ctx->retry_timer_sec);
        cy_ota_start_timer(ctx, ctx->retry_timer_sec, CY_OTA_EVENT_START_UPDATE);
    }
}

/******************************************************************************
 *
 * State Machine Functions
 *
 *****************************************************************************/

static cy_rslt_t cy_ota_wait_for_start(cy_ota_context_t *ctx)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    uint32_t  waitfor;

    /* reset counters & flags */
    ctx->contact_server_retry_count = 0;
    ctx->download_retry_count = 0;
    ctx->stop_OTA_session = 0;
    cy_ota_set_last_error(ctx, CY_RSLT_SUCCESS);

    /* clear received / written info before we start */
    cy_ota_clear_curr_connection_info(ctx);

    /* For MQTT, Create a unique topic name for later use
     * Do this each time we start a session.
     */
    ctx->mqtt.unique_topic[0] = 0;
    cy_ota_mqtt_create_unique_topic(ctx);

    /* clear any old events */
    waitfor = CY_OTA_EVENT_THREAD_EVENTS;
    cy_rtos_waitbits_event(&ctx->ota_event, &waitfor, 1, 0, 1);

    while (1)
    {
        IotLogDebug("%s() Wait for timer event to start us off \n", __func__);

        /* get event */
        waitfor = CY_OTA_EVENT_THREAD_EVENTS;
        result = cy_rtos_waitbits_event(&ctx->ota_event, &waitfor, 1, 0, CY_OTA_WAIT_FOR_EVENTS_MS);
        IotLogDebug("%s() OTA Agent cy_rtos_waitbits_event: 0x%lx type:%d mod:0x%lx code:%d\n", __func__, waitfor,
                    CY_RSLT_GET_TYPE(result), CY_RSLT_GET_MODULE(result), CY_RSLT_GET_CODE(result) );

        /* We only want to act on events we are waiting on.
         * For timeouts, just loop around.
         * Each if statement needs to end with
         *  continue    - loop around again
         *  break       - which exits the agent loop
         *  nothing     - fall through to the state machine loop
         */
        if (waitfor == 0)
        {
            continue;
        }

        /* act on event */
        if (waitfor & CY_OTA_EVENT_SHUTDOWN_NOW)
        {
            cy_ota_stop_timer(ctx);
            IotLogDebug("%s() SHUTDOWN NOW \n", __func__);
            result = CY_RSLT_OTA_EXITING;
            break;
        }

        if (waitfor & CY_OTA_EVENT_START_UPDATE)
        {
            /* Start an update here ! */
            result = CY_RSLT_SUCCESS;
            break;
        }   /* CY_OTA_EVENT_START_UPDATE*/

    } /* while (1) */

    return result;
}


/* determine the "flow" */
static cy_rslt_t cy_ota_determine_flow(cy_ota_context_t *ctx)
{
    cy_rslt_t                   result = CY_RSLT_SUCCESS;

    /* possibly restore the connection */
    result = cy_ota_setup_connection_type(ctx);
    IotLogDebug("%s() cy_ota_setup_connection_type() result: 0x%lx to server %s:%d.\n", __func__,
            result, ctx->curr_server->pHostName, ctx->curr_server->port);

    if ( (result == CY_RSLT_SUCCESS) || (result == CY_RSLT_OTA_CHANGING_SERVER) )
    {
        /* changing server is ok for detecting flow */
        if (ctx->network_params.use_get_job_flow == CY_OTA_JOB_FLOW)
        {
            IotLogDebug("%s() result CY_RSLT_OTA_USE_JOB_FLOW\n", __func__);
            result = CY_RSLT_OTA_USE_JOB_FLOW;
        }
        else
        {
            IotLogDebug("%s() result CY_RSLT_OTA_USE_DIRECT_FLOW\n", __func__);
            result = CY_RSLT_OTA_USE_DIRECT_FLOW;
        }
    }

    /* Set up http.file
     * - if Job flow, App set to the name of the Job file
     * - If direct flow, App set to the name of the OTA Image file
     * */
    memset(ctx->http.file, 0x00, sizeof(ctx->http.file));
    strncpy(ctx->http.file, ctx->network_params.http.file, (sizeof(ctx->http.file) - 1) );
    IotLogDebug("%s() ctx->http.file: %s\n", __func__, ctx->http.file);

    IotLogDebug("%s() returning: 0x%lx\n", __func__, result);
    return result;
}

static cy_rslt_t cy_ota_open_filesystem(cy_ota_context_t *ctx)
{
    cy_rslt_t                   result = CY_RSLT_SUCCESS;

    /* If we are retrying to download the Data and we didn't write anything
     *   we do not need to erase the FLASH.
     */
    if ( (ctx->download_retry_count == 0) ||
         (ctx->total_bytes_written > 0) )
    {
        result = cy_ota_storage_open(ctx);
    }

    IotLogDebug("%s() returning: 0x%lx\n", __func__, result);
    return result;
}

static cy_rslt_t cy_ota_close_filesystem(cy_ota_context_t *ctx)
{
    cy_rslt_t                   result = CY_RSLT_SUCCESS;

    result = cy_ota_storage_close(ctx);

    IotLogDebug("%s() returning: 0x%lx\n", __func__, result);
    return result;
}

static cy_rslt_t cy_ota_connect(cy_ota_context_t *ctx)
{
    cy_rslt_t                   result = CY_RSLT_SUCCESS;

    /* ChecK if we are already connection */
    if (ctx->device_connected == 1)
    {
        IotLogError("%s() Already connected!\n", __func__);
        return CY_RSLT_OTA_ALREADY_CONNECTED;
    }

    /* make the connection */
    if (ctx->curr_connect_type == CY_OTA_CONNECTION_MQTT)
    {
        result = cy_ota_mqtt_connect(ctx);
    }
    else if ( (ctx->curr_connect_type == CY_OTA_CONNECTION_HTTP) ||
              (ctx->curr_connect_type == CY_OTA_CONNECTION_HTTPS) )
    {
        result = cy_ota_http_connect(ctx);
    }
    else
    {
        IotLogError("%s() CONNECT Invalid job Connection type :%d\n", __func__, ctx->curr_connect_type);
        result = CY_RSLT_OTA_ERROR_GET_JOB;
    }

    if (result != CY_RSLT_SUCCESS)
    {
        /* run disconnect for sub-systems to clean up, don't care about result */
        cy_ota_disconnect(ctx);
    }
    else
    {
        /* keep track of the connected state. */
        ctx->device_connected = 1;
    }

    IotLogDebug("%s returning: 0x%lx\n", __func__, result);
    return result;
}

static cy_rslt_t cy_ota_disconnect(cy_ota_context_t *ctx)
{
    cy_rslt_t                   result = CY_RSLT_SUCCESS;

    IotLogDebug("%d:%s() ctx->curr_state: %d", __LINE__, __func__, ctx->curr_state);

    /* we will not check for last_error or APP stopped, we always want to disconnect */
    if (ctx->curr_connect_type == CY_OTA_CONNECTION_MQTT)
    {
        result = cy_ota_mqtt_disconnect(ctx);
    }
    else if ( (ctx->curr_connect_type == CY_OTA_CONNECTION_HTTP) ||
              (ctx->curr_connect_type == CY_OTA_CONNECTION_HTTPS) )
    {
        result = cy_ota_http_disconnect(ctx);
    }

    /* We are no longer connected, clear the flag */
    ctx->device_connected = 0;

    IotLogDebug("%s returning: 0x%lx\n", __func__, result);
    return result;
}


static cy_rslt_t cy_ota_job_download(cy_ota_context_t *ctx)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    /* Use CY_OTA_JOB_CHECK_TIME_SECS to set timer for when we decide we can't get the Job */
    if (ctx->job_check_timeout_sec > 0)
    {
        IotLogDebug("\n\n%s() START DOWNLOAD CHECK TIMER %ld secs\n\n", __func__, ctx->job_check_timeout_sec);
        cy_ota_start_timer(ctx, ctx->job_check_timeout_sec, CY_OTA_EVENT_DATA_DOWNLOAD_TIMEOUT);
    }

    if (ctx->curr_connect_type == CY_OTA_CONNECTION_MQTT)
    {
        result = cy_ota_mqtt_get_job(ctx);
    }
    else if ( (ctx->curr_connect_type == CY_OTA_CONNECTION_HTTP) ||
              (ctx->curr_connect_type == CY_OTA_CONNECTION_HTTPS) )
    {
        result = cy_ota_http_get_job(ctx);
    }

    /* stop the "check for an update" timer */
    cy_ota_stop_timer(ctx);

    IotLogDebug("%s returning: 0x%lx\n", __func__, result);
    return result;
}

static cy_rslt_t cy_ota_job_parse(cy_ota_context_t *ctx)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    ctx->parsed_job.parse_result = cy_ota_parse_job_info(ctx, ctx->job_doc, strlen(ctx->job_doc));
    IotLogDebug("%s() cy_ota_parse_job_info result: 0x%lx\n", __func__, ctx->parsed_job.parse_result);

    if ( (ctx->parsed_job.parse_result != CY_RSLT_SUCCESS) &&
         (ctx->parsed_job.parse_result != CY_RSLT_OTA_CHANGING_SERVER) )
    {
        result = ctx->parsed_job.parse_result;
    }

    IotLogDebug("%s() returning: 0x%lx (parsed result:0x%lx)\n", __func__, result, ctx->parsed_job.parse_result);
    return result;
}

static cy_rslt_t cy_ota_job_redirect(cy_ota_context_t *ctx)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    IotLogInfo("%s() parse_result:0x%lx\n", __func__, ctx->parsed_job.parse_result);
    if (ctx->parsed_job.parse_result == CY_RSLT_OTA_CHANGING_SERVER)
    {
        ctx->curr_connect_type = ctx->parsed_job.connect_type;
        result = cy_ota_setup_connection_type(ctx);
        IotLogInfo("%s() JOB document redirect (result: 0x%lx )to different server %s:%d.\n", __func__,
                result, ctx->curr_server->pHostName, ctx->curr_server->port);

        if (result == CY_RSLT_OTA_CHANGING_SERVER)
        {
            result = CY_RSLT_SUCCESS;
        }

        /* We need to send a unique topic for the Publisher script
         * to send us OTA Image through MQTT Broker.
         */
        ctx->mqtt.use_unique_topic = 1;

    }
    else if (ctx->parsed_job.parse_result != CY_RSLT_SUCCESS)
    {
        IotLogWarn("%s() JOB document redirect failure.\n", __func__);
        result = CY_RSLT_OTA_ERROR_REDIRECT;
    }
    else
    {
        /* Data Broker/Server is the same as for Job
         */

        /* We need to send a unique topic for the Publisher script
         * to send us OTA Image through MQTT Broker.
         */
        ctx->mqtt.use_unique_topic = 1;
    }

    IotLogDebug("%s returning: 0x%lx\n", __func__, result);
    return result;
}

static cy_rslt_t cy_ota_data_download(cy_ota_context_t *ctx)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    /* stop the "check for an update" timer */
    cy_ota_stop_timer(ctx);

    /* Use CY_OTA_DATA_CHECK_TIME_SECS to set timer for when we decide we can't get the Job */
    if (ctx->data_check_timeout_sec > 0)
    {
        IotLogDebug("\n\n%s() START DOWNLOAD CHECK TIMER %ld secs\n\n", __func__, ctx->data_check_timeout_sec);
        cy_ota_start_timer(ctx, ctx->data_check_timeout_sec, CY_OTA_EVENT_DATA_DOWNLOAD_TIMEOUT);
    }

    /* clear received / written info before we start */
    cy_ota_clear_received_stats(ctx);

    /* get_data functions use callback */
    if (ctx->curr_connect_type == CY_OTA_CONNECTION_MQTT)
    {
        result = cy_ota_mqtt_get_data(ctx);
    }
    else if ( (ctx->curr_connect_type == CY_OTA_CONNECTION_HTTP) ||
              (ctx->curr_connect_type == CY_OTA_CONNECTION_HTTPS) )
    {
        result = cy_ota_http_get_data(ctx);
    }

    /* stop the "check for an update" timer */
    cy_ota_stop_timer(ctx);

    IotLogDebug("%s returning: 0x%lx\n", __func__, result);
    return result;
}

static cy_rslt_t cy_ota_verify_data(cy_ota_context_t *ctx)
{
    cy_rslt_t                   result = CY_RSLT_SUCCESS;

    result = cy_ota_storage_verify(ctx);
    if (result == CY_RSLT_SUCCESS)
    {
        /* set to reboot after sending result */
        ctx->reboot_after_sending_result = ctx->agent_params.reboot_upon_completion;
    }

    IotLogDebug("%s returning: 0x%lx\n", __func__, result);
    return result;
}

static cy_rslt_t cy_ota_result_redirect(cy_ota_context_t *ctx)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    /* Redirect to initial connection for reporting result if using get Job Flow */
    if (ctx->network_params.use_get_job_flow == CY_OTA_JOB_FLOW)
    {
        ctx->curr_connect_type = ctx->network_params.initial_connection;
        result = cy_ota_setup_connection_type(ctx);
    }
    else
    {
        /* we return CY_RSLT_OTA_USE_DIRECT_FLOW if we are not going to send result */
        IotLogInfo("%d %s() Direct FLOW", __LINE__, __func__);
        result = CY_RSLT_OTA_USE_DIRECT_FLOW;
    }

    return result;
}

static cy_rslt_t cy_ota_result_send(cy_ota_context_t *ctx)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    if (ctx->curr_connect_type == CY_OTA_CONNECTION_MQTT)
    {
        result = cy_ota_mqtt_report_result(ctx, cy_ota_last_error);
    }
    else if ( (ctx->curr_connect_type == CY_OTA_CONNECTION_HTTP) ||
              (ctx->curr_connect_type == CY_OTA_CONNECTION_HTTPS) )
    {
        result = cy_ota_http_report_result(ctx, cy_ota_last_error);
    }

    IotLogDebug("%s returning: 0x%lx\n", __func__, result);
    return result;
}


static cy_rslt_t cy_ota_complete(cy_ota_context_t *ctx)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    /* disconnect (if we are still connected) */
    cy_ota_disconnect(ctx);

    /* close the storage (if still open) */
    cy_ota_close_filesystem(ctx);

    if ( ( (cy_ota_last_error == CY_RSLT_SUCCESS) ||
           (cy_ota_last_error == CY_RSLT_OTA_USE_DIRECT_FLOW) ) &&
         (ctx->stop_OTA_session == 0) &&
         (ctx->reboot_after_sending_result != 0) )
    {
        /* Not really a warning, just want to make sure the message gets printed */
        IotLogWarn("%s()   RESETTING NOW !!!!\n", __func__);
        cy_rtos_delay_milliseconds(1000);
        NVIC_SystemReset();
    }

    /* start timer for the next check */
    cy_ota_start_next_timer(ctx);

    IotLogDebug("%s returning: 0x%lx\n", __func__, result);
    return result;
}

/*****************************************************************************
 * This is the main loop for the OTA Agent
 * - incorporates the State Machine
 *****************************************************************************/

static void cy_ota_agent( cy_thread_arg_t arg )
{
    cy_ota_callback_results_t   cb_result = CY_OTA_CB_RSLT_OTA_CONTINUE;
    cy_ota_context_t            *ctx = (cy_ota_context_t *)arg;
    int                         stay_in_state_loop;
    CY_OTA_CONTEXT_ASSERT(ctx);

    IotLogDebug("%s() Entered New OTA Agent Thread \n", __func__);

    /* let cy_ota_agent_start() know we are alive */
    cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_RUNNING_EXITING, 0);

    /* waiting for an event  to start us off */
    cy_ota_set_state(ctx, CY_OTA_STATE_AGENT_WAITING);

    ctx->stop_OTA_session = 0;
    cy_ota_set_last_error(ctx, CY_RSLT_SUCCESS);

    /* Start the initial timer */
    cy_ota_start_initial_timer(ctx);

    while (ctx->curr_state != CY_OTA_STATE_EXITING)
    {
        cy_rslt_t   result;

        stay_in_state_loop = 1;
        while (stay_in_state_loop && (ctx->curr_state != CY_OTA_STATE_EXITING))
        {
            int idx;
            IotLogDebug("\n\n\nStart of state machine loop: %d %s",
                        ctx->curr_state, cy_ota_get_state_string(ctx->curr_state));

            /* look through state table and find the state ! */
            for (idx = 0; idx < CY_OTA_NUM_STATE_TABLE_ENTRIES; idx++)
            {
                if (cy_ota_state_table[idx].curr_state == ctx->curr_state)
                {
                    cy_ota_agent_state_t new_state = ctx->curr_state;

                    /* We found our state! Assume success*/
                    result = CY_RSLT_SUCCESS;

                    /*  Call the App callback function */
                    cb_result = CY_OTA_CB_RSLT_OTA_CONTINUE;
                    if (cy_ota_state_table[idx].send_start_cb != false)
                    {
                        IotLogInfo("%d : %s() CALLING CB STATE_CHANGE %s stop_OTA_session:%d ", __LINE__, __func__,
                                cy_ota_get_state_string(ctx->curr_state), ctx->stop_OTA_session);
                        cb_result = cy_ota_internal_call_cb(ctx, CY_OTA_REASON_STATE_CHANGE, ctx->curr_state);
                    }
                    switch( cb_result )
                    {
                    default:
                    case CY_OTA_CB_RSLT_OTA_CONTINUE:
                        if (cy_ota_state_table[idx].state_function != NULL)
                        {
                            result = cy_ota_state_table[idx].state_function(ctx);
                            if ( (ctx->curr_state == CY_OTA_STATE_AGENT_WAITING) &&
                                 (result == CY_RSLT_OTA_EXITING) )
                            {
                                /* exit state_machine_loop, as we finished the session */
                                stay_in_state_loop = 0;
                                goto _exit_ota_agent;
                            }
                            else if ( ( (ctx->curr_state == CY_OTA_STATE_JOB_CONNECT) ||
                                   (ctx->curr_state == CY_OTA_STATE_DATA_CONNECT) ||
                                   (ctx->curr_state == CY_OTA_STATE_RESULT_CONNECT) ) &&
                                 (result == CY_RSLT_OTA_ALREADY_CONNECTED) )
                            {
                                /* If we are already connected, move along */
                                result = CY_RSLT_SUCCESS;
                            }
                        }
                        break;
                    case CY_OTA_CB_RSLT_OTA_STOP:
                        IotLogError("App callback STATE_CHANGE for state %s - App returned Stop OTA session",
                                cy_ota_get_state_string(cy_ota_state_table[idx].curr_state));
                        result = CY_RSLT_OTA_ERROR_APP_RETURNED_STOP;
                        ctx->stop_OTA_session = 1;
                        break;
                    case CY_OTA_CB_RSLT_APP_SUCCESS:
                        result = CY_RSLT_SUCCESS;
                        break;
                    case CY_OTA_CB_RSLT_APP_FAILED:
                        IotLogError("App callback STATE_CHANGE for state %s - App returned failure.",
                                cy_ota_get_state_string(cy_ota_state_table[idx].curr_state));
                        result = CY_RSLT_OTA_ERROR_APP_RETURNED_STOP;
                        break;
                    }

                    /* When we get here, either the OTA Agent function or the App function has been called.
                     * Look at the result to see if we succeeded or not.
                     */
                    if (result == CY_RSLT_SUCCESS)  /* Note: CY_RSLT_OTA_USE_JOB_FLOW == CY_RSLT_SUCCESS */
                    {
                        /* assume new success state */
                        new_state = cy_ota_state_table[idx].success_state;

                        /* call the App callback function with success if there is a reason */
                        cb_result = CY_OTA_CB_RSLT_OTA_CONTINUE;
                        cb_result = cy_ota_internal_call_cb(ctx, CY_OTA_REASON_SUCCESS, ctx->curr_state);
                        switch( cb_result )
                        {
                        default:
                        case CY_OTA_CB_RSLT_OTA_CONTINUE:
                            /* nothing to do here */
                            break;
                        case CY_OTA_CB_RSLT_OTA_STOP:
                            IotLogError("App callback SUCCESS for state %s - App returned Stop OTA session",
                                cy_ota_get_state_string(cy_ota_state_table[idx].curr_state));
                            result = CY_RSLT_OTA_ERROR_APP_RETURNED_STOP;
                            ctx->stop_OTA_session = 1;
                            break;
                        case CY_OTA_CB_RSLT_APP_SUCCESS:
                            /* nothing to do here */
                            break;
                        case CY_OTA_CB_RSLT_APP_FAILED:
                            IotLogError("App callback SUCCESS for state %s - App returned failure.",
                                cy_ota_get_state_string(cy_ota_state_table[idx].curr_state));
                            result = cy_ota_state_table[idx].failure_result;
                            break;
                        }
                    }

                    /* Here we have:
                     * - called the App CB with the start reason
                     * - possibly called the OTA Agent function
                     * - called the App CB with the success reason
                     *
                     * If we have a bad result here, we call the APP with the failure result.
                     */
                    if (result != CY_RSLT_SUCCESS)
                    {
                        /* either the complete callback or the function failed */
                        new_state = cy_ota_state_table[idx].failure_state;

                        if ( (ctx->curr_state == CY_OTA_STATE_START_UPDATE) &&
                             (result == CY_RSLT_OTA_USE_DIRECT_FLOW) )
                        {
                            /* this case means we skip sending the Result, but no error */
                            result = CY_RSLT_SUCCESS;
                            cy_ota_set_last_error(ctx, CY_RSLT_SUCCESS);
                        }
                        else if (result == CY_RSLT_OTA_ERROR_APP_RETURNED_STOP)
                        {
                            cy_ota_set_last_error(ctx, CY_RSLT_OTA_ERROR_APP_RETURNED_STOP);
                        }
                        else
                        {
                            cy_ota_set_last_error(ctx, cy_ota_state_table[idx].failure_result);
                        }
                        /* call the App callback function with failure if there is a reason */
                        cb_result = CY_OTA_CB_RSLT_OTA_CONTINUE;
                        cb_result = cy_ota_internal_call_cb(ctx, CY_OTA_REASON_FAILURE, ctx->curr_state);
                        switch( cb_result )
                        {
                        default:
                        case CY_OTA_CB_RSLT_OTA_CONTINUE:
                            /* nothing to do here */
                            break;
                        case CY_OTA_CB_RSLT_OTA_STOP:
                            IotLogError("App callback FAILURE for state %s - App returned Stop OTA session",
                                cy_ota_get_state_string(cy_ota_state_table[idx].curr_state));
                            result = CY_RSLT_OTA_ERROR_APP_RETURNED_STOP;
                            ctx->stop_OTA_session = 1;
                            break;
                        case CY_OTA_CB_RSLT_APP_SUCCESS:
                            /* don't care about success here */
                            break;
                        case CY_OTA_CB_RSLT_APP_FAILED:
                            /* nothing to do here */
                            IotLogError("App callback FAILURE for state %s - App returned failure.",
                                cy_ota_get_state_string(cy_ota_state_table[idx].curr_state));
                            break;
                        }
                    }

                    IotLogDebug("%d : %s() mid State Machine result:0x%lx   last_error:%s   curr state: %s   new state: %s",
                                __LINE__, __func__, result, cy_ota_get_error_string(cy_ota_last_error),
                                cy_ota_get_state_string(ctx->curr_state), cy_ota_get_state_string(new_state));

                    /* Check for a last_error or CY_OTA_CB_RSLT_OTA_STOP
                     */
                    if (ctx->stop_OTA_session != 0)
                    {
                        new_state = cy_ota_state_table[idx].app_stop_state;
                        IotLogWarn("%d : %s() stop_OTA_session:%d - change to state: %d %s", __LINE__, __func__,
                                    ctx->stop_OTA_session, new_state, cy_ota_get_state_string(new_state));
                    }
                    else if ( (ctx->curr_state == CY_OTA_STATE_DATA_DOWNLOAD) &&
                              (cy_ota_last_error == CY_RSLT_OTA_ERROR_GET_DATA) )
                    {
                        /* Data Download failed */
                        /* We may be heading for a data download retry. */
                        if (++ctx->download_retry_count < CY_OTA_MAX_DOWNLOAD_TRIES)
                        {
                            IotLogInfo("%d : %s() state:%s retry_count:%d", __LINE__, __func__,
                                        cy_ota_get_state_string(ctx->curr_state), ctx->download_retry_count);
                            /* We are still connected, just try to download again
                             * Always check if we need to erase the storage
                             */
                            new_state = CY_OTA_STATE_STORAGE_OPEN;
                            cy_ota_set_last_error(ctx, CY_RSLT_SUCCESS);
                            result = CY_RSLT_SUCCESS;
                        }
                    }
                    else if ( ( (ctx->curr_state == CY_OTA_STATE_JOB_CONNECT) ||
                                (ctx->curr_state == CY_OTA_STATE_DATA_CONNECT) ||
                                (ctx->curr_state == CY_OTA_STATE_RESULT_CONNECT) ) &&
                                (cy_ota_last_error == CY_RSLT_OTA_ERROR_CONNECT) )
                    {
                        /* Re-try Connect */
                        if (result == CY_RSLT_SUCCESS)
                        {
                            /* we connected, reset the retry counter */
                            ctx->contact_server_retry_count = 0;
                        }
                        else if (++ctx->contact_server_retry_count < CY_OTA_CONNECT_RETRIES)
                        {
                            /* Retry */
                            IotLogDebug("%d : %s() state:%s retry_count:%d", __LINE__, __func__,
                                        cy_ota_get_state_string(ctx->curr_state), ctx->contact_server_retry_count);
                            new_state = CY_OTA_STATE_AGENT_WAITING;
                            cy_ota_set_last_error(ctx, CY_RSLT_SUCCESS);
                            cy_ota_start_retry_timer(ctx);
                        }
                    }
                    else if ( cy_ota_last_error != CY_RSLT_SUCCESS)
                    {
                        new_state = cy_ota_state_table[idx].app_stop_state;
                        IotLogWarn("%d : %s() last_error: 0x%lx  %s - change to state: %d %s", __LINE__, __func__,
                                    cy_ota_last_error, cy_ota_get_error_string(cy_ota_last_error),
                                    new_state, cy_ota_get_state_string(new_state));
                    }

                    IotLogDebug("      End of state loop new: %d %s", new_state, cy_ota_get_state_string(new_state) );

                    cy_ota_set_state(ctx, new_state);
                    break;
                }

            } /* for state_machine_loop */

            if (idx >= CY_OTA_NUM_STATE_TABLE_ENTRIES)
            {
                IotLogWarn(">>>>> We are in a state not in the state table! state: %d %s",
                            ctx->curr_state, cy_ota_get_state_string(ctx->curr_state));

            }

        }   /* while (stay_in_state_loop && ctx->curr_state != CY_OTA_STATE_EXITING)  State Machine loop */

    }   /* while (ctx->curr_state != CY_OTA_STATE_EXITING)  Main loop*/

_exit_ota_agent:

    cy_ota_stop_timer(ctx);

    IotLogDebug("%s() CY_OTA_EVENT_RUNNING_EXITING\n", __func__);
    /* let mainline know we are exiting */
    cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_RUNNING_EXITING, 0);

    cy_rtos_exit_thread();

}

/******************************************************************************
 *
 * External Functions
 *
 *****************************************************************************/

cy_rslt_t cy_ota_agent_start(cy_ota_network_params_t *network_params, cy_ota_agent_params_t *agent_params, cy_ota_context_ptr *ctx_handle)
{
    cy_rslt_t           result = CY_RSLT_TYPE_ERROR;
    cy_ota_context_t    *ctx;
    uint32_t            waitfor;

    /* sanity checks */
    if (ctx_handle == NULL)
    {
        return CY_RSLT_OTA_ERROR_BADARG;
    }
    if ( (network_params == NULL) || (agent_params == NULL) || (ctx_handle == NULL) )
    {
        return CY_RSLT_OTA_ERROR_BADARG;
    }
    if (network_params->initial_connection == CY_OTA_CONNECTION_MQTT)
    {
        if (cy_ota_mqtt_validate_network_params(network_params) != CY_RSLT_SUCCESS)
        {
            IotLogError("%s() MQTT Network Parameters incorrect!\n", __func__);
            return CY_RSLT_OTA_ERROR_BADARG;
        }
    }
    else if ( (network_params->initial_connection == CY_OTA_CONNECTION_HTTP) ||
              (network_params->initial_connection == CY_OTA_CONNECTION_HTTPS) )
    {
        if (cy_ota_http_validate_network_params(network_params) != CY_RSLT_SUCCESS)
        {
            IotLogError("%s() HTTP Network Parameters incorrect!\n", __func__);
            return CY_RSLT_OTA_ERROR_BADARG;
        }
    }
    else
    {
        IotLogError("%s() Incorrect Network Connection (%d)!\n", __func__, network_params->initial_connection);
        return CY_RSLT_OTA_ERROR_BADARG;
    }

    if (cy_ota_validate_agent_params(agent_params) != CY_RSLT_SUCCESS)
    {
        IotLogError("%s() Agent Parameters incorrect!\n", __func__);
        return CY_RSLT_OTA_ERROR_BADARG;
    }

    if (ota_context_only_one != NULL)
    {
        IotLogError("%s() OTA context already created!\n", __func__);
        return CY_RSLT_OTA_ERROR_ALREADY_STARTED;
    }

    /* Create our OTA context structure
     * If we have any errors after this, we need to free the RAM
     *
     * set result value
     * use goto _ota_init_err;
     */
    IotLogDebug("%s() allocate OTA context 0x%x bytes!\n", __func__, sizeof(cy_ota_context_t) );
    ctx = (cy_ota_context_t *)malloc(sizeof(cy_ota_context_t) );
    if (ctx == NULL)
    {
        IotLogError("%s() Out of memory for OTA context!\n", __func__);
        goto _ota_init_err;
    }
    memset(ctx, 0x00, sizeof(cy_ota_context_t) );

    ctx->curr_state = CY_OTA_STATE_INITIALIZING;

    /* copy over the initial parameters */
    memcpy(&ctx->network_params, network_params, sizeof(cy_ota_network_params_t) );
    memcpy(&ctx->agent_params, agent_params, sizeof(cy_ota_agent_params_t) );

    /* Set up starting Broker / Server */
    ctx->curr_connect_type =ctx->network_params.initial_connection;
    result = cy_ota_setup_connection_type(ctx);
    if (result == CY_RSLT_OTA_ERROR_BADARG)
    {
        IotLogError("%s() Bad Network Connection type:%d result:0x%lx!\n", __func__, network_params->initial_connection, result);
        goto _ota_init_err;
    }

    /* timer values */
    ctx->initial_timer_sec = CY_OTA_INITIAL_CHECK_SECS;
    ctx->next_timer_sec = CY_OTA_NEXT_CHECK_INTERVAL_SECS;
    ctx->retry_timer_sec = CY_OTA_RETRY_INTERVAL_SECS;
    ctx->job_check_timeout_sec = CY_OTA_JOB_CHECK_TIME_SECS;
    ctx->data_check_timeout_sec = CY_OTA_DATA_CHECK_TIME_SECS;
    ctx->check_timeout_sec = CY_OTA_CHECK_TIME_SECS;
    ctx->packet_timeout_sec = CY_OTA_PACKET_INTERVAL_SECS;

    /* create event flags */
    result = cy_rtos_init_event(&ctx->ota_event);
    if (result != CY_RSLT_SUCCESS)
    {
        /* Event create failed */
        IotLogError("%s() Event Create Failed!\n", __func__);
        goto _ota_init_err;
    }

    /* Create timer */
   result = cy_rtos_init_timer(&ctx->ota_timer, CY_TIMER_TYPE_ONCE,
                               cy_ota_timer_callback, (cy_timer_callback_arg_t)ctx);
   if (result != CY_RSLT_SUCCESS)
   {
       /* Event create failed */
       IotLogError("%s() Timer Create Failed!\n", __func__);
       goto _ota_init_err;
   }

   /* set our tag */
   ctx->tag = CY_OTA_TAG;

   /* create OTA Agent thread */
   result = cy_rtos_create_thread(&ctx->ota_agent_thread, cy_ota_agent,
                                   "CY OTA Agent", NULL, OTA_AGENT_THREAD_STACK_SIZE,
                                   CY_RTOS_PRIORITY_NORMAL, ctx);
   if (result != CY_RSLT_SUCCESS)
   {
       /* Thread create failed */
       IotLogError("%s() OTA Agent Thread Create Failed!\n", __func__);
       goto _ota_init_err;
    }

    /* wait for signal from started thread */
    waitfor = CY_OTA_EVENT_RUNNING_EXITING;
    IotLogDebug("%s() Wait for Thread to start\n", __func__);
    result = cy_rtos_waitbits_event(&ctx->ota_event, &waitfor, 1, 1, 1000);
    if (result != CY_RSLT_SUCCESS)
    {
        /* Thread create failed ? */
        IotLogError("%s() OTA Agent Thread Create No response\n", __func__);
        goto _ota_init_err;
    }

    /* return context to user */
    *ctx_handle = ctx;
    /* keep track of it */
    ota_context_only_one = ctx;

    return CY_RSLT_SUCCESS;

_ota_init_err:

    IotLogError("%s() Init failed: 0x%lx\n", __func__, result);
    if (ctx != NULL)
    {
        cy_ota_agent_stop( (cy_ota_context_ptr *)&ctx);
    }
    *ctx_handle = NULL;

    return CY_RSLT_TYPE_ERROR;

}

/* --------------------------------------------------------------- */
cy_rslt_t cy_ota_get_update_now(cy_ota_context_ptr ota_ctxt)
{
    cy_ota_context_t *ctx = (cy_ota_context_t *)ota_ctxt;

    /* sanity check */
    if (ctx == NULL)
    {
        IotLogError("%s() BAD ARG\n", __func__);
        return CY_RSLT_OTA_ERROR_BADARG;
    }
    CY_OTA_CONTEXT_ASSERT(ctx);

    if (ctx->curr_state < CY_OTA_STATE_AGENT_WAITING )
    {
        printf("%s curr: %d   agent_waiting:%d\r\n", __func__, ctx->curr_state, CY_OTA_STATE_AGENT_WAITING);
        return CY_RSLT_OTA_ERROR_GENERAL;
    }

    if (ctx->curr_state > CY_OTA_STATE_AGENT_WAITING )
    {
        return CY_RSLT_OTA_ERROR_ALREADY_STARTED;
    }

    cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_START_UPDATE, 0);

    return CY_RSLT_SUCCESS;
}

/* --------------------------------------------------------------- */

cy_rslt_t cy_ota_agent_stop(cy_ota_context_ptr *ctx_handle)
{
    cy_rslt_t           result;
    uint32_t            waitfor;
    cy_ota_context_t    *ctx;

    /* sanity check */
    if (ctx_handle == NULL )
    {
        return CY_RSLT_OTA_ERROR_BADARG;
    }
    if (*ctx_handle == NULL)
    {
        return CY_RSLT_OTA_ERROR_BADARG;
    }

    ctx = (cy_ota_context_t *)*ctx_handle;
    CY_OTA_CONTEXT_ASSERT(ctx);

    ctx->curr_state = CY_OTA_STATE_EXITING;

    /* Signal thread to end */
    cy_rtos_setbits_event(&ctx->ota_event, CY_OTA_EVENT_SHUTDOWN_NOW, 0);

    /* wait for signal from started thread */
    waitfor = CY_OTA_EVENT_RUNNING_EXITING;
    IotLogDebug("%s() Wait for Thread to exit\n", __func__);
    result = cy_rtos_waitbits_event(&ctx->ota_event, &waitfor, 1, 1, 1000);
    if (result != CY_RSLT_SUCCESS)
    {
        /* Thread exit failed ? */
        IotLogError("%s() OTA Agent Thread Exit No response\n", __func__);
    }

    /* wait for thread to exit */
    cy_rtos_join_thread(&ctx->ota_agent_thread);

    printf("%d : %s() timer:%p\r\n", __LINE__, __func__, &ctx->ota_timer);
    /* clear timer */
    cy_rtos_deinit_timer(&ctx->ota_timer);

    /* clear events */
    cy_rtos_deinit_event(&ctx->ota_event);

    memset(ctx, 0x00, sizeof(cy_ota_context_t) );
    free(ctx);

    *ctx_handle = NULL;
    ota_context_only_one = NULL;

    IotLogDebug("%s() DONE\n", __func__);
    return CY_RSLT_SUCCESS;
}

/* --------------------------------------------------------------- */

cy_rslt_t cy_ota_get_state(cy_ota_context_ptr ctx_ptr, cy_ota_agent_state_t *state )
{
    cy_ota_context_t *ctx = (cy_ota_context_t *)ctx_ptr;
    CY_OTA_CONTEXT_ASSERT(ctx);

    /* sanity check */
    if ( (ctx == NULL) || (state == NULL) )
    {
        return CY_RSLT_OTA_ERROR_BADARG;
    }

    *state = ctx->curr_state;
    return CY_RSLT_SUCCESS;
}

/* --------------------------------------------------------------- */
cy_rslt_t cy_ota_validated(void)
{
    return cy_ota_storage_validated();
}

/* --------------------------------------------------------------- */
cy_rslt_t cy_ota_get_last_error(void)
{
    return cy_ota_last_error;
}

/* --------------------------------------------------------------- */
const char *cy_ota_get_error_string(cy_rslt_t error)
{
    int i;
    for (i = 0; i < CY_OTA_NUM_ERROR_STRINGS; i++)
    {
        if (error == cy_ota_error_strings[i].error)
        {
            return cy_ota_error_strings[i].string;
        }
    }

    return "INVALID_ARGUMENT";
}


/* --------------------------------------------------------------- */

const char *cy_ota_get_state_string(cy_ota_agent_state_t state)
{
    if (state >= CY_OTA_NUM_STATES)
    {
        return "INVALID STATE";
    }
    return cy_ota_state_strings[state];
}

/* --------------------------------------------------------------- */

const char *cy_ota_get_callback_reason_string(cy_ota_cb_reason_t reason)
{
    if (reason >= CY_OTA_LAST_REASON)
    {
        return "INVALID REASON";
    }
    return cy_ota_reason_strings[reason];
}
