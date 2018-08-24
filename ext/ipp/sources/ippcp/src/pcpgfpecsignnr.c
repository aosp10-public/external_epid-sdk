/*############################################################################
  # Copyright 1999-2018 Intel Corporation
  #
  # Licensed under the Apache License, Version 2.0 (the "License");
  # you may not use this file except in compliance with the License.
  # You may obtain a copy of the License at
  #
  #     http://www.apache.org/licenses/LICENSE-2.0
  #
  # Unless required by applicable law or agreed to in writing, software
  # distributed under the License is distributed on an "AS IS" BASIS,
  # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  # See the License for the specific language governing permissions and
  # limitations under the License.
  ############################################################################*/

/* 
// 
//  Purpose:
//     Cryptography Primitive.
//     NR signature generation and verification
// 
//  Contents:
//     ippsGFpECSignNR()
//     ippsGFpECVerifyNR()
// 
*/

#include "owndefs.h"
#include "owncp.h"
#include "pcpeccp.h"


/*F*
//    Name: ippsGFpECSignNR
//
// Purpose: NR Signature Generation.
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pEC
//                               NULL == pMsg
//                               NULL == pRegPrivate
//                               NULL == pEphPrivate
//                               NULL == pSignC
//                               NULL == pSignD
//                               NULL == pScratchBuffer
//
//    ippStsContextMatchErr      illegal pEC->idCtx
//                               illegal pMsg->idCtx
//                               illegal pRegPrivate->idCtx
//                               illegal pEphPrivate->idCtx
//                               illegal pSignC->idCtx
//                               illegal pSignD->idCtx
//
//    ippStsMessageErr           Msg < 0, Msg >= order
//
//    ippStsRangeErr             not enough room for:
//                               signC
//                               signD
//
//    ippStsErr                 (0==signC) || (0==signD)
//
//    ippStsNoErr                no errors
//
// Parameters:
//    pMsg           pointer to the message representative to be signed
//    pRegPrivate    pointer to the regular private key
//    pEphPrivate    pointer to the ephemeral private key
//    pSignR,pSignS  pointer to the signature
//    pEC            pointer to the EC context
//    pScratchBuffer pointer to buffer (1 mul_point operation)
//
*F*/
IPPFUN(IppStatus, ippsGFpECSignNR,(const IppsBigNumState* pMsg,
                                   const IppsBigNumState* pRegPrivate,
                                   const IppsBigNumState* pEphPrivate,
                                   IppsBigNumState* pSignC, IppsBigNumState* pSignD,
                                   IppsGFpECState* pEC,
                                   Ipp8u* pScratchBuffer))
{
   IppsGFpState*  pGF;
   gsModEngine* pGFE;

   /* EC context and buffer */
   IPP_BAD_PTR2_RET(pEC, pScratchBuffer);
   pEC = (IppsGFpECState*)( IPP_ALIGNED_PTR(pEC, ECGFP_ALIGNMENT) );
   IPP_BADARG_RET(!ECP_TEST_ID(pEC), ippStsContextMatchErr);

   pGF = ECP_GFP(pEC);
   pGFE = GFP_PMA(pGF);
   IPP_BADARG_RET(1<GFP_EXTDEGREE(pGFE), ippStsNotSupportedModeErr);

   /* test message representative */
   IPP_BAD_PTR1_RET(pMsg);
   pMsg = (IppsBigNumState*)( IPP_ALIGNED_PTR(pMsg, ALIGN_VAL) );
   IPP_BADARG_RET(!BN_VALID_ID(pMsg), ippStsContextMatchErr);

   /* test signature */
   IPP_BAD_PTR2_RET(pSignC, pSignD);
   pSignC = (IppsBigNumState*)( IPP_ALIGNED_PTR(pSignC, BN_ALIGNMENT) );
   pSignD = (IppsBigNumState*)( IPP_ALIGNED_PTR(pSignD, BN_ALIGNMENT) );
   IPP_BADARG_RET(!BN_VALID_ID(pSignC), ippStsContextMatchErr);
   IPP_BADARG_RET(!BN_VALID_ID(pSignD), ippStsContextMatchErr);
   IPP_BADARG_RET((BN_ROOM(pSignC)*BITSIZE(BNU_CHUNK_T)<ECP_ORDBITSIZE(pEC)), ippStsRangeErr);
   IPP_BADARG_RET((BN_ROOM(pSignD)*BITSIZE(BNU_CHUNK_T)<ECP_ORDBITSIZE(pEC)), ippStsRangeErr);

   /* test private keys */
   IPP_BAD_PTR2_RET(pRegPrivate, pEphPrivate);
   pRegPrivate = (IppsBigNumState*)( IPP_ALIGNED_PTR(pRegPrivate, ALIGN_VAL) );
   IPP_BADARG_RET(!BN_VALID_ID(pRegPrivate), ippStsContextMatchErr);
   pEphPrivate = (IppsBigNumState*)( IPP_ALIGNED_PTR(pEphPrivate, ALIGN_VAL) );
   IPP_BADARG_RET(!BN_VALID_ID(pEphPrivate), ippStsContextMatchErr);

   {
      gsModEngine* pMontR = ECP_MONT_R(pEC);
      BNU_CHUNK_T* pOrder = MOD_MODULUS(pMontR);
      int orderLen = MOD_LEN(pMontR);

      BNU_CHUNK_T* dataC = BN_NUMBER(pSignC);
      BNU_CHUNK_T* dataD = BN_NUMBER(pSignD);
      BNU_CHUNK_T* buffD = BN_BUFFER(pSignD);

      /* test value of private keys: regPrivate<order, ephPrivate<order */
      IPP_BADARG_RET(BN_NEGATIVE(pRegPrivate) ||
                     (0<=cpCmp_BNU(BN_NUMBER(pRegPrivate), BN_SIZE(pRegPrivate), pOrder, orderLen)), ippStsIvalidPrivateKey);
      IPP_BADARG_RET(BN_NEGATIVE(pEphPrivate) ||
                     (0<=cpCmp_BNU(BN_NUMBER(pEphPrivate), BN_SIZE(pEphPrivate), pOrder, orderLen)), ippStsIvalidPrivateKey);
      /* test mesage: 0<msg<order */
      IPP_BADARG_RET(BN_NEGATIVE(pMsg) ||
                     (0<=cpCmp_BNU(BN_NUMBER(pMsg), BN_SIZE(pMsg), pOrder, orderLen)), ippStsMessageErr);

      {
         int elmLen = GFP_FELEN(pGFE);
         int ns;

         /* compute ephemeral public key */
         IppsGFpECPoint ephPublic;
         cpEcGFpInitPoint(&ephPublic, cpEcGFpGetPool(1, pEC), 0, pEC);
         gfec_MulBasePoint(&ephPublic,
                           BN_NUMBER(pEphPrivate), BN_SIZE(pEphPrivate),
                           pEC, pScratchBuffer);

         /* x = (ephPublic.x) mod order */
         gfec_GetPoint(dataC, NULL, &ephPublic, pEC);
         GFP_METHOD(pGFE)->decode(dataC, dataC, pGFE);
         ns = cpMod_BNU(dataC, elmLen, pOrder, orderLen);
         cpGFpElementPadd(dataC+ns, orderLen-ns, 0);

         cpEcGFpReleasePool(1, pEC);

         /* C = (ephPublic.x + msg) mod order */
         ZEXPAND_COPY_BNU(dataD, orderLen, BN_NUMBER(pMsg), BN_SIZE(pMsg));
         cpModAdd_BNU(dataC, dataC, dataD, pOrder, orderLen, dataD);

         /* check c!=0 */
         if(GFP_IS_ZERO(dataC, orderLen)) return ippStsErr;

         /* D = (ephPrivate - regPrivate*C) mod order */
         ZEXPAND_COPY_BNU(buffD, orderLen, BN_NUMBER(pRegPrivate),BN_SIZE(pRegPrivate));
         cpMontEnc_BNU_EX(dataD, buffD, orderLen, pMontR);
         cpMontMul_BNU(buffD, dataD, dataC, pMontR);
         ZEXPAND_COPY_BNU(dataD, orderLen, BN_NUMBER(pEphPrivate),BN_SIZE(pEphPrivate));
         cpModSub_BNU(dataD, dataD, buffD, pOrder, orderLen, buffD);

         /* signC */
         ns = orderLen;
         FIX_BNU(dataC, ns);
         BN_SIGN(pSignC) = ippBigNumPOS;
         BN_SIZE(pSignC) = ns;
         /* signD */
         ns = orderLen;
         FIX_BNU(dataD, ns);
         BN_SIGN(pSignD) = ippBigNumPOS;
         BN_SIZE(pSignD) = ns;

         return ippStsNoErr;
      }
   }
}


