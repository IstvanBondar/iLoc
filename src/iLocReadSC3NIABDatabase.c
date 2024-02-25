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
#ifdef PGSQL

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
extern char InAgency[VALLEN];                     /* author for input assocs */
extern char OutAgency[VALLEN];      /* author for new hypocentres and assocs */
extern PGconn *conn;

/*
 * Functions:
 *    ReadEventFromSC3NIABDatabase
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
 *     ReadEventFromSC3NIABDatabase
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
int ReadEventFromSC3NIABDatabase(EVREC *ep, HYPREC *hp[], PHAREC *pp[],
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
        fprintf(logfp, "ReadEventFromSC3NIABDatabase: evid %d: cannot allocate memory\n", ep->evid);
        fprintf(errfp, "ReadEventFromSC3NIABDatabase: evid %d: cannot allocate memory\n", ep->evid);
        errorcode = 1;
        return 1;
    }
    *pp = (PHAREC *)calloc(ep->numPhase, sizeof(PHAREC));
    if (*pp == NULL) {
        fprintf(logfp, "ReadEventFromSC3NIABDatabase: evid %d: cannot allocate memory\n", ep->evid);
        fprintf(errfp, "ReadEventFromSC3NIABDatabase: evid %d: cannot allocate memory\n", ep->evid);
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
 *     ReadEventFromSC3NIABDatabase
 *  Calls:
 *     PrintPgsqlError, DropSpace
 */
