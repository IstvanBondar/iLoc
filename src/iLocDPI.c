//-----------------------------------------------------------------------------
// Copyright (c) 2018 Oracle and/or its affiliates.  All rights reserved.
// This program is free software: you can modify it and/or redistribute it
// under the terms of:
//
// (i)  the Universal Permissive License v 1.0 or at your option, any
//      later version (http://oss.oracle.com/licenses/upl); and/or
//
// (ii) the Apache License v 2.0. (http://www.apache.org/licenses/LICENSE-2.0)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// dpi.c
//   Include this file in your project in order to embed ODPI-C source without
// having to compile files individually. Only the definitions in the file
// include/dpi.h are intended to be used publicly. Each file can also be
// compiled independently if that is preferable.
//-----------------------------------------------------------------------------
#ifdef ORASQL

#include "iLoc.h"

#include "./ODPIsrc/dpiConn.c"
#include "./ODPIsrc/dpiContext.c"
#include "./ODPIsrc/dpiData.c"
#include "./ODPIsrc/dpiDebug.c"
#include "./ODPIsrc/dpiDeqOptions.c"
#include "./ODPIsrc/dpiEnqOptions.c"
#include "./ODPIsrc/dpiEnv.c"
#include "./ODPIsrc/dpiError.c"
#include "./ODPIsrc/dpiGen.c"
#include "./ODPIsrc/dpiGlobal.c"
#include "./ODPIsrc/dpiHandleList.c"
#include "./ODPIsrc/dpiHandlePool.c"
#include "./ODPIsrc/dpiLob.c"
#include "./ODPIsrc/dpiMsgProps.c"
#include "./ODPIsrc/dpiObjectAttr.c"
#include "./ODPIsrc/dpiObject.c"
#include "./ODPIsrc/dpiObjectType.c"
#include "./ODPIsrc/dpiOci.c"
#include "./ODPIsrc/dpiOracleType.c"
#include "./ODPIsrc/dpiPool.c"
#include "./ODPIsrc/dpiRowid.c"
#include "./ODPIsrc/dpiStmt.c"
#include "./ODPIsrc/dpiSubscr.c"
#include "./ODPIsrc/dpiUtils.c"
#include "./ODPIsrc/dpiVar.c"

#endif
