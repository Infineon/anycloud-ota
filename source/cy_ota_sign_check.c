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
 * Cypress OTA Download Check downloaded chunks with a signature
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "cy_ota_api.h"
#include "cy_ota_internal.h"

/* mbed tls includes */
#if !defined( MBEDTLS_CONFIG_FILE )
    #include "mbedtls/config.h"
#else
    #include MBEDTLS_CONFIG_FILE
#endif

#include "mbedtls/platform.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha1.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509_crt.h"

#ifdef OTA_SIGNING_SUPPORT

/***********************************************************************
 *
 * defines & enums
 *
 **********************************************************************/

/**
 * @brief Buffer size for storing cryptographic hash computation
 */
#define OTA_MBEDTLS_SHA256_DIGEST_LENGTH     (32)

#define OTA_FILE_SIG_KEY_STR_MAX_LENGTH     (32)

/**
 * @brief File Signature Key
 *
 * The OTA signature scheme for SHA256 ECDSA
 */
const char OTA_sha256_ecdsa_scheme[ OTA_FILE_SIG_KEY_STR_MAX_LENGTH ] = "sig-sha256-ecdsa";

/**
 * @brief Start of certificate string
 */
static const char cy_ota_sign_cert_begin[] = "-----BEGIN CERTIFICATE-----";
/**
 * @brief End of certificate string
 */
static const char cy_ota_sign_cert_end[] = "-----END CERTIFICATE-----";

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

typedef struct ota_sign_info_s {
    uint16_t    signature_length;
} ota_sign_info_t;

/***********************************************************************
 *
 * Variables
 *
 **********************************************************************/

static const uint8_t signature_cert[] = "";

/***********************************************************************
 *
 * functions
 *
 **********************************************************************/

/**
 * @brief Decode the local certificate PEM
 *
 * @param[in]   ptr to the certificate
 * @param[out]  handle to store ptr to the decoded certificate
 * @param[out]  ptr to store size of decoded certificate
 *
 * @return CY_RSLT_SUCCESS
 *         CY_RSLT_MODULE_OTA_BADARG
 *         CY_RSLT_MODULE_OTA_VERIFY_ERROR
 *         CY_RSLT_MODULE_OTA_OUT_OF_MEMORY_ERROR
 */
