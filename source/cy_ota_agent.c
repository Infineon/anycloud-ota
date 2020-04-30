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


/***********************************************************************
 *
 * defines & enums
 *
 **********************************************************************/
#define OTA_AGENT_THREAD_STACK_SIZE    (8 * 1024)       /* > 4k needed for printf()calls in agent thread */

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
 * Variables & data
 *
 **********************************************************************/
const char *cy_ota_reason_strings[CY_OTA_LAST_REASON] = {
    "OTA AGENT STARTED",

    "OTA EXCEEDED RETRIES",
    "OTA SERVER CONNECT FAILED",
    "OTA DOWNLOAD FAILED",
    "OTA SERVER DROPPED CONNECTION",
    "OTA_FLASH WRITE_FAILED",
    "OTA VERIFY FAILED",
    "OTA INVALID VERSION",

    "OTA CONNECTING",
    "OTA CONENCTED TO SERVER",
    "OTA DOWNLOAD STARTED",
    "OTA DOWNLOAD PERCENT",
    "OTA DOWNLOAD COMPLETED",
    "OTA DISCONNECTED",
    "OTA VERIFIED",
    "OTA COMPLETED",
    "NO UPDATE AVAILABLE"
};

const char *cy_ota_state_strings[CY_OTA_LAST_STATE] = {
    "OTA STATE NOT INITIALIZED",
    "OTA STATE EXITING",
    "OTA STATE INITIALIZING",
    "OTA STATE AGENT WAITING",
    "OTA STATE CONNECTING",
    "OTA STATE DOWNLOADING",
    "OTA STATE DONWLOAD COMPLETE",
    "OTA STATE DISCONNECTING",
    "OTA STATE VERIFYING"
};

const char *cy_ota_error_strings[CY_OTA_LAST_ERROR] = {
    "OTA NO ERRORS",
    "OTA ERROR CONNECTING TO SERVER",
    "OTA ERROR DOWNLOADING OTA IMAGE",
    "OTA ERROR NO UPDATE AVAILABLE",
    "OTA ERROR WRITING TO FLASH",
    "OTA ERROR OTA IMAGE VERIFICATION FAILURE",
    "OTA ERROR IMAGE HAS INVALID VERSION",
    "OTA ERROR SERVER DROPPED THE CONNECTION, OTA AGENT WILL RE-CONNECT",
    "OTA ERROR MQTT SUBSCRIBE FAILED"
};


/* Define a stack buffer so we don't require a malloc when creating thread Aligned on 8-byte boundary! */
__attribute__((aligned(CY_RTOS_ALIGNMENT))) static uint8_t cy_ota_agent_stack[OTA_AGENT_THREAD_STACK_SIZE];

/* so we only allow one instance of the ota context */
static void *ota_context_only_one;

/* hold last error so app can retrieve after OTA exits */
static cy_ota_error_t          last_error;             /**< Last OTA error                             */

/***********************************************************************
 *
 * Functions
 *
 **********************************************************************/
/**
 * @brief OTA internal Callback to User
 *
 * @param   ctx     - OTA context
 * @param   reason  - OTA Callback reason
 * @param   value   - DOWNLOAD %
 *
 * @return  N/A
 */
void cy_ota_internal_call_cb( cy_ota_context_t *ctx, cy_ota_cb_reason_t reason, uint32_t value)
{
    CY_OTA_CONTEXT_ASSERT(ctx);
    if (ctx->agent_params.cb_func != NULL)
    {
        IotLogDebug("%s() CB r:%d v:%ld\n", __func__, reason, value);
        ctx->agent_params.cb_func(reason, value, ctx->agent_params.cb_arg);
    }
}
/* --------------------------------------------------------------- */
/* --------------------------------------------------------------- */

static void cy_ota_set_state(cy_ota_context_t *ctx, cy_ota_agent_state_t state)
{
    CY_OTA_CONTEXT_ASSERT(ctx);

    /* sanity check */
    if (state >= CY_OTA_LAST_STATE )
    {
        IotLogError("%s() BAD STATE: %d\n", __func__, state);
    }
    else
    {
        IotLogDebug("%s() state: %d\n", __func__, state);
        ctx->curr_state = state;
    }
}

