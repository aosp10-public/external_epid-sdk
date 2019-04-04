/*******************************************************************************
* Copyright 2014-2018 Intel Corporation
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
//     Security Hash Standard
//     General Functionality
// 
//  Contents:
//        cpInitHash()
//
*/

#include "owndefs.h"
#include "owncp.h"
#include "pcphash.h"
#include "pcphash_func.h"
#include "pcptool.h"

int cpInitHash(IppsHashState* pCtx, IppHashAlgId algID)
{
   /* setup default processing function */
   HASH_FUNC(pCtx) = cpHashProcFunc[algID];

   /* update default processing function if SHA-NI enabled */
   #if (_IPP>=_IPP_P8) || (_IPP32E>=_IPP32E_Y8)
   if( IsFeatureEnabled(ippCPUID_SHA) ) {

      #if defined(_ENABLE_ALG_SHA1_)
      if(ippHashAlg_SHA1==algID)
         HASH_FUNC(pCtx) = UpdateSHA1ni;
      #endif

      #if defined(_ENABLE_ALG_SHA256_) || defined(_ENABLE_ALG_SHA224_)
      if(ippHashAlg_SHA256==algID || ippHashAlg_SHA224==algID)
         HASH_FUNC(pCtx) = UpdateSHA256ni;
      #endif
   }
   #endif

   /* setup optional agr of processing function */
   HASH_FUNC_PAR(pCtx) = cpHashProcFuncOpt[algID];

   return cpReInitHash(pCtx, algID);
}
