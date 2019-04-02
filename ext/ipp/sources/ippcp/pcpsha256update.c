/*******************************************************************************
* Copyright 2002-2018 Intel Corporation
* All Rights Reserved.
*
* If this  software was obtained  under the  Intel Simplified  Software License,
* the following terms apply:
*
* The source code,  information  and material  ("Material") contained  herein is
* owned by Intel Corporation or its  suppliers or licensors,  and  title to such
* Material remains with Intel  Corporation or its  suppliers or  licensors.  The
* Material  contains  proprietary  information  of  Intel or  its suppliers  and
* licensors.  The Material is protected by  worldwide copyright  laws and treaty
* provisions.  No part  of  the  Material   may  be  used,  copied,  reproduced,
* modified, published,  uploaded, posted, transmitted,  distributed or disclosed
* in any way without Intel's prior express written permission.  No license under
* any patent,  copyright or other  intellectual property rights  in the Material
* is granted to  or  conferred  upon  you,  either   expressly,  by implication,
* inducement,  estoppel  or  otherwise.  Any  license   under such  intellectual
* property rights must be express and approved by Intel in writing.
*
* Unless otherwise agreed by Intel in writing,  you may not remove or alter this
* notice or  any  other  notice   embedded  in  Materials  by  Intel  or Intel's
* suppliers or licensors in any way.
*
*
* If this  software  was obtained  under the  Apache License,  Version  2.0 (the
* "License"), the following terms apply:
*
* You may  not use this  file except  in compliance  with  the License.  You may
* obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
*
*
* Unless  required  by   applicable  law  or  agreed  to  in  writing,  software
* distributed under the License  is distributed  on an  "AS IS"  BASIS,  WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
* See the   License  for the   specific  language   governing   permissions  and
* limitations under the License.
*******************************************************************************/

/* 
// 
//  Purpose:
//     Cryptography Primitive.
//     Digesting message according to SHA256
// 
//  Contents:
//        ippsSHA256Update()
//
*/

#include "owndefs.h"
#include "owncp.h"
#include "pcphash.h"
#include "pcphash_rmf.h"
#include "pcptool.h"
#include "pcpsha256stuff.h"

/*F*
//    Name: ippsSHA256Update
//
// Purpose: Updates intermadiate digest based on input stream.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pSrc == NULL
//                            pState == NULL
//    ippStsContextMatchErr   pState->idCtx != idCtxSHA256
//    ippStsLengthErr         len <0
//    ippStsNoErr             no errors
//
// Parameters:
//    pSrc        pointer to the input stream
//    len         input stream length
//    pState      pointer to the SHA256 state
//
*F*/
IPPFUN(IppStatus, ippsSHA256Update,(const Ipp8u* pSrc, int len, IppsSHA256State* pState))
{
   /* test state pointer and ID */
   IPP_BAD_PTR1_RET(pState);
   pState = (IppsSHA256State*)( IPP_ALIGNED_PTR(pState, SHA256_ALIGNMENT) );
   IPP_BADARG_RET(idCtxSHA256 !=HASH_CTX_ID(pState), ippStsContextMatchErr);

   /* test input length */
   IPP_BADARG_RET((len<0), ippStsLengthErr);
   /* test source pointer */
   IPP_BADARG_RET((len && !pSrc), ippStsNullPtrErr);

   /*
   // handle non empty message
   */
   if(len) {
      /* select processing function */
      #if (_SHA_NI_ENABLING_==_FEATURE_ON_)
      cpHashProc updateFunc = UpdateSHA256ni;
      #elif (_SHA_NI_ENABLING_==_FEATURE_TICKTOCK_)
      cpHashProc updateFunc = IsFeatureEnabled(ippCPUID_SHA)? UpdateSHA256ni : UpdateSHA256;
      #else
      cpHashProc updateFunc = UpdateSHA256;
      #endif

      int procLen;

      int idx = HAHS_BUFFIDX(pState);
      Ipp8u* pBuffer = HASH_BUFF(pState);
      Ipp64u lenLo = HASH_LENLO(pState) +len;

      /* if non empty internal buffer filling */
      if(idx) {
         /* copy from input stream to the internal buffer as match as possible */
         procLen = IPP_MIN(len, (MBS_SHA256-idx));
         CopyBlock(pSrc, pBuffer+idx, procLen);
         
         /* update message pointer and length */
         pSrc += procLen;
         len  -= procLen;
         idx  += procLen;

         /* update digest if buffer full */
         if( MBS_SHA256 == idx) {
            updateFunc(HASH_VALUE(pState), pBuffer, MBS_SHA256, sha256_cnt);
            idx = 0;
         }
      }

      /* main message part processing */
      procLen = len & ~(MBS_SHA256-1);
      if(procLen) {
         updateFunc(HASH_VALUE(pState), pSrc, procLen, sha256_cnt);
         pSrc += procLen;
         len  -= procLen;
      }

      /* store rest of message into the internal buffer */
      if(len) {
         CopyBlock(pSrc, pBuffer, len);
         idx += len;
      }

      /* update length of processed message */
      HASH_LENLO(pState) = lenLo;
      HAHS_BUFFIDX(pState) = idx;
   }

   return ippStsNoErr;
}