/* --------------------------------------------------------------- *
 * Timer Functions
 * --------------------------------------------------------------- */

void cy_ota_timer_callback(cy_timer_callback_arg_t arg)
{
    cy_ota_context_t *ctx = (cy_ota_context_t *)arg;
    CY_OTA_CONTEXT_ASSERT(ctx);

    IotLogDebug("%s() new event:%d\n", __func__, ctx->ota_timer_event);
    cy_rtos_setbits_event(&ctx->ota_event, ctx->ota_timer_event, 0);
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

    cy_ota_stop_timer(ctx);
    ctx->ota_timer_event = event;
    result = cy_rtos_start_timer(&ctx->ota_timer, num_ms);
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

cy_rslt_t cy_ota_redirect(cy_ota_context_t *ctx)
{
    CY_OTA_CONTEXT_ASSERT(ctx);

    /* Things to do on a re-direct
     * - point to new URL & port #
     * - change transport type
     *
     * TODO: What to do about credentials - are they the same for both servers?
     */
    return CY_RSLT_MODULE_OTA_ERROR;
}

/* --------------------------------------------------------------- */
cy_rslt_t cy_ota_clear_received_stats(cy_ota_context_t *ctx)
{
    /* sanity check */
    if (ctx == NULL)
    {
        IotLogError("%s() BAD ARG\n", __func__);
        return CY_RSLT_MODULE_OTA_BADARG;
    }
    CY_OTA_CONTEXT_ASSERT(ctx);

    ctx->last_offset = 0;
    ctx->last_packet_received = 0;
    ctx->last_size = 0;
    ctx->total_bytes_written = 0;
    ctx->total_image_size = 0;
    ctx->total_packets = 0;

    return CY_RSLT_SUCCESS;
}
/* --------------------------------------------------------------- */

cy_rslt_t cy_ota_get_update_now(cy_ota_context_ptr ota_ctxt)
{
    cy_ota_context_t *ctx = (cy_ota_context_t *)ota_ctxt;

    /* sanity check */
    if (ctx == NULL)
    {
        IotLogError("%s() BAD ARG\n", __func__);
        return CY_RSLT_MODULE_OTA_BADARG;
    }
    CY_OTA_CONTEXT_ASSERT(ctx);

    if (ctx->curr_state < CY_OTA_STATE_AGENT_WAITING )
    {
        return CY_RSLT_MODULE_OTA_ERROR;
    }

    if (ctx->curr_state > CY_OTA_STATE_AGENT_WAITING )
    {
        return CY_RSLT_MODULE_OTA_ALREADY_STARTED;
    }

    cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_START_UPDATE, 0);

    return CY_RSLT_SUCCESS;
}

/* --------------------------------------------------------------- */

