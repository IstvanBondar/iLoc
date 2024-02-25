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
#ifdef SC3DB
#ifdef MYSQLDB

#include "iLoc.h"
extern int verbose;
extern FILE *logfp;
extern FILE *errfp;
extern int errorcode;
extern int numAgencies;
extern char agencies[MAXBUF][AGLEN];
extern struct timeval t0;
extern char InputDB[VALLEN];       /* read data from this DB account, if any */
extern char OutputDB[VALLEN];    /* write results to this DB account, if any */
extern char NextidDB[VALLEN];               /* get new ids from this account */
extern char InAgency[VALLEN];                     /* author for input assocs */
extern char OutAgency[VALLEN];      /* author for new hypocentres and assocs */
extern MYSQL *mysql;

/*
 * Functions:
 *    ReadEventFromSC3MysqlDatabase
 */

/*
 * Local functions
 */
static int GetSC3Event(EVREC *ep);
static int GetSC3Hypocenter(EVREC *ep, HYPREC h[]);
static int GetSC3Magnitude(EVREC *ep, HYPREC h[], char *magbloc);
static int GetSC3Phases(EVREC *ep, PHAREC p[]);


/*
 *  Title:
 *     ReadEventFromSC3MysqlDatabase
 *  Synopsis:
 *     Reads hypocentre, phase and amplitude data from SeisComp3 MySQL database
 *     Allocates memory for the arrays of structures needed.
 *  Input Arguments:
 *     ep - pointer to event info
 *  Output Arguments:
 *     hp      - array of hypocentre structures
 *     pp[]    - array of phase structures
 *     magbloc - reported magnitudes
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     main
 *  Calls:
 *     GetSC3Event, GetSC3Hypocenter, GetSC3Magnitude, GetSC3Phases
 */
int ReadEventFromSC3MysqlDatabase(EVREC *ep, HYPREC *hp[], PHAREC *pp[],
        char *magbloc)
{
/*
 *  Get preferred origin, numHypo and numPhase from hypocenter and event tables
 */
    if (GetSC3Event(ep))
        return 1;
    if (verbose)
        fprintf(logfp, "    evid=%d preferred origin=%d numHypo=%d numPhase=%d\n",
                ep->evid, ep->PreferredOrid, ep->numHypo, ep->numPhase);
/*
 *  Allocate memory to hypocenter, phase and stamag structures
 */
    *hp = (HYPREC *)calloc(ep->numHypo, sizeof(HYPREC));
    if (*hp == NULL) {
        fprintf(logfp, "ReadEventFromSC3MysqlDatabase: evid %d: cannot allocate memory\n", ep->evid);
        fprintf(errfp, "ReadEventFromSC3MysqlDatabase: evid %d: cannot allocate memory\n", ep->evid);
        errorcode = 1;
        return 1;
    }
    *pp = (PHAREC *)calloc(ep->numPhase, sizeof(PHAREC));
    if (*pp == NULL) {
        fprintf(logfp, "ReadEventFromSC3MysqlDatabase: evid %d: cannot allocate memory\n", ep->evid);
        fprintf(errfp, "ReadEventFromSC3MysqlDatabase: evid %d: cannot allocate memory\n", ep->evid);
        Free(*hp);
        errorcode = 1;
        return 1;
    }
/*
 *  Fill array of hypocenter structures.
 */
    if (GetSC3Hypocenter(ep, *hp)) {
        Free(*hp);
        Free(*pp);
        return 1;
    }
/*
 *  get reported magnitudes
 */
    if (GetSC3Magnitude(ep, *hp, magbloc)) {
        Free(*hp);
        Free(*pp);
        return 1;
    }
/*
 *  Fill array of phase structures.
 */
    if (GetSC3Phases(ep, *pp)) {
        Free(*hp);
        Free(*pp);
        return 1;
    }
    return 0;
}

/*
 *  Title:
 *     GetSC3Event
 *  Synopsis:
 *     Gets evid, orid, event type of the preferred origin,
 *     and the number of hypocenters, phases, stations and reporting agencies.
 *  Input Arguments:
 *     ep - pointer to structure containing event information.
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     ReadEventFromSC3MysqlDatabase
 *  Calls:
 *     PrintMysqlError, DropSpace
 */