/*F*
//    Name: ippsGFpECVerifyNR
//
// Purpose: NR Signature Verification.
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pEC
//                               NULL == pMsg
//                               NULL == pRegPublic
//                               NULL == pSignC
//                               NULL == pSignD
//                               NULL == pResult
//                               NULL == pScratchBuffer
//
//    ippStsContextMatchErr      illegal pECC->idCtx
//                               illegal pMsgDigest->idCtx
//                               illegal pRegPublic->idCtx
//                               illegal pSignC->idCtx
//                               illegal pSignD->idCtx
//
//    ippStsMessageErr           Msg < 0, Msg >= order
//
//    ippStsRangeErr             SignC < 0 or SignD < 0
//
//    ippStsOutOfRangeErr        bitsize(pRegPublic) != bitsize(prime)
//
//    ippStsNoErr                no errors
//
// Parameters:
//    pMsg           pointer to the message representative to being signed
//    pRegPublic     pointer to the regular public key
//    pSignC,pSignD  pointer to the signature
//    pResult        pointer to the result: ippECValid/ippECInvalidSignature
//    pEC            pointer to the ECCP context
//    pScratchBuffer pointer to buffer (2 mul_point operation)
//
*F*/
IPPFUN(IppStatus, ippsGFpECVerifyNR,(const IppsBigNumState* pMsg,
                                     const IppsGFpECPoint* pRegPublic,
                                     const IppsBigNumState* pSignC, const IppsBigNumState* pSignD,
                                     IppECResult* pResult,
                                     IppsGFpECState* pEC,
                                     Ipp8u* pScratchBuffer))
{
   IppsGFpState*  pGF;
   gsModEngine* pGFE;

   /* EC context and buffer */
   IPP_BAD_PTR2_RET(pEC, pScratchBuffer);
   pEC = (IppsGFpECState*)( IPP_ALIGNED_PTR(pEC, ECGFP_ALIGNMENT) );
   IPP_BADARG_RET(!ECP_TEST_ID(pEC), ippStsContextMatchErr);

   pGF = ECP_GFP(pEC);
   pGFE = GFP_PMA(pGF);
   IPP_BADARG_RET(1<GFP_EXTDEGREE(pGFE), ippStsNotSupportedModeErr);

   /* test message representative */
   IPP_BAD_PTR1_RET(pMsg);
   pMsg = (IppsBigNumState*)( IPP_ALIGNED_PTR(pMsg, ALIGN_VAL) );
   IPP_BADARG_RET(!BN_VALID_ID(pMsg), ippStsContextMatchErr);

   /* test regular public key */
   IPP_BAD_PTR1_RET(pRegPublic);
   IPP_BADARG_RET( !ECP_POINT_TEST_ID(pRegPublic), ippStsContextMatchErr );
   IPP_BADARG_RET( ECP_POINT_FELEN(pRegPublic)!=GFP_FELEN(pGFE), ippStsOutOfRangeErr);

   /* test signature */
   IPP_BAD_PTR2_RET(pSignC, pSignD);
   pSignC = (IppsBigNumState*)( IPP_ALIGNED_PTR(pSignC, ALIGN_VAL) );
   pSignD = (IppsBigNumState*)( IPP_ALIGNED_PTR(pSignD, ALIGN_VAL) );
   IPP_BADARG_RET(!BN_VALID_ID(pSignC), ippStsContextMatchErr);
   IPP_BADARG_RET(!BN_VALID_ID(pSignD), ippStsContextMatchErr);
   IPP_BADARG_RET(BN_NEGATIVE(pSignC), ippStsRangeErr);
   IPP_BADARG_RET(BN_NEGATIVE(pSignD), ippStsRangeErr);

   /* test result */
   IPP_BAD_PTR1_RET(pResult);

   {
      IppECResult vResult = ippECInvalidSignature;

      gsModEngine* pMontR = ECP_MONT_R(pEC);
      BNU_CHUNK_T* pOrder = MOD_MODULUS(pMontR);
      int orderLen = MOD_LEN(pMontR);

      /* test mesage: 0<msg<order */
      IPP_BADARG_RET(BN_NEGATIVE(pMsg) ||
                     (0<=cpCmp_BNU(BN_NUMBER(pMsg), BN_SIZE(pMsg), pOrder, orderLen)), ippStsMessageErr);
      /* test signature value */
      if(!cpEqu_BNU_CHUNK(BN_NUMBER(pSignC), BN_SIZE(pSignC), 0) &&
         !cpEqu_BNU_CHUNK(BN_NUMBER(pSignD), BN_SIZE(pSignD), 0) &&
         0>cpCmp_BNU(BN_NUMBER(pSignC), BN_SIZE(pSignC), pOrder, orderLen) &&
         0>cpCmp_BNU(BN_NUMBER(pSignD), BN_SIZE(pSignD), pOrder, orderLen)) {

         int elmLen = GFP_FELEN(pGFE);
         int pelmLen = GFP_PELEN(pGFE);

         BNU_CHUNK_T* h1 = cpGFpGetPool(3, pGFE);
         BNU_CHUNK_T* h2 = h1+pelmLen;
         BNU_CHUNK_T* f  = h2+pelmLen;

         IppsGFpECPoint P;
         cpEcGFpInitPoint(&P, cpEcGFpGetPool(1, pEC),0, pEC);

         /* P = [d]BasePoint + [c]publicKey */
         ZEXPAND_COPY_BNU(h1, orderLen, BN_NUMBER(pSignD),BN_SIZE(pSignD));
         ZEXPAND_COPY_BNU(h2, orderLen, BN_NUMBER(pSignC),BN_SIZE(pSignC));
         gfec_BasePointProduct(&P,
                               h1, orderLen, pRegPublic, h2, orderLen,
                               pEC, pScratchBuffer);

         /* get P.X */
         if(gfec_GetPoint(h1, NULL, &P, pEC)) {
            /* x = int(P.x) mod order */
            GFP_METHOD(pGFE)->decode(h1, h1, pGFE);
            elmLen = cpMod_BNU(h1, elmLen, pOrder, orderLen);
            cpGFpElementPadd(h1+elmLen, orderLen-elmLen, 0);

            /* and recover msg f = (signC -x) mod order */
            ZEXPAND_COPY_BNU(f, orderLen, BN_NUMBER(pMsg),BN_SIZE(pMsg));
            cpModSub_BNU(h1, h2, h1, pOrder, orderLen, h2);
            if(GFP_EQ(f, h1, orderLen))
               vResult = ippECValid;
         }

         cpEcGFpReleasePool(1, pEC);
         cpGFpReleasePool(3, pGFE);
      }

      *pResult = vResult;
      return ippStsNoErr;
   }
}