static void cy_ota_agent( cy_thread_arg_t arg )
{
    cy_ota_context_t *ctx = (cy_ota_context_t *)arg;
    CY_OTA_CONTEXT_ASSERT(ctx);

    IotLogDebug("%s() Entered OTA Agent Thread \n", __func__);

    /* let cy_ota_agent_start() know we are alive */
    cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_RUNNING, 0);

    /* waiting for an event */
    last_error = OTA_ERROR_NONE;
    cy_ota_set_state(ctx, CY_OTA_STATE_AGENT_WAITING);

    while(1)
    {
        uint32_t    waitfor;
        cy_rslt_t   result;

        /* get event */
        waitfor = OTA_EVENT_AGENT_THREAD_EVENTS;
        result = cy_rtos_waitbits_event(&ctx->ota_event, &waitfor, 1, 0, CY_OTA_WAIT_FOR_EVENTS_MS);
        IotLogDebug("%s() MQTT cy_rtos_waitbits_event: 0x%lx type:%d mod:0x%lx code:%d\n", __func__, waitfor,
                    CY_RSLT_GET_TYPE(result), CY_RSLT_GET_MODULE(result), CY_RSLT_GET_CODE(result) );

        /* We only want to act on events we are waiting on.
         * For timeouts, just loop around.
         */
        if (waitfor == 0)
        {
            continue;
        }

        /* act on event */
        if (waitfor & OTA_EVENT_AGENT_SHUTDOWN_NOW)
        {
            cy_ota_stop_timer(ctx);
            IotLogDebug("%s() SHUTDOWN NOW \n", __func__);
            break;
        }

        if (waitfor & OTA_EVENT_AGENT_START_INITIAL_TIMER)
        {
            /* clear retries and error state */
            ctx->ota_retries = 0;
            last_error = OTA_ERROR_NONE;
            cy_ota_set_state(ctx, CY_OTA_STATE_AGENT_WAITING);

            /* Use CY_OTA_INITIAL_CHECK_SECS to set timer */
            IotLogDebug("%s() START INITIAL TIMER %ld secs\n", __func__, ctx->initial_timer_sec);
            cy_ota_start_timer(ctx, ctx->initial_timer_sec, OTA_EVENT_AGENT_START_UPDATE);
            continue;
        }
        if (waitfor & OTA_EVENT_AGENT_START_NEXT_TIMER)
        {
            /* clear retries and error state */
            ctx->ota_retries = 0;
            last_error = OTA_ERROR_NONE;
            cy_ota_set_state(ctx, CY_OTA_STATE_AGENT_WAITING);

            if (ctx->next_timer_sec > 0 )
            {
                IotLogDebug("%s() START NEXT TIMER %ld secs\n", __func__, ctx->next_timer_sec);
                cy_ota_start_timer(ctx, ctx->next_timer_sec, OTA_EVENT_AGENT_START_UPDATE);
            }
            continue;
        }
        if (waitfor & OTA_EVENT_AGENT_START_RETRY_TIMER)
        {
            /* clear error state */
            last_error = OTA_ERROR_NONE;
            cy_ota_set_state(ctx, CY_OTA_STATE_AGENT_WAITING);

            if (ctx->ota_retries++ < CY_OTA_RETRIES)
            {
                /* Use CY_OTA_RETRY_CHECK_INTERVAL_SECS to set timer */
                IotLogDebug("%s() START RETRY TIMER %ld secs\n", __func__, ctx->retry_timer_sec);
                cy_ota_start_timer(ctx, ctx->retry_timer_sec, OTA_EVENT_AGENT_START_UPDATE);
            }
            else
            {
                cy_ota_internal_call_cb(ctx, CY_OTA_REASON_EXCEEDED_RETRIES, 0);
                /* wait normal next interval time to re-start trying */
                cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_START_NEXT_TIMER, 0);
            }
            continue;
        }
        if (waitfor & OTA_EVENT_AGENT_START_UPDATE)
        {
            /* reset counters */
            ctx->contact_server_retry_count = 0;
            ctx->download_retry_count = 0;

            /* open before we connect - this takes ~17 secs, we don't want to interfere with anything */
            cy_ota_storage_open(ctx);

            IotLogDebug("%s() send event OTA_EVENT_AGENT_CONNECT\n", __func__);
            /* tell ourselves to start the process by connecting to the server */
            cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_CONNECT, 0);
            continue;
        }
        if (waitfor & OTA_EVENT_AGENT_CONNECT)
        {
            /* clear lst error  */
            last_error = OTA_ERROR_NONE;

            cy_ota_set_state(ctx, CY_OTA_STATE_CONNECTING);
            cy_ota_internal_call_cb(ctx, CY_OTA_REASON_CONNECTING, 0);

            IotLogDebug("%s() call cy_ota_connect(%p, 0x%lx)\n", __func__, ctx, ctx->tag);
            if (ctx->curr_transport == CY_OTA_TRANSPORT_MQTT)
            {
                result = cy_ota_mqtt_connect(ctx);
            }
#ifdef OTA_HTTP_SUPPORT
            else if (ctx->curr_transport == CY_OTA_TRANSPORT_HTTP)
            {
                result = cy_ota_http_connect(ctx);
            }
#endif
            else
            {
                IotLogError("%s() CONNECT Invalid job transport type :%d\n", __func__, ctx->curr_transport);
            }
            if (result != CY_RSLT_SUCCESS)
            {
                /* set error info */
                last_error = OTA_ERROR_CONNECTING;
                cy_ota_internal_call_cb(ctx, CY_OTA_REASON_SERVER_CONNECT_FAILED, 0);

                /* server connect failed */
                /* run disconnect for sub-systems to clean up, don't care about result */
                cy_ota_set_state(ctx, CY_OTA_STATE_DISCONNECTING);
                if (ctx->curr_transport == CY_OTA_TRANSPORT_MQTT)
                {
                    (void)cy_ota_mqtt_disconnect(ctx);
                }

    #ifdef OTA_HTTP_SUPPORT
                if (ctx->curr_transport == CY_OTA_TRANSPORT_HTTP)
                {
                    (void)cy_ota_http_disconnect(ctx);
                }
    #endif
                cy_ota_internal_call_cb(ctx, CY_OTA_REASON_DISCONNECTED, 0);

                /* check to retry without completely bailing */
                if (++ctx->contact_server_retry_count < CY_OTA_CONNECT_RETRIES)
                {
                    /* try again */
                    IotLogDebug("%s() Retry connect: send event OTA_EVENT_AGENT_CONNECT\n", __func__);
                    cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_CONNECT, 0);
                }
                else
                {
                    /* wait until retry interval and retry */
                    IotLogDebug("%s() Exceeded contact_server_retry_count: send event OTA_EVENT_AGENT_START_RETRY_TIMER\n", __func__);
                    cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_START_RETRY_TIMER, 0);
                }
                cy_ota_set_state(ctx, CY_OTA_STATE_AGENT_WAITING);
                continue;
            }
            else
            {
                /* server connect succeeded, clear error */
                last_error = OTA_ERROR_NONE;

                cy_ota_internal_call_cb(ctx, CY_OTA_REASON_CONENCTED_TO_SERVER, 0);

                IotLogDebug("%s() send event OTA_EVENT_AGENT_DOWNLOAD\n", __func__);
                /* tell ourselves to download the job */
                cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_DOWNLOAD, 0);

                /* Use CY_OTA_CHECK_TIME_SECS to set timer for when we stop checking */
                if (ctx->check_timeout_sec > 0)
                {
                    IotLogDebug("%s() START DOWNLOAD CHECK TIMER %ld secs\n", __func__, ctx->check_timeout_sec);
                    cy_ota_start_timer(ctx, ctx->check_timeout_sec, OTA_EVENT_AGENT_DOWNLOAD_TIMEOUT);
                }
            }
            continue;
        }

        if (waitfor & OTA_EVENT_AGENT_DOWNLOAD)
        {
            /* clear last error */
            last_error = OTA_ERROR_NONE;

            cy_ota_set_state(ctx, CY_OTA_STATE_DOWNLOADING);
            cy_ota_internal_call_cb(ctx, CY_OTA_REASON_DOWNLOAD_STARTED, 0);

            /* clear received / written info before we start */
            cy_ota_clear_received_stats(ctx);

            if (ctx->curr_transport == CY_OTA_TRANSPORT_MQTT)
            {
                result = cy_ota_mqtt_get(ctx);
            }
#ifdef OTA_HTTP_SUPPORT
            else if (ctx->curr_transport == CY_OTA_TRANSPORT_HTTP)
            {
                result = cy_ota_http_get(ctx);
            }
#endif

            /* close storage doesn't matter if we succeeded or failed */
            cy_ota_storage_close(ctx);

            if (result != CY_RSLT_SUCCESS)
            {
                /* set error info */
                last_error = OTA_ERROR_DOWNLOADING;

                IotLogDebug("%s() CY_OTA_REASON_DOWNLOAD_FAILED : retries:%d\n", __func__, ctx->download_retry_count);

                /* download failed */
                if (result == CY_RSLT_MODULE_OTA_WRITE_STORAGE_ERROR)
                {
                    /* set error info */
                    last_error = OTA_ERROR_WRITING_TO_FLASH;

                    /* no more retries if there was a storage error */
                    cy_ota_set_state(ctx, CY_OTA_STATE_DISCONNECTING);
                    cy_ota_internal_call_cb(ctx, CY_OTA_REASON_OTA_FLASH_WRITE_ERROR, 0);
                    cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_DISCONNECT, 0);
                }
                else if (result == CY_RSLT_MODULE_OTA_INVALID_VERSION)
                {
                    /* set error info */
                    last_error = OTA_ERROR_INVALID_VERSION;

                    /* no more retries if the version # is invalid */
                    cy_ota_set_state(ctx, CY_OTA_STATE_DISCONNECTING);
                    cy_ota_internal_call_cb(ctx, CY_OTA_REASON_OTA_INVALID_VERSION, 0);
                    cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_DISCONNECT, 0);
                }
                else if (result == CY_RSLT_MODULE_OTA_MQTT_DROPPED_CNCT)
                {
                    /* MQTT Broker didn't answer ping, just try again
                     * Disconnect/reconnect. Do not increase retry count.
                     */
                    /* set error info */
                    last_error = OTA_ERROR_SERVER_DROPPED;

                    if ( ctx->network_params.u.mqtt.app_mqtt_connection != NULL)
                    {
                        /* The app provided an MQTT connection, exit
                         * the OTA Agent thread.
                         */
                        cy_ota_internal_call_cb(ctx, CY_OTA_REASON_SERVER_DROPPED_US, 0);
                        IotLogWarn("%s() Exiting OTA Agent - App created MQTT Connection and we got disconnected.\n", __func__);
                        break;
                    }
                    else
                    {
                        /* If OTA Agent made the MQTT connection, re-try */
                        cy_ota_set_state(ctx, CY_OTA_STATE_AGENT_WAITING);
                        cy_ota_internal_call_cb(ctx, CY_OTA_REASON_SERVER_DROPPED_US, 0);
                        cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_CONNECT, 0 );
                    }
                }
                else if (result == CY_RSLT_MODULE_NO_UPDATE_AVAILABLE)
                {
                    /* No update available */
                    cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_DOWNLOAD_TIMEOUT, 0);
                }
                else
                {
                    if (++ctx->download_retry_count < CY_OTA_MAX_DOWNLOAD_TRIES)
                    {
                        /* open again before retrying the download */
                        cy_ota_storage_open(ctx);

                        /* retry */
                        cy_ota_set_state(ctx, CY_OTA_STATE_DOWNLOADING);
                        cy_ota_internal_call_cb(ctx, CY_OTA_REASON_DOWNLOAD_FAILED, 0);
                        cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_DOWNLOAD, 0);
                    }
                    else
                    {
                            /* no more retries */
                        cy_ota_set_state(ctx, CY_OTA_STATE_DISCONNECTING);
                        cy_ota_internal_call_cb(ctx, CY_OTA_REASON_DOWNLOAD_FAILED, 0);
                        cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_DISCONNECT, 0);
                    }
                }
                continue;
            }
            else
            {
                /* Download completed, clear error */
                last_error = OTA_ERROR_NONE;

                IotLogDebug("%s() CY_OTA_REASON_DOWNLOAD_COMPLETED\n", __func__);
                cy_ota_internal_call_cb(ctx, CY_OTA_REASON_DOWNLOAD_COMPLETED, 0);

                IotLogDebug("%s() send event OTA_EVENT_AGENT_DISCONNECT\n", __func__);
                /* tell ourselves to disconnect */
                cy_ota_set_state(ctx, CY_OTA_STATE_DISCONNECTING);
                cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_DISCONNECT, 0);
            }
            continue;
        }
        if (waitfor & OTA_EVENT_AGENT_DOWNLOAD_TIMEOUT)
        {
            last_error = OTA_ERROR_NO_UPDATE;

            IotLogDebug("%s() CY_OTA_REASON_NO_UPDATE\n", __func__);
            cy_ota_internal_call_cb(ctx, CY_OTA_REASON_NO_UPDATE, 0);

            IotLogDebug("%s() send event OTA_EVENT_AGENT_DISCONNECT\n", __func__);
            /* tell ourselves to disconnect */
            cy_ota_set_state(ctx, CY_OTA_STATE_DISCONNECTING);
            cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_DISCONNECT, 0);
        }
        if (waitfor & OTA_EVENT_AGENT_DISCONNECT)
        {
            cy_ota_set_state(ctx, CY_OTA_STATE_DISCONNECTING);
            if (ctx->curr_transport == CY_OTA_TRANSPORT_MQTT)
            {
                result = cy_ota_mqtt_disconnect(ctx);
            }

#ifdef OTA_HTTP_SUPPORT
            if (ctx->curr_transport == CY_OTA_TRANSPORT_HTTP)
            {
                result = cy_ota_http_disconnect(ctx);
            }

            /* TODO: Special case where we get a re-direct from MQTT to get the OTA Image data from HTTP server */
            if (ctx->redirect_to_http)
            {
                ctx->redirect_to_http = false;

                IotLogDebug("%s() redirected to HTTP server.\n", __func__);
                if (cy_ota_redirect(ctx) == CY_RSLT_SUCCESS)
                {
                    cy_ota_set_state(ctx, CY_OTA_STATE_AGENT_WAITING);
                    cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_START_UPDATE, 0);
                    continue;
                }
            }
