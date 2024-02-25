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
#ifdef ISCDB
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

/*
 * Functions:
 *    ReadEventFromISCPgsqldatabase
 */

/*
 * Local functions
 */
static int GetISCEvent(EVREC *ep);
static int GetISCHypocenter(EVREC *ep, HYPREC h[]);
static int GetISCMagnitude(EVREC *ep, HYPREC h[], char *magbloc);
static int GetISCPhases(EVREC *ep, PHAREC p[]);

extern PGconn *conn;

/*
 *  Title:
 *     ReadEventFromISCPgsqlDatabase
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
 *     GetISCEvent, GetISCHypocenter, GetISCMagnitude, GetISCPhases, Free
 */
int ReadEventFromISCPgsqlDatabase(EVREC *ep, HYPREC *hp[], PHAREC *pp[],
        char *magbloc)
{
/*
 *  Get prime, numHypo and numPhase from hypocenter and event tables
 */
    if (GetISCEvent(ep))
        return 1;
    if (verbose)
        fprintf(logfp, "    evid=%d prime=%d numHypo=%d numPhase=%d\n",
                ep->evid, ep->PreferredOrid, ep->numHypo, ep->numPhase);
/*
 *  Allocate memory to hypocenter, phase and stamag structures
 */
    *hp = (HYPREC *)calloc(ep->numHypo, sizeof(HYPREC));
    if (*hp == NULL) {
        fprintf(logfp, "ReadEventFromISCPgsqlDatabase: evid %d: cannot allocate memory\n", ep->evid);
        fprintf(errfp, "ReadEventFromISCPgsqlDatabase: evid %d: cannot allocate memory\n", ep->evid);
        errorcode = 1;
        return 1;
    }
    *pp = (PHAREC *)calloc(ep->numPhase, sizeof(PHAREC));
    if (*pp == NULL) {
        fprintf(logfp, "ReadEventFromISCPgsqlDatabase: evid %d: cannot allocate memory\n", ep->evid);
        fprintf(errfp, "ReadEventFromISCPgsqlDatabase: evid %d: cannot allocate memory\n", ep->evid);
        Free(*hp);
        errorcode = 1;
        return 1;
    }
/*
 *  Fill array of hypocenter structures.
 */
    if (GetISCHypocenter(ep, *hp)) {
        Free(*hp);
        Free(*pp);
        return 1;
    }
/*
 *  get reported magnitudes
 */
    if (GetISCMagnitude(ep, *hp, magbloc)) {
        Free(*hp);
        Free(*pp);
        return 1;
    }
/*
 *  Fill array of phase structures.
 */
    if (GetISCPhases(ep, *pp)) {
        Free(*hp);
        Free(*pp);
        return 1;
    }
    return 0;
}

/*
 *  Title:
 *     GetISCEvent
 *  Synopsis:
 *     Gets prime, event type, number of hypocenters, phases, readings,
 *     stations and reporting agencies for an event from DB.
 *  Input Arguments:
 *     ep - pointer to structure containing event information.
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     ReadEventFromISCPgsqldatabase
 *  Calls:
 *     PrintPgsqlError, DropSpace
 */