static int GetSC3Event(EVREC *ep)
{
    MYSQL_RES *res_set = (MYSQL_RES *)NULL;
    MYSQL_ROW row;
    char psql[2048], sql[2048], w[65];
    int i, num_rows;
    double depth = 0.;
    ep->ISChypid = 0;
    ep->OutputDBPreferredOrid = ep->OutputDBISChypid = 0;
/*
 *  Get evid, orid, depth and etype of the preferred origin
 */
    sprintf(psql, "select e._oid, o._oid, o.depth_value,         \
                          coalesce(e.typeCertainty, 'k'),        \
                          coalesce(e.type, 'e'),                 \
                          coalesce(e.preferredMagnitudeID, 'X'), \
                          e.preferredOriginID                    \
                     from %sEvent e, %sOrigin o,                 \
                          %sPublicObject ep, %sPublicObject op   \
                    where ep.publicID = '%s'                     \
                      and ep._oid = e._oid                       \
                      and op.publicID = preferredOriginID        \
                      and op._oid = o._oid",
            InputDB, InputDB, NextidDB, NextidDB, ep->EventID);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetSC3Event: %s\n", sql);
    if (mysql_query(mysql, sql))
        PrintMysqlError("GetSC3Event: error: preferred origin");
    else {
        res_set = mysql_store_result(mysql);
        num_rows = mysql_num_rows(res_set);
        if (num_rows == 1) {
            row = mysql_fetch_row(res_set);
            ep->evid = atoi(row[0]);
            ep->PreferredOrid = ep->ISChypid = atoi(row[1]);
            depth = atof(row[2]);
            strcpy(w, row[3]);
            if (streq(w, "suspected"))
                strcpy(ep->etype, "s");
            else
                strcpy(ep->etype, "k");
            strcpy(w, row[4]);
            if (streq(w, "nuclear explosion"))
                strcat(ep->etype, "n");
            else if (streq(w, "explosion") ||
                     streq(w, "quarry blast") ||
                     streq(w, "mining explosion") ||
                     streq(w, "chemical explosion") ||
                     streq(w, "experimental explosion") ||
                     streq(w, "rock burst") ||
                     streq(w, "kx") || streq(w, "sx") ||
                     streq(w, "km") || streq(w, "sm") ||
                     streq(w, "kh") || streq(w, "sh"))
                strcat(ep->etype, "x");
            else if (streq(w, "induced event"))
                strcat(ep->etype, "i");
            else if (streq(w, "landslide"))
                strcpy(ep->etype, "ls");
            else
                strcat(ep->etype, "e");
            mysql_free_result(res_set);
            strcpy(ep->prefmagid, row[5]);
            strcpy(ep->prefOriginID, row[6]);
        }
        else {
            mysql_free_result(res_set);
            fprintf(errfp, "GetSC3Event: %s no preferred origin found\n",
                    ep->EventID);
            fprintf(logfp, "GetSC3Event: %s no preferred origin found\n",
                    ep->EventID);
            return 1;
        }
    }
/*
 *  A depth value larger than 700 km is a good indicator for a bogus event.
 */
    if (depth > 700.) {
        fprintf(errfp, "%s: depth (%.0f) is larger than 700 km! ",
                ep->EventID, depth);
        fprintf(errfp, "Possibly bogus event, exiting.\n");
        fprintf(logfp, "%s: depth (%.0f) is larger than 700 km! ",
                ep->EventID, depth);
        fprintf(logfp, "Possibly bogus event, exiting.\n");
        return 1;
    }
/*
 *  Get preferred origin and ISChypid in OutputDB if it differs from InputDB
 */
    if (strcmp(InputDB, OutputDB)) {
        ep->OutputDBPreferredOrid = ep->OutputDBISChypid = 0;
        sprintf(psql, "select o._oid, e.preferredOriginID          \
                         from %sEvent e, %sOrigin o,               \
                              %sPublicObject ep, %sPublicObject op \
                        where ep.publicID = '%s'                   \
                          and ep._oid = e._oid                     \
                          and op.publicID = preferredOriginID      \
                          and op._oid = o._oid",
                OutputDB, OutputDB, NextidDB, NextidDB, ep->EventID);
        DropSpace(psql, sql);
        if (verbose > 2) fprintf(logfp, "            GetSC3Event: %s\n", sql);
        if (mysql_query(mysql, sql))
            PrintMysqlError("GetSC3Event: error: preferred origin");
        else {
            res_set = mysql_use_result(mysql);
            row = mysql_fetch_row(res_set);
            ep->OutputDBPreferredOrid = atoi(row[0]);
            strcpy(ep->OutputDBprefOriginID, row[1]);
        }
        mysql_free_result(res_set);
    }
/*
 *  Get hypocentre count
 */
    sprintf(psql, "select count(*)                                \
                     from %sOriginReference r, %sPublicObject ep  \
                    where ep.publicID = '%s'                      \
                      and r._parent_oid = ep._oid",
            InputDB, NextidDB, ep->EventID);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetSC3Event: %s\n", sql);
    if (mysql_query(mysql, sql))
        PrintMysqlError("GetSC3Event: error: hypocentre count");
    else {
        res_set = mysql_use_result(mysql);
        row = mysql_fetch_row(res_set);
        ep->numHypo = atoi(row[0]);
    }
    mysql_free_result(res_set);
    if (ep->numHypo == 0) {
        fprintf(errfp, "GetSC3Event %d: no hypocentres found\n", ep->evid);
        fprintf(logfp, "GetSC3Event %d: no hypocentres found\n", ep->evid);
        return 1;
    }
    if (verbose > 1) fprintf(logfp, "        %d hypocentres\n", ep->numHypo);
/*
 *  Get phase and station count
 */
    sprintf(psql, "select count(r._oid),                           \
                          count(distinct p.waveformID_stationCode, \
                                         p.waveformID_networkCode) \
                     from %sArrival r, %sPick p, %sPublicObject rp \
                    where r._parent_oid = %d                       \
                      and rp.publicID = r.pickID                   \
                      and rp._oid = p._oid",
            InputDB, InputDB, NextidDB, ep->PreferredOrid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetSC3Event: %s\n", sql);
    if (mysql_query(mysql, sql))
        PrintMysqlError("GetSC3Event: error: phase count");
    else {
        res_set = mysql_use_result(mysql);
        row = mysql_fetch_row(res_set);
        ep->numPhase = atoi(row[0]);
        ep->numSta = ep->numReading = atoi(row[1]);
    }
    mysql_free_result(res_set);
    if (ep->numPhase == 0) {
        fprintf(errfp, "GetSC3Event %d: no phases found\n", ep->evid);
        fprintf(logfp, "GetSC3Event %d: no phases found\n", ep->evid);
        return 1;
    }
    if (ep->numSta == 0) {
        fprintf(errfp, "GetSC3Event %d: no stations found\n", ep->evid);
        fprintf(logfp, "GetSC3Event %d: no stations found\n", ep->evid);
        return 1;
    }
    if (verbose > 1)
        fprintf(logfp, "        %d phases, %d readings, %d stations\n",
                ep->numPhase, ep->numReading, ep->numSta);
/*
 *  Get agencies that reported phases
 */
    sprintf(psql, "select distinct p.creationInfo_agencyID         \
                     from %sArrival r, %sPick p, %sPublicObject rp \
                    where r._parent_oid = %d                       \
                      and rp.publicID = r.pickID                   \
                      and rp._oid = p._oid",
            InputDB, InputDB, NextidDB, ep->PreferredOrid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetSC3Event: %s\n", sql);
    if (mysql_query(mysql, sql))
        PrintMysqlError("GetSC3Event: error: agency count");
    else {
        res_set = mysql_store_result(mysql);
        ep->numAgency = numAgencies = mysql_num_rows(res_set);
        if (ep->numAgency == 0) {
            fprintf(errfp, "GetSC3Event %d: no agencies found\n", ep->evid);
            fprintf(logfp, "GetSC3Event %d: no agencies found\n", ep->evid);
            mysql_free_result(res_set);
            return 1;
        }
        for (i = 0; i < numAgencies; i++) {
            row = mysql_fetch_row(res_set);
            strcpy(agencies[i], row[0]);
        }
        mysql_free_result(res_set);
    }
    if (verbose > 1) fprintf(logfp, "        %d agencies\n", ep->numAgency);
    return 0;
}

/*
 *  Title:
 *     GetSC3Hypocenter
 *  Synopsis:
 *     Reads reported hypocentres for an event from DB.
 *         Fields from the Origin table:
 *            author
 *            hypid, origid
 *            origin time, lat, lon, depth
 *            depfix, epifix, timfix
 *            nsta, ndefsta, nass, ndef
 *            mindist, maxdist, azimgap, sgap
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
 *     ReadEventFromSC3MysqlDatabase
 * Calls:
 *     PrintMysqlError, DropSpace, HumanToEpoch
 */
static int GetSC3Hypocenter(EVREC *ep, HYPREC h[])
{
    MYSQL_RES *res_set = (MYSQL_RES *)NULL;
    MYSQL_ROW row;
    char sql[4096], psql[4096], auth[AGLEN], prevauth[AGLEN];
    int i, msec = 0;
/*
 *  Get preferred origin
 */
    sprintf(psql,
           "select coalesce(creationInfo_agencyID, '%s'), _oid,              \
                   time_value, coalesce(time_value_ms, 0),                   \
                   latitude_value, longitude_value,                          \
                   coalesce(depth_value, 0),                                 \
                   coalesce(depth_uncertainty, 0),                           \
                   coalesce(epicenterFixed, 0),                              \
                   coalesce(timeFixed, 0),                                   \
                   coalesce(quality_associatedStationCount, 0),              \
                   coalesce(quality_usedStationCount, 0),                    \
                   coalesce(quality_associatedPhaseCount, 0),                \
                   coalesce(quality_usedPhaseCount, 0),                      \
                   coalesce(quality_minimumDistance, 0),                     \
                   coalesce(quality_maximumDistance, 0),                     \
                   coalesce(quality_azimuthalGap, 360),                      \
                   coalesce(quality_secondaryAzimuthalGap, 360),             \
                   coalesce(uncertainty_minHorizontalUncertainty, 0),        \
                   coalesce(uncertainty_maxHorizontalUncertainty, 0),        \
                   coalesce(uncertainty_azimuthMaxHorizontalUncertainty, 0), \
                   coalesce(time_uncertainty, 0),                            \
                   coalesce(quality_standardError, 0),                       \
                   coalesce(creationInfo_author, '%s')                       \
              from %sOrigin                                                  \
             where _oid = %d",
            InAgency, InAgency, InputDB, ep->PreferredOrid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetSC3Hypocenter: %s\n", sql);
    if (mysql_query(mysql, sql))
        PrintMysqlError("GetSC3Hypocenter: preferred origin");
    else {
        res_set = mysql_use_result(mysql);
        row = mysql_fetch_row(res_set);
        i = 0;
/*
 *      initialize
 */
        h[i].depfix = h[i].epifix = h[i].timfix = h[i].rank = 0;
        strcpy(h[i].etype, ep->etype);
        h[i].sminax = h[i].smajax = h[i].strike = NULLVAL;
        h[i].stime = h[i].sdepth = h[i].sdobs = NULLVAL;
        h[i].nsta = h[i].ndefsta = h[i].nass = h[i].ndef = NULLVAL;
        h[i].mindist = h[i].maxdist = h[i].azimgap = h[i].sgap = NULLVAL;
/*
 *      populate
 */
        strcpy(h[i].agency, row[0]);
        strcpy(h[i].rep, row[23]);
        strcpy(h[i].origid, row[1]);
        h[i].hypid = atoi(row[1]);
        msec = atoi(row[3]);
        h[i].time = HumanToEpoch(row[2], msec, 1);
        h[i].lat  = atof(row[4]);
        h[i].lon  = atof(row[5]);
        h[i].depth = atof(row[6]);
        if (strcmp(row[7], "0"))  h[i].depfix = 1;
        else                      h[i].sdepth = atof(row[7]);
        if (strcmp(row[8], "0"))  h[i].epifix = 1;
        if (strcmp(row[9], "0"))  h[i].timfix = 1;
        if (strcmp(row[10], "0")) h[i].nsta = atoi(row[10]);
        if (strcmp(row[11], "0")) h[i].ndefsta = atoi(row[11]);
        if (strcmp(row[12], "0")) h[i].nass = atoi(row[12]);
        if (strcmp(row[13], "0")) h[i].ndef = atoi(row[13]);
        if (strcmp(row[14], "0")) h[i].mindist = atof(row[14]);
        if (strcmp(row[15], "0")) h[i].maxdist = atof(row[15]);
        if (strcmp(row[16], "0")) h[i].azimgap = atof(row[16]);
        if (strcmp(row[17], "0")) h[i].sgap = atof(row[17]);
        if (strcmp(row[18], "0")) h[i].sminax = atof(row[18]);
        if (strcmp(row[19], "0")) h[i].smajax = atof(row[19]);
        if (strcmp(row[20], "0")) h[i].strike = atof(row[20]);
        if (strcmp(row[21], "0")) h[i].stime = atof(row[21]);
        if (strcmp(row[22], "0")) h[i].sdobs = atof(row[22]);
        if (verbose > 1)
            fprintf(logfp, "        i=%d evid=%d hypid=%d agency=%s\n",
                    i, ep->evid, h[i].hypid, h[i].agency);
        mysql_free_result(res_set);
    }
/*
 *  If the preferred origin is the only hypocenter, we are done
 */
    if (ep->numHypo == 1)
        return 0;
/*
 *  Get rest of the hypocenters
 */
    sprintf(psql,
           "select coalesce(o.creationInfo_agencyID, '%s'), o._oid,          \
                   o.time_value, coalesce(o.time_value_ms, 0),               \
                   o.latitude_value, o.longitude_value,                      \
                   coalesce(o.depth_value, 0),                               \
                   coalesce(o.depth_uncertainty, 0),                         \
                   coalesce(o.epicenterFixed, 0),                            \
                   coalesce(o.timeFixed, 0),                                 \
                   coalesce(o.quality_associatedStationCount, 0),            \
                   coalesce(o.quality_usedStationCount, 0),                  \
                   coalesce(o.quality_associatedPhaseCount, 0),              \
                   coalesce(o.quality_usedPhaseCount, 0),                    \
                   coalesce(o.quality_minimumDistance, 0),                   \
                   coalesce(o.quality_maximumDistance, 0),                   \
                   coalesce(o.quality_azimuthalGap, 360),                    \
                   coalesce(o.quality_secondaryAzimuthalGap, 360),           \
                   coalesce(o.uncertainty_confidenceEllipsoid_semiMinorAxisLength, 0), \
                   coalesce(o.uncertainty_confidenceEllipsoid_semiMajorAxisLength, 0), \
                   coalesce(o.uncertainty_confidenceEllipsoid_majorAxisAzimuth, 0),    \
                   coalesce(o.time_uncertainty, 0),                          \
                   coalesce(o.quality_standardError, 0),                     \
                   coalesce(o.creationInfo_author, '%s')                     \
              from %sOrigin o, %sOriginReference r, %sPublicObject rp        \
             where r._parent_oid = %d                                        \
               and rp.publicID = r.OriginID                                  \
               and rp._oid = o._oid                                          \
               and o._oid <> %d                                              \
               and coalesce(o.depth_value,0) between 0 and 700               \
             order by o.creationInfo_agencyID,                               \
                   o.creationInfo_creationTime desc",
            InAgency, InAgency, InputDB, InputDB, NextidDB, ep->evid,
            ep->PreferredOrid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetSC3Hypocenter: %s\n", sql);
    if (mysql_query(mysql, sql))
        PrintMysqlError("GetSC3Hypocenter: hypocentre");
    else {
        res_set = mysql_use_result(mysql);
        strcpy(prevauth, h[0].agency);
        i = 1;
        while ((row = mysql_fetch_row(res_set))) {
            strcpy(auth, row[0]);
/*
 *          consider distinct agencies only
 */
            if (streq(auth, prevauth))
                continue;
/*
 *          initialize
 */
            h[i].depfix = h[i].epifix = h[i].timfix = h[i].rank = 0;
            strcpy(h[i].etype, ep->etype);
            h[i].sminax = h[i].smajax = h[i].strike = NULLVAL;
            h[i].stime = h[i].sdepth = h[i].sdobs = NULLVAL;
            h[i].nsta = h[i].ndefsta = NULLVAL;
            h[i].nass = h[i].ndef = NULLVAL;
            h[i].mindist = h[i].maxdist = NULLVAL;
            h[i].azimgap = h[i].sgap = NULLVAL;
/*
 *          populate
 */
            strcpy(h[i].agency, auth);
            strcpy(h[i].rep, row[23]);
            strcpy(h[i].origid, row[1]);
            h[i].hypid = atoi(row[1]);
            msec = atoi(row[3]);
            h[i].time = HumanToEpoch(row[2], msec, 1);
            h[i].lat  = atof(row[4]);
            h[i].lon  = atof(row[5]);
            h[i].depth = atof(row[6]);
            if (strcmp(row[7], "0"))  h[i].depfix = 1;
            else                      h[i].sdepth = atof(row[7]);
            if (strcmp(row[8], "0"))  h[i].epifix = 1;
            if (strcmp(row[9], "0"))  h[i].timfix = 1;
            if (strcmp(row[10], "0")) h[i].nsta = atoi(row[10]);
            if (strcmp(row[11], "0")) h[i].ndefsta = atoi(row[11]);
            if (strcmp(row[12], "0")) h[i].nass = atoi(row[12]);
            if (strcmp(row[13], "0")) h[i].ndef = atoi(row[13]);
            if (strcmp(row[14], "0")) h[i].mindist = atof(row[14]);
            if (strcmp(row[15], "0")) h[i].maxdist = atof(row[15]);
            if (strcmp(row[16], "0")) h[i].azimgap = atof(row[16]);
            if (strcmp(row[17], "0")) h[i].sgap = atof(row[17]);
            if (strcmp(row[18], "0")) h[i].sminax = atof(row[18]);
            if (strcmp(row[19], "0")) h[i].smajax = atof(row[19]);
            if (strcmp(row[20], "0")) h[i].strike = atof(row[20]);
            if (strcmp(row[21], "0")) h[i].stime = atof(row[21]);
            if (strcmp(row[22], "0")) h[i].sdobs = atof(row[22]);
            if (verbose > 1)
                fprintf(logfp, "        i=%d evid=%d hypid=%d agency=%s\n",
                        i, ep->evid, h[i].hypid, h[i].agency);
            strcpy(prevauth, auth);
            i++;
            if (i > ep->numHypo) {
                fprintf(errfp, "GetSC3Hypocenter: unexpected numHypo %d\n", i);
                fprintf(logfp, "GetSC3Hypocenter: unexpected numHypo %d\n", i);
                mysql_free_result(res_set);
                return 1;
            }
        }
        mysql_free_result(res_set);
/*
 *      number of hypocentres reported by distinct agencies
 */
        ep->numHypo = i;
    }
    return 0;
}

/*
 *  Title:
 *     GetSC3Magnitude
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
 *     ReadEventFromSC3MysqlDatabase
 * Calls:
 *     PrintMysqlError, DropSpace
 */
static int GetSC3Magnitude(EVREC *ep, HYPREC h[], char *magbloc)
{
    MYSQL_RES *res_set = (MYSQL_RES *)NULL;
    MYSQL_ROW row;
    char sql[4096], psql[4096], auth[AGLEN], mtype[AGLEN];
    double mag, smag;
    int i, nsta;
/*
 *  accumulate magnitude block in a string for output
 */
    strcpy(magbloc, "");
    for (i = 0; i < ep->numHypo; i++) {
        sprintf(psql,
               "select coalesce(magnitude_value, -9),         \
                       coalesce(nullif(type, ''), 'Q'),       \
                       coalesce(magnitude_uncertainty, -1),   \
                       coalesce(stationCount, -1),            \
                       creationInfo_agencyID                  \
                  from %sMagnitude m, %sPublicObject p        \
                 where m._parent_oid = %d                     \
                   and m._oid = p._oid                        \
                   and p.publicID != '%s'                     \
                   and m.creationInfo_agencyID != '%s'        \
                 order by type",
                InputDB, NextidDB, h[i].hypid, ep->prefmagid, OutAgency);
        DropSpace(psql, sql);
        if (verbose > 2)
            fprintf(logfp, "            GetSC3Magnitude: %s\n", sql);
        if (mysql_query(mysql, sql))
            PrintMysqlError("GetSC3Magnitude: magnitude block");
        else {
            res_set = mysql_use_result(mysql);
            while ((row = mysql_fetch_row(res_set))) {
                mag = atof(row[0]);
                if (mag < -8)
                    continue;
                if (streq(row[1], "Q")) strcpy(mtype, "");
                else strcpy(mtype, row[1]);
                smag = atof(row[2]);
                nsta = atoi(row[3]);
                strcpy(auth, row[4]);
                sprintf(magbloc, "%s%-5s %4.1f ", magbloc, mtype, mag);
                if (smag < 0) sprintf(magbloc, "%s    ", magbloc);
                else          sprintf(magbloc, "%s%3.1f ", magbloc, smag);
                if (nsta < 0) sprintf(magbloc, "%s     ", magbloc);
                else          sprintf(magbloc, "%s%4d ", magbloc, nsta);
                sprintf(magbloc, "%s%-9s %d\n", magbloc, auth, h[i].hypid);
            }
        }
        mysql_free_result(res_set);
    }
/*
 *  get preferred magnitude
 */

    sprintf(psql,
           "select coalesce(magnitude_value, -9),         \
                   coalesce(nullif(type, ''), 'Q'),       \
                   coalesce(magnitude_uncertainty, -1),   \
                   coalesce(stationCount, -1),            \
                   creationInfo_agencyID,                 \
                   m._parent_oid                          \
              from %sMagnitude m, %sPublicObject p        \
             where m._oid = p._oid                        \
               and p.publicID = '%s'                      \
               and m.creationInfo_agencyID != '%s'        \
             order by type",
            InputDB, NextidDB, ep->prefmagid, OutAgency);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetSC3Magnitude: %s\n", sql);
    if (mysql_query(mysql, sql))
        PrintMysqlError("GetSC3Magnitude: magnitude block");
    else {
        res_set = mysql_use_result(mysql);
        while ((row = mysql_fetch_row(res_set))) {
            mag = atof(row[0]);
            if (mag < -8)
                continue;
            if (streq(row[1], "Q")) strcpy(mtype, "");
            else strcpy(mtype, row[1]);
            smag = atof(row[2]);
            nsta = atoi(row[3]);
            strcpy(auth, row[4]);
            i = atoi(row[5]);
            sprintf(magbloc, "%s%-5s %4.1f ", magbloc, mtype, mag);
            if (smag < 0) sprintf(magbloc, "%s    ", magbloc);
            else          sprintf(magbloc, "%s%3.1f ", magbloc, smag);
            if (nsta < 0) sprintf(magbloc, "%s     ", magbloc);
            else          sprintf(magbloc, "%s%4d ", magbloc, nsta);
            sprintf(magbloc, "%s%-9s %d\n", magbloc, auth, i);
        }
    }
    mysql_free_result(res_set);
    return 0;
}

/*
 *  Title:
 *     GetSC3Phases
 *  Synopsis:
 *     Reads phase and amplitude data associated to the preferred origin
 *         Fields from the Arrival, Pick, Station tables:
 *             arrival id, agency,
 *             arrival time, associated phase, reported phase,
 *             station, network, location, channel,
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
 *     ReadEventFromSC3MysqlDatabase
 *  Calls:
 *     PrintMysqlError, DropSpace, HumanToEpoch
 */
static int GetSC3Phases(EVREC *ep, PHAREC p[])
{
    MYSQL_RES *res_set = (MYSQL_RES *)NULL;
    MYSQL_ROW row;
    char sql[4096], psql[4096], PrevAuth[AGLEN], PrevSta[STALEN];
    int i, m, phid, rdid;
    int numPhase = 0, msec = 0;
/*
 *  Get phases associated to the preferred origin
 */
    sprintf(psql, "select r._oid,                                       \
                          coalesce(p.creationInfo_agencyID, '%s'),      \
                          p.time_value, coalesce(p.time_value_ms, 0),   \
                          coalesce(r.phase_code, ''),                   \
                          coalesce(p.phaseHint_code, r.phase_code, ''), \
                          p.waveformID_stationCode,                     \
                          coalesce(p.waveformID_networkCode, '--'),     \
                          coalesce(nullif(p.waveformID_locationCode, ''), '--'), \
                          coalesce(p.waveformID_channelCode, '\?\?\?'), \
                          coalesce(p.horizontalSlowness_value, 0),      \
                          coalesce(p.backazimuth_value, 0),             \
                          coalesce(r.distance, 0),                      \
                          coalesce(p.polarity, ' '),                    \
                          coalesce(p.onset, ' '),                       \
                          s.latitude, s.longitude,                      \
                          coalesce(s.elevation, 0),                     \
                          r.pickID,                                     \
                          coalesce(p.creationInfo_author, '%s')         \
                     from %sArrival r, %sPick p, %sPublicObject rp,     \
                          %sNetwork n, %sStation s                      \
                    where r._parent_oid = %d                            \
                      and rp.publicID = r.pickID                        \
                      and rp._oid = p._oid                              \
                      and p.waveformID_networkCode = n.code             \
                      and p.waveformID_stationCode = s.code             \
                      and s._parent_oid = n._oid                        \
                      and p.time_value between s.start and              \
                          coalesce(s.end, '3001-01-01')                 \
                    order by r._oid",
              InAgency, InAgency, InputDB, InputDB,
              NextidDB, NextidDB, NextidDB, ep->PreferredOrid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetSC3Phases: %s\n", sql);
    if (mysql_query(mysql, sql))
        PrintMysqlError("GetSC3Phases: phases");
    else {
        res_set = mysql_store_result(mysql);
        numPhase = mysql_num_rows(res_set);
        if (numPhase != ep->numPhase) {
            fprintf(errfp, "GetSC3Phases: unexpected number phases!\n");
            fprintf(errfp, "    evid=%d preferred origin=%d epnumPhase=%d numPhase=%d\n",
                    ep->evid, ep->PreferredOrid, ep->numPhase, numPhase);
            fprintf(errfp, "    often due to a missing station\n");
            fprintf(logfp, "GetSC3Phases: unexpected number phases!\n");
            fprintf(logfp, "    evid=%d preferred origin=%d epnumPhase=%d numPhase=%d\n",
                    ep->evid, ep->PreferredOrid, ep->numPhase, numPhase);
            fprintf(logfp, "    often due to a missing station\n");
            if (numPhase < ep->numPhase) {
                ep->numPhase = numPhase;
            }
            else {
                mysql_free_result(res_set);
                return 1;
            }
        }
        i = 0;
        while ((row = mysql_fetch_row(res_set))) {
            p[i].repid = NULLVAL;
            p[i].comp = ' ';
            p[i].delta = p[i].slow = p[i].azim = NULLVAL;
            p[i].phase_fixed = p[i].force_undef = p[i].init = 0;
            p[i].numamps = 0;
            p[i].hypid = ep->PreferredOrid;
            strcpy(p[i].arrid, row[0]);
            if (verbose > 3)
                fprintf(logfp, "        i=%d arrid=%s\n", i, p[i].arrid);
            p[i].phid = atoi(row[0]);
            strcpy(p[i].auth, row[1]);
            strcpy(p[i].rep, row[19]);
            strcpy(p[i].agency, "FDSN");
            msec = atoi(row[3]);
            p[i].time = HumanToEpoch(row[2], msec, 1);
            strcpy(p[i].phase, row[4]);
            strcpy(p[i].ReportedPhase, row[5]);
            strcpy(p[i].sta, row[6]);
            strcpy(p[i].prista, row[6]);
            strcpy(p[i].deploy, row[7]);
            strcpy(p[i].lcn, row[8]);
            sprintf(p[i].fdsn, "%s.%s.%s.%s",
                    p[i].agency, p[i].deploy, p[i].sta, p[i].lcn);
            strcpy(p[i].pch, row[9]);
            if (p[i].pch[2] != '?')   p[i].comp = p[i].pch[2];
            if (strcmp(row[10], "0")) p[i].slow = atof(row[10]);
            if (strcmp(row[11], "0")) p[i].azim = atof(row[11]);
            if (strcmp(row[12], "0")) p[i].delta = atof(row[12]);
            p[i].sp_fm = row[13][0];
            p[i].detchar = row[14][0];
            p[i].StaLat  = atof(row[15]);
            p[i].StaLon  = atof(row[16]);
            p[i].StaElev = atof(row[17]);
            strcpy(p[i].pickid, row[18]);
            i++;
        }
        mysql_free_result(res_set);
    }
/*
 *  get amplitudes associated to the preferred origin
 */
    sprintf(psql, "select r._oid, a._oid, a.type,                   \
                          coalesce(a.amplitude_value, 0),           \
                          coalesce(a.period_value, 0),              \
                          coalesce(a.snr, 0),                       \
                          a.pickID,                                 \
                          coalesce(a.waveformID_channelCode, '\?\?\?') \
                     from %sAmplitude a, %sArrival r                \
                    where r._parent_oid = %d                        \
                      and r.pickID = a.pickid                       \
                    order by r._oid",
            InputDB, InputDB, ep->PreferredOrid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetSC3Phases: %s\n", sql);
    if (mysql_query(mysql, sql))
        PrintMysqlError("GetSC3Phases: amplitudes");
    else {
        res_set = mysql_use_result(mysql);
        while ((row = mysql_fetch_row(res_set))) {
            phid = atoi(row[0]);
            for (i = 0; i < ep->numPhase; i++) {
                m = p[i].numamps;
                if (phid == p[i].phid) {
                    if (streq(row[2], "snr")) {
                        p[i].snr = atof(row[5]);
                        continue;
                    }
                    p[i].a[m].ampdef = p[i].a[m].magid = 0;
                    p[i].a[m].magnitude = p[i].a[m].logat = NULLVAL;
                    p[i].a[m].ampid = atoi(row[1]);
                    strcpy(p[i].a[m].magtype, row[2]);
                    p[i].a[m].amp = atof(row[3]);
                    p[i].a[m].per = atof(row[4]);
                    p[i].a[m].snr = atof(row[5]);
                    strcpy(p[i].a[m].amplitudeid, row[6]);
                    strcpy(p[i].a[m].ach, row[7]);
/*
 *                  If no component is given use the one from phase
 */
                    if (p[i].a[m].ach[2] != '?')
                        p[i].a[m].comp = p[i].a[m].ach[2];
                    else if (p[i].comp)
                        p[i].a[m].comp = p[i].comp;
                    else
                        p[i].a[m].comp = ' ';
                    p[i].numamps++;
                }
            }
        }
        mysql_free_result(res_set);
    }
/*
 *  Sort phase structures by increasing delta, sta, time, auth
 */
    if (verbose > 2)
        fprintf(logfp, "            GetSC3Phases: sort phases\n");
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

#endif
#endif  /* SC3DB */

/*  EOF */
