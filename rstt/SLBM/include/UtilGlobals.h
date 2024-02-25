//- ****************************************************************************
//-
//- Copyright 2009 National Technology & Engineering Solutions of Sandia, LLC
//- (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
//- Government retains certain rights in this software.
//-
//- BSD Open Source License
//- All rights reserved.
//-
//- Redistribution and use in source and binary forms, with or without
//- modification, are permitted provided that the following conditions are met:
//-
//-   1. Redistributions of source code must retain the above copyright notice,
//-      this list of conditions and the following disclaimer.
//-
//-   2. Redistributions in binary form must reproduce the above copyright
//-      notice, this list of conditions and the following disclaimer in the
//-      documentation and/or other materials provided with the distribution.
//-
//-   3. Neither the name of the copyright holder nor the names of its
//-      contributors may be used to endorse or promote products derived from
//-      this software without specific prior written permission.
//-
//- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
//- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//- POSSIBILITY OF SUCH DAMAGE.
//-
//- ****************************************************************************

#ifndef UtilGlobals_H
#define UtilGlobals_H

// **** _BEGIN UTIL NAMESPACE_ *************************************************

namespace util {

// Definition of dllimport and dllexport here for Windows only
// Note that definitions are repeated for various tools/libraries.
//--------------------------
#if defined(_WIN32) || defined(WIN32)

  // exports when building UTIL dll, imports when linking to header files in
  // UTIL (Note that UTIL_EXPORTS should be defined when building a UTIL
  // DLL, and should  not be defined when linking with the UTIL DLL)
  #ifdef  UTIL_EXPORTS
  #define UTIL_EXP_IMP __declspec(dllexport)
  #else
  #define UTIL_EXP_IMP __declspec(dllimport)
  #endif

  // exports DLL for classes and functions that ONLY export
  // (Note that this is mainly used for templated classes that are not imported)
  #define UTIL_EXP     __declspec(dllexport)

#else  // Sun does not need these

  #define UTIL_EXP_IMP
  #define UTIL_EXP

#endif
//--------------------------

} // end namespace util

#endif // UtilGlobals_H
