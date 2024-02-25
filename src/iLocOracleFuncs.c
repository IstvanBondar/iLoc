/*
 * Copyright (c) 2018, Istvan Bondar,
 * Written by Istvan Bondar, ibondar2014@gmail.com
 *
 * BSD Open Source License.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifdef ORASQL

#include "iLoc.h"
extern int verbose;
extern FILE *logfp;
extern FILE *errfp;
extern dpiConn *conn;                          /* Oracle database connection */
extern dpiContext *gContext;

/*
 * Print Oracle error message
 */
int PrintOraError(dpiStmt *stmt, char *message)
{
    dpiErrorInfo info;
    dpiContext_getError(gContext, &info);
    fprintf(errfp, "ERROR: %s (%s: %s, %s)\n", message, info.message,
            info.fnName, info.action);
    fprintf(logfp, "ERROR: %s (%s: %s, %s)\n", message, info.message,
            info.fnName, info.action);
    dpiStmt_release(stmt);
    return 1;
}

/*
 * Connect to Oracle DB
 *     The DPI library will also be initialized, if needed.
 */
int OraConnect(char *DBuser, char *DBpasswd, char *constr)
{
    const char *usr, *passwd, *host;
    usr = DBuser;
    passwd = DBpasswd;
    host = constr;
    dpiErrorInfo errorInfo;
    if (dpiContext_create(DPI_MAJOR_VERSION, DPI_MINOR_VERSION, &gContext,
            &errorInfo) < 0) {
        fprintf(errfp, "FN: %s\n", errorInfo.fnName);
        fprintf(errfp, "ACTION: %s\n", errorInfo.action);
        fprintf(errfp, "MSG: %.*s\n", errorInfo.messageLength, errorInfo.message);
        return PrintOraError(NULL, "Unable to create initial DPI context.");
    }
    if (dpiConn_create(gContext, usr, strlen(usr), passwd, strlen(passwd),
                       host, strlen(host), NULL, NULL, &conn) < 0) {
        dpiContext_getError(gContext, &errorInfo);
        fprintf(errfp, "FN: %s\n", errorInfo.fnName);
        fprintf(errfp, "ACTION: %s\n", errorInfo.action);
        fprintf(errfp, "MSG: %.*s\n", errorInfo.messageLength, errorInfo.message);
        fprintf(errfp, "user=%s passwd=%s host=%s\n", usr, passwd, host);
        return PrintOraError(NULL, "Cannot connect to database.");
    }
    if (verbose) fprintf(logfp, "    Connected to db\n");
    return 0;
}

/*
 * Disconnect from Oracle DB
 */
void OraDisconnect(void)
{
    dpiContext_destroy(gContext);
    dpiConn_close(conn, DPI_MODE_CONN_CLOSE_DEFAULT, NULL, 0);
    if (verbose) fprintf(logfp, "    Disconnected from db\n");
}

#endif
/*  EOF */