static int GetISCEvent(EVREC *ep)
{
    int i;
    PGresult *res_set = (PGresult *)NULL;
    char psql[2048], sql[2048];
    ep->ISChypid = 0;
    ep->OutputDBPreferredOrid = 0;
    ep->OutputDBISChypid = 0;
/*
 *  Get hypid of ISC hypocentre if it exists
 */
    sprintf(psql, "select hypid              \
                     from %shypocenter       \
                    where deprecated is null \
                      and author = '%s'      \
                      and isc_evid = %d", InputDB, InAgency, ep->evid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetISCEvent: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetISCEvent: error: isc hypocentre");
    else {
        if (PQntuples(res_set) == 0)
            ep->ISChypid = 0;
        else if (PQntuples(res_set) == 1)
            ep->ISChypid = atoi(PQgetvalue(res_set, 0, 0));
        else {
/*
 *          if more than one ISC hypocentre exists,
 *          find the one with association records
 */
            PQclear(res_set);
            sprintf(psql, "select distinct h.hypid     \
                             from %shypocenter h, %sassociation a \
                            where h.deprecated is null \
                              and h.author = '%s'      \
                              and h.author = a.author  \
                              and h.hypid = a.hypid    \
                              and h.isc_evid = %d",
                    InputDB, InputDB, InAgency, ep->evid);
            DropSpace(psql, sql);
            if (verbose > 2) fprintf(logfp, "            GetISCEvent: %s\n", sql);
            if ((res_set = PQexec(conn, sql)) == NULL)
                PrintPgsqlError("GetISCEvent: error: isc hypocentre");
            else
                ep->ISChypid = atoi(PQgetvalue(res_set, 0, 0));
        }
    }
    PQclear(res_set);
    if (verbose > 1) fprintf(logfp, "        ISChypid=%d\n", ep->ISChypid);
/*
 *  Get hypid and etype of prime hypocentre
 */
    sprintf(psql, "select prime_hyp, coalesce(etype, 'ke') \
                     from %sevent where evid = %d", InputDB, ep->evid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetISCEvent: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetISCEvent: error: prime hypocentre");
    else if (PQntuples(res_set) == 1) {
        ep->PreferredOrid = atoi(PQgetvalue(res_set, 0, 0));
        strcpy(ep->etype, PQgetvalue(res_set, 0, 1));
        PQclear(res_set);
    }
    else {
        PQclear(res_set);
        fprintf(errfp, "GetISCEvent: %d no prime found\n", ep->evid);
        fprintf(logfp, "GetISCEvent: %d no prime found\n", ep->evid);
        return 1;
    }
/*
 *  Get prime hypid and ISChypid in OutputDB if it differs from InputDB
 */
    if (strcmp(InputDB, OutputDB)) {
        ep->OutputDBPreferredOrid = 0;
        sprintf(psql, "select prime_hyp from %sevent where evid = %d",
                OutputDB, ep->evid);
        DropSpace(psql, sql);
        if (verbose > 2) fprintf(logfp, "            GetISCEvent: %s\n", sql);
        if ((res_set = PQexec(conn, sql)) == NULL) {
            PrintPgsqlError("GetISCEvent: error: prime hypocentre");
        }
        else if (PQntuples(res_set) == 1) {
            ep->OutputDBPreferredOrid = atoi(PQgetvalue(res_set, 0, 0));
        }
        else {
            ep->OutputDBPreferredOrid = 0;
        }
        PQclear(res_set);
        sprintf(psql, "select hypid from %shypocenter       \
                        where author = '%s' and isc_evid = %d",
                OutputDB, OutAgency, ep->evid);
        DropSpace(psql, sql);
        if (verbose > 2) fprintf(logfp, "            GetISCEvent: %s\n", sql);
        if ((res_set = PQexec(conn, sql)) == NULL) {
            PrintPgsqlError("GetISCEvent: error: isc hypocentre");
        }
        else if (PQntuples(res_set) == 1) {
            ep->OutputDBISChypid = atoi(PQgetvalue(res_set, 0, 0));
        }
        else {
            ep->OutputDBISChypid = 0;
        }
        PQclear(res_set);
    }
/*
 *  Get hypocentre count
 */
    sprintf(psql, "select count(hypid)                             \
                     from %shypocenter                             \
                    where (deprecated is null or deprecated = 'M') \
                      and hypid = pref_hypid                       \
                      and isc_evid = %d", InputDB, ep->evid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetISCEvent: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetISCEvent: error: hypocentre count");
    else {
        ep->numHypo = atoi(PQgetvalue(res_set, 0, 0));
    }
    PQclear(res_set);
    if (ep->numHypo == 0) {
        fprintf(errfp, "GetISCEvent %d: no hypocentres found\n", ep->evid);
        fprintf(logfp, "GetISCEvent %d: no hypocentres found\n", ep->evid);
        return 1;
    }
    if (verbose > 1) fprintf(logfp, "        %d hypocentres\n", ep->numHypo);
/*
 *  Get phase, reading and station count
 */
    sprintf(psql, "select count(p.phid), count(distinct p.rdid),       \
                          count(distinct a.sta)                        \
                     from %sphase p, %sassociation a                   \
                    where a.hypid = %d                                 \
                      and a.author = '%s'                              \
                      and p.phid = a.phid                              \
                      and (p.deprecated is null or p.deprecated = 'M') \
                      and  a.deprecated is null",
            InputDB, InputDB, ep->PreferredOrid, InAgency);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetISCEvent: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetISCEvent: error: phase count");
    else {
        ep->numPhase = atoi(PQgetvalue(res_set, 0, 0));
        ep->numReading = atoi(PQgetvalue(res_set, 0, 1));
        ep->numSta = atoi(PQgetvalue(res_set, 0, 2));
    }
    PQclear(res_set);
    if (ep->numPhase == 0) {
        fprintf(errfp, "GetISCEvent %d: no phases found\n", ep->evid);
        fprintf(logfp, "GetISCEvent %d: no phases found\n", ep->evid);
        return 1;
    }
    if (ep->numReading == 0) {
        fprintf(errfp, "GetISCEvent %d: no readings found\n", ep->evid);
        fprintf(logfp, "GetISCEvent %d: no readings found\n", ep->evid);
        return 1;
    }
    if (ep->numSta == 0) {
        fprintf(errfp, "GetISCEvent %d: no stations found\n", ep->evid);
        fprintf(logfp, "GetISCEvent %d: no stations found\n", ep->evid);
        return 1;
    }
    if (verbose > 1)
        fprintf(logfp, "        %d phases, %d readings, %d stations\n",
                ep->numPhase, ep->numReading, ep->numSta);
/*
 *  Get agencies that reported phases
 */
    sprintf(psql, "select distinct r.reporter                          \
                     from %sphase p, %sassociation a, %sreport r       \
                    where a.hypid = %d                                 \
                      and a.author = '%s'                              \
                      and p.phid = a.phid                              \
                      and r.repid = p.reporter                         \
                      and (p.deprecated is null or p.deprecated = 'M') \
                      and  a.deprecated is null",
            InputDB, InputDB, InputDB, ep->PreferredOrid, InAgency);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetISCEvent: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetISCEvent: error: agency count");
    else {
        ep->numAgency = numAgencies = PQntuples(res_set);
        if (ep->numAgency == 0) {
            fprintf(errfp, "GetISCEvent %d: no agencies found\n", ep->evid);
            fprintf(logfp, "GetISCEvent %d: no agencies found\n", ep->evid);
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
 *     GetISCHypocenter
 *  Synopsis:
 *     Reads reported hypocentres for an event from DB.
 *         Fields from the hypocenter table:
 *            hypid
 *            origin epoch time
 *            lat, lon
 *            depth (depdp if depth is null)
 *            depfix, epifix, timfix
 *            nsta, ndefsta, nass, ndef
 *            mindist, maxdist, azimgap
 *            author
 *        Fields from the hypoc_err table:
 *            hypid
 *            sminax, smajax, strike
 *            stime, sdepth
 *            sdobs
 *        Fields from the hypoc_acc table:
 *            hypid
 *            score
 *     Sorts hypocentres by score, the first being the prime hypocentre.
 *  Input Arguments:
 *     ep  - pointer to event info
 *     h[] - array of hypocentres.
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     ReadEventFromISCPgsqldatabase
 * Calls:
 *     PrintPgsqlError, DropSpace
 */
static int GetISCHypocenter(EVREC *ep, HYPREC h[])
{
    PGresult *res_set = (PGresult *)NULL;
    HYPREC temp;
    char sql[2048], psql[2048];
    int numHypo = 0, numerrs = 0;
    int i, j, row;
    int hypid = 0, score = 0;
    int sec = 0, msec = 0;
    double minax = 0., majax = 0., strike = 0., stime = 0.;
    double sdepth = 0., sdobs = 0.;
/*
 *  Get hypocenters
 */
    sprintf(psql, "select hypid, extract(epoch from day), coalesce(msec, 0), \
                          lat, lon, coalesce(depth, depdp),                  \
                          coalesce(depfix, '0'), coalesce(epifix, '0'),      \
                          coalesce(timfix, '0'), nsta, ndefsta, nass, ndef,  \
                          mindist, maxdist, azimgap, author                  \
                     from %shypocenter                                       \
                    where (deprecated is null or deprecated = 'M')           \
                      and hypid = pref_hypid                                 \
                      and isc_evid = %d                                      \
                 order by hypid", InputDB, ep->evid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetISCHypocenter: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetISCHypocenter: hypocentre");
    else {
        numHypo = PQntuples(res_set);
        for (i = 0; i < numHypo; i++) {
            h[i].depfix = 0;
            h[i].epifix = 0;
            h[i].timfix = 0;
            h[i].rank = 0;
            strcpy(h[i].etype, ep->etype);
            h[i].sminax = NULLVAL;
            h[i].smajax = NULLVAL;
            h[i].strike = NULLVAL;
            h[i].stime = NULLVAL;
            h[i].sdepth = NULLVAL;
            h[i].sdobs = NULLVAL;
            h[i].hypid = atoi(PQgetvalue(res_set, i, 0));
            sec  = atoi(PQgetvalue(res_set, i, 1));
            msec = atoi(PQgetvalue(res_set, i, 2));
            h[i].time = (double)sec + (double)msec / 1000.;
            h[i].lat  = atof(PQgetvalue(res_set, i, 3));
            h[i].lon  = atof(PQgetvalue(res_set, i, 4));
            if (PQgetisnull(res_set, i, 5))       { h[i].depth = NULLVAL; }
            else          { h[i].depth = atof(PQgetvalue(res_set, i, 5)); }
            if (strcmp(PQgetvalue(res_set, i, 6), "0"))  h[i].depfix = 1;
            if (strcmp(PQgetvalue(res_set, i, 7), "0"))  h[i].epifix = 1;
            if (strcmp(PQgetvalue(res_set, i, 8), "0"))  h[i].timfix = 1;
            if (PQgetisnull(res_set, i, 9))        { h[i].nsta = NULLVAL; }
            else           { h[i].nsta = atoi(PQgetvalue(res_set, i, 9)); }
            if (PQgetisnull(res_set, i, 10))    { h[i].ndefsta = NULLVAL; }
            else       { h[i].ndefsta = atoi(PQgetvalue(res_set, i, 10)); }
            if (PQgetisnull(res_set, i, 11))       { h[i].nass = NULLVAL; }
            else          { h[i].nass = atoi(PQgetvalue(res_set, i, 11)); }
            if (PQgetisnull(res_set, i, 12))       { h[i].ndef = NULLVAL; }
            else          { h[i].ndef = atoi(PQgetvalue(res_set, i, 12)); }
            if (PQgetisnull(res_set, i, 13))    { h[i].mindist = NULLVAL; }
            else       { h[i].mindist = atof(PQgetvalue(res_set, i, 13)); }
            if (PQgetisnull(res_set, i, 14))    { h[i].maxdist = NULLVAL; }
            else       { h[i].maxdist = atof(PQgetvalue(res_set, i, 14)); }
            if (PQgetisnull(res_set, i, 15))    { h[i].azimgap = NULLVAL; }
            else       { h[i].azimgap = atof(PQgetvalue(res_set, i, 15)); }
            strcpy(h[i].agency, PQgetvalue(res_set, i, 16));
            if (verbose > 1)
                fprintf(logfp, "        i=%d evid=%d hypid=%d agency=%s\n",
                        i, ep->evid, h[i].hypid, h[i].agency);
        }
    }
    PQclear(res_set);
    if (numHypo != ep->numHypo) {
        fprintf(errfp, "GetISCHypocenter: unexpected numHypo %d\n", numHypo);
        fprintf(logfp, "GetISCHypocenter: unexpected numHypo %d\n", numHypo);
        return 1;
    }
/*
 *  Get hypocenter uncertainties
 */
    sprintf(psql, "select e.hypid, sminax, smajax, strike, \
                          stime, sdepth, sdobs             \
                     from %shypocenter h, %shypoc_err e    \
                    where isc_evid = %d                    \
                      and e.hypid = h.hypid                \
                      and h.hypid = h.pref_hypid           \
                      and (h.deprecated is null or h.deprecated = 'M') \
                 order by h.hypid;", InputDB, InputDB, ep->evid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetISCHypocenter: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetISCHypocenter: hypoc_err");
    else {
        numerrs = PQntuples(res_set);
        for (row = 0; row < numerrs; row++) {
            hypid = atoi(PQgetvalue(res_set, row, 0));
            if (PQgetisnull(res_set, row, 1))      { minax = NULLVAL; }
            else         { minax = atof(PQgetvalue(res_set, row, 1)); }
            if (PQgetisnull(res_set, row, 2))      { majax = NULLVAL; }
            else         { majax = atof(PQgetvalue(res_set, row, 2)); }
            if (PQgetisnull(res_set, row, 3))     { strike = NULLVAL; }
            else        { strike = atof(PQgetvalue(res_set, row, 3)); }
            if (PQgetisnull(res_set, row, 4))      { stime = NULLVAL; }
            else         { stime = atof(PQgetvalue(res_set, row, 4)); }
            if (PQgetisnull(res_set, row, 5))     { sdepth = NULLVAL; }
            else        { sdepth = atof(PQgetvalue(res_set, row, 5)); }
            if (PQgetisnull(res_set, row, 6))      { sdobs = NULLVAL; }
            else         { sdobs = atof(PQgetvalue(res_set, row, 6)); }
            for (i = 0; i < numHypo; i++) {
                if (hypid == h[i].hypid) {
                    h[i].sminax = minax;
                    h[i].smajax = majax;
                    h[i].strike = strike;
                    h[i].stime = stime;
                    h[i].sdepth = sdepth;
                    h[i].sdobs = sdobs;
                    break;
                }
            }
        }
    }
    PQclear(res_set);
/*
 *  Get hypocenter scores
 */
    sprintf(psql, "select h.hypid, a.score              \
                     from %shypocenter h, %shypoc_acc a \
                    where h.isc_evid = %d               \
                      and a.hypid = h.hypid             \
                      and h.hypid = h.pref_hypid        \
                      and (h.deprecated is null or h.deprecated = 'M') \
                 order by h.hypid;", InputDB, InputDB, ep->evid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetISCHypocenter: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetISCHypocenter: hypoc_acc");
    else {
        numerrs = PQntuples(res_set);
        for (row = 0; row < numerrs; row++) {
            hypid = atoi(PQgetvalue(res_set, row, 0));
            if (PQgetisnull(res_set, row, 1))       { score = 0.; }
            else     { score = atof(PQgetvalue(res_set, row, 1)); }
            for (i = 0; i < numHypo; i++) {
                if (hypid == h[i].hypid) {
                    h[i].rank = score;
                    break;
                }
            }
        }
    }
    PQclear(res_set);
/*
 *  set h[0] to prime
 */
    for (i = 1; i < numHypo; i++) {
        if (h[i].hypid == ep->PreferredOrid) {
            swap(h[i], h[0]);
            break;
        }
    }
/*
 *  sort the rest by score
 */
    for (i = 1; i < numHypo - 1; i++) {
        for (j = i + 1; j < numHypo; j++) {
            if (h[i].rank < h[j].rank) {
                swap(h[i], h[j]);
            }
        }
    }
    return 0;
}

/*
 *  Title:
 *     GetISCMagnitude
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
 *     get_sc3_data
 * Calls:
 *     PrintPgsqlError, DropSpace
 */
static int GetISCMagnitude(EVREC *ep, HYPREC h[], char *magbloc)
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
           "select coalesce(magnitude, -9),                  \
                   coalesce(replace(magtype, ' ', ''), 'Q'), \
                   coalesce(uncertainty, -1),                \
                   coalesce(nsta, -1), author                \
              from %snetmag                                  \
             where hypid = %d                                \
             order by type",
        InputDB, h[i].hypid);
        DropSpace(psql, sql);
        if (verbose > 2) fprintf(logfp, "            GetISCMagnitude: %s\n", sql);
        if ((res_set = PQexec(conn, sql)) == NULL)
            PrintPgsqlError("GetISCMagnitude: magnitude block");
        else {
            n = PQntuples(res_set);
            for (j = 0; j < n; j++) {
                mag = atof(PQgetvalue(res_set, j, 0));
                if (mag < -8)
                    continue;
                if (streq(PQgetvalue(res_set, j, 1), "Q")) strcpy(mtype, "");
                else strcpy(mtype, PQgetvalue(res_set, j, 1));
                smag = atof(PQgetvalue(res_set, j, 2));
                nsta = atoi(PQgetvalue(res_set, j, 3));
                strcpy(mtype, PQgetvalue(res_set, j, 4));
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
 *     GetISCPhases
 *  Synopsis:
 *     Reads phase, amplitude, and reporter association data for an event
 *         from DB.
 *     Performs three seperate queries as phases may or may not be associated
 *         by their reporter and may or may not have one or more amplitudes.
 *     Reads phases ISC associated to the prime hypocentre
 *         Fields from the association, phase, site and reporter tables:
 *             phase.phid
 *             phase.rdid
 *             phase.day, phase.msec (arrival epoch time)
 *             phase.phase (reported phase)
 *             phase.sta
 *             phase.slow, phase.azim
 *             phase.chan, phase.sp_fm, phase.emergent, phase.impulsive
 *             association.reporter (reporter id)
 *             association.phase_fixed
 *             site.lat, site.lon, site.elev
 *             site.prista (site.sta if prista is null)
 *             reporter.reporter (reporting agency)
 *             association.delta
 *             association.phase (ISC phase)
 *             association.nondef
 *     Reads reported associations for any of the phases selected above.
 *         Replaces reported phase name with the reported association if it was
 *         reidentified by NEIC, CSEM, EHB or IASPEI.
 *     Reads amplitudes associated to the prime hypocentre.
 *         Fields from the amplitude table:
 *             phid
 *             amp, per
 *             logat
 *             amptype
 *             chan
 *             ampid
 *     Sorts phase structures so that they ordered by delta, prista, rdid, time.
 *         Uses prista rather than sta so that duplicates weighted properly.
 *     Marks initial arrivals in readings.
 *  Input Arguments:
 *     ep  - pointer to event info
 *     p[] - array of phase structures
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     ReadEventFromISCPgsqldatabase
 *  Calls:
 *     PrintPgsqlError, DropSpace
 */
static int GetISCPhases(EVREC *ep, PHAREC p[])
{
    char sql[2048], psql[2048];
    int i, m, row, prev_rdid;
    int numPhase = 0, numamps = 0, numrepas = 0, rephypset = 0;
    int ass_phid = 0, amp_phid = 0, amp_ampid = 0, hypid = 0;
    int sec = 0, msec = 0, emergent = 0, impulsive = 0;
    double amp = 0., per = 0., logat = 0.;
    char ass_phase[PHALEN], chan[10], w[10];
    char amptype[10], ampchan[10];
    PGresult *res_set = (PGresult *)NULL;
/*
 *  Get phases ISC associated to the prime hyp
 *     site.net is null for registered stations!
 */
    sprintf(psql, "select p.phid, rdid,                  \
                          extract(epoch from day), msec, \
                          coalesce(p.phase, ''),         \
                          p.sta, p.slow, p.azim,         \
                          coalesce(chan, '   '),         \
                          coalesce(sp_fm, ' '),          \
                          coalesce(emergent, '0'),       \
                          coalesce(impulsive, '0'),      \
                          a.reporter,                    \
                          coalesce(phase_fixed, '0'),    \
                          s.lat, s.lon, s.elev,          \
                          coalesce(prista, s.sta),       \
                          coalesce(r.reporter, ''),      \
                          delta,                         \
                          coalesce(a.phase, ''),         \
                          coalesce(a.nondef, '0')        \
                     from %sphase p, %ssite s,           \
                          %sassociation a, %sreport r    \
                    where hypid = %d                     \
                      and a.author = '%s'                \
                      and a.deprecated is null           \
                      and a.phid = p.phid                \
                      and (p.deprecated is null or p.deprecated = 'M') \
                      and s.sta = p.sta                  \
                      and s.net is null                  \
                      and p.reporter = r.repid           \
                 order by p.phid",
            InputDB, InputDB, InputDB, InputDB, ep->PreferredOrid, InAgency);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetISCPhases: %s\n", sql);
    if ((res_set = PQexec(conn, psql)) == NULL)
        PrintPgsqlError("GetISCPhases: phases");
    else {
        numPhase = PQntuples(res_set);
        /* Check for something very odd - */
        /* GetISCEvent does same query to fill ep->numPhase. */
        if (numPhase != ep->numPhase) {
            fprintf(errfp, "GetISCPhases: unexpected number phases!\n");
            fprintf(errfp, "    evid=%d prime=%d epnumPhase=%d numPhase=%d\n",
                    ep->evid, ep->PreferredOrid, ep->numPhase, numPhase);
            fprintf(errfp, "    often due to a missing station in site\n");
            fprintf(logfp, "GetISCPhases: unexpected number phases!\n");
            fprintf(logfp, "    evid=%d prime=%d epnumPhase=%d numPhase=%d\n",
                    ep->evid, ep->PreferredOrid, ep->numPhase, numPhase);
            fprintf(logfp, "    often due to a missing station in site\n");
            PQclear(res_set);
            return 1;
        }
        for (i = 0; i < numPhase; i++) {
            strcpy(p[i].arrid, PQgetvalue(res_set, i, 0));
            p[i].phid = atoi(PQgetvalue(res_set, i, 0));
            p[i].rdid = atoi(PQgetvalue(res_set, i, 1));
            if (verbose > 3)
                fprintf(logfp, "        i=%d phid=%d rdid=%d\n",
                        i, p[i].phid, p[i].rdid);
            if (PQgetisnull(res_set, i, 2))         { sec = NULLVAL; }
            else            { sec = atoi(PQgetvalue(res_set, i, 2)); }
            if (PQgetisnull(res_set, i, 3))              { msec = 0; }
            else           { msec = atoi(PQgetvalue(res_set, i, 3)); }
            /* arrival epoch time */
            if (sec != NULLVAL)
                p[i].time = (double)sec + (double)msec / 1000.;
            else
                p[i].time = NULLVAL;
            strcpy(p[i].ReportedPhase, PQgetvalue(res_set, i, 4));
            strcpy(p[i].sta, PQgetvalue(res_set, i, 5));
            if (PQgetisnull(res_set, i, 6))        { p[i].slow = NULLVAL; }
            else           { p[i].slow = atof(PQgetvalue(res_set, i, 6)); }
            if (PQgetisnull(res_set, i, 7))        { p[i].azim = NULLVAL; }
            else           { p[i].azim = atof(PQgetvalue(res_set, i, 7)); }
            strcpy(chan,  PQgetvalue(res_set, i, 8));
            strcpy(p[i].pch, chan);
            if (chan[2] != '?') p[i].comp = chan[2];
            else                p[i].comp = ' ';
            strcpy(w, PQgetvalue(res_set, i, 9));
            p[i].sp_fm = w[0];
            strcpy(w, PQgetvalue(res_set, i, 10));
            emergent = (streq(w, "0")) ? 0 : 1;
            strcpy(w, PQgetvalue(res_set, i, 11));
            impulsive = (streq(w, "0")) ? 0 : 1;
            if (emergent && impulsive) p[i].detchar = 'q';
            else if (impulsive)        p[i].detchar = 'i';
            else if (emergent)         p[i].detchar = 'e';
            else                       p[i].detchar = ' ';
            p[i].repid = atoi(PQgetvalue(res_set, i, 12));
            strcpy(w, PQgetvalue(res_set, i, 13));
            p[i].phase_fixed = (streq(w, "0")) ? 0 : 1;
            p[i].StaLat = atof(PQgetvalue(res_set, i, 14));
            p[i].StaLon = atof(PQgetvalue(res_set, i, 15));
            if (PQgetisnull(res_set, i, 16))        { p[i].StaElev = 0.; }
            else      { p[i].StaElev = atof(PQgetvalue(res_set, i, 16)); }
            strcpy(p[i].prista, PQgetvalue(res_set, i, 17));
            strcpy(p[i].rep, PQgetvalue(res_set, i, 18));
            strcpy(p[i].auth, p[i].rep);
            strcpy(p[i].agency, p[i].rep);
            if (PQgetisnull(res_set, i, 19))      { p[i].delta = NULLVAL; }
            else         { p[i].delta = atof(PQgetvalue(res_set, i, 19)); }
            strcpy(p[i].phase, PQgetvalue(res_set, i, 20));
            strcpy(w, PQgetvalue(res_set, i, 21));
            p[i].force_undef = (streq(w, "0")) ? 0 : 1;
            p[i].numamps = 0;
//            strcpy(p[i].agency, "FDSN");
            strcpy(p[i].deploy, "IR");
            strcpy(p[i].lcn, "--");
            sprintf(p[i].fdsn, "FDSN.IR.%s.--", p[i].sta);
        }
    }
    PQclear(res_set);
/*
 *  Select reported associations for any of the phases selected above.
 *  Use phase from reported association over one from phase in case
 *      re-identified by NEIC, CSEM, EHB or IASPEI
 */
    sprintf(psql, "select a1.hypid, coalesce(a1.phase, ''), a1.phid     \
                     from %sassociation a1, %sassociation a2, %sphase p \
                    where a2.hypid = %d                                 \
                      and a2.author = '%s'                              \
                      and a2.deprecated is null                         \
                      and p.phid = a2.phid                              \
                      and (p.deprecated is null or p.deprecated = 'M')  \
                      and a1.phid = a2.phid                             \
                      and a1.author != '%s'                             \
                 order by a1.phid",
            InputDB, InputDB, InputDB, ep->PreferredOrid, InAgency, InAgency);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetISCPhases: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("GetISCPhases: reported associations");
    else {
        numrepas = PQntuples(res_set);
        for (row = 0; row < numrepas; row++) {
            hypid = atoi(PQgetvalue(res_set, row, 0));
            strcpy(ass_phase, PQgetvalue(res_set, row, 1));
            ass_phid = atoi(PQgetvalue(res_set, row, 2));
            for (i = 0; i < numPhase; i++) {
                rephypset = 1;
                if (ass_phid == p[i].phid &&
                    (streq(p[i].agency, "NEIC") ||
                     streq(p[i].agency, "NEIS") ||
                     streq(p[i].agency, "EHB")  ||
                     streq(p[i].agency, "CSEM") ||
                     streq(p[i].agency, "IASPEI"))) {
                    p[i].hypid = hypid;
                    rephypset = 0;
                    if (strlen(ass_phase) > 0)
                        strcpy(p[i].ReportedPhase, ass_phase);
                }
                if (rephypset)
                    p[i].hypid = NULLVAL;
            }
        }
    }
    PQclear(res_set);
/*
 *  get amplitudes
 */
    sprintf(psql, "select am.phid, am.amp, am.per, am.logat,   \
                          coalesce(am.amptype, ''),            \
                          coalesce(am.chan, '   '), am.ampid   \
                     from %samplitude am, %sassociation a, %sphase p \
                    where a.hypid = %d                         \
                      and a.author = '%s'                      \
                      and a.deprecated is null                 \
                      and p.phid = a.phid                      \
                      and (p.deprecated is null or p.deprecated = 'M') \
                      and am.phid = p.phid                     \
                      and am.deprecated is null                \
                 order by am.phid",
            InputDB, InputDB, InputDB, ep->PreferredOrid, InAgency);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            GetISCPhases: %s\n", sql);
    if ((res_set = PQexec(conn, psql)) == NULL)
        PrintPgsqlError("GetISCPhases: amplitudes");
    else {
        numamps = PQntuples(res_set);
        for (row = 0; row < numamps; row++) {
            amp_phid = atoi(PQgetvalue(res_set, row, 0));
            if (PQgetisnull(res_set, row, 1))        { amp = NULLVAL; }
            else           { amp = atof(PQgetvalue(res_set, row, 1)); }
            if (PQgetisnull(res_set, row, 2))        { per = NULLVAL; }
            else           { per = atof(PQgetvalue(res_set, row, 2)); }
            if (PQgetisnull(res_set, row, 3))      { logat = NULLVAL; }
            else         { logat = atof(PQgetvalue(res_set, row, 3)); }
            strcpy(amptype, PQgetvalue(res_set, row, 4));
            strcpy(ampchan, PQgetvalue(res_set, row, 5));
            amp_ampid = atoi(PQgetvalue(res_set, row, 6));
            for (i = 0; i < numPhase; i++) {
                m = p[i].numamps;
                if (amp_phid == p[i].phid) {
/*
 *                  by default, amplitudes are interpreted as zero-to-peak
 *                  0-to-p and p-to-p are interpreted,
 *                  everything else is ignored
 */
                    if (streq("0-to-p", amptype))
                        p[i].a[m].amp = amp;
                    else if (streq("p-to-p", amptype))
                        p[i].a[m].amp = amp / 2.;
                    else if (streq("velocity", amptype))
                        continue;
                    else if (streq("s", amptype))
                        continue;
                    else if (strlen(amptype))
                        continue;
                    else
                        p[i].a[m].amp = amp;
                    p[i].a[m].ampid = amp_ampid;
                    p[i].a[m].per = per;
                    p[i].a[m].logat = logat;
/*
 *                  If no component given for amplitude then use one from phase
 */
                    strcpy(p[i].a[m].ach, ampchan);
                    if (ampchan[2] == 'Z' ||
                        ampchan[2] == 'E' ||
                        ampchan[2] == 'N')  p[i].a[m].comp = ampchan[2];
                    else if (p[i].comp)     p[i].a[m].comp = p[i].comp;
                    else                    p[i].a[m].comp = ' ';
/*
 *                  initializations
 */
                    p[i].a[m].ampdef = p[i].a[m].magid = 0;
                    p[i].a[m].magnitude = NULLVAL;
                    strcpy(p[i].a[m].magtype, "");
                    p[i].numamps++;
                }
            }
        }
    }
    PQclear(res_set);
/*
 *  Sort phase structures so that they ordered by delta, prista, rdid, time.
 *  prista rather than sta used so that duplicates weighted properly.
 */
    if (verbose > 2)
        fprintf(logfp, "            GetISCPhases: sort phases (%.2f)\n", secs(&t0));
    SortPhasesFromDatabase(numPhase, p);
/*
 *  mark initial arrivals in readings
 */
    prev_rdid = -1;
    for (i = 0; i < numPhase; i++) {
        p[i].init = 0;
        if (p[i].rdid != prev_rdid)
            p[i].init = 1;
        prev_rdid = p[i].rdid;
    }
    if (verbose > 2)
        PrintPhases(numPhase, p);
    return 0;
}

#endif
#endif  /* ISCDB */

/*  EOF */