static int GetSC3Event(EVREC *ep)
{
    PGresult *res_set = (PGresult *)NULL;
    char psql[2048], sql[2048], w[65];
    int i;
    double depth = 0.;
    ep->ISChypid = 0;
    ep->OutputDBPreferredOrid = ep->OutputDBISChypid = 0;
/*
 *  Get evid, orid, depth and etype of the preferred origin
 */
    sprintf(psql, "select e._oid, o._oid, o.m_depth_value,        \
                          coalesce(e.m_typeCertainty, 'k'),       \
                          coalesce(e.m_type, 'e'),                \
                          coalesce(e.m_preferredMagnitudeID, 'X') \
                     from %sEvent e, %sOrigin o,                  \
                          %sPublicObject ep, %sPublicObject op    \
                    where ep.m_publicID = '%s'                    \
                      and ep._oid = e._oid                        \
                      and op.m_publicID = e.m_preferredOriginID   \
                      and op._oid = o._oid",
            InputDB, InputDB, InputDB, InputDB, ep->EventID);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetSC3Event: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetSC3Event: error: preferred origin");
    else {
        if (PQntuples(res_set) == 1) {
            ep->evid = atoi(PQgetvalue(res_set, 0, 0));
            ep->PreferredOrid = ep->ISChypid = atoi(PQgetvalue(res_set, 0, 1));
            depth = atof(PQgetvalue(res_set, 0, 2));
            strcpy(w, PQgetvalue(res_set, 0, 3));
            if (streq(w, "suspected"))
                strcpy(ep->etype, "s");
            else
                strcpy(ep->etype, "k");
            strcpy(w, PQgetvalue(res_set, 0, 4));
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
            strcpy(ep->prefmagid, PQgetvalue(res_set, 0, 5));
            PQclear(res_set);
        }
        else {
            PQclear(res_set);
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
        sprintf(psql, "select o._oid                                \
                         from %sEvent e, %sOrigin o,                \
                              %sPublicObject ep, %sPublicObject op  \
                        where ep.m_publicID = '%s'                  \
                          and ep._oid = e._oid                      \
                          and op.m_publicID = e.m_preferredOriginID \
                          and op._oid = o._oid",
                OutputDB, OutputDB, OutputDB, OutputDB, ep->EventID);
        DropSpace(psql, sql);
        if (verbose > 2) fprintf(logfp, "            GetSC3Event: %s\n", sql);
        if ((res_set = PQexec(conn, sql)) == NULL)
            PrintPgsqlError("GetSC3Event: error: preferred origin");
        else
            ep->OutputDBPreferredOrid = atoi(PQgetvalue(res_set, 0, 0));
        PQclear(res_set);
    }
/*
 *  Get hypocentre count
 */
    sprintf(psql, "select count(*)                                \
                     from %sOriginReference r, %sPublicObject ep  \
                    where ep.m_publicID = '%s'                    \
                      and r._parent_oid = ep._oid",
            InputDB, InputDB, ep->EventID);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetSC3Event: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetSC3Event: error: hypocentre count");
    else
        ep->numHypo = atoi(PQgetvalue(res_set, 0, 0));
    PQclear(res_set);
    if (ep->numHypo == 0) {
        fprintf(errfp, "GetSC3Event %d: no hypocentres found\n", ep->evid);
        fprintf(logfp, "GetSC3Event %d: no hypocentres found\n", ep->evid);
        return 1;
    }
    if (verbose > 1) fprintf(logfp, "        %d hypocentres\n", ep->numHypo);
/*
 *  Get phase and station count
 */
    sprintf(psql, "select count(r._oid),                             \
                          count(distinct p.m_waveformID_stationCode) \
                     from %sArrival r, %sPick p, %sPublicObject rp   \
                    where r._parent_oid = %d                         \
                      and rp.m_publicID = r.m_pickID                 \
                      and rp._oid = p._oid",
            InputDB, InputDB, InputDB, ep->PreferredOrid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetSC3Event: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetSC3Event: error: phase count");
    else {
        ep->numPhase = atoi(PQgetvalue(res_set, 0, 0));
        ep->numSta = ep->numReading = atoi(PQgetvalue(res_set, 0, 1));
    }
    PQclear(res_set);
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
    sprintf(psql, "select distinct p.m_creationInfo_agencyID       \
                     from %sArrival r, %sPick p, %sPublicObject rp \
                    where r._parent_oid = %d                       \
                      and rp.m_publicID = r.m_pickID               \
                      and rp._oid = p._oid",
            InputDB, InputDB, InputDB, ep->PreferredOrid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetSC3Event: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetSC3Event: error: agency count");
    else {
        ep->numAgency = numAgencies = PQntuples(res_set);
        if (ep->numAgency == 0) {
            fprintf(errfp, "GetSC3Event %d: no agencies found\n", ep->evid);
            fprintf(logfp, "GetSC3Event %d: no agencies found\n", ep->evid);
            PQclear(res_set);
            return 1;
        }
        for (i = 0; i < numAgencies; i++) {
            strcpy(agencies[i], PQgetvalue(res_set, i, 0));
        }
        PQclear(res_set);
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
 *     ReadEventFromSC3NIABDatabase
 * Calls:
 *     PrintPgsqlError, DropSpace, HumanToEpoch
 */
static int GetSC3Hypocenter(EVREC *ep, HYPREC h[])
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[4096], psql[4096], auth[AGLEN], prevauth[AGLEN];
    int i, j, msec = 0, numHypo = 0;
/*
 *  Get preferred origin
 */
    sprintf(psql,
           "select coalesce(m_creationInfo_agencyID, '%s'), _oid,              \
                   m_time_value, coalesce(m_time_value_ms, 0),                 \
                   m_latitude_value, m_longitude_value,                        \
                   coalesce(m_depth_value, 0),                                 \
                   coalesce(m_depth_uncertainty, 0),                           \
                   coalesce(m_epicenterFixed, 'f'),                              \
                   coalesce(m_timeFixed, 'f'),                                   \
                   coalesce(m_quality_associatedStationCount, 0),              \
                   coalesce(m_quality_usedStationCount, 0),                    \
                   coalesce(m_quality_associatedPhaseCount, 0),                \
                   coalesce(m_quality_usedPhaseCount, 0),                      \
                   coalesce(m_quality_minimumDistance, 0),                     \
                   coalesce(m_quality_maximumDistance, 0),                     \
                   coalesce(m_quality_azimuthalGap, 360),                      \
                   coalesce(m_quality_secondaryAzimuthalGap, 360),             \
                   coalesce(m_uncertainty_minHorizontalUncertainty, 0),        \
                   coalesce(m_uncertainty_maxHorizontalUncertainty, 0),        \
                   coalesce(m_uncertainty_azimuthMaxHorizontalUncertainty, 0), \
                   coalesce(m_time_uncertainty, 0),                            \
                   coalesce(m_quality_standardError, 0),                       \
                   coalesce(m_creationInfo_author, '%s')                       \
              from %sOrigin                                                    \
             where _oid = %d",
            InAgency, InAgency, InputDB, ep->PreferredOrid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetSC3Hypocenter: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetSC3Hypocenter: preferred origin");
    else {
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
        strcpy(h[i].agency, PQgetvalue(res_set, i, 0));
        strcpy(h[i].rep, PQgetvalue(res_set, i, 23));
        strcpy(h[i].origid, PQgetvalue(res_set, i, 1));
        h[i].hypid = atoi(PQgetvalue(res_set, i, 1));
        msec = atoi(PQgetvalue(res_set, i, 3));
        h[i].time = HumanToEpoch(PQgetvalue(res_set, i, 2), msec, 1);
        h[i].lat  = atof(PQgetvalue(res_set, i, 4));
        h[i].lon  = atof(PQgetvalue(res_set, i, 5));
        h[i].depth = atof(PQgetvalue(res_set, i, 6));
        if (strcmp(PQgetvalue(res_set, i, 7), "0"))
            h[i].depfix = 1;
        else
            h[i].sdepth = atof(PQgetvalue(res_set, i, 7));
        if (strcmp(PQgetvalue(res_set, i, 8), "f"))
            h[i].epifix = 1;
        if (strcmp(PQgetvalue(res_set, i, 9), "f"))
            h[i].timfix = 1;
        if (strcmp(PQgetvalue(res_set, i, 10), "0"))
            h[i].nsta = atoi(PQgetvalue(res_set, i, 10));
        if (strcmp(PQgetvalue(res_set, i, 11), "0"))
            h[i].ndefsta = atoi(PQgetvalue(res_set, i, 11));
        if (strcmp(PQgetvalue(res_set, i, 12), "0"))
            h[i].nass = atoi(PQgetvalue(res_set, i, 12));
        if (strcmp(PQgetvalue(res_set, i, 13), "0"))
            h[i].ndef = atoi(PQgetvalue(res_set, i, 13));
        if (strcmp(PQgetvalue(res_set, i, 14), "0"))
            h[i].mindist = atof(PQgetvalue(res_set, i, 14));
        if (strcmp(PQgetvalue(res_set, i, 15), "0"))
            h[i].maxdist = atof(PQgetvalue(res_set, i, 15));
        if (strcmp(PQgetvalue(res_set, i, 16), "0"))
            h[i].azimgap = atof(PQgetvalue(res_set, i, 16));
        if (strcmp(PQgetvalue(res_set, i, 17), "0"))
            h[i].sgap = atof(PQgetvalue(res_set, i, 17));
        if (strcmp(PQgetvalue(res_set, i, 18), "0"))
            h[i].sminax = atof(PQgetvalue(res_set, i, 18));
        if (strcmp(PQgetvalue(res_set, i, 19), "0"))
            h[i].smajax = atof(PQgetvalue(res_set, i, 19));
        if (strcmp(PQgetvalue(res_set, i, 20), "0"))
            h[i].strike = atof(PQgetvalue(res_set, i, 20));
        if (strcmp(PQgetvalue(res_set, i, 21), "0"))
            h[i].stime = atof(PQgetvalue(res_set, i, 21));
        if (strcmp(PQgetvalue(res_set, i, 22), "0"))
            h[i].sdobs = atof(PQgetvalue(res_set, i, 22));
        if (verbose > 1)
            fprintf(logfp, "        i=%d evid=%d hypid=%d agency=%s\n",
                    i, ep->evid, h[i].hypid, h[i].agency);
        i++;
    }
    PQclear(res_set);
/*
 *  If the preferred origin is the only hypocenter, we are done
 */
    if (ep->numHypo == 1)
        return 0;
/*
 *  Get rest of the hypocenters
 */
    sprintf(psql,
           "select coalesce(o.m_creationInfo_agencyID, '%s'), o._oid, \
                   o.m_time_value, coalesce(o.m_time_value_ms, 0),    \
                   o.m_latitude_value, o.m_longitude_value,           \
                   coalesce(o.m_depth_value, 0),                      \
                   coalesce(o.m_depth_uncertainty, 0),                \
                   coalesce(o.m_epicenterFixed, 'f'),                   \
                   coalesce(o.m_timeFixed, 'f'),                        \
                   coalesce(o.m_quality_associatedStationCount, 0),   \
                   coalesce(o.m_quality_usedStationCount, 0),         \
                   coalesce(o.m_quality_associatedPhaseCount, 0),     \
                   coalesce(o.m_quality_usedPhaseCount, 0),           \
                   coalesce(o.m_quality_minimumDistance, 0),          \
                   coalesce(o.m_quality_maximumDistance, 0),          \
                   coalesce(o.m_quality_azimuthalGap, 360),           \
                   coalesce(o.m_quality_secondaryAzimuthalGap, 360),  \
                   coalesce(o.m_uncertainty_confidenceEllipsoid_semiMinorAxisLength, 0), \
                   coalesce(o.m_uncertainty_confidenceEllipsoid_semiMajorAxisLength, 0), \
                   coalesce(o.m_uncertainty_confidenceEllipsoid_majorAxisAzimuth, 0),    \
                   coalesce(o.m_time_uncertainty, 0),                 \
                   coalesce(o.m_quality_standardError, 0),            \
                   coalesce(o.m_creationInfo_author, '%s')            \
              from %sOrigin o, %sOriginReference r, %sPublicObject rp \
             where r._parent_oid = %d                                 \
               and rp.m_publicID = r.m_OriginID                       \
               and rp._oid = o._oid                                   \
               and o._oid <> %d                                       \
               and coalesce(o.m_depth_value,0) between 0 and 700      \
             order by o.m_creationInfo_agencyID,                      \
                   o.m_creationInfo_creationTime desc",
            InAgency, InAgency, InputDB, InputDB, InputDB, ep->evid,
            ep->PreferredOrid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetSC3Hypocenter: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetSC3Hypocenter: hypocentre");
    else {
        numHypo = PQntuples(res_set);
        strcpy(prevauth, h[0].agency);
        for (j = 0; j < numHypo; j++) {
            strcpy(auth, PQgetvalue(res_set, j, 0));
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
            strcpy(h[i].rep, PQgetvalue(res_set, j, 23));
            strcpy(h[i].origid, PQgetvalue(res_set, j, 1));
            h[i].hypid = atoi(PQgetvalue(res_set, j, 1));
            msec = atoi(PQgetvalue(res_set, j, 3));
            h[i].time = HumanToEpoch(PQgetvalue(res_set, j, 2), msec, 1);
            h[i].lat  = atof(PQgetvalue(res_set, j, 4));
            h[i].lon  = atof(PQgetvalue(res_set, j, 5));
            h[i].depth = atof(PQgetvalue(res_set, j, 6));
            if (strcmp(PQgetvalue(res_set, j, 7), "0"))
                h[i].depfix = 1;
            else
                h[i].sdepth = atof(PQgetvalue(res_set, j, 7));
            if (strcmp(PQgetvalue(res_set, j, 8), "f"))
                h[i].epifix = 1;
            if (strcmp(PQgetvalue(res_set, j, 9), "f"))
                h[i].timfix = 1;
            if (strcmp(PQgetvalue(res_set, j, 10), "0"))
                h[i].nsta = atoi(PQgetvalue(res_set, j, 10));
            if (strcmp(PQgetvalue(res_set, j, 11), "0"))
                h[i].ndefsta = atoi(PQgetvalue(res_set, j, 11));
            if (strcmp(PQgetvalue(res_set, j, 12), "0"))
                h[i].nass = atoi(PQgetvalue(res_set, j, 12));
            if (strcmp(PQgetvalue(res_set, j, 13), "0"))
                h[i].ndef = atoi(PQgetvalue(res_set, j, 13));
            if (strcmp(PQgetvalue(res_set, j, 14), "0"))
                h[i].mindist = atof(PQgetvalue(res_set, j, 14));
            if (strcmp(PQgetvalue(res_set, j, 15), "0"))
                h[i].maxdist = atof(PQgetvalue(res_set, j, 15));
            if (strcmp(PQgetvalue(res_set, j, 16), "0"))
                h[i].azimgap = atof(PQgetvalue(res_set, j, 16));
            if (strcmp(PQgetvalue(res_set, j, 17), "0"))
                h[i].sgap = atof(PQgetvalue(res_set, j, 17));
            if (strcmp(PQgetvalue(res_set, j, 18), "0"))
                h[i].sminax = atof(PQgetvalue(res_set, j, 18));
            if (strcmp(PQgetvalue(res_set, j, 19), "0"))
                h[i].smajax = atof(PQgetvalue(res_set, j, 19));
            if (strcmp(PQgetvalue(res_set, j, 20), "0"))
                h[i].strike = atof(PQgetvalue(res_set, j, 20));
            if (strcmp(PQgetvalue(res_set, j, 21), "0"))
                h[i].stime = atof(PQgetvalue(res_set, j, 21));
            if (strcmp(PQgetvalue(res_set, j, 22), "0"))
                h[i].sdobs = atof(PQgetvalue(res_set, j, 22));
            if (verbose > 1)
                fprintf(logfp, "        i=%d evid=%d hypid=%d agency=%s\n",
                        i, ep->evid, h[i].hypid, h[i].agency);
            strcpy(prevauth, auth);
            i++;
            if (i > ep->numHypo) {
                fprintf(errfp, "GetSC3Hypocenter: unexpected numHypo %d\n", i);
                fprintf(logfp, "GetSC3Hypocenter: unexpected numHypo %d\n", i);
                PQclear(res_set);
                return 1;
            }
        }
        PQclear(res_set);
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
 *     ReadEventFromSC3NIABDatabase
 * Calls:
 *     PrintPgsqlError, DropSpace
 */
static int GetSC3Magnitude(EVREC *ep, HYPREC h[], char *magbloc)
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[4096], psql[4096], auth[AGLEN], mtype[AGLEN];
    double mag, smag;
    int i, j, nsta, n;
/*
 *  accumulate magnitude block in a string for output
 */
    strcpy(magbloc, "");
    for (i = 0; i < ep->numHypo; i++) {
        sprintf(psql,
               "select coalesce(m_magnitude_value, -9),       \
                       coalesce(nullif(m_type, ''), 'Q'),     \
                       coalesce(m_magnitude_uncertainty, -1), \
                       coalesce(m_stationCount, -1),          \
                       m_creationInfo_agencyID                \
                  from %sMagnitude m, %sPublicObject p        \
                 where m._parent_oid = %d                     \
                   and m._oid = p._oid                        \
                   and p.m_publicID != '%s'                   \
                   and m.m_creationInfo_agencyID != '%s'      \
                 order by m_type",
                InputDB, InputDB, h[i].hypid, ep->prefmagid, OutAgency);
        DropSpace(psql, sql);
        if (verbose > 2)
            fprintf(logfp, "            GetSC3Magnitude: %s\n", sql);
        if ((res_set = PQexec(conn, sql)) == NULL)
            PrintPgsqlError("GetSC3Magnitude: magnitude block");
        else {
            n = PQntuples(res_set);
            for (j = 0; j < n; j++) {
                mag = atof(PQgetvalue(res_set, j, 0));
                if (mag < -8)
                    continue;
                if (streq(PQgetvalue(res_set, j, 1), "Q")) strcpy(mtype, "");
                else                strcpy(mtype, PQgetvalue(res_set, j, 1));
                smag = atof(PQgetvalue(res_set, j, 2));
                nsta = atoi(PQgetvalue(res_set, j, 3));
                strcpy(auth, PQgetvalue(res_set, j, 4));
                sprintf(magbloc, "%s%-5s %4.1f ", magbloc, mtype, mag);
                if (smag < 0) sprintf(magbloc, "%s    ", magbloc);
                else          sprintf(magbloc, "%s%3.1f ", magbloc, smag);
                if (nsta < 0) sprintf(magbloc, "%s     ", magbloc);
                else          sprintf(magbloc, "%s%4d ", magbloc, nsta);
                sprintf(magbloc, "%s%-9s %d\n", magbloc, auth, h[i].hypid);
            }
        }
        PQclear(res_set);
    }
/*
 *  get preferred magnitude
 */

    sprintf(psql,
           "select coalesce(m_magnitude_value, -9),       \
                   coalesce(nullif(m_type, ''), 'Q'),     \
                   coalesce(m_magnitude_uncertainty, -1), \
                   coalesce(m_stationCount, -1),          \
                   m_creationInfo_agencyID,               \
                   m._parent_oid                          \
              from %sMagnitude m, %sPublicObject p        \
             where m._oid = p._oid                        \
               and p.m_publicID = '%s'                    \
               and m.m_creationInfo_agencyID != '%s'      \
             order by m_type",
            InputDB, InputDB, ep->prefmagid, OutAgency);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetSC3Magnitude: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetSC3Magnitude: magnitude block");
    else {
        mag = atof(PQgetvalue(res_set, 0, 0));
        if (streq(PQgetvalue(res_set, 0, 1), "Q")) strcpy(mtype, "");
        else                strcpy(mtype, PQgetvalue(res_set, 0, 1));
        smag = atof(PQgetvalue(res_set, 0, 2));
        nsta = atoi(PQgetvalue(res_set, 0, 3));
        strcpy(auth, PQgetvalue(res_set, 0, 4));
        i = atoi(PQgetvalue(res_set, 0, 5));
        sprintf(magbloc, "%s%-5s %4.1f ", magbloc, mtype, mag);
        if (smag < 0) sprintf(magbloc, "%s    ", magbloc);
        else          sprintf(magbloc, "%s%3.1f ", magbloc, smag);
        if (nsta < 0) sprintf(magbloc, "%s     ", magbloc);
        else          sprintf(magbloc, "%s%4d ", magbloc, nsta);
        sprintf(magbloc, "%s%-9s %d\n", magbloc, auth, i);
    }
    PQclear(res_set);
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
 *     ReadEventFromSC3NIABDatabase
 *  Calls:
 *     PrintPgsqlError, DropSpace, HumanToEpoch
 */
static int GetSC3Phases(EVREC *ep, PHAREC p[])
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[4096], psql[4096], PrevAuth[AGLEN], PrevSta[STALEN], w[10];
    int i, j, m, phid, rdid, numamps = 0;
    int numPhase = 0, msec = 0;
/*
 *  Get phases associated to the preferred origin
 */
    sprintf(psql, "select r._oid,                                           \
                          coalesce(p.m_creationInfo_agencyID, '%s'),        \
                          p.m_time_value, coalesce(p.m_time_value_ms, 0),   \
                          coalesce(r.m_phase_code, ''),                     \
                          coalesce(p.m_phaseHint_code, r.m_phase_code, ''), \
                          p.m_waveformID_stationCode,                       \
                          coalesce(p.m_waveformID_networkCode, '--'),       \
                          coalesce(nullif(p.m_waveformID_locationCode, ''), '--'), \
                          coalesce(p.m_waveformID_channelCode, '\?\?\?'),   \
                          coalesce(p.m_horizontalSlowness_value, 0),        \
                          coalesce(p.m_backazimuth_value, 0),               \
                          coalesce(r.m_distance, 0),                        \
                          coalesce(p.m_polarity, ' '),                      \
                          coalesce(p.m_onset, ' '),                         \
                          s.m_latitude, s.m_longitude,                      \
                          coalesce(s.m_elevation, 0),                       \
                          r.m_pickID,                                       \
                          coalesce(p.m_creationInfo_author, '%s')           \
                     from %sArrival r, %sPick p, %sPublicObject rp,         \
                          %sNetwork n, %sStation s                          \
                    where r._parent_oid = %d                                \
                      and rp.m_publicID = r.m_pickID                        \
                      and rp._oid = p._oid                                  \
                      and p.m_waveformID_networkCode = n.m_code             \
                      and p.m_waveformID_stationCode = s.m_code             \
                      and s._parent_oid = n._oid                            \
                      and p.m_time_value between s.m_start and              \
                          coalesce(s.m_end, '3001-01-01')                   \
                    order by r._oid",
              InAgency, InAgency, InputDB, InputDB,
              InputDB, InputDB, InputDB, ep->PreferredOrid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetSC3Phases: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetSC3Phases: phases");
    else {
        numPhase = PQntuples(res_set);
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
                PQclear(res_set);
                return 1;
            }
        }
        for (i = 0; i < ep->numPhase; i++) {
/*
 *          initialize
 */
            p[i].repid = NULLVAL;
            p[i].comp = ' ';
            p[i].delta = p[i].slow = p[i].azim = NULLVAL;
            p[i].phase_fixed = p[i].force_undef = p[i].init = 0;
            p[i].numamps = 0;
            p[i].hypid = ep->PreferredOrid;
/*
 *          populate
 */
            strcpy(p[i].arrid, PQgetvalue(res_set, i, 0));
            if (verbose > 3)
                fprintf(logfp, "        i=%d arrid=%s\n", i, p[i].arrid);
            p[i].phid = atoi(PQgetvalue(res_set, i, 0));
            strcpy(p[i].auth, PQgetvalue(res_set, i, 1));
            strcpy(p[i].rep, PQgetvalue(res_set, i, 19));
            strcpy(p[i].agency, "FDSN");
            msec = atoi(PQgetvalue(res_set, i, 3));
            p[i].time = HumanToEpoch(PQgetvalue(res_set, i, 2), msec, 1);
            strcpy(p[i].phase, PQgetvalue(res_set, i, 4));
            strcpy(p[i].ReportedPhase, PQgetvalue(res_set, i, 5));
            strcpy(p[i].sta, PQgetvalue(res_set, i, 6));
            strcpy(p[i].prista, PQgetvalue(res_set, i, 6));
            strcpy(p[i].deploy, PQgetvalue(res_set, i, 7));
            strcpy(p[i].lcn, PQgetvalue(res_set, i, 8));
            sprintf(p[i].fdsn, "%s.%s.%s.%s",
                    p[i].agency, p[i].deploy, p[i].sta, p[i].lcn);
            strcpy(p[i].pch, PQgetvalue(res_set, i, 9));
            if (p[i].pch[2] != '?')   p[i].comp = p[i].pch[2];
            if (strcmp(PQgetvalue(res_set, i, 10), "0"))
                p[i].slow = atof(PQgetvalue(res_set, i, 10));
            if (strcmp(PQgetvalue(res_set, i, 11), "0"))
                p[i].azim = atof(PQgetvalue(res_set, i, 11));
            if (strcmp(PQgetvalue(res_set, i, 12), "0"))
                p[i].delta = atof(PQgetvalue(res_set, i, 12));
            strcpy(w, PQgetvalue(res_set, i, 13));
            p[i].sp_fm = w[0];
            strcpy(w, PQgetvalue(res_set, i, 14));
            p[i].detchar = w[0];
            p[i].StaLat  = atof(PQgetvalue(res_set, i, 15));
            p[i].StaLon  = atof(PQgetvalue(res_set, i, 16));
            p[i].StaElev = atof(PQgetvalue(res_set, i, 17));
            strcpy(p[i].pickid, PQgetvalue(res_set, i, 18));
        }
        PQclear(res_set);
    }
/*
 *  get amplitudes associated to the preferred origin
 */
    sprintf(psql, "select r._oid, a._oid, a.m_type,                      \
                          coalesce(a.m_amplitude_value, 0),              \
                          coalesce(a.m_period_value, 0),                 \
                          coalesce(a.m_snr, 0),                          \
                          a.m_pickID,                                    \
                          coalesce(a.m_waveformID_channelCode, '\?\?\?') \
                     from %sAmplitude a, %sArrival r                     \
                    where r._parent_oid = %d                             \
                      and r.m_pickID = a.m_pickid                        \
                    order by r._oid",
            InputDB, InputDB, ep->PreferredOrid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetSC3Phases: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetSC3Phases: amplitudes");
    else {
        numamps = PQntuples(res_set);
        for (j = 0; j < numamps; j++) {
            phid = atoi(PQgetvalue(res_set, j, 0));
            for (i = 0; i < ep->numPhase; i++) {
                m = p[i].numamps;
                if (phid == p[i].phid) {
                    if (streq(PQgetvalue(res_set, j, 2), "snr")) {
                        p[i].snr = atof(PQgetvalue(res_set, j, 5));
                        continue;
                    }
/*
 *                  initializations
 */
                    p[i].a[m].ampdef = p[i].a[m].magid = 0;
                    p[i].a[m].magnitude = p[i].a[m].logat = NULLVAL;
/*
 *                  populate
 */
                    p[i].a[m].ampid = atoi(PQgetvalue(res_set, j, 1));
                    strcpy(p[i].a[m].magtype, PQgetvalue(res_set, j, 2));
                    p[i].a[m].amp = atof(PQgetvalue(res_set, j, 3));
                    p[i].a[m].per = atof(PQgetvalue(res_set, j, 4));
                    p[i].a[m].snr = atof(PQgetvalue(res_set, j, 5));
                    strcpy(p[i].a[m].amplitudeid, PQgetvalue(res_set, j, 6));
                    strcpy(p[i].a[m].ach, PQgetvalue(res_set, j, 7));
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
        PQclear(res_set);
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