#endif
            cy_ota_internal_call_cb(ctx, CY_OTA_REASON_DISCONNECTED, 0);

            if (last_error != OTA_ERROR_NONE)
            {
                IotLogDebug("%s() Don't verify, we had error: %d\n", __func__, last_error);
                cy_ota_set_state(ctx, CY_OTA_STATE_AGENT_WAITING);
                cy_ota_internal_call_cb(ctx, CY_OTA_REASON_DOWNLOAD_FAILED, 0);
                cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_START_NEXT_TIMER, 0);

            }
            else
            {
                IotLogDebug("%s() send event OTA_EVENT_AGENT_VERIFY\n", __func__);
                /* tell ourselves to verify */
                cy_ota_set_state(ctx, CY_OTA_STATE_VERIFYING);
                cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_VERIFY, 0);
            }
            continue;
        }
        if (waitfor & OTA_EVENT_AGENT_VERIFY)
        {
            cy_ota_set_state(ctx, CY_OTA_STATE_VERIFYING);

            result = cy_ota_storage_verify(ctx);
            if (result != CY_RSLT_SUCCESS)
            {
                /* download failed */
                last_error = OTA_ERROR_VERIFY_FAILED;

                IotLogDebug("%s() CY_OTA_REASON_OTA_VERIFY_FAILED\n", __func__);
                cy_ota_set_state(ctx, CY_OTA_STATE_AGENT_WAITING);
                cy_ota_internal_call_cb(ctx, CY_OTA_REASON_OTA_VERIFY_FAILED, 0);
                cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_START_NEXT_TIMER, 0);
            }
            else
            {
                IotLogDebug("%s() CY_OTA_REASON_OTA_VERIFIED\n", __func__);
                cy_ota_set_state(ctx, CY_OTA_STATE_AGENT_WAITING);
                cy_ota_internal_call_cb(ctx, CY_OTA_REASON_OTA_VERIFIED, 0);

                if (ctx->agent_params.reboot_upon_completion == 1)
                {
                    /* Not really an error, just want to make sure the message gets printed */
                    IotLogError("%s()   RESETTING NOW !!!!\n", __func__);
                    cy_rtos_delay_milliseconds(1000);
                    NVIC_SystemReset();
                }
                cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_START_NEXT_TIMER, 0);
            }
            continue;
        }
    }

    cy_ota_stop_timer(ctx);

    IotLogDebug("%s() OTA_EVENT_AGENT_EXITING\n", __func__);
    /* let mainline know we are exiting */
    cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_EXITING, 0);

    cy_rtos_exit_thread();
}



