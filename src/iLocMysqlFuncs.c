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
#ifdef MYSQLDB

#include "iLoc.h"
extern int verbose;
extern FILE *logfp;
extern FILE *errfp;

extern MYSQL *mysql;
/*
 * Functions:
 *    PrintMysqlError
 *    MysqlConnect
 *    MysqlDisconnect
 */

/*
 * Print SQL error message
 */
void PrintMysqlError(char *message)
{
    fprintf(errfp, "ERROR: %s %s\n", message, mysql_error(mysql));
    fprintf(logfp, "ERROR: %s %s\n", message, mysql_error(mysql));
}

/*
 * Connect to MySQL DB
 *    assumes that connection info is stored in $HOME/.my.cnf
 */
int MysqlConnect(void)
{
    if (mysql_library_init(0, NULL, NULL)) {
        fprintf(stderr, "ERROR: could not initialize MySQL library!\n");
        return 1;
    }
    mysql = mysql_init(mysql);
    mysql_options(mysql, MYSQL_READ_DEFAULT_GROUP, "iLoc");
    if (!mysql_real_connect(mysql, NULL, NULL, NULL, NULL,
                            0, NULL, CLIENT_REMEMBER_OPTIONS)) {
        fprintf(stderr, "ERROR: Failed to connect to database: %s\n",
                mysql_error(mysql));
        return 1;
    }
    if (verbose) fprintf(stderr, "    Connected to db\n");
    return 0;
}

/*
 * Disconnect from MySQL DB
 */
void MysqlDisconnect(void)
{
    mysql_close(mysql);
    mysql_library_end();
    if (verbose) fprintf(stderr, "    Disconnected from db\n");
}

#endif
/*  EOF */