static cy_rslt_t cy_ota_read_certificate(const uint8_t *certificate,
                                        uint8_t **decoded_certificate,
                                        uint32_t *decoded_certificate_length)
{
    int     ret;
    uint8_t *certificate_begin;
    uint8_t *certificate_end;
    uint8_t *int_decoded_certificate;
    size_t  int_decoded_certificate_length;

    /* sanity checks */
    if ( (certificate == NULL) || (decoded_certificate == NULL) || (decoded_certificate_length == NULL) )
    {
        IotLogError( "%s() No Bad args %p %p %p \r\n", __func__, certificate, decoded_certificate, decoded_certificate_length);
        return CY_RSLT_MODULE_OTA_BADARG;
    }


    IotLogError("signature_cert:%p passed in:%p begin:%p ", signature_cert, certificate, cy_ota_sign_cert_begin);
    IotLogError("              >%.*s<", 27, signature_cert);
    /* Skip the "BEGIN CERTIFICATE" */
    certificate_begin = (uint8_t *)strstr ((char *)certificate, cy_ota_sign_cert_begin);
    if (certificate_begin == NULL)
    {
        IotLogError( "%s() No Begin found for Certificate\r\n", __func__);
        return CY_RSLT_MODULE_OTA_VERIFY_ERROR;
    }
    certificate_begin += sizeof(cy_ota_sign_cert_begin);

    /* Find the "END CERTIFICATE" */
    certificate_end =  (uint8_t *)strstr((char *)certificate_begin, cy_ota_sign_cert_end);
    if (certificate_end == NULL)
    {
        IotLogError( "%s() No END found for Certificate\r\n", __func__);
        return CY_RSLT_MODULE_OTA_VERIFY_ERROR;
    }

    ret = mbedtls_base64_decode(NULL, 0, &int_decoded_certificate_length, certificate_begin, certificate_end - certificate_begin);
    if (ret != 0 )
    {
        IotLogError( "%s() mbedtls_base64_decode() for length failed\r\n", __func__);
        return CY_RSLT_MODULE_OTA_VERIFY_ERROR;
    }
    int_decoded_certificate = (uint8_t *) Iot_DefaultMalloc(int_decoded_certificate_length);
    if (int_decoded_certificate == NULL)
    {
        IotLogError( "%s() Failed to decode the Certificate - no memory \r\n", __func__);
        return CY_RSLT_MODULE_OTA_OUT_OF_MEMORY_ERROR;
    }
    ret = mbedtls_base64_decode(int_decoded_certificate, int_decoded_certificate_length, &int_decoded_certificate_length, certificate_begin, certificate_end - certificate_begin);
    if (ret != 0 )
    {

        IotLogError( "%s() mbedtls_base64_decode() failed\r\n", __func__);
        return CY_RSLT_MODULE_OTA_VERIFY_ERROR;
    }

    *decoded_certificate = int_decoded_certificate;
    *decoded_certificate_length = int_decoded_certificate_length;
    return CY_RSLT_SUCCESS;
}

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
cy_rslt_t cy_ota_sign_check_chunk( cy_ota_storage_write_info_t *chunk_info)
{
    int         ret;
    cy_rslt_t   result = CY_RSLT_SUCCESS;
    uint8_t     *decoded_certificate = NULL;
    uint32_t    decoded_certificate_length;

    ota_sign_info_t *incoming_sign_info;
    uint8_t         *incoming_digest;
    uint32_t        incoming_digest_length;

    mbedtls_sha256_context  sha256_context;
    uint8_t                 sign_output[OTA_MBEDTLS_SHA256_DIGEST_LENGTH];
    mbedtls_x509_crt        x509_cert_context;

    mbedtls_pk_context      mbedtls_pk_ctx;

    /* sanity check */
    if ( (chunk_info == NULL) || (chunk_info->signature_ptr == NULL) )
    {
        return CY_RSLT_MODULE_OTA_BADARG;
    }

    /* get pointer to incoming certificate */
    incoming_sign_info   = (ota_sign_info_t *)(chunk_info->signature_ptr);
    incoming_digest_length = incoming_sign_info->signature_length;
    /* Point past the size of the ota_sign_info_t struct to point to the digest */
    incoming_digest      = (uint8_t *)&incoming_sign_info[1];

    /* read in our certificate */
    result = cy_ota_read_certificate( signature_cert, &decoded_certificate, &decoded_certificate_length);
    if ( result != CY_RSLT_SUCCESS)
    {
        IotLogError( "%s() cy_ota_read_certificate() failed\r\n", __func__);
        goto sign_check_exit;
    }

    /*
     * We only support base64 SHA256 ECDSA signing scheme
     */
    if (strcmp(chunk_info->signature_scheme, OTA_sha256_ecdsa_scheme) != 0)
    {
        IotLogError( "%s() This scheme is not supported at this time: %.*s\r\n", __func__,
                     CY_OTA_MAX_SIGN_LENGTH, chunk_info->signature_scheme);
        result = CY_RSLT_MODULE_OTA_VERIFY_ERROR;
        goto sign_check_exit;
    }

    /*  Initialize and run */
    mbedtls_sha256_init(&sha256_context);
    ret = mbedtls_sha256_starts_ret(&sha256_context, 0);      /* 0 for 256 */
    if (ret != 0)
    {
        IotLogError("%d:%s() error mbedtls_sha256_init()\n", __LINE__, __func__);
        result = CY_RSLT_MODULE_OTA_VERIFY_ERROR;
        goto sign_check_exit;
    }

    ret = mbedtls_sha256_update_ret(&sha256_context, decoded_certificate, decoded_certificate_length);
    if (ret != 0)
    {
        IotLogError("%d:%s() error mbedtls_sha256_update_ret()\n", __LINE__, __func__);
        result = CY_RSLT_MODULE_OTA_VERIFY_ERROR;
        goto sign_check_exit;
    }

    ret = mbedtls_sha256_finish_ret(&sha256_context, sign_output);
    if (ret != 0)
    {
        IotLogError("%d:%s() error mbedtls_sha256_finish_ret()\n", __LINE__, __func__);
        result = CY_RSLT_MODULE_OTA_VERIFY_ERROR;
        goto sign_check_exit;
    }

    mbedtls_x509_crt_init(&x509_cert_context);
    ret = mbedtls_x509_crt_parse( &x509_cert_context, ( const unsigned char * ) decoded_certificate, decoded_certificate_length );
    if (ret != 0)
    {
        IotLogError("%d:%s() error mbedtls_x509_crt_init()\n", __LINE__, __func__);
        result = CY_RSLT_MODULE_OTA_VERIFY_ERROR;
        goto sign_check_exit;
    }

    ret = mbedtls_pk_verify(&mbedtls_pk_ctx, MBEDTLS_MD_SHA256,
                            sign_output, sizeof(sign_output),
                            incoming_digest, incoming_digest_length );
    if (ret != 0)
    {
        IotLogError("%d:%s() mbedtls_pk_verify()y\n", __LINE__, __func__);
        result = CY_RSLT_MODULE_OTA_VERIFY_ERROR;
        goto sign_check_exit;
    }

sign_check_exit:

    /* free certificate context */
    mbedtls_x509_crt_free(&x509_cert_context);

    /* free the SHA context */
    mbedtls_sha256_free(&sha256_context);

    /* free the decoded cert ram */
    if (decoded_certificate != NULL)
    {
        Iot_DefaultFree(decoded_certificate);
    }

    return result;
}
#endif  /* OTA_SIGNING_SUPPORT */