/* --------------------------------------------------------------- */

cy_rslt_t cy_ota_agent_start(cy_ota_network_params_t *network_params, cy_ota_agent_params_t *agent_params, cy_ota_context_ptr *ctx_handle)
{
    cy_rslt_t           result = CY_RSLT_TYPE_ERROR;
    cy_ota_context_t    *ctx;
    uint32_t            waitfor;

    /* sanity checks */
    if (ctx_handle == NULL)
    {
        return CY_RSLT_MODULE_OTA_BADARG;
    }
    if ( (network_params == NULL) || (agent_params == NULL) || (ctx_handle == NULL) )
    {
        return CY_RSLT_MODULE_OTA_BADARG;
    }
    if (network_params->transport == CY_OTA_TRANSPORT_MQTT)
    {
        if (cy_ota_mqtt_validate_network_params(network_params) != CY_RSLT_SUCCESS)
        {
            IotLogError("%s() MQQT Network Parameters incorrect!\n", __func__);
            return CY_RSLT_MODULE_OTA_BADARG;
        }
    }
#ifdef OTA_HTTP_SUPPORT
    else if (network_params->transport == CY_OTA_TRANSPORT_HTTP)
    {
        if (cy_ota_http_validate_network_params(network_params) != CY_RSLT_SUCCESS)
        {
            IotLogError("%s() HTTP Network Parameters incorrect!\n", __func__);
            return CY_RSLT_MODULE_OTA_BADARG;
        }
    }
#endif
    else
    {
        IotLogError("%s() Incorrect Network Transport!\n", __func__);
    }

    if (cy_ota_validate_agent_params(agent_params) != CY_RSLT_SUCCESS)
    {
        IotLogError("%s() Agent Parameters incorrect!\n", __func__);
        return CY_RSLT_MODULE_OTA_BADARG;
    }

    if (ota_context_only_one != NULL)
    {
        IotLogError("%s() OTA context already created!\n", __func__);
        return CY_RSLT_MODULE_OTA_ALREADY_STARTED;
    }

    /* create our context structure */
    ctx = (cy_ota_context_t *)malloc(sizeof(cy_ota_context_t));
    if (ctx == NULL)
    {
        IotLogError("%s() Out of memory for OTA context!\n", __func__);
        goto _ota_init_err;
    }
    memset(ctx, 0x00, sizeof(cy_ota_context_t));

    ctx->curr_state = CY_OTA_STATE_INITIALIZING;

    /* copy over the initial parameters */
    memcpy(&ctx->network_params, network_params, sizeof(cy_ota_network_params_t));
    memcpy(&ctx->agent_params, agent_params, sizeof(cy_ota_agent_params_t));

    /* start with requested transport and server info */
    ctx->curr_transport = ctx->network_params.transport;
    ctx->curr_server    = ctx->network_params.server;

    /* timer values */
    ctx->initial_timer_sec = CY_OTA_INITIAL_CHECK_SECS;
    ctx->next_timer_sec = CY_OTA_NEXT_CHECK_INTERVAL_SECS;
    ctx->retry_timer_sec = CY_OTA_RETRY_INTERVAL_SECS;
    ctx->check_timeout_sec = CY_OTA_CHECK_TIME_SECS;
    ctx->packet_timeout_sec = CY_OTA_PACKET_INTERVAL_SECS;

    /* create event flags */
    result = cy_rtos_init_event(&ctx->ota_event);
    if (result != CY_RSLT_SUCCESS)
    {
        /* Event create failed */
        IotLogError( "%s() Event Create Failed!\n", __func__);
        goto _ota_init_err;
    }

    /* Create timer */
   result = cy_rtos_init_timer(&ctx->ota_timer, CY_TIMER_TYPE_ONCE,
                               cy_ota_timer_callback, (cy_timer_callback_arg_t)ctx);
   if (result != CY_RSLT_SUCCESS)
   {
       /* Event create failed */
       IotLogError( "%s() Timer Create Failed!\n", __func__);
       goto _ota_init_err;
   }

   /* set our tag */
   ctx->tag = CY_OTA_TAG;

   /* create OTA Agent thread */
   result = cy_rtos_create_thread(&ctx->ota_agent_thread, cy_ota_agent,
                                   "CY OTA Agent", cy_ota_agent_stack, sizeof(cy_ota_agent_stack),
                                   CY_RTOS_PRIORITY_BELOWNORMAL, ctx);
   if (result != CY_RSLT_SUCCESS)
   {
       /* Thread create failed */
       IotLogError( "%s() OTA Agent Thread Create Failed!\n", __func__);
       goto _ota_init_err;
    }

    /* wait for signal from started thread */
    waitfor = OTA_EVENT_AGENT_RUNNING;
    IotLogDebug( "%s() Wait for Thread to start\n", __func__);
    result = cy_rtos_waitbits_event(&ctx->ota_event, &waitfor, 1, 1, 1000);
    if (result != CY_RSLT_SUCCESS)
    {
        /* Thread create failed ? */
        IotLogError( "%s() OTA Agent Thread Create No response\n", __func__);
        goto _ota_init_err;
    }

    /* return context to user */
    *ctx_handle = ctx;
    /* keep track of it */
    ota_context_only_one = ctx;

    /* if there is an initial timer value, use it */
    if (ctx->initial_timer_sec != 0)
    {
        cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_START_INITIAL_TIMER, 0);
    }

    return CY_RSLT_SUCCESS;

