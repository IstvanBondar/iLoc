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
#ifdef PGSQL

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
extern PGconn *conn;

/*
 * Functions:
 *    ReadEventFromIDCNIABDatabase
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
 *     ReadEventFromIDCNIABDatabase
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
int ReadEventFromIDCNIABDatabase(EVREC *ep, HYPREC *hp[], PHAREC *pp[],
        char *magbloc)
{
/*
 *  Get preferred orid, numHypo and numPhase from origin table
 */
    if (GetIDCEvent(ep))
        return 1;
    if (verbose)
        fprintf(logfp, "    evid=%d preferred orid=%d numHypo=%d numPhase=%d\n",
                ep->evid, ep->PreferredOrid, ep->numHypo, ep->numPhase);
/*
 *  Allocate memory to hypocenter, phase and stamag structures
 */
    *hp = (HYPREC *)calloc(ep->numHypo, sizeof(HYPREC));
    if (*hp == NULL) {
        fprintf(logfp, "ReadEventFromIDCNIABDatabase: evid %d: cannot allocate memory\n", ep->evid);
        fprintf(errfp, "ReadEventFromIDCNIABDatabase: evid %d: cannot allocate memory\n", ep->evid);
        errorcode = 1;
        return 1;
    }
    *pp = (PHAREC *)calloc(ep->numPhase, sizeof(PHAREC));
    if (*pp == NULL) {
        fprintf(logfp, "ReadEventFromIDCNIABDatabase: evid %d: cannot allocate memory\n", ep->evid);
        fprintf(errfp, "ReadEventFromIDCNIABDatabase: evid %d: cannot allocate memory\n", ep->evid);
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
 *     ReadEventFromIDCNIABDatabase
 *  Calls:
 *     PrintPgsqlError, DropSpace
 */
static int GetIDCEvent(EVREC *ep)
{
    int i;
    PGresult *res_set = (PGresult *)NULL;
    char psql[2048], sql[2048];
    ep->numHypo = ep->PreferredOrid = ep->ISChypid = 0;
    ep->OutputDBPreferredOrid = ep->OutputDBISChypid = 0;
/*
 *  Get hypocentre count
 */
    sprintf(sql, "select count(*) from origin where evid = %d", ep->evid);
    if (verbose > 2) fprintf(logfp, "            GetIDCEvent: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetIDCEvent: error: hypocentre count");
    else {
        ep->numHypo = atoi(PQgetvalue(res_set, 0, 0));
    }
    PQclear(res_set);
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
                     from origin where evid = %d order by orid",
            ep->evid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetIDCEvent: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetIDCEvent: error: preferred orid");
    else {
        i = ep->numHypo - 1;
        ep->PreferredOrid = atoi(PQgetvalue(res_set, i, 0));
/*
 *      preferred origin was created by OutAgency?
 */
        if (streq(PQgetvalue(res_set, i, 1), OutAgency))
            ep->ISChypid = ep->PreferredOrid;
        strcpy(ep->etype, PQgetvalue(res_set, i, 2));
    }
    PQclear(res_set);
/*
 *  Get phase and station counts
 */
    sprintf(psql, "select count(arid), count(distinct sta) \
                     from assoc where orid = %d",
            ep->PreferredOrid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetIDCEvent: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetIDCEvent: error: phase count");
    else {
        ep->numPhase = atoi(PQgetvalue(res_set, 0, 0));
        ep->numSta = ep->numReading = atoi(PQgetvalue(res_set, 0, 1));
    }
    PQclear(res_set);
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
                     from assoc a, arrival r               \
                    where a.orid = %d and r.arid = a.arid",
            ep->PreferredOrid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetIDCEvent: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetIDCEvent: error: agency count");
    else {
        ep->numAgency = numAgencies = PQntuples(res_set);
        if (ep->numAgency == 0) {
            fprintf(errfp, "GetIDCEvent %d: no agencies found\n", ep->evid);
            fprintf(logfp, "GetIDCEvent %d: no agencies found\n", ep->evid);
            PQclear(res_set);
            return 1;
        }
        for (i = 0; i < numAgencies; i++)
            strcpy(agencies[i], PQgetvalue(res_set, i, 0));
        PQclear(res_set);
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
 *     ReadEventFromIDCNIABDatabase
 * Calls:
 *     PrintPgsqlError, DropSpace
 */
static int GetIDCHypocenter(EVREC *ep, HYPREC h[])
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[2048], psql[2048], s[4], Auth[AGLEN], PrevAuth[AGLEN];
    int numHypo = 0;
    int i = 0, j;
/*
 *  Get preferred origin
 */
    sprintf(psql, "select o.orid, o.time, o.lat, o.lon,             \
                          coalesce(o.depth, o.depdp), o.dtype,      \
                          o.nass, o.ndef, coalesce(o.auth, 'IDC'),  \
                          e.sdobs, e.stime, e.sdepth,               \
                          e.smajax, e.sminax, e.strike              \
                     from origin o, origerr e                       \
                    where o.orid = %d and o.evid = %d               \
                      and o.orid = e.orid",
            ep->PreferredOrid, ep->evid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetIDCHypocenter: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetIDCHypocenter: hypocentre");
    else {
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
        h[i].hypid = atoi(PQgetvalue(res_set, i, 0));
        h[i].time = atof(PQgetvalue(res_set, i, 1));
        h[i].lat  = atof(PQgetvalue(res_set, i, 2));
        h[i].lon  = atof(PQgetvalue(res_set, i, 3));
        if (PQgetisnull(res_set, i, 4))      { h[i].depth = NULLVAL; }
        else         { h[i].depth = atof(PQgetvalue(res_set, i, 4)); }
        strcpy(s, PQgetvalue(res_set, i, 5));
        if (streq(s, "r") || streq(s, "g")) h[i].depfix = 1;
        if (PQgetisnull(res_set, i, 6))       { h[i].nass = NULLVAL; }
        else          { h[i].nass = atoi(PQgetvalue(res_set, i, 6)); }
        if (PQgetisnull(res_set, i, 7))       { h[i].ndef = NULLVAL; }
        else          { h[i].ndef = atoi(PQgetvalue(res_set, i, 7)); }
        strcpy(h[i].agency, PQgetvalue(res_set, i, 8));
        strcpy(h[i].rep, PQgetvalue(res_set, i, 8));
        if (PQgetisnull(res_set, i, 9))      { h[i].sdobs = NULLVAL; }
        else         { h[i].sdobs = atof(PQgetvalue(res_set, i, 9)); }
        if (PQgetisnull(res_set, i, 10))     { h[i].stime = NULLVAL; }
        else        { h[i].stime = atof(PQgetvalue(res_set, i, 10)); }
        if (PQgetisnull(res_set, i, 11))    { h[i].sdepth = NULLVAL; }
        else       { h[i].sdepth = atof(PQgetvalue(res_set, i, 11)); }
        if (PQgetisnull(res_set, i, 12))    { h[i].smajax = NULLVAL; }
        else       { h[i].smajax = atof(PQgetvalue(res_set, i, 12)); }
        if (PQgetisnull(res_set, i, 13))    { h[i].sminax = NULLVAL; }
        else       { h[i].sminax = atof(PQgetvalue(res_set, i, 13)); }
        if (PQgetisnull(res_set, i, 14))    { h[i].strike = NULLVAL; }
        else       { h[i].strike = atof(PQgetvalue(res_set, i, 14)); }
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
    sprintf(psql, "select o.orid, o.time, o.lat, o.lon,             \
                          coalesce(o.depth, o.depdp), o.dtype,      \
                          o.nass, o.ndef, coalesce(o.auth, 'IDC'),  \
                          e.sdobs, e.stime, e.sdepth,               \
                          e.smajax, e.sminax, e.strike              \
                     from origin o, origerr e                       \
                    where o.orid <> %d and o.evid = %d              \
                      and o.orid = e.orid",
            ep->PreferredOrid, ep->evid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetIDCHypocenter: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetIDCHypocenter: hypocentre");
    else {
        numHypo = PQntuples(res_set);
        strcpy(PrevAuth, h[0].agency);
        for (j = 0; j < numHypo; j++) {
            strcpy(Auth, PQgetvalue(res_set, j, 8));
/*
 *          consider distinct agencies only
 */
            if (streq(Auth, PrevAuth))
                continue;
/*
 *          initialize
 */
            h[i].depfix = h[i].epifix = h[i].timfix = h[i].rank = 0;
            strcpy(h[i].etype, ep->etype);
            h[i].nsta = h[i].ndefsta = NULLVAL;
            h[i].mindist = h[i].maxdist = h[i].azimgap = h[i].sgap = NULLVAL;
/*
 *          populate
 */
            h[i].hypid = atoi(PQgetvalue(res_set, j, 0));
            h[i].time = atof(PQgetvalue(res_set, j, 1));
            h[i].lat  = atof(PQgetvalue(res_set, j, 2));
            h[i].lon  = atof(PQgetvalue(res_set, j, 3));
            if (PQgetisnull(res_set, j, 4))      { h[i].depth = NULLVAL; }
            else         { h[i].depth = atof(PQgetvalue(res_set, j, 4)); }
            strcpy(s, PQgetvalue(res_set, j, 5));
            if (streq(s, "r") || streq(s, "g")) h[i].depfix = 1;
            if (PQgetisnull(res_set, j, 6))       { h[i].nass = NULLVAL; }
            else          { h[i].nass = atoi(PQgetvalue(res_set, j, 6)); }
            if (PQgetisnull(res_set, j, 7))       { h[i].ndef = NULLVAL; }
            else          { h[i].ndef = atoi(PQgetvalue(res_set, j, 7)); }
            strcpy(h[i].agency, Auth);
            strcpy(h[i].rep, Auth);
            if (PQgetisnull(res_set, j, 9))      { h[i].sdobs = NULLVAL; }
            else         { h[i].sdobs = atof(PQgetvalue(res_set, j, 9)); }
            if (PQgetisnull(res_set, j, 10))     { h[i].stime = NULLVAL; }
            else        { h[i].stime = atof(PQgetvalue(res_set, j, 10)); }
            if (PQgetisnull(res_set, j, 11))    { h[i].sdepth = NULLVAL; }
            else       { h[i].sdepth = atof(PQgetvalue(res_set, j, 11)); }
            if (PQgetisnull(res_set, j, 12))    { h[i].smajax = NULLVAL; }
            else       { h[i].smajax = atof(PQgetvalue(res_set, j, 12)); }
            if (PQgetisnull(res_set, j, 13))    { h[i].sminax = NULLVAL; }
            else       { h[i].sminax = atof(PQgetvalue(res_set, j, 13)); }
            if (PQgetisnull(res_set, j, 14))    { h[i].strike = NULLVAL; }
            else       { h[i].strike = atof(PQgetvalue(res_set, j, 14)); }
            if (verbose > 1)
                fprintf(logfp, "        i=%d evid=%d hypid=%d agency=%s\n",
                        i, ep->evid, h[i].hypid, h[i].agency);
            strcpy(PrevAuth, Auth);
            i++;
            if (i > ep->numHypo) {
                fprintf(errfp, "GetIDCHypocenter: unexpected numHypo %d\n", i);
                fprintf(logfp, "GetIDCHypocenter: unexpected numHypo %d\n", i);
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
 *     ReadEventFromIDCNIABDatabase
 * Calls:
 *     PrintPgsqlError, DropSpace
 */
static int GetIDCMagnitude(EVREC *ep, HYPREC h[], char *magbloc)
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
        sprintf(psql, "select magnitude, magtype, uncertainty, \
                              nsta, coalesce(auth, 'IDC')      \
                         from netmag                           \
                        where orid = %d                        \
                        order by magtype",
                h[i].hypid);
        DropSpace(psql, sql);
        if (verbose > 2) fprintf(logfp, "            GetIDCMagnitude: %s\n", sql);
        if ((res_set = PQexec(conn, sql)) == NULL)
            PrintPgsqlError("GetIDCMagnitude: magnitude block");
        else {
            n = PQntuples(res_set);
            for (j = 0; j < n; j++) {
                mag = atof(PQgetvalue(res_set, j, 0));
                strcpy(mtype, PQgetvalue(res_set, j, 1));
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
 *     ReadEventFromIDCNIABDatabase
 *  Calls:
 *     PrintPgsqlError, DropSpace
 */
static int GetIDCPhases(EVREC *ep, PHAREC p[])
{
    char sql[2048], psql[2048], PrevAuth[AGLEN], PrevSta[STALEN];
    int i, m, j, rdid;
    int numPhase = 0, numamps = 0, arid = 0, ampid = 0;
    double amp = 0., per = 0.;
    char chan[10], w[10];
    char amptype[10], ampchan[10];
    PGresult *res_set = (PGresult *)NULL;
/*
 *  Get phases associated to the preferred origin
 */
    sprintf(psql, "select a.arid, a.sta, r.chan, a.delta, a.phase, \
                          r.iphase, r.time, r.slow, r.azimuth,     \
                          coalesce(r.auth, '%s'), r.fm, r.qual,    \
                          s.lat, s.lon, 1000. * s.elev             \
                     from assoc a, arrival r, site s               \
                    where a.orid = %d                              \
                      and a.arid = r.arid                          \
                      and s.sta = a.sta                            \
                      and r.jdate >= s.ondate                      \
                      and (s.offdate = -1 or r.jdate <= s.offdate) \
                    order by a.arid",
            InAgency, ep->PreferredOrid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetIDCPhases: %s\n", sql);
    if ((res_set = PQexec(conn, psql)) == NULL)
        PrintPgsqlError("GetIDCPhases: phases");
    else {
        numPhase = PQntuples(res_set);
        if (numPhase != ep->numPhase) {
            fprintf(errfp, "GetIDCPhases: unexpected number phases!\n");
            fprintf(errfp, "    evid=%d orid=%d epnumPhase=%d numPhase=%d\n",
                    ep->evid, ep->PreferredOrid, ep->numPhase, numPhase);
            fprintf(errfp, "    often due to a missing station in site\n");
            fprintf(logfp, "GetIDCPhases: unexpected number phases!\n");
            fprintf(logfp, "    evid=%d orid=%d epnumPhase=%d numPhase=%d\n",
                    ep->evid, ep->PreferredOrid, ep->numPhase, numPhase);
            fprintf(logfp, "    often due to a missing station in site\n");
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
            strcpy(p[i].pch, "???");
/*
 *          populate
 */
            strcpy(p[i].arrid, PQgetvalue(res_set, i, 0));
            p[i].phid = atoi(PQgetvalue(res_set, i, 0));
            if (verbose > 3)
                fprintf(logfp, "        i=%d phid=%d\n", i, p[i].phid);
            strcpy(p[i].sta, PQgetvalue(res_set, i, 1));
            strcpy(p[i].prista, p[i].sta);
            strcpy(chan,  PQgetvalue(res_set, i, 2));
            if (chan[2] == 'Z' || chan[2] == 'E' || chan[2] == 'N') {
                strcpy(p[i].pch, chan);
                p[i].comp = chan[2];
            }
            if (PQgetisnull(res_set, i, 3))       { p[i].delta = NULLVAL; }
            else          { p[i].delta = atof(PQgetvalue(res_set, i, 3)); }
            strcpy(p[i].phase, PQgetvalue(res_set, i, 4));
            strcpy(p[i].ReportedPhase, PQgetvalue(res_set, i, 5));
            if (PQgetisnull(res_set, i, 6))        { p[i].time = NULLVAL; }
            else           { p[i].time = atof(PQgetvalue(res_set, i, 6)); }
            if (PQgetisnull(res_set, i, 7))        { p[i].slow = NULLVAL; }
            else           { p[i].slow = atof(PQgetvalue(res_set, i, 7)); }
            if (PQgetisnull(res_set, i, 8))        { p[i].azim = NULLVAL; }
            else           { p[i].azim = atof(PQgetvalue(res_set, i, 8)); }
            strcpy(p[i].rep, PQgetvalue(res_set, i, 9));
            strcpy(p[i].auth, p[i].rep);
            strcpy(p[i].agency, p[i].rep);
            strcpy(w, PQgetvalue(res_set, i, 10));
            p[i].sp_fm = w[0];
            p[i].lp_fm = w[1];
            strcpy(w, PQgetvalue(res_set, i, 11));
            p[i].detchar = w[0];
            p[i].StaLat = atof(PQgetvalue(res_set, i, 12));
            p[i].StaLon = atof(PQgetvalue(res_set, i, 13));
            if (PQgetisnull(res_set, i, 13))         { p[i].StaElev = 0.; }
            else       { p[i].StaElev = atof(PQgetvalue(res_set, i, 4)); }
            strcpy(p[i].deploy, "IR");
            strcpy(p[i].lcn, "--");
            sprintf(p[i].fdsn, "FDSN.IR.%s.--", p[i].sta);
        }
    }
    PQclear(res_set);
/*
 *  get amplitudes
 */
    sprintf(psql, "select m.arid, m.ampid, m.amp, m.per, \
                          coalesce(m.amptype, ' '),      \
                          coalesce(m.chan, '\?\?\?')     \
                     from amplitude m, assoc a           \
                    where a.orid = %d                    \
                      and m.arid = a.arid                \
                 order by m.arid",
            ep->PreferredOrid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetIDCPhases: %s\n", sql);
    if ((res_set = PQexec(conn, psql)) == NULL)
        PrintPgsqlError("GetIDCPhases: amplitudes");
    else {
        numamps = PQntuples(res_set);
        for (j = 0; j < numamps; j++) {
            arid = atoi(PQgetvalue(res_set, j, 0));
            ampid = atoi(PQgetvalue(res_set, j, 1));
            if (PQgetisnull(res_set, j, 2)) { amp = NULLVAL; }
            else    { amp = atof(PQgetvalue(res_set, j, 2)); }
            if (PQgetisnull(res_set, j, 3)) { per = NULLVAL; }
            else    { per = atof(PQgetvalue(res_set, j, 3)); }
            strcpy(amptype, PQgetvalue(res_set, j, 4));
            strcpy(ampchan, PQgetvalue(res_set, j, 5));
            for (i = 0; i < ep->numPhase; i++) {
                m = p[i].numamps;
                if (arid == p[i].phid) {
/*
 *                  initializations
 */
                    p[i].a[m].ampdef = p[i].a[m].magid = 0;
                    p[i].a[m].magnitude = p[i].a[m].logat = NULLVAL;
                    strcpy(p[i].a[m].magtype, "");
/*
 *                  check for valid peak-to-peak or zero-to-peak amplitudes
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
 *                  Find out component
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
        }
    }
    PQclear(res_set);
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

#endif  /* PGSQL */
#endif  /* IDCDB */

/*  EOF */
