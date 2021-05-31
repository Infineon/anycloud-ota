/*
 * Copyright 2016-2020, Cypress Semiconductor Corporation or a subsidiary of
 * Cypress Semiconductor Corporation. All Rights Reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software"), is owned by Cypress Semiconductor Corporation
 * or one of its subsidiaries ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products. Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
 */
/*
 ********************************************************************
 *    File Name: ecc_pp.c
 *
 *    Abstract: simple pairing algorithms
 *
 *    Functions:
 *            --
 *
 *    $History:$
 *
 ********************************************************************
*/
#include "multprecision.h"
#include "ecc_pp.h"
#include <stdio.h>
#include <string.h>

EC curve = {
    { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0, 0x0, 0x0, 0x1, 0xFFFFFFFF },
    { 0xFC632551, 0xF3B9CAC2, 0xA7179E84, 0xBCE6FAAD, 0xFFFFFFFF, 0xFFFFFFFF, 0x0, 0xFFFFFFFF },
    {
        { 0xd898c296, 0xf4a13945, 0x2deb33a0, 0x77037d81, 0x63a440f2, 0xf8bce6e5, 0xe12c4247, 0x6b17d1f2},
        { 0x37bf51f5, 0xcbb64068, 0x6b315ece, 0x2bce3357, 0x7c0f9e16, 0x8ee7eb4a, 0xfe1a7f9b, 0x4fe342e2},
        { 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}
    }
};

UINT32 nprime[KEY_LENGTH_DWORDS] = { 0xEE00BC4F, 0xCCD1C8AA, 0x7D74D2E4, 0x48C94408, 0xC588C6F6, 0x50FE77EC, 0xA9D6281C, 0x60D06633};

UINT32 rr[KEY_LENGTH_DWORDS] = { 0xBE79EEA2, 0x83244C95, 0x49BD6FA6, 0x4699799C, 0x2B6BEC59, 0x2845B239, 0xF3D95620, 0x66E12D94 };

/* TODO: defined in libbtstack.a(p_256_ecc_pp.c), not in header file? */
void ECC_Add(Point *r, Point *p, PointAff *q);

// ECDSA Verify
BOOL32 ecdsa_verify(unsigned char* digest, unsigned char* signature, Point* key)
{
   UINT32 e[KEY_LENGTH_DWORDS];
   UINT32 r[KEY_LENGTH_DWORDS];
   UINT32 s[KEY_LENGTH_DWORDS];

   UINT32 u1[KEY_LENGTH_DWORDS];
   UINT32 u2[KEY_LENGTH_DWORDS];

   UINT32 tmp1[KEY_LENGTH_DWORDS];
   UINT32 tmp2[KEY_LENGTH_DWORDS];

   Point p1, p2;
   UINT32 i;

   // swap input data endianess
   for(i = 0; i < KEY_LENGTH_DWORDS; i++)
   {
       // import digest to long integer
       e[KEY_LENGTH_DWORDS-1-i] = BE_SWAP(digest, 4*i);

       // import signature to long integer
       r[KEY_LENGTH_DWORDS-1-i] = BE_SWAP(signature, 4*i);
       s[KEY_LENGTH_DWORDS-1-i] = BE_SWAP(signature, KEY_LENGTH_BYTES+4*i);
   }

   // compute s' = s ^ -1 mod n
   MP_InvMod(tmp1, s, modn);

   // convert s' to montgomery domain
   MP_MultMont(tmp2, tmp1, (DWORD*)rr);

   // convert e to montgomery domain
   MP_MultMont(tmp1, e, (DWORD*)rr);

   // compute u1 = e * s' mod n
   MP_MultMont(u1, tmp1, tmp2);

   // convert r to montgomery domain
   MP_MultMont(tmp1, r, (DWORD*)rr);

   // compute u2 = r * s' (mod n)
   MP_MultMont(u2, tmp1, tmp2);

   // set tmp1 = 1
   MP_Init(tmp1);
   tmp1[0]=1;

   // convert u1 to normal domain
   MP_MultMont(u1, u1, tmp1);

   // convert u2 to normal domain
   MP_MultMont(u2, u2, tmp1);

   // compute (x,y) = u1G + u2QA
   if(key)
   {
       // if public key is given, using legacy method
       ECC_PM(&p1, &(curve.G), u1, KEY_LENGTH_DWORDS);
       ECC_PM(&p2, key, u2, KEY_LENGTH_DWORDS);
       ECC_Add(&p1, &p1, (PointAff*)&p2);

       // convert point to affine domain
       MP_InvMod(tmp1, p1.z, modp);
       MP_MersennsSquaMod(p1.z, tmp1);
       MP_MersennsMultMod(p1.x, p1.x, p1.z);
   }
   else
   {
       printf("Error\n");
#if 0
       // if public key is not given, using pre-computed method
       ecdsa_fp_shamir(&p1, u1, u2);
#endif
   }

   // w = r (mod n)
   if(MP_CMP(r, modp) >= 0)
       MP_Sub(r, r, modp);

   // verify r == x ?
   if(memcmp(r, p1.x, KEY_LENGTH_BYTES))
       return FALSE;

   // signature match, return true
   return TRUE;
}