_ota_init_err:

    IotLogError("%s() Init failed: 0x%lx\n", __func__, result);
    if (ctx != NULL)
    {
        cy_ota_agent_stop((cy_ota_context_ptr *)&ctx);
    }
    *ctx_handle = NULL;

    return CY_RSLT_TYPE_ERROR;

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
        return CY_RSLT_MODULE_OTA_BADARG;
    }
    if (*ctx_handle == NULL)
    {
        return CY_RSLT_MODULE_OTA_BADARG;
    }

    ctx = (cy_ota_context_t *)*ctx_handle;
    CY_OTA_CONTEXT_ASSERT(ctx);

    ctx->curr_state = CY_OTA_STATE_EXITING;

    /* Signal thread to end */
    cy_rtos_setbits_event(&ctx->ota_event, OTA_EVENT_AGENT_SHUTDOWN_NOW, 0);

    /* wait for signal from started thread */
    waitfor = OTA_EVENT_AGENT_EXITING;
    IotLogDebug( "%s() Wait for Thread to exit\n", __func__);
    result = cy_rtos_waitbits_event(&ctx->ota_event, &waitfor, 1, 1, 1000);
    if (result != CY_RSLT_SUCCESS)
    {
        /* Thread exit failed ? */
        IotLogError( "%s() OTA Agent Thread Exit No response\n", __func__);
    }

    /* wait for thread to exit */
    cy_rtos_join_thread(&ctx->ota_agent_thread);

    /* clear timer */
    cy_rtos_deinit_timer(&ctx->ota_timer);

    /* clear events */
    cy_rtos_deinit_event(&ctx->ota_event);

    memset(ctx, 0x00, sizeof(cy_ota_context_t));
    free(ctx);

    *ctx_handle = NULL;
    ota_context_only_one = NULL;

    IotLogDebug( "%s() DONE\n", __func__);
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
        return CY_RSLT_MODULE_OTA_BADARG;
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
cy_ota_error_t cy_ota_last_error(void)
{
    return last_error;
}

/* --------------------------------------------------------------- */
const char *cy_ota_get_error_string(cy_ota_error_t error)
{
    if (error >= CY_OTA_LAST_ERROR)
    {
        return "INVALID_ARGUMENT";
    }
    return cy_ota_error_strings[error];
}


/* --------------------------------------------------------------- */

const char *cy_ota_get_state_string(cy_ota_agent_state_t state)
{
    if ( state >= CY_OTA_LAST_STATE)
    {
        return "INVALID STATE";
    }
    return cy_ota_state_strings[state];
}

/* --------------------------------------------------------------- */

const char *cy_ota_get_callback_reason_string(cy_ota_cb_reason_t reason)
{
    if ( reason >= CY_OTA_LAST_REASON)
    {
        return "INVALID REASON";
    }
    return cy_ota_reason_strings[reason];
}
