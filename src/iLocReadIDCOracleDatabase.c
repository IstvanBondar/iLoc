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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS and CONTRIBUTorS "AS IS"
 * and ANY EXPRESS or IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY and FITNESS For A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE For ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, or CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS or SERVICES;
 * LOSS OF USE, DATA, or PROFITS; or BUSINESS INTERRUPTION) HOWEVER CAUSED and
 * ON ANY THEorY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, or TorT
 * (INCLUDING NEGLIGENCE or OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifdef IDCDB
#ifdef ORASQL

#include "iLoc.h"
extern int verbose;
extern FILE *logfp;
extern FILE *errfp;
extern int errorcode;
extern int numAgencies;
extern char agencies[MAXBUF][AGLEN];
extern struct timeval t0;
extern char OutAgency[VALLEN];      /* author for new hypocentres and assocs */
extern char InAgency[VALLEN];                     /* author for input assocs */
extern char InputDB[VALLEN];       /* read data from this DB account, if any */
extern char OutputDB[VALLEN];    /* write results to this DB account, if any */
extern dpiConn *conn;                          /* Oracle database connection */
extern dpiContext *gContext;

/*
 * Functions:
 *    ReadEventFromIDCdatabase
 */

/*
 * Local functions
 */
static int GetIDCEvent(EVREC *ep);
static int GetIDCHypocenter(EVREC *ep, HYPREC h[]);
static int GetIDCMagnitude(EVREC *ep, HYPREC h[], char *magbloc);
static int GetIDCPhases(EVREC *ep, PHAREC p[]);


/*
 *  Title:
 *     ReadEventFromIDCdatabase
 *  Synopsis:
 *     Reads hypocentre, phase and amplitude data from database.
 *     Allocates memory for the arrays of structures needed.
 *  Input Arguments:
 *     ep - pointer to event info
 *  Output Arguments:
 *     hp        - array of hypocentre structures
 *     pp[]      - array of phase structures
 *     magbloc   - reported magnitudes
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     main
 *  Calls:
 *     GetIDCEvent, GetIDCHypocenter, GetIDCMagnitude, GetIDCPhases, Free
 */
int ReadEventFromIDCOraDatabase(EVREC *ep, HYPREC *hp[], PHAREC *pp[],
        char *magbloc)
{
/*
 *  Get prime, numHypo and numPhase from origin table
 *  check if event table exists; create one if not
 */
    if (GetIDCEvent(ep))
        return 1;
    if (verbose)
        fprintf(logfp, "    evid=%d prime=%d numHypo=%d numPhase=%d\n",
                ep->evid, ep->PreferredOrid, ep->numHypo, ep->numPhase);
/*
 *  Allocate memory to hypocenter, phase and stamag structures
 */
    *hp = (HYPREC *)calloc(ep->numHypo, sizeof(HYPREC));
    if (*hp == NULL) {
        fprintf(logfp, "ReadEventFromIDCdatabase: evid %d: cannot allocate memory\n", ep->evid);
        fprintf(errfp, "ReadEventFromIDCdatabase: evid %d: cannot allocate memory\n", ep->evid);
        errorcode = 1;
        return 1;
    }
    *pp = (PHAREC *)calloc(ep->numPhase, sizeof(PHAREC));
    if (*pp == NULL) {
        fprintf(logfp, "ReadEventFromIDCdatabase: evid %d: cannot allocate memory\n", ep->evid);
        fprintf(errfp, "ReadEventFromIDCdatabase: evid %d: cannot allocate memory\n", ep->evid);
        Free(*hp);
        errorcode = 1;
        return 1;
    }
/*
 *  Fill array of hypocenter structures.
 */
    if (GetIDCHypocenter(ep, *hp)) {
        Free(*hp);
        Free(*pp);
        return 1;
    }
/*
 *  get reported magnitudes
 */
    if (GetIDCMagnitude(ep, *hp, magbloc)) {
        Free(*hp);
        Free(*pp);
        return 1;
    }
/*
 *  Fill array of phase structures.
 */
    if (GetIDCPhases(ep, *pp)) {
        Free(*hp);
        Free(*pp);
        return 1;
    }
    return 0;
}

/*
 *  Title:
 *     GetIDCEvent
 *  Synopsis:
 *     Gets orid, event type of the preferred origin,
 *     and the number of hypocenters, phases, stations and reporting agencies.
 *  Input Arguments:
 *     ep - pointer to structure containing event information.
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     ReadEventFromIDCdatabase
 *  Calls:
 *     PrintOraError, DropSpace
 */
static int GetIDCEvent(EVREC *ep)
{
    dpiStmt *stmt;
    dpiData *colData[3];
    dpiNativeTypeNum nativeTypeNum;
    const char *query;
    char psql[2048], sql[2048], auth[AGLEN];
    uint32_t numQueryColumns, bufferRowIndex;
    int found = 0, i;
    ep->numHypo = ep->PreferredOrid = ep->ISChypid = 0;
    ep->OutputDBPreferredOrid = ep->OutputDBISChypid = 0;
/*
 *  Get hypocentre count
 */
    sprintf(sql, "select count(*) from %sorigin where evid = %d",
            InputDB, ep->evid);
    query = sql;
    if (verbose > 2) fprintf(logfp, "            GetIDCEvent: %s\n", query);
    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
        return PrintOraError(stmt, "GetIDCEvent: prepareStmt");
    if (dpiStmt_execute(stmt, 0, &numQueryColumns) < 0)
        return PrintOraError(stmt, "GetIDCEvent: execute");
    do {
        if (dpiStmt_fetch(stmt, &found, &bufferRowIndex) < 0)
            return PrintOraError(stmt, "GetIDCEvent: fetch");
        if (!found) break;
        if (dpiStmt_getQueryValue(stmt, 1, &nativeTypeNum, &colData[0]) < 0)
            return PrintOraError(stmt, "GetIDCEvent: getQueryValue");
        ep->numHypo = (int)colData[0]->value.asDouble;
    } while (found);
    dpiStmt_release(stmt);
    if (ep->numHypo == 0) {
        fprintf(errfp, "GetIDCEvent %d: no hypocentres found\n", ep->evid);
        fprintf(logfp, "GetIDCEvent %d: no hypocentres found\n", ep->evid);
        return 1;
    }
    if (verbose > 1) fprintf(logfp, "        %d hypocentres\n", ep->numHypo);
/*
 *  Get orid, auth and event type of preferred origin
 *      As event table is not populated (IDC practice),
 *      assume that the largest orid is the preferred one
 */
    sprintf(psql, "select orid, coalesce(auth, 'IDC'),              \
                          coalesce(replace(etype, '-', 'ke'), 'ke') \
                     from %sorigin where evid = %d order by orid",
            InputDB, ep->evid);
    DropSpace(psql, sql);
    query = sql;
    if (verbose > 2) fprintf(logfp, "            GetIDCEvent: %s\n", query);
    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
        return PrintOraError(stmt, "GetIDCEvent: prepareStmt");
    if (dpiStmt_execute(stmt, 0, &numQueryColumns) < 0)
        return PrintOraError(stmt, "GetIDCEvent: execute");
    do {
        if (dpiStmt_fetch(stmt, &found, &bufferRowIndex) < 0)
            return PrintOraError(stmt, "GetIDCEvent: fetch");
        if (!found) break;
        if (dpiStmt_getQueryValue(stmt, 1, &nativeTypeNum, &colData[0]) < 0 ||
            dpiStmt_getQueryValue(stmt, 2, &nativeTypeNum, &colData[1]) < 0 ||
            dpiStmt_getQueryValue(stmt, 3, &nativeTypeNum, &colData[2]) < 0)
            return PrintOraError(stmt, "GetIDCEvent: getQueryValue");
        ep->PreferredOrid = (int)colData[0]->value.asInt64;
        memcpy(auth, colData[1]->value.asBytes.ptr, colData[1]->value.asBytes.length);
        auth[colData[1]->value.asBytes.length] = '\0';
        if (streq(auth, OutAgency))
            ep->ISChypid = ep->PreferredOrid;
        memcpy(ep->etype, colData[2]->value.asBytes.ptr, colData[2]->value.asBytes.length);
        ep->etype[colData[2]->value.asBytes.length] = '\0';
    } while (found);
    dpiStmt_release(stmt);
/*
 *  Get preferred origin in OutputDB if it differs from InputDB
 *      Assumes that evid is the same in the two accounts!
 */
    if (strcmp(InputDB, OutputDB)) {
        sprintf(psql, "select orid, auth from %sorigin \
                        where evid = %d order by orid",
            OutputDB, ep->evid);
        DropSpace(psql, sql);
        query = sql;
        if (verbose > 2) fprintf(logfp, "            GetIDCEvent: %s\n", query);
        if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
            return PrintOraError(stmt, "GetIDCEvent: prepareStmt");
        if (dpiStmt_execute(stmt, 0, &numQueryColumns) < 0)
            return PrintOraError(stmt, "GetIDCEvent: execute");
        do {
            if (dpiStmt_fetch(stmt, &found, &bufferRowIndex) < 0)
                return PrintOraError(stmt, "GetIDCEvent: fetch");
            if (!found) break;
            if (dpiStmt_getQueryValue(stmt, 1, &nativeTypeNum, &colData[0]) < 0 ||
                dpiStmt_getQueryValue(stmt, 2, &nativeTypeNum, &colData[1]) < 0)
                return PrintOraError(stmt, "GetIDCEvent: getQueryValue");
            ep->OutputDBPreferredOrid = (int)colData[0]->value.asInt64;
            memcpy(auth, colData[1]->value.asBytes.ptr, colData[1]->value.asBytes.length);
            auth[colData[1]->value.asBytes.length] = '\0';
            if (streq(auth, OutAgency))
                ep->OutputDBISChypid = ep->OutputDBPreferredOrid;
        } while (found);
        dpiStmt_release(stmt);
    }
/*
 *  Get phase and station counts
 */
    sprintf(psql, "select count(arid), count(distinct sta) \
                     from %sassoc where orid = %d",
            InputDB, ep->PreferredOrid);
    DropSpace(psql, sql);
    query = sql;
    if (verbose > 2) fprintf(logfp, "            GetIDCEvent: %s\n", query);
    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
        return PrintOraError(stmt, "GetIDCEvent: prepareStmt");
    if (dpiStmt_execute(stmt, 0, &numQueryColumns) < 0)
        return PrintOraError(stmt, "GetIDCEvent: execute");
    do {
        if (dpiStmt_fetch(stmt, &found, &bufferRowIndex) < 0)
            return PrintOraError(stmt, "GetIDCEvent: fetch");
        if (!found) break;
        if (dpiStmt_getQueryValue(stmt, 1, &nativeTypeNum, &colData[0]) < 0 ||
            dpiStmt_getQueryValue(stmt, 2, &nativeTypeNum, &colData[1]) < 0)
            return PrintOraError(stmt, "GetIDCEvent: getQueryValue");
        ep->numPhase = (int)colData[0]->value.asDouble;
        ep->numSta = ep->numReading = (int)colData[1]->value.asDouble;
    } while (found);
    dpiStmt_release(stmt);
    if (ep->numPhase == 0) {
        fprintf(errfp, "GetIDCEvent %d: no phases found\n", ep->evid);
        fprintf(logfp, "GetIDCEvent %d: no phases found\n", ep->evid);
        return 1;
    }
    if (ep->numSta == 0) {
        fprintf(errfp, "GetIDCEvent %d: no stations found\n", ep->evid);
        fprintf(logfp, "GetIDCEvent %d: no stations found\n", ep->evid);
        return 1;
    }
    if (verbose > 1)
        fprintf(logfp, "        %d phases, %d readings, %d stations\n",
                ep->numPhase, ep->numReading, ep->numSta);
/*
 *  Get agencies that reported phases
 */
    sprintf(psql, "select distinct coalesce(r.auth, 'IDC') \
                     from %sassoc a, %sarrival r           \
                    where a.orid = %d and r.arid = a.arid",
            InputDB, InputDB, ep->PreferredOrid);
    DropSpace(psql, sql);
    query = sql;
    if (verbose > 2) fprintf(logfp, "            GetIDCEvent: %s\n", query);
    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
        return PrintOraError(stmt, "GetIDCEvent: prepareStmt");
    if (dpiStmt_execute(stmt, 0, &numQueryColumns) < 0)
        return PrintOraError(stmt, "GetIDCEvent: execute");
    i = 0;
    do {
        if (dpiStmt_fetch(stmt, &found, &bufferRowIndex) < 0)
            return PrintOraError(stmt, "GetIDCEvent: fetch");
        if (!found) break;
        if (dpiStmt_getQueryValue(stmt, 1, &nativeTypeNum, &colData[0]) < 0)
            return PrintOraError(stmt, "GetIDCEvent: getQueryValue");
        memcpy(agencies[i], colData[0]->value.asBytes.ptr, colData[0]->value.asBytes.length);
        agencies[i][colData[0]->value.asBytes.length] = '\0';
        i++;
    } while (found);
    dpiStmt_release(stmt);
    ep->numAgency = numAgencies = i;
    if (ep->numAgency == 0) {
        fprintf(errfp, "GetIDCEvent %d: no agencies found\n", ep->evid);
        fprintf(logfp, "GetIDCEvent %d: no agencies found\n", ep->evid);
        return 1;
    }
    if (verbose > 1) fprintf(logfp, "        %d agencies\n", ep->numAgency);
    return 0;
}

/*
 *  Title:
 *     GetIDCHypocenter
 *  Synopsis:
 *     Reads reported hypocentres for an event from DB.
 *         Fields from the origin table:
 *            orid
 *            origin epoch time
 *            lat, lon
 *            depth (depdp if depth is null)
 *            depth type
 *            nass, ndef
 *            author
 *        Fields from the origerr table:
 *            orid
 *            sminax, smajax, strike
 *            stime, sdepth
 *            sdobs
 *     Sorts hypocentres so that the first is the preferred origin.
 *  Input Arguments:
 *     ep  - pointer to event info
 *     h[] - array of hypocentres.
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     ReadEventFromIDCdatabase
 * Calls:
 *     PrintOraError, DropSpace
 */
static int GetIDCHypocenter(EVREC *ep, HYPREC h[])
{
    dpiStmt *stmt;
    dpiData *data;
    dpiNativeTypeNum nativeTypeNum;
    const char *query;
    uint32_t numQueryColumns, bufferRowIndex;
    int found = 0, i;
    char sql[2048], psql[2048], Auth[AGLEN], PrevAuth[AGLEN];
/*
 *  Get preferred origin
 */
    sprintf(psql, "select o.orid, o.time, o.lat, o.lon,             \
                          coalesce(o.depth, o.depdp), o.dtype,      \
                          o.nass, o.ndef, coalesce(o.auth, 'IDC'),  \
                          e.sdobs, e.stime, e.sdepth,               \
                          e.smajax, e.sminax, e.strike              \
                     from %sorigin o, %sorigerr e                   \
                    where o.orid = %d and o.evid = %d               \
                      and o.orid = e.orid",
            InputDB, InputDB, ep->PreferredOrid, ep->evid);
    DropSpace(psql, sql);
    query = sql;
    if (verbose > 2) fprintf(logfp, "            GetIDCHypocenter: %s\n", query);
    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
        return PrintOraError(stmt, "GetIDCHypocenter: prepareStmt");
    if (dpiStmt_execute(stmt, 0, &numQueryColumns) < 0)
        return PrintOraError(stmt, "GetIDCHypocenter: execute");
    if (dpiStmt_fetch(stmt, &found, &bufferRowIndex) < 0)
        return PrintOraError(stmt, "GetIDCHypocenter: fetch");
    if (!found) {
        fprintf(errfp, "GetIDCHypocenter: no hypocenter found\n");
        fprintf(logfp, "GetIDCHypocenter: no hypocenter found\n");
        return 1;
    }
/*
 *  initialize
 */
    i = 0;
    h[i].depfix = h[i].epifix = h[i].timfix = h[i].rank = 0;
    strcpy(h[i].etype, ep->etype);
    h[i].nsta = h[i].ndefsta = NULLVAL;
    h[i].mindist = h[i].maxdist = h[i].azimgap = h[i].sgap = NULLVAL;
/*
 *  populate
 */
    if (dpiStmt_getQueryValue(stmt, 1, &nativeTypeNum, &data) < 0)
        return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
    h[i].hypid = (int)data->value.asInt64;
    if (dpiStmt_getQueryValue(stmt, 2, &nativeTypeNum, &data) < 0)
        return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
    h[i].time = data->value.asDouble;
    if (dpiStmt_getQueryValue(stmt, 3, &nativeTypeNum, &data) < 0)
        return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
    h[i].lat = data->value.asDouble;
    if (dpiStmt_getQueryValue(stmt, 4, &nativeTypeNum, &data) < 0)
        return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
    h[i].lon = data->value.asDouble;
    if (dpiStmt_getQueryValue(stmt, 5, &nativeTypeNum, &data) < 0)
        return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
    if (data->isNull) { h[i].depth = NULLVAL; }
    else              { h[i].depth = data->value.asDouble; }
    if (dpiStmt_getQueryValue(stmt, 6, &nativeTypeNum, &data) < 0)
        return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
    if (streq(data->value.asBytes.ptr, "r") ||
        streq(data->value.asBytes.ptr, "g"))
        h[i].depfix = 1;
    if (dpiStmt_getQueryValue(stmt, 7, &nativeTypeNum, &data) < 0)
        return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
    if (data->isNull) { h[i].nass = NULLVAL; }
    else              { h[i].nass = (int)data->value.asInt64; }
    if (dpiStmt_getQueryValue(stmt, 8, &nativeTypeNum, &data) < 0)
        return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
    if (data->isNull) { h[i].ndef = NULLVAL; }
    else              { h[i].ndef = (int)data->value.asInt64; }
    if (dpiStmt_getQueryValue(stmt, 9, &nativeTypeNum, &data) < 0)
        return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
    memcpy(h[i].agency, data->value.asBytes.ptr, data->value.asBytes.length);
    h[i].agency[data->value.asBytes.length] = '\0';
    strcpy(h[i].rep, h[i].agency);
    if (dpiStmt_getQueryValue(stmt, 10, &nativeTypeNum, &data) < 0)
        return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
    if (data->isNull) { h[i].sdobs = NULLVAL; }
    else              { h[i].sdobs = data->value.asDouble; }
    if (dpiStmt_getQueryValue(stmt, 11, &nativeTypeNum, &data) < 0)
        return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
    if (data->isNull) { h[i].stime = NULLVAL; }
    else              { h[i].stime = data->value.asDouble; }
    if (dpiStmt_getQueryValue(stmt, 12, &nativeTypeNum, &data) < 0)
        return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
    if (data->isNull) { h[i].sdepth = NULLVAL; }
    else              { h[i].sdepth = data->value.asDouble; }
    if (dpiStmt_getQueryValue(stmt, 13, &nativeTypeNum, &data) < 0)
        return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
    if (data->isNull) { h[i].smajax = NULLVAL; }
    else              { h[i].smajax = data->value.asDouble; }
    if (dpiStmt_getQueryValue(stmt, 14, &nativeTypeNum, &data) < 0)
        return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
    if (data->isNull) { h[i].sminax = NULLVAL; }
    else              { h[i].sminax = data->value.asDouble; }
    if (dpiStmt_getQueryValue(stmt, 15, &nativeTypeNum, &data) < 0)
        return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
    if (data->isNull) { h[i].strike = NULLVAL; }
    else              { h[i].strike = data->value.asDouble; }
    if (verbose > 1)
        fprintf(logfp, "        i=%d evid=%d hypid=%d agency=%s\n",
                i, ep->evid, h[i].hypid, h[i].agency);
    dpiStmt_release(stmt);
/*
 *  If the preferred origin is the only hypocenter, we are done
 */
    if (ep->numHypo == 1)
        return 0;
/*
 *  Get rest of the hypocenters
 */
    strcpy(PrevAuth, h[0].agency);
    sprintf(psql, "select o.orid, o.time, o.lat, o.lon,             \
                          coalesce(o.depth, o.depdp), o.dtype,      \
                          o.nass, o.ndef, coalesce(o.auth, 'IDC'),  \
                          e.sdobs, e.stime, e.sdepth,               \
                          e.smajax, e.sminax, e.strike              \
                     from %sorigin o, %sorigerr e                   \
                    where o.orid <> %d and o.evid = %d              \
                      and o.orid = e.orid                           \
                    order by o.auth, o.lddate",
            InputDB, InputDB, ep->PreferredOrid, ep->evid);
    DropSpace(psql, sql);
    query = sql;
    if (verbose > 2) fprintf(logfp, "            GetIDCHypocenter: %s\n", query);
    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
        return PrintOraError(stmt, "GetIDCHypocenter: prepareStmt");
    if (dpiStmt_execute(stmt, 0, &numQueryColumns) < 0)
        return PrintOraError(stmt, "GetIDCHypocenter: execute");
    do {
        if (dpiStmt_fetch(stmt, &found, &bufferRowIndex) < 0)
            return PrintOraError(stmt, "GetIDCHypocenter: fetch");
        if (!found) break;
        if (dpiStmt_getQueryValue(stmt, 9, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
        memcpy(Auth, data->value.asBytes.ptr, data->value.asBytes.length);
        Auth[data->value.asBytes.length] = '\0';
/*
 *      consider distinct agencies only
 */
        if (streq(Auth, PrevAuth))
            continue;
/*
 *      initialize
 */
        h[i].depfix = h[i].epifix = h[i].timfix = h[i].rank = 0;
        strcpy(h[i].etype, ep->etype);
        h[i].nsta = h[i].ndefsta = NULLVAL;
        h[i].mindist = h[i].maxdist = h[i].azimgap = h[i].sgap = NULLVAL;
/*
 *      populate
 */
        if (dpiStmt_getQueryValue(stmt, 1, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
        h[i].hypid = (int)data->value.asInt64;
        if (dpiStmt_getQueryValue(stmt, 2, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
        h[i].time = data->value.asDouble;
        if (dpiStmt_getQueryValue(stmt, 3, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
        h[i].lat = data->value.asDouble;
        if (dpiStmt_getQueryValue(stmt, 4, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
        h[i].lon = data->value.asDouble;
        if (dpiStmt_getQueryValue(stmt, 5, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
        if (data->isNull) { h[i].depth = NULLVAL; }
        else              { h[i].depth = data->value.asDouble; }
        if (dpiStmt_getQueryValue(stmt, 6, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
        if (streq(data->value.asBytes.ptr, "r") ||
            streq(data->value.asBytes.ptr, "g"))
            h[i].depfix = 1;
        if (dpiStmt_getQueryValue(stmt, 7, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
        if (data->isNull) { h[i].nass = NULLVAL; }
        else              { h[i].nass = (int)data->value.asInt64; }
        if (dpiStmt_getQueryValue(stmt, 8, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
        if (data->isNull) { h[i].ndef = NULLVAL; }
        else              { h[i].ndef = (int)data->value.asInt64; }
        strcpy(h[i].agency, Auth);
        strcpy(h[i].rep, Auth);
        if (dpiStmt_getQueryValue(stmt, 10, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
        if (data->isNull) { h[i].sdobs = NULLVAL; }
        else              { h[i].sdobs = data->value.asDouble; }
        if (dpiStmt_getQueryValue(stmt, 11, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
        if (data->isNull) { h[i].stime = NULLVAL; }
        else              { h[i].stime = data->value.asDouble; }
        if (dpiStmt_getQueryValue(stmt, 12, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
        if (data->isNull) { h[i].sdepth = NULLVAL; }
        else              { h[i].sdepth = data->value.asDouble; }
        if (dpiStmt_getQueryValue(stmt, 13, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
        if (data->isNull) { h[i].smajax = NULLVAL; }
        else              { h[i].smajax = data->value.asDouble; }
        if (dpiStmt_getQueryValue(stmt, 14, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
        if (data->isNull) { h[i].sminax = NULLVAL; }
        else              { h[i].sminax = data->value.asDouble; }
        if (dpiStmt_getQueryValue(stmt, 15, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCHypocenter: getQueryValue");
        if (data->isNull) { h[i].strike = NULLVAL; }
        else              { h[i].strike = data->value.asDouble; }
        if (verbose > 1)
            fprintf(logfp, "        i=%d evid=%d hypid=%d agency=%s\n",
                        i, ep->evid, h[i].hypid, h[i].agency);
        strcpy(PrevAuth, Auth);
        i++;
    } while (found);
    dpiStmt_release(stmt);
    if (i > ep->numHypo) {
        fprintf(errfp, "GetIDCHypocenter: unexpected numHypo %d\n", i);
        fprintf(logfp, "GetIDCHypocenter: unexpected numHypo %d\n", i);
        return 1;
    }
/*
 *  number of hypocentres reported by distinct agencies
 */
    ep->numHypo = i;
    return 0;
}

/*
 *  Title:
 *     GetIDCMagnitude
 *  Synopsis:
 *     Reads reported magnitudes for an event from DB.
 *         Fields from the Magnitude table:
 *            agency
 *            type
 *            magnitude
 *            magnitude uncertainty
 *            nsta
 *     Puts preferred magnitude last into magbloc.
 *  Input Arguments:
 *     ep  - pointer to event info
 *     h[] - array of hypocentres.
 *  Output Arguments:
 *     magbloc - reported magnitudes
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     ReadEventFromIDCdatabase
 * Calls:
 *     PrintOraError, DropSpace
 */
static int GetIDCMagnitude(EVREC *ep, HYPREC h[], char *magbloc)
{
    dpiStmt *stmt;
    dpiData *data;
    dpiNativeTypeNum nativeTypeNum;
    const char *query;
    uint32_t numQueryColumns, bufferRowIndex;
    int found = 0, i, nsta;
    char sql[2048], psql[2048], auth[AGLEN], mtype[AGLEN];
    double mag, smag;
/*
 *  accumulate magnitude block in a string for output
 */
    strcpy(magbloc, "");
    for (i = 0; i < ep->numHypo; i++) {
        sprintf(psql, "select magnitude, magtype, uncertainty, \
                              nsta, coalesce(auth, 'IDC')      \
                         from %snetmag                         \
                        where orid = %d                        \
                        order by magtype",
                InputDB, h[i].hypid);
        DropSpace(psql, sql);
        query = sql;
        if (verbose > 2)
            fprintf(logfp, "            GetIDCMagnitude: %s\n", query);
        if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
             return PrintOraError(stmt, "GetIDCMagnitude: prepareStmt");
        if (dpiStmt_execute(stmt, 0, &numQueryColumns) < 0)
            return PrintOraError(stmt, "GetIDCMagnitude: execute");
        do {
            if (dpiStmt_fetch(stmt, &found, &bufferRowIndex) < 0)
                return PrintOraError(stmt, "GetIDCMagnitude: fetch");
            if (!found) break;
            if (dpiStmt_getQueryValue(stmt, 1, &nativeTypeNum, &data) < 0)
                return PrintOraError(stmt, "GetIDCMagnitude: getQueryValue");
            mag = data->value.asDouble;
            if (dpiStmt_getQueryValue(stmt, 2, &nativeTypeNum, &data) < 0)
                return PrintOraError(stmt, "GetIDCMagnitude: getQueryValue");
            memcpy(mtype, data->value.asBytes.ptr, data->value.asBytes.length);
            mtype[data->value.asBytes.length] = '\0';
            if (dpiStmt_getQueryValue(stmt, 3, &nativeTypeNum, &data) < 0)
                return PrintOraError(stmt, "GetIDCMagnitude: getQueryValue");
            smag = data->value.asDouble;
            if (dpiStmt_getQueryValue(stmt, 4, &nativeTypeNum, &data) < 0)
                return PrintOraError(stmt, "GetIDCMagnitude: getQueryValue");
            nsta = (int)data->value.asInt64;
            if (dpiStmt_getQueryValue(stmt, 5, &nativeTypeNum, &data) < 0)
                return PrintOraError(stmt, "GetIDCMagnitude: getQueryValue");
            memcpy(auth, data->value.asBytes.ptr, data->value.asBytes.length);
            auth[data->value.asBytes.length] = '\0';
            sprintf(magbloc, "%s%-5s %4.1f ", magbloc, mtype, mag);
            if (smag < 0) sprintf(magbloc, "%s    ", magbloc);
            else          sprintf(magbloc, "%s%3.1f ", magbloc, smag);
            if (nsta < 0) sprintf(magbloc, "%s     ", magbloc);
            else          sprintf(magbloc, "%s%4d ", magbloc, nsta);
            sprintf(magbloc, "%s%-9s %d\n", magbloc, auth, h[i].hypid);
        } while (found);
        dpiStmt_release(stmt);
    }
    return 0;
}

/*
 *  Title:
 *     GetIDCPhases
 *  Synopsis:
 *     Reads phase and amplitude data associated to the preferred origin
 *         Fields from the Arrival, Pick, Station tables:
 *             arrival id, agency,
 *             arrival time, associated phase, reported phase,
 *             station, channel,
 *             station latitude, longitude, elevation,
 *             slowness, azimuth, distance,
 *             onset, polarity
 *         Fields from the Amplitude table:
 *             amplitude id, magtype,
 *             amplitude, period, channel, snr
 *     Sorts phase structures so that they ordered by delta, prista, rdid, time
 *     Marks initial arrivals in readings
 *  Input Arguments:
 *     ep  - pointer to event info
 *     p[] - array of phase structures
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     ReadEventFromIDCdatabase
 *  Calls:
 *     PrintOraError, DropSpace
 */
static int GetIDCPhases(EVREC *ep, PHAREC p[])
{
    dpiStmt *stmt;
    dpiData *data;
    dpiNativeTypeNum nativeTypeNum;
    const char *query;
    uint32_t numQueryColumns, bufferRowIndex;
    int found = 0, i, m, rdid;
    char sql[2048], psql[2048], PrevAuth[AGLEN], PrevSta[STALEN];
    int numPhase = 0, arid = 0, ampid = 0;
    double amp = 0., per = 0.;
    char chan[10], w[10];
    char amptype[10], ampchan[10];
/*
 *  Get phases associated to the preferred origin
 */
    sprintf(psql, "select a.arid, a.sta, r.chan, a.delta, a.phase, \
                          r.iphase, r.time, r.slow, r.azimuth,     \
                          coalesce(r.auth, '%s'), r.fm, r.qual,    \
                          s.lat, s.lon, 1000. * s.elev             \
                     from %sassoc a, %sarrival r, %ssite s         \
                    where a.orid = %d                              \
                      and a.arid = r.arid                          \
                      and s.sta = a.sta                            \
                      and r.jdate >= s.ondate                      \
                      and (s.offdate = -1 or r.jdate <= s.offdate) \
                    order by a.arid",
            InAgency, InputDB, InputDB, InputDB, ep->PreferredOrid);
    DropSpace(psql, sql);
    query = sql;
    if (verbose > 2)  fprintf(logfp, "            GetIDCPhases: %s\n", query);
    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
         return PrintOraError(stmt, "GetIDCPhases: prepareStmt");
    if (dpiStmt_execute(stmt, 0, &numQueryColumns) < 0)
        return PrintOraError(stmt, "GetIDCPhases: execute");
    i = 0;
    do {
        if (dpiStmt_fetch(stmt, &found, &bufferRowIndex) < 0)
            return PrintOraError(stmt, "GetIDCPhases: fetch");
        if (!found || i == ep->numPhase) break;
/*
 *      initialize
 */
        p[i].repid = NULLVAL;
        p[i].comp = ' ';
        p[i].delta = p[i].slow = p[i].azim = NULLVAL;
        p[i].phase_fixed = p[i].force_undef = p[i].init = 0;
        p[i].numamps = 0;
        p[i].hypid = ep->PreferredOrid;
        strcpy(p[i].pch, "???");
/*
 *      populate
 */
        if (dpiStmt_getQueryValue(stmt, 1, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        p[i].phid = (int)data->value.asInt64;
        sprintf(p[i].arrid, "%d", p[i].phid);
        if (verbose > 3)
            fprintf(logfp, "        i=%d phid=%d\n", i, p[i].phid);
        if (dpiStmt_getQueryValue(stmt, 2, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        memcpy(p[i].sta, data->value.asBytes.ptr, data->value.asBytes.length);
        p[i].sta[data->value.asBytes.length] = '\0';
        strcpy(p[i].prista, p[i].sta);
        strcpy(p[i].deploy, "IR");
        strcpy(p[i].lcn, "--");
        sprintf(p[i].fdsn, "FDSN.IR.%s.--", p[i].sta);
        if (dpiStmt_getQueryValue(stmt, 3, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        memcpy(chan, data->value.asBytes.ptr, data->value.asBytes.length);
        chan[data->value.asBytes.length] = '\0';
        if (chan[2] == 'Z' || chan[2] == 'E' || chan[2] == 'N') {
            strcpy(p[i].pch, chan);
            p[i].comp = chan[2];
        }
        if (dpiStmt_getQueryValue(stmt, 4, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        if (data->isNull) { p[i].delta = NULLVAL; }
        else              { p[i].delta = data->value.asDouble; }
        if (dpiStmt_getQueryValue(stmt, 5, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        memcpy(p[i].phase, data->value.asBytes.ptr, data->value.asBytes.length);
        p[i].phase[data->value.asBytes.length] = '\0';
        if (dpiStmt_getQueryValue(stmt, 6, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        memcpy(p[i].ReportedPhase, data->value.asBytes.ptr, data->value.asBytes.length);
        p[i].ReportedPhase[data->value.asBytes.length] = '\0';
        if (dpiStmt_getQueryValue(stmt, 7, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        if (data->isNull) { p[i].time = NULLVAL; }
        else              { p[i].time = data->value.asDouble; }
        if (dpiStmt_getQueryValue(stmt, 8, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        if (data->isNull) { p[i].slow = NULLVAL; }
        else              { p[i].slow = data->value.asDouble; }
        if (dpiStmt_getQueryValue(stmt, 9, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        if (data->isNull) { p[i].azim = NULLVAL; }
        else              { p[i].azim = data->value.asDouble; }
        if (dpiStmt_getQueryValue(stmt, 10, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        memcpy(p[i].rep, data->value.asBytes.ptr, data->value.asBytes.length);
        p[i].rep[data->value.asBytes.length] = '\0';
        strcpy(p[i].auth, p[i].rep);
        strcpy(p[i].agency, p[i].rep);
        if (dpiStmt_getQueryValue(stmt, 11, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        memcpy(w, data->value.asBytes.ptr, data->value.asBytes.length);
        w[data->value.asBytes.length] = '\0';
        p[i].sp_fm = w[0];
        p[i].lp_fm = w[1];
        if (dpiStmt_getQueryValue(stmt, 12, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        memcpy(w, data->value.asBytes.ptr, data->value.asBytes.length);
        w[data->value.asBytes.length] = '\0';
        p[i].detchar = w[0];
        if (dpiStmt_getQueryValue(stmt, 13, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        p[i].StaLat = data->value.asDouble;
        if (dpiStmt_getQueryValue(stmt, 14, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        p[i].StaLon = data->value.asDouble;
        if (dpiStmt_getQueryValue(stmt, 15, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        if (data->isNull) { p[i].StaElev = NULLVAL; }
        else              { p[i].StaElev = data->value.asDouble; }
        i++;
    } while (found);
    dpiStmt_release(stmt);
    numPhase = i;
    if (numPhase != ep->numPhase) {
        fprintf(errfp, "GetIDCPhases: unexpected number phases!\n");
        fprintf(errfp, "    evid=%d orid=%d epnumPhase=%d numPhase=%d\n",
                ep->evid, ep->PreferredOrid, ep->numPhase, numPhase);
        fprintf(errfp, "    often due to a missing station in site\n");
        fprintf(logfp, "GetIDCPhases: unexpected number phases!\n");
        fprintf(logfp, "    evid=%d orid=%d epnumPhase=%d numPhase=%d\n",
                ep->evid, ep->PreferredOrid, ep->numPhase, numPhase);
        fprintf(logfp, "    often due to a missing station in site\n");
        if (numPhase < ep->numPhase)
            ep->numPhase = numPhase;
        else
            return 1;
    }
/*
 *  get amplitudes
 */
    sprintf(psql, "select m.arid, m.ampid, m.amp, m.per, \
                          coalesce(m.amptype, ' '),      \
                          coalesce(m.chan, '\?\?\?')     \
                     from %samplitude m, %sassoc a       \
                    where a.orid = %d                    \
                      and m.arid = a.arid                \
                 order by m.arid",
            InputDB, InputDB, ep->PreferredOrid);
    DropSpace(psql, sql);
    query = sql;
    if (verbose > 2)  fprintf(logfp, "            GetIDCPhases: %s\n", query);
    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
         return PrintOraError(stmt, "GetIDCPhases: prepareStmt");
    if (dpiStmt_execute(stmt, 0, &numQueryColumns) < 0)
        return PrintOraError(stmt, "GetIDCPhases: execute");
    do {
        if (dpiStmt_fetch(stmt, &found, &bufferRowIndex) < 0)
            return PrintOraError(stmt, "GetIDCPhases: fetch");
        if (!found) break;
        if (dpiStmt_getQueryValue(stmt, 1, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        arid = (int)data->value.asInt64;
        if (dpiStmt_getQueryValue(stmt, 2, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        ampid = (int)data->value.asInt64;
        if (dpiStmt_getQueryValue(stmt, 3, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        if (data->isNull) { amp = NULLVAL; }
        else              { amp = data->value.asDouble; }
        if (dpiStmt_getQueryValue(stmt, 4, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        if (data->isNull) { per = NULLVAL; }
        else              { per = data->value.asDouble; }
        if (dpiStmt_getQueryValue(stmt, 5, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        memcpy(amptype, data->value.asBytes.ptr, data->value.asBytes.length);
        amptype[data->value.asBytes.length] = '\0';
        if (dpiStmt_getQueryValue(stmt, 6, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDCPhases: getQueryValue");
        memcpy(ampchan, data->value.asBytes.ptr, data->value.asBytes.length);
        ampchan[data->value.asBytes.length] = '\0';
        for (i = 0; i < ep->numPhase; i++) {
            m = p[i].numamps;
            if (arid == p[i].phid) {
/*
 *              initializations
 */
                p[i].a[m].ampdef = p[i].a[m].magid = 0;
                p[i].a[m].magnitude = p[i].a[m].logat = NULLVAL;
                strcpy(p[i].a[m].magtype, "");
/*
 *              check for valid peak-to-peak or zero-to-peak amplitudes
 */
                if (strstr(amptype, "A5") != NULL ||
                    strstr(amptype, "ALR") != NULL) {
                    if (strchr(amptype, '2') == NULL)
                        p[i].a[m].amp = amp;
                    else
                        p[i].a[m].amp = amp / 2.;
                }
                else
                    continue;
                p[i].a[m].ampid = ampid;
                sprintf(p[i].a[m].amplitudeid, "%d", ampid);
                p[i].a[m].per = per;
/*
 *              Find out component
 */
                strcpy(p[i].a[m].ach, ampchan);
                if (streq(ampchan, "mb_beam"))
                    p[i].a[m].comp = 'Z';
                else if (toupper(ampchan[2]) == 'Z' ||
                         toupper(ampchan[1]) == 'Z')
                    p[i].a[m].comp = 'Z';
                else if (toupper(ampchan[2]) == 'E' ||
                         toupper(ampchan[1]) == 'E')
                    p[i].a[m].comp = 'E';
                else if (toupper(ampchan[2]) == 'N' ||
                         toupper(ampchan[1]) == 'N')
                    p[i].a[m].comp = 'N';
                else if (p[i].comp)
                    p[i].a[m].comp = p[i].comp;
                else
                    p[i].a[m].comp = ' ';
                p[i].numamps++;
            }
        }
    } while (found);
    dpiStmt_release(stmt);
/*
 *  Sort phase structures by increasing delta, sta, time, auth
 */
    if (verbose > 2)
        fprintf(logfp, "            GetIDCPhases: sort phases\n");
    SortPhasesFromISF(ep->numPhase, p);
/*
 *  Split phases into readings
 */
    PrevSta[0] = '\0';
    strcpy(PrevAuth, InAgency);
    rdid = 1;
    for (i = 0; i < ep->numPhase; i++) {
        if (strcmp(p[i].sta, PrevSta)) {
            p[i].rdid = rdid++;
            p[i].init = 1;
        }
        else if (strcmp(p[i].auth, PrevAuth)) {
            p[i].rdid = rdid++;
            p[i].init = 1;
        }
        else {
            p[i].rdid = p[i-1].rdid;
        }
        strcpy(PrevSta, p[i].sta);
        strcpy(PrevAuth, p[i].auth);
    }
    ep->numReading = rdid - 1;
/*
 *  Sort phase structures again so that they ordered by
 *  delta, prista, rdid, time.
 */
    SortPhasesFromDatabase(ep->numPhase, p);
    if (verbose > 2)
        PrintPhases(ep->numPhase, p);
    return 0;
}

#endif  /* ORASQL */
#endif  /* IDCDB */

/*  EOF */
