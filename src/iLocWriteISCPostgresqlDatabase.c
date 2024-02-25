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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS and CONTRIBUTORS "AS IS"
 * and ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY and FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED and
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifdef ISCDB
#ifdef PGSQL

#include "iLoc.h"
extern int verbose;                                        /* verbose level */
extern FILE *logfp;
extern FILE *errfp;
extern int errorcode;
extern struct timeval t0;
extern char InputDB[VALLEN];       /* read data from this DB account, if any */
extern char OutputDB[VALLEN];    /* write results to this DB account, if any */
extern char OutAgency[VALLEN];      /* author for new hypocentres and assocs */
extern char InAgency[VALLEN];                     /* author for input assocs */
extern int repid;                                             /* reporter id */
extern int MinNetmagSta;                 /* min number of stamags for netmag */

/*
 * Functions:
 *    WriteEventToISCPgsqlDatabase
 *    RemoveISCHypocenter
 *    ReplaceISCPrime
 *    ReplaceISCAssociation
 */

/*
 * Local functions
 */
static void UpdateISCEvent(EVREC *ep);
static void WriteISCEvent(EVREC *ep);
static int WriteISCHypocenter(EVREC *ep, SOLREC *sp, FE *fep);
static void WriteISCHypocenterError(EVREC *ep, SOLREC *sp);
static void WriteISCHypocenterAccuracy(EVREC *ep, SOLREC *sp, HYPQUAL *hq);
static int WriteISCNetmag(SOLREC *sp);
static int WriteISCStamag(SOLREC *sp, STAMAG **stamag);
static int WriteISCReadingmag(SOLREC *sp, STAMAG **rdmag, MSZH *mszh);
static int WriteISCAmplitudemag(SOLREC *sp, PHAREC p[]);
static void WriteISCAssociation(SOLREC *sp, PHAREC p[]);
static void DeleteISChypid(int ISChypid);
static void DeleteISCAssociation(int old_prime, char *author);
static void DeleteISCmagnitudes(int ISChypid);
static int GetISCNextID(char *sequence, int *nextid);

extern PGconn *conn;

/*
 *  Title:
 *     WriteEventToISCPgsqlDatabase
 *  Synopsis:
 *     Writes solution to database.
 *     Removes old ISC solution if any, and populates hypocenter table.
 *     Populates or updates event table if necessary.
 *     Populates hypoc_err, hypoc_acc and network_quality tables.
 *     Deletes previous ISC associations and populates association table.
 *     Populates netmag, stamag, rdmag, ampmag and ms_zh tables.
 *  Input Arguments:
 *     ep     - pointer to event info
 *     sp     - pointer to current solution
 *     p[]    - array of phase structures
 *     hq     - pointer to hypocentre quality structure
 *     stamag - array of station magnitude structures
 *     rdmag  - array of reading magnitude structures
 *     mszh   - array of MS vertical/horizontal magnitude structures
 *     fep    - pointer to Flinn_Engdahl region number structure
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     Locator
 *  Calls:
 *     WriteISCHypocenter, WriteISCEvent, UpdateISCEvent,
 *     WriteISCHypocenterError, WriteISCHypocenterAccuracy,
 *     DeleteISCAssociation, WriteISCAssociation, WriteISCNetmag,
 *     WriteISCStamag, WriteISCReadingmag, WriteISCAmplitudemag
 */
int WriteEventToISCPgsqlDatabase(EVREC *ep, SOLREC *sp, PHAREC p[],
        HYPQUAL *hq, STAMAG **stamag, STAMAG **rdmag, MSZH *mszh, FE *fep)
{
    int hypid = ep->PreferredOrid, old_ISChypid = ep->ISChypid;
    char author[VALLEN];
    strcpy(author, InAgency);
    if (verbose)
        fprintf(logfp, "        WriteEventToISCPgsqlDatabase: prev_hypid=%d evid=%d\n",
                ep->ISChypid, ep->evid);
/*
 *  populate hypocenter table; remove old ISC solution
 */
    if (verbose) fprintf(logfp, "        WriteISCHypocenter\n");
    if (WriteISCHypocenter(ep, sp, fep))
        return 1;
    if (strcmp(InputDB, OutputDB) && !ep->OutputDBPreferredOrid) {
/*
 *      populate event table in OutputDB if necessary
 */
        if (verbose) fprintf(logfp, "        WriteISCEvent\n");
        WriteISCEvent(ep);
    }
    else if (old_ISChypid == 0) {
/*
 *      update event table if new hypocentre
 */
        if (verbose) fprintf(logfp, "        UpdateISCEvent\n");
        UpdateISCEvent(ep);
    }
    sp->hypid = ep->ISChypid;
/*
 *  populate hypoc_err table
 */
    if (verbose) fprintf(logfp, "        WriteISCHypocenterError\n");
    WriteISCHypocenterError(ep, sp);
/*
 *  populate hypoc_acc and network_quality tables
 */
    if (verbose) fprintf(logfp, "        WriteISCHypocenterAccuracy\n");
    WriteISCHypocenterAccuracy(ep, sp, hq);
/*
 *  delete previous associations
 */
    if (verbose) fprintf(logfp, "        DeleteISCAssociation\n");
    if (strcmp(InputDB, OutputDB) && ep->OutputDBPreferredOrid) {
        hypid = ep->OutputDBPreferredOrid;
        strcpy(author, OutAgency);
    }
    DeleteISCAssociation(hypid, author);
/*
 *  populate association table
 */
    if (verbose) fprintf(logfp, "        WriteISCAssociation\n");
    WriteISCAssociation(sp, p);
/*
 *  populate netmag table
 */
    if (sp->nnetmag) {
        if (verbose) fprintf(logfp, "        WriteISCNetmag\n");
        if (WriteISCNetmag(sp))
            return 1;
    }
/*
 *  populate stamag, rdmag and ampmag tables
 */
    if (sp->nstamag) {
        if (verbose) fprintf(logfp, "        WriteISCStamag\n");
        if (WriteISCStamag(sp, stamag))
            return 1;
        if (verbose) fprintf(logfp, "        WriteISCReadingmag\n");
        if (WriteISCReadingmag(sp, rdmag, mszh))
            return 1;
        if (verbose) fprintf(logfp, "        WriteISCAmplitudemag\n");
        if (WriteISCAmplitudemag(sp, p))
            return 1;
    }
/*
 *  set prime hypid
 */
    ep->PreferredOrid = ep->ISChypid;
    if (verbose)
        fprintf(logfp, "        WriteEventToISCPgsqlDatabase (%.2f)\n", secs(&t0));
    return 0;
}

/*
 *  Title:
 *     UpdateISCEvent
 *  Synopsis:
 *     Updates prime_hyp in the event table.
 *     Only called if the ISC hypocentre is new.
 *  Input Arguments:
 *     ep - pointer to event info
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     WriteEventToISCPgsqlDatabase
 *  Calls:
 *     PrintPgsqlError, DropSpace
 */
static void UpdateISCEvent(EVREC *ep)
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[MAXBUF], psql[MAXBUF], errmsg[MAXBUF];
/*
 *  Update prime_hyp
 */
    sprintf(psql, "update %sevent set prime_hyp = %d, moddate = now() \
                    where evid = %d", OutputDB, ep->ISChypid, ep->evid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            UpdateISCEvent: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("UpdateISCEvent:");
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "UpdateISCEvent: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
}

/*
 *  Title:
 *     WriteISCEvent
 *  Synopsis:
 *     Populates event table in OutputDB if event is not yet there
 *  Input Arguments:
 *     ep - pointer to event info
 *  Called by:
 *     WriteEventToISCPgsqlDatabase
 *  Calls:
 *     PrintPgsqlError, DropSpace
 */
static void WriteISCEvent(EVREC *ep)
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[MAXBUF], psql[MAXBUF], errmsg[MAXBUF];
    sprintf(psql, "insert into %sevent                  \
                   (author, evid, lddate, moddate,      \
                    prime_hyp, ready, reporter, etype)  \
                   values ('%s', %d, now(), now(), %d, 'R', %d, '%s')",
            OutputDB, OutAgency, ep->evid, ep->ISChypid, repid, ep->etype);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            WriteISCEvent: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("WriteISCEvent:");
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "WriteISCEvent: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
}

/*
 *  Title:
 *     WriteISCHypocenter
 *  Synopsis:
 *     Populates hypocenter table.
 *     Gets a new hypid if previous prime was not ISC.
 *     Deletes previous prime if it was ISC.
 *     Sets depfix flag:
 *         null: Free-depth solution
 *            A: Depth fixed by ISC Analyst
 *            S: Anthropogenic event; depth fixed to surface
 *            G: Depth fixed to ISC default depth grid
 *            R: Depth fixed to ISC default region depth
 *            M: Depth fixed to median depth of reported hypocentres
 *            B: Beyond depth limits; depth fixed to 0/600 km
 *            H: Depth fixed to depth of a reported hypocentre
 *            D: Depth fixed to depth-phase depth
 *  Input Arguments:
 *     ep  - pointer to event info
 *     sp  - pointer to current solution
 *     fep - pointer to Flinn_Engdahl region number structure
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     WriteEventToISCPgsqlDatabase
 *  Calls:
 *     PrintPgsqlError, DropSpace, GetISCNextID, GregionNumber, GregToSreg
 */
static int WriteISCHypocenter(EVREC *ep, SOLREC *sp, FE *fep)
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[2048], psql[2048], errmsg[2048];
    char s[100], depfix[2];
    int nextid = 0, day = 0, msec = 0, grn = 0, srn = 0;
    double x = 0., z = 0.;
    if      (sp->FixedDepthType == 8) strcpy(depfix, "A"); /* analyst */
    else if (sp->FixedDepthType == 1) strcpy(depfix, "B"); /* beyond limit */
    else if (sp->FixedDepthType == 2) strcpy(depfix, "H"); /* hypocentre */
    else if (sp->FixedDepthType == 3) strcpy(depfix, "D"); /* depdp */
    else if (sp->FixedDepthType == 4) strcpy(depfix, "S"); /* surface */
    else if (sp->FixedDepthType == 5) strcpy(depfix, "G"); /* depth grid */
    else if (sp->FixedDepthType == 6) strcpy(depfix, "M"); /* median depth */
    else if (sp->FixedDepthType == 7) strcpy(depfix, "R"); /* GRN depth */
    else                          strcpy(depfix, "F"); /* free depth */
/*
 *  origin time: day (seconds) and msec (milliseconds)
 *     note that sprintf does the rounding
 *     negative epoch times are handled correctly
 */
    sprintf(s, "%.3f\n", sp->time);
    x = atof(s);
    day = (int)x;
    sprintf(s, "%.3f", x - day);
    z = atof(s);
    if (z < 0.) {
        z += 1.;
        day--;
    }
    msec = (int)(1000. * z);
/*
 *  Flinn-Engdahl grn and srn
 */
    grn = GregionNumber(sp->lat, sp->lon, fep);
    srn = GregToSreg(grn);
/*
 *  previous prime is not ISC; get a new hypid
 */
    if (ep->ISChypid == 0) {
        if (GetISCNextID("hypid", &nextid))
            return 1;
        if (verbose > 2)
            fprintf(logfp, "            new hypid=%d\n", nextid);
    }
/*
 *  remove prime flag from previous prime
 */
    sprintf(sql, "update %shypocenter set prime = NULL where isc_evid = %d",
            OutputDB, ep->evid);
    if (verbose > 2)
        fprintf(logfp, "            WriteISCHypocenter: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("WriteISCHypocenter: ");
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK ) {
        sprintf(errmsg, "WriteISCHypocenter: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
/*
 *  delete previous prime if it was ISC
 */
    if (!nextid) {
        if (streq(InputDB, OutputDB)) {
            DeleteISChypid(ep->ISChypid);
        }
        else if (ep->OutputDBISChypid) {
            DeleteISChypid(ep->OutputDBISChypid);
        }
    }
    else {
        ep->ISChypid = nextid;
    }
/*
 *  populate hypocenter table
 */
    sprintf(psql, "insert into %shypocenter                                \
                   (hypid, pref_hypid, isc_evid, day, msec, lat, lon, srn, \
                    grn, depth, depdp, ndp, depfix, epifix, timfix, ndef,  \
                    nass, nsta, ndefsta, nrank, mindist, maxdist, azimgap, \
                    reporter, author, prime, etype, lddate) ", OutputDB);
    sprintf(psql, "%s values (%d, %d, %d, ",
            psql, ep->ISChypid, ep->ISChypid, ep->evid);
    sprintf(psql, "%s timestamp 'epoch' + %d * interval '1 second', %d,",
            psql, day, msec);
    sprintf(psql, "%s %f, %f, %d, %d, ", psql, sp->lat, sp->lon, srn, grn);
    if (sp->depdp == NULLVAL)
        sprintf(psql, "%s %f, NULL, NULL, ", psql, sp->depth);
    else
        sprintf(psql, "%s %f, %f, %d, ", psql, sp->depth, sp->depdp, sp->ndp);
/*
 *  depfix for free depth solutions is set to NULL
 */
    if (streq(depfix, "F")) sprintf(psql, "%s NULL, ", psql);
    else                    sprintf(psql, "%s '%s', ", psql, depfix);
    if (sp->epifix) sprintf(psql, "%s 'F', ", psql);
    else            sprintf(psql, "%s NULL, ", psql);
    if (sp->timfix) sprintf(psql, "%s 'F', ", psql);
    else            sprintf(psql, "%s NULL, ", psql);
    sprintf(psql, "%s %d, %d, %d, %d, %d, ",
            psql, sp->ndef, sp->nass, sp->nreading, sp->ndefsta, sp->prank);
    if (sp->mindist == NULLVAL)
        sprintf(psql, "%s NULL, NULL, NULL,", psql);
    else
        sprintf(psql, "%s %f, %f, %f,",
                psql, sp->mindist, sp->maxdist, sp->azimgap);
    sprintf(psql, "%s %d, '%s', 'P', '%s', now())",
            psql, repid, OutAgency, ep->etype);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            WriteISCHypocenter: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("WriteISCHypocenter:");
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "WriteISCHypocenter: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
    if (verbose) fprintf(logfp, "    WriteISCHypocenter: done\n");
    return 0;
}

/*
 *  Title:
 *     WriteISCHypocenterError
 *  Synopsis:
 *     Populates hypoc_err table.
 *  Input Arguments:
 *     ep  - pointer to event info
 *     sp  - pointer to current solution
 *  Called by:
 *     WriteEventToISCPgsqlDatabase
 *  Calls:
 *     PrintPgsqlError, DropSpace
 */
static void WriteISCHypocenterError(EVREC *ep, SOLREC *sp)
{
    extern double ConfidenceLevel;                       /* from config file */
    PGresult *res_set = (PGresult *)NULL;
    int i;
    char sql[2048], psql[2048], errmsg[2048];
    char conf[5];
    if      (fabs(ConfidenceLevel - 90.) < DEPSILON) strcpy(conf, "90");
    else if (fabs(ConfidenceLevel - 95.) < DEPSILON) strcpy(conf, "95");
    else if (fabs(ConfidenceLevel - 98.) < DEPSILON) strcpy(conf, "98");
    else                                             strcpy(conf, "NULL");
    sprintf(psql, "insert into %shypoc_err                                    \
                   (hypid, stt, stx, sty, stz, sxx, sxy, syy, syz, szx, szz,  \
                    sdobs, smajax, sminax, strike, stime, slon, slat, sdepth, \
                    sdepdp, reporter, author, ConfidenceLevel, lddate) values (%d, ",
                OutputDB, ep->ISChypid);
/*
 *  stt
 */
    if (sp->timfix)
        sprintf(psql, "%s NULL, NULL, NULL, NULL, ",psql);
    else {
        if (sp->covar[0][0] > 9999.99)
            sprintf(psql, "%s %f, ", psql, 9999.999);
        else
            sprintf(psql, "%s %f, ", psql, sp->covar[0][0]);
/*
 *      stx, sty
 */
        if (sp->epifix)
            sprintf(psql, "%s NULL, NULL, ",psql);
        else {
            if (fabs(sp->covar[0][1]) > 9999.99)
                sprintf(psql, "%s %f, ", psql, 9999.999);
            else
                sprintf(psql, "%s %f, ", psql, sp->covar[0][1]);
            if (fabs(sp->covar[0][2]) > 9999.99)
                sprintf(psql, "%s %f, ", psql, 9999.999);
            else
                sprintf(psql, "%s %f, ", psql, sp->covar[0][2]);
        }
/*
 *      stz
 */
        if (sp->depfix)
            sprintf(psql, "%s NULL, ",psql);
        else {
            if (fabs(sp->covar[0][3]) > 9999.99)
                sprintf(psql, "%s %f, ", psql, 9999.999);
            else
                sprintf(psql, "%s %f, ", psql, sp->covar[0][3]);
        }
    }
/*
 *  sxx, sxy, syy
 */
    if (sp->epifix)
        sprintf(psql, "%s NULL, NULL, NULL, NULL, NULL, ",psql);
    else {
        if (sp->covar[1][1] > 9999.99)
            sprintf(psql, "%s %f, ", psql, 9999.999);
        else
            sprintf(psql, "%s %f, ", psql, sp->covar[1][1]);
        if (fabs(sp->covar[1][2]) > 9999.99)
            sprintf(psql, "%s %f, ", psql, 9999.999);
        else
            sprintf(psql, "%s %f, ", psql, sp->covar[1][2]);
        if (fabs(sp->covar[2][2]) > 9999.99)
            sprintf(psql, "%s %f, ", psql, 9999.999);
        else
            sprintf(psql, "%s %f, ", psql, sp->covar[2][2]);
/*
 *      szx, syz
 */
        if (sp->depfix)
            sprintf(psql, "%s NULL, NULL, ",psql);
        else {
            if (fabs(sp->covar[1][3]) > 9999.99)
                sprintf(psql, "%s %f, ", psql, 9999.999);
            else
                sprintf(psql, "%s %f, ", psql, sp->covar[1][3]);
            if (fabs(sp->covar[2][3]) > 9999.99)
                sprintf(psql, "%s %f, ", psql, 9999.999);
            else
                sprintf(psql, "%s %f, ", psql, sp->covar[2][3]);
        }
    }
/*
 *  szz
 */
    if (sp->depfix)
        sprintf(psql, "%s NULL, ",psql);
    else {
        if (sp->covar[3][3] > 9999.99)
            sprintf(psql, "%s %f, ", psql, 9999.999);
        else
            sprintf(psql, "%s %f, ", psql, sp->covar[3][3]);
    }
/*
 *  uncertainties
 */
    if (sp->sdobs == NULLVAL)
        sprintf(psql, "%s NULL, ", psql);
    else if (sp->sdobs > 9999.99)
        sprintf(psql, "%s %f, ", psql, 9999.999);
    else
        sprintf(psql, "%s %f, ", psql, sp->sdobs);
    if (sp->smajax == NULLVAL)
        sprintf(psql, "%s NULL, NULL, NULL, ", psql);
    else {
        if (sp->sminax > 9999.99)
            sprintf(psql, "%s %f, %f, %f, ",
                    psql, 9999.999, 9999.999, sp->strike);
        else if (sp->smajax > 9999.99)
            sprintf(psql, "%s %f, %f, %f, ",
                    psql, 9999.999, sp->sminax, sp->strike);
        else
            sprintf(psql, "%s %f, %f, %f, ",
                    psql, sp->smajax, sp->sminax, sp->strike);
    }
    for (i = 0; i < 4; i++) {
        if (sp->error[i] == NULLVAL)
            sprintf(psql, "%s NULL, ", psql);
        else if (fabs(sp->error[i]) > 999.99)
            sprintf(psql, "%s %f, ", psql, 999.999);
        else
            sprintf(psql, "%s %f, ", psql, sp->error[i]);
    }
    if (sp->depdp_error == NULLVAL)
        sprintf(psql, "%s NULL, ", psql);
    else
        sprintf(psql, "%s %f, ", psql, sp->depdp_error);
/*
 *  reporter, author, confidence level
 */
    if (streq(conf, "NULL"))
        sprintf(psql, "%s %d, '%s', NULL, now())",
                psql, repid, OutAgency);
    else
        sprintf(psql, "%s %d, '%s', '%s', now())",
                psql, repid, OutAgency, conf);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp,"            WriteISCHypocenterError: %s\n",sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("WriteISCHypocenterError:");
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "WriteISCHypocenterError: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
}

/*
 *  Title:
 *     WriteISCHypocenterAccuracy
 *  Synopsis:
 *     Populates hypoc_acc and network_quality tables.
 *  Input Arguments:
 *     ep  - pointer to event info
 *     sp  - pointer to current solution
 *     hq  - pointer to hypocentre quality structure
 *  Called by:
 *     WriteEventToISCPgsqlDatabase
 *  Calls:
 *     PrintPgsqlError, DropSpace
 */
static void WriteISCHypocenterAccuracy(EVREC *ep, SOLREC *sp, HYPQUAL *hq)
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[2048], psql[2048], errmsg[2048];
/*
 *  populate hypoc_acc table
 */
    sprintf(psql, "insert into %shypoc_acc                           \
                   (hypid, reporter, nstaloc, nsta10, gtcand, score) \
                   values (%d, %d, %d, %d, %d, %f)",
            OutputDB, ep->ISChypid, repid, sp->ndefsta,
            hq->numStaWithin10km, hq->GT5candidate, hq->score);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            WriteISCHypocenterAccuracy: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("WriteISCHypocenterAccuracy:");
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "WriteISCHypocenterAccuracy: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
/*
 *  populate network_quality table
 */
    if (hq->LocalNetwork.maxdist > 0.) {
        sprintf(psql, "insert into %snetwork_quality           \
                       (hypid, reporter, type, du, gap,        \
                        secondary_gap, nsta, mindist, maxdist) \
                       values (%d, %d, 'local', %f, %f, %f, %d, %f, %f)",
                OutputDB, ep->ISChypid, repid, hq->LocalNetwork.du,
                hq->LocalNetwork.gap, hq->LocalNetwork.sgap,
                hq->LocalNetwork.ndefsta, hq->LocalNetwork.mindist,
                hq->LocalNetwork.maxdist);
        DropSpace(psql, sql);
        if (verbose > 2) fprintf(logfp, "            put_hyp: %s\n", sql);
        if ((res_set = PQexec(conn, sql)) == NULL)
            PrintPgsqlError("put_hyp: network_quality:");
        else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
            sprintf(errmsg, "put_hyp: network_quality: %d",
                    PQresultStatus(res_set));
            PrintPgsqlError(errmsg);
        }
        PQclear(res_set);
    }
    if (hq->NearRegionalNetwork.maxdist > 0.) {
        sprintf(psql, "insert into %snetwork_quality           \
                       (hypid, reporter, type, du, gap,        \
                        secondary_gap, nsta, mindist, maxdist) \
                       values (%d, %d, 'near', %f, %f, %f, %d, %f, %f)",
                OutputDB, ep->ISChypid, repid, hq->NearRegionalNetwork.du,
                hq->NearRegionalNetwork.gap, hq->NearRegionalNetwork.sgap,
                hq->NearRegionalNetwork.ndefsta, hq->NearRegionalNetwork.mindist,
                hq->NearRegionalNetwork.maxdist);
        DropSpace(psql, sql);
        if (verbose > 2) fprintf(logfp, "            put_hyp: %s\n", sql);
        if ((res_set = PQexec(conn, sql)) == NULL)
            PrintPgsqlError("put_hyp: network_quality:");
        else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
            sprintf(errmsg, "put_hyp: network_quality: %d",
                    PQresultStatus(res_set));
            PrintPgsqlError(errmsg);
        }
        PQclear(res_set);
    }
    if (hq->TeleseismicNetwork.maxdist > 0.) {
        sprintf(psql, "insert into %snetwork_quality           \
                       (hypid, reporter, type, du, gap,        \
                        secondary_gap, nsta, mindist, maxdist) \
                       values (%d, %d, 'tele', %f, %f, %f, %d, %f, %f)",
                OutputDB, ep->ISChypid, repid, hq->TeleseismicNetwork.du,
                hq->TeleseismicNetwork.gap, hq->TeleseismicNetwork.sgap,
                hq->TeleseismicNetwork.ndefsta, hq->TeleseismicNetwork.mindist,
                hq->TeleseismicNetwork.maxdist);
        DropSpace(psql, sql);
        if (verbose > 2) fprintf(logfp, "            put_hyp: %s\n", sql);
        if ((res_set = PQexec(conn, sql)) == NULL)
            PrintPgsqlError("put_hyp: network_quality:");
        else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
            sprintf(errmsg, "put_hyp: network_quality: %d",
                    PQresultStatus(res_set));
            PrintPgsqlError(errmsg);
        }
        PQclear(res_set);
    }
    if (hq->FullNetwork.maxdist > 0.) {
        sprintf(psql, "insert into %snetwork_quality           \
                       (hypid, reporter, type, du, gap,        \
                        secondary_gap, nsta, mindist, maxdist) \
                       values (%d, %d, 'whole', %f, %f, %f, %d, %f, %f)",
                OutputDB, ep->ISChypid, repid, hq->FullNetwork.du,
                hq->FullNetwork.gap, hq->FullNetwork.sgap,
                hq->FullNetwork.ndefsta, hq->FullNetwork.mindist,
                hq->FullNetwork.maxdist);
        DropSpace(psql, sql);
        if (verbose > 2) fprintf(logfp, "            put_hyp: %s\n", sql);
        if ((res_set = PQexec(conn, sql)) == NULL)
            PrintPgsqlError("put_hyp: network_quality:");
        else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
            sprintf(errmsg, "put_hyp: network_quality: %d",
                    PQresultStatus(res_set));
            PrintPgsqlError(errmsg);
        }
        PQclear(res_set);
    }
}

/*
 *  Title:
 *     WriteISCNetmag
 *  Synopsis:
 *     Populates netmag table
 *  Input Arguments:
 *     sp - pointer to current solution
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     WriteEventToISCPgsqlDatabase
 *  Uses:
 *     PrintPgsqlError, DropSpace, getid
 */
static int WriteISCNetmag(SOLREC *sp)
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[2048], psql[2048], errmsg[2048];
    int i, nextid = 0;
    for (i = 0; i < MAXMAG; i++) {
        if (sp->mag[i].numDefiningMagnitude < MinNetmagSta) continue;
        if (GetISCNextID("magid", &nextid))
            return 1;
        sp->mag[i].magid = nextid;
        sprintf(psql, "insert into %snetmag                               \
                       (hypid, magid, magtype, magnitude, nsta, reporter, \
                        uncertainty, nagency, author, lddate) values ", OutputDB);
        sprintf(psql, "%s (%d, %d, '%s', %f, %d, %d, %f, %d, '%s', now())",
                psql, sp->hypid, sp->mag[i].magid, sp->mag[i].magtype,
                sp->mag[i].magnitude, sp->mag[i].numDefiningMagnitude, repid,
                sp->mag[i].uncertainty, sp->mag[i].numMagnitudeAgency, OutAgency);
        DropSpace(psql, sql);
        if (verbose > 2) fprintf(logfp, "            WriteISCNetmag: %s\n", sql);
        if ((res_set = PQexec(conn, sql)) == NULL)
            PrintPgsqlError("WriteISCNetmag:");
        else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
            sprintf(errmsg, "WriteISCNetmag: %d",
                    PQresultStatus(res_set));
            PrintPgsqlError(errmsg);
        }
        PQclear(res_set);
    }
    return 0;
}

/*
 *  Title:
 *     WriteISCStamag
 *  Synopsis:
 *     Populates stamag table.
 *     Station magnitudes are written to the stamag table even if no network
 *        magnitude is calculated. In that case the magid field is set to null.
 *  Input Arguments:
 *     sp     - pointer to current solution
 *     stamag - array of stamag structures
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     WriteEventToISCPgsqlDatabase
 *  Uses:
 *     PrintPgsqlError, DropSpace, getid
 */
static int WriteISCStamag(SOLREC *sp, STAMAG **stamag)
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[2048], psql[2048], errmsg[2048];
    STAMAG *smag = (STAMAG *)NULL;
    int i, j, nextid = 0;
/*
 *  populate stamag table with station magnitudes
 */
    for (j = 0; j < MAXMAG; j++) {
        smag = stamag[j];
        for (i = 0; i < sp->mag[j].numDefiningMagnitude; i++) {
            if (GetISCNextID("stamag_stamagid_seq", &nextid))
                return 1;
            if (sp->mag[j].magid == NULLVAL) {
                sprintf(psql, "insert into %sstamag                      \
                           (magid, hypid, magtype, magnitude, magdef,    \
                            sta, author, reporter, stamagid, lddate)     \
                           values (NULL, %d, '%s', %f, '%d', '%s', '%s', \
                                   %d, %d, now())",
                        OutputDB, sp->hypid, sp->mag[j].magtype, smag[i].magnitude,
                        smag[i].magdef, smag[i].sta, OutAgency, repid, nextid);
            }
            else {
                sprintf(psql, "insert into %sstamag                    \
                           (magid, hypid, magtype, magnitude, magdef,  \
                            sta, author, reporter, stamagid, lddate)   \
                           values (%d, %d, '%s', %f, '%d', '%s', '%s', \
                                   %d, %d, now())",
                        OutputDB, sp->mag[j].magid, sp->hypid, sp->mag[j].magtype,
                        smag[i].magnitude, smag[i].magdef, smag[i].sta,
                        OutAgency, repid, nextid);
            }
            DropSpace(psql, sql);
            if (verbose > 2) fprintf(logfp, "            WriteISCStamag: %s\n", sql);
            if ((res_set = PQexec(conn, sql)) == NULL)
                PrintPgsqlError("WriteISCStamag:");
            else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
                sprintf(errmsg, "WriteISCStamag: %d", PQresultStatus(res_set));
                PrintPgsqlError(errmsg);
            }
            PQclear(res_set);
        }
    }
    return 0;
}

/*
 *  Title:
 *     WriteISCReadingmag
 *  Synopsis:
 *     Populates readingmag table.
 *     Reading magnitudes are written to the readingmag table even if no network
 *        magnitude is calculated. In that case the magid field is set to null.
 *  Input Arguments:
 *     sp    - pointer to current solution
 *     rdmag - array of rdmag structures
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     WriteEventToISCPgsqlDatabase
 *  Uses:
 *     PrintPgsqlError, DropSpace, getid
 */
static int WriteISCReadingmag(SOLREC *sp, STAMAG **rdmag, MSZH *mszh)
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[2048], psql[2048], errmsg[2048];
    STAMAG *rmag = (STAMAG *)NULL;
    int i, j, nextid = 0;
/*
 *  populate reading_mag table with reading magnitudes
 */
    for (j = 0; j < MAXMAG; j++) {
        rmag = rdmag[j];
        for (i = 0; i < sp->mag[j].numAssociatedMagnitude; i++) {
            if (GetISCNextID("rdmagid", &nextid))
                return 1;
            if (sp->mag[j].magid == NULLVAL) {
                sprintf(psql, "insert into %sreadingmag              \
                        (magid, hypid, rdid, mtypeid, magnitude,     \
                         magdef, sta, author, repid, rdmagid) values \
                        (NULL, %d, %d, %d, %f, %d, '%s', '%s', %d, %d)",
                        OutputDB, sp->hypid, rmag[i].rdid, rmag[i].mtypeid,
                        rmag[i].magnitude, rmag[i].magdef, rmag[i].sta,
                        OutAgency, repid, nextid);
            }
            else {
                sprintf(psql, "insert into %sreadingmag              \
                        (magid, hypid, rdid, mtypeid, magnitude,     \
                         magdef, sta, author, repid, rdmagid) values \
                        (%d, %d, %d, %d, %f, %d, '%s', '%s', %d, %d)",
                        OutputDB, sp->mag[j].magid, sp->hypid, rmag[i].rdid,
                        rmag[i].mtypeid, rmag[i].magnitude, rmag[i].magdef,
                        rmag[i].sta, OutAgency, repid, nextid);
            }
            DropSpace(psql, sql);
            if (verbose > 2) fprintf(logfp, "            WriteISCReadingmag: %s\n", sql);
            if ((res_set = PQexec(conn, sql)) == NULL)
                PrintPgsqlError("WriteISCReadingmag:");
            else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
                sprintf(errmsg, "WriteISCReadingmag: %d", PQresultStatus(res_set));
                PrintPgsqlError(errmsg);
            }
            PQclear(res_set);
        }
        if (sp->mag[j].mtypeid == 2) {
/*
 *          populate MSZH table
 */
            for (i = 0; i < sp->mag[j].numAssociatedMagnitude; i++) {
                if (GetISCNextID("mszhid", &nextid))
                    return 1;
                sprintf(psql, "insert into %sms_zh                \
                               (mszhid, hypid, rdid, msz, mszdef, \
                                msh, mshdef,repid, author)        \
                               values (%d, %d, %d, ",
                        OutputDB, nextid, sp->hypid, mszh[i].rdid);
                if (mszh[i].msz == NULLVAL)
                    sprintf(psql, "%s NULL, %d, ", psql, mszh[i].mszdef);
                else
                    sprintf(psql, "%s %f, %d, ", psql, mszh[i].msz, mszh[i].mszdef);
                if (mszh[i].msh == NULLVAL)
                    sprintf(psql, "%s NULL, %d, ", psql, mszh[i].mshdef);
                else
                    sprintf(psql, "%s %f, %d, ", psql, mszh[i].msh, mszh[i].mshdef);
                sprintf(psql, "%s %d, '%s')", psql, repid, OutAgency);
                DropSpace(psql, sql);
                if (verbose > 2)
                    fprintf(logfp, "            WriteISCReadingmag: %s\n", sql);
                if ((res_set = PQexec(conn, sql)) == NULL)
                    PrintPgsqlError("WriteISCReadingmag:");
                else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
                    sprintf(errmsg, "put_rdmad: %d", PQresultStatus(res_set));
                    PrintPgsqlError(errmsg);
                }
                PQclear(res_set);
            }
        }
    }
    return 0;
}

/*
 *  Title:
 *     DeleteISCAssociation
 *  Synopsis:
 *     Deletes previous ISC association records.
 *  Input Arguments:
 *     old_prime - hypid of previous prime hypocentre
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     WriteEventToISCPgsqlDatabase
 *  Calls:
 *     PrintPgsqlError, DropSpace
 */
static void DeleteISCAssociation(int old_prime, char *author)
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[2048], psql[2048], errmsg[2048];
/*
 *  Delete ISC associations for this event.
 */
    sprintf(psql, "delete from %sassociation \
                   where author = '%s' and hypid = %d",
            OutputDB, author, old_prime);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            DeleteISCAssociation: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("DeleteISCAssociation:");
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "DeleteISCAssociation: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
}

/*
 *  Title:
 *     WriteISCAssociation
 *  Synopsis:
 *     Populates association table.
 *  Input Arguments:
 *     sp  - pointer to current solution
 *     p[] - array of phase structures
 *  Called by:
 *     WriteEventToISCPgsqlDatabase
 *  Calls:
 *     PrintPgsqlError, DropSpace
 */
static void WriteISCAssociation(SOLREC *sp, PHAREC p[])
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[2048], psql[2048], errmsg[2048];
    int i, j, k, n;
    char phase_fixed[2], phase[11], nondef[2];
    double timeres = 0., weight = 0.;
    int phase_fixed_ind, timeres_ind, nondef_ind;
/*
 *
 *  populate association table
 *
 */
    for (i = 0; i < sp->numPhase; i++) {
        timeres_ind = 1;
        if      (p[i].timeres == NULLVAL)   timeres_ind = 0;
        else if (isnan(p[i].timeres))       timeres_ind = 0;
        else if (p[i].timeres >= 1000000.)  timeres = 999999.;
        else if (p[i].timeres <= -1000000.) timeres = -999999.;
        else                                timeres = p[i].timeres;
/*
 *      preserve phase_fixed and nondef flags for the next pass
 */
        phase_fixed_ind = nondef_ind = 0;
        if (p[i].phase_fixed) {
            strcpy(phase_fixed, "F");
            phase_fixed_ind = 1;
        }
        if (p[i].force_undef) {
            strcpy(nondef, "U");
            nondef_ind = 1;
        }
/*
 *      insert association record
 */
        sprintf(psql, "insert into %sassociation                      \
                      (hypid, phid, phase, sta, delta, esaz, seaz,    \
                       timedef, weight, timeres, phase_fixed, nondef, \
                       deprecated, author, reporter, lddate)          \
                       values (%d, %d, ",
                OutputDB, sp->hypid, p[i].phid);
/*
 *      fix for apostrophes in phasenames (e.g. P'P'df)
 */
        if (strstr(p[i].phase, "'")) {
            n = (int)strlen(p[i].phase);
            for (k = 0, j = 0; j < n; j++) {
                if (p[i].phase[j] == '\'')
                    phase[k++] = '\'';
                phase[k++] = p[i].phase[j];
            }
            phase[k] = '\0';
        }
        else
            strcpy(phase, p[i].phase);
        if (phase[0])        sprintf(psql, "%s '%s', ", psql, phase);
        else                 sprintf(psql, "%s NULL, ", psql);
        sprintf(psql, "%s '%s', %f, %f, %f, ",
                psql, p[i].sta, p[i].delta, p[i].esaz, p[i].seaz);
        if (p[i].timedef) {
            if (p[i].deltim == NULLVAL)
                sprintf(psql, "%s 'T', NULL, ", psql);
            else {
                weight = 1. / p[i].deltim;
                sprintf(psql, "%s 'T', %f, ", psql, weight);
            }
        }
        else                 sprintf(psql, "%s NULL, NULL,", psql);
        if (timeres_ind)     sprintf(psql, "%s %f, ", psql, timeres);
        else                 sprintf(psql, "%s NULL, ", psql);
        if (phase_fixed_ind) sprintf(psql, "%s '%s', ", psql, phase_fixed);
        else                 sprintf(psql ,"%s NULL, ", psql);
        if (nondef_ind)      sprintf(psql, "%s '%s', ", psql, nondef);
        else                 sprintf(psql ,"%s NULL, ", psql);
        sprintf(psql, "%s NULL, '%s', %d, now())",
                psql, OutAgency, p[i].repid);
        DropSpace(psql, sql);
        if (verbose > 2) fprintf(logfp, "            WriteISCAssociation: %s\n", sql);
        if ((res_set = PQexec(conn, sql)) == NULL)
            PrintPgsqlError("WriteISCAssociation:");
        else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
            sprintf(errmsg, "WriteISCAssociation: %d", PQresultStatus(res_set));
            PrintPgsqlError(errmsg);
        }
        PQclear(res_set);
    }
}

/*
 *  Title:
 *     WriteISCAmplitudemag
 *  Synopsis:
 *     Populates ampmag table.
 *     Deletes previous ampmag entries.
 *     Amplitude magnitudes are written to the ampmag table even if no network
 *        magnitude is calculated.
 *  Input Arguments:
 *     sp  - pointer to current solution
 *     p[] - array of phase structures
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     WriteEventToISCPgsqlDatabase
 *  Calls:
 *     PrintPgsqlError, DropSpace
 */
static int WriteISCAmplitudemag(SOLREC *sp, PHAREC p[])
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[2048], psql[2048], errmsg[2048];
    int i, j, nextid = 0;
/*
 *  populate ampmag table
 */
    for (i = 0; i < sp->numPhase; i++) {
        for (j = 0; j < p[i].numamps; j++) {
            if (!p[i].a[j].mtypeid) continue;
            if (GetISCNextID("ampmagid", &nextid))
                return 1;
            sprintf(psql, "insert into %sampmag                    \
                           (ampmagid, hypid, rdid, ampid, mtypeid, \
                           magnitude, ampdef, repid, author)       \
                           values (%d, %d, %d, %d, %d, %f, %d, %d, '%s')",
                    OutputDB, nextid, sp->hypid, p[i].rdid, p[i].a[j].ampid,
                    p[i].a[j].mtypeid, p[i].a[j].magnitude, p[i].a[j].ampdef,
                    repid, OutAgency);
            DropSpace(psql, sql);
            if (verbose > 2)
                fprintf(logfp, "            WriteISCAmplitudemag: %s\n", sql);
            if ((res_set = PQexec(conn, sql)) == NULL)
                PrintPgsqlError("WriteISCAmplitudemag:");
            else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
                sprintf(errmsg, "WriteISCAmplitudemag: %d", PQresultStatus(res_set));
                PrintPgsqlError(errmsg);
            }
            PQclear(res_set);
        }
    }
    return 0;
}

/*
 *  Title:
 *     UpdateMagnitudesInISCPgsqlDatabase
 *  Synopsis:
 *     Writes magnitudes to ISC PostgreSQl database.
 *     Populates netmag, stamag, rdmag, ampmag and ms_zh tables.
 *  Input Arguments:
 *     ep        - pointer to event info
 *     sp        - pointer to current solution
 *     p[]       - array of phase structures
 *     stamag - array of station magnitude structures
 *     rdmag  - array of reading magnitude structures
 *     mszh   - array of MS vertical/horizontal magnitude structures
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     Locator
 *  Calls:
 *     DeleteISCmagnitudes, WriteISCNetmag, WriteISCStamag, WriteISCReadingmag,
 *     WriteISCAmplitudemag
 */
int UpdateMagnitudesInISCPgsqlDatabase(EVREC *ep, SOLREC *sp, PHAREC p[],
                            STAMAG **stamag, STAMAG **rdmag, MSZH *mszh)
{
    if (verbose)
        fprintf(logfp, "        UpdateMagnitudesInISCPgsqlDatabase");
/*
 *  delete previous magnitudes
 */
    DeleteISCmagnitudes(sp->hypid);
/*
 *  populate netmag table
 */
    if (sp->nnetmag) {
        if (verbose) fprintf(logfp, "        WriteISCNetmag\n");
        if (WriteISCNetmag(sp))
            return 1;
    }
/*
 *  populate stamag, rdmag and ampmag tables
 */
    if (sp->nstamag) {
        if (verbose) fprintf(logfp, "        WriteISCStamag\n");
        if (WriteISCStamag(sp, stamag))
            return 1;
        if (verbose) fprintf(logfp, "        WriteISCReadingmag\n");
        if (WriteISCReadingmag(sp, rdmag, mszh))
            return 1;
        if (verbose) fprintf(logfp, "        WriteISCAmplitudemag\n");
        if (WriteISCAmplitudemag(sp, p))
            return 1;
    }
    return 0;
}

/*
 *  Title:
 *     RemoveISCHypocenter
 *  Synopsis:
 *     Removes existing ISC hypocentre from database if locator failed to get
 *     a convergent solution.
 *  Input Arguments:
 *     ep - pointer to event info
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     Locator
 *  Calls:
 *     PrintPgsqlError
 *  Notes:
 *     Need to run ReplaceISCPrime and ReplaceISCAssociation after this.
 */
void RemoveISCHypocenter(EVREC *ep)
{
    char sql[MAXBUF];
    PGresult *res_set = (PGresult *)NULL;
    int hypid = 0;
    if (streq(InputDB, OutputDB))
        hypid = ep->ISChypid;
    else
        hypid = ep->OutputDBISChypid;
    if (hypid) {
        if (verbose) fprintf(logfp, "    RemoveISCHypocenter: %d\n", hypid);
        DeleteISChypid(hypid);
        sprintf(sql, "delete from %spub_comments \
                      where id_name = 'hypid' and id_value = %d",
                OutputDB, hypid);
        if ((res_set = PQexec(conn, sql)) == NULL)
            PrintPgsqlError("RemoveISCHypocenter: pub_comments:");
        PQclear(res_set);
        if (verbose)
            fprintf(logfp, "    RemoveISCHypocenter: hypocentre removed.\n");
    }
}

/*
 *  Title:
 *     DeleteISChypid
 *  Synopsis:
 *     Removes existing ISC hypocentre from database.
 *  Input Arguments:
 *     ISChypid - ISC hypid
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     WriteISCHypocenter, RemoveISCHypocenter
 *  Calls:
 *     PrintPgsqlError
 */
static void DeleteISChypid(int ISChypid)
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[MAXBUF], errmsg[MAXBUF];
/*
 *  delete hypid from hypocenter
 */
    sprintf(sql, "delete from %shypocenter where hypid = %d",
            OutputDB, ISChypid);
    if ((res_set = PQexec(conn, sql)) == NULL) {
        PrintPgsqlError("DeleteISChypid: hypoc:");
    }
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "DeleteISChypid: hypoc: %d",
                PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
/*
 *  delete hypid from hypoc_err
 */
    sprintf(sql, "delete from %shypoc_err where hypid = %d",
            OutputDB, ISChypid);
    if ((res_set = PQexec(conn, sql)) == NULL) {
        PrintPgsqlError("DeleteISChypid: hypoc_err:");
    }
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "DeleteISChypid: hypoc_err: %d",
               PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
/*
 *  delete hypid from hypoc_acc
 */
    sprintf(sql, "delete from %shypoc_acc where hypid = %d",
            OutputDB, ISChypid);
    if ((res_set = PQexec(conn, sql)) == NULL) {
        PrintPgsqlError("DeleteISChypid: hypoc_acc:");
    }
    else if ( PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "DeleteISChypid: hypoc_acc: %d",
                PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
/*
 *  delete hypid from network_quality
 */
    sprintf(sql, "delete from %snetwork_quality where hypid = %d",
            OutputDB, ISChypid);
    if ((res_set = PQexec(conn, sql)) == NULL) {
        PrintPgsqlError("DeleteISChypid: network_quality:");
    }
    else if ( PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "DeleteISChypid: network_quality: %d",
                PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
    if (verbose)
        fprintf(logfp, "    DeleteISChypid: %s hypocentre removed.\n",
                InAgency);
}

/*
 *  Title:
 *     DeleteISCmagnitudes
 *  Synopsis:
 *     Removes existing ISC magnitudes from database.
 *  Input Arguments:
 *     ISChypid - ISC hypid
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     DeleteISChypid, UpdateMagnitudesInISCPgsqlDatabase
 *  Calls:
 *     PrintPgsqlError
 */
static void DeleteISCmagnitudes(int ISChypid)
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[MAXBUF], errmsg[MAXBUF];
/*
 *  delete hypid from netmag
 */
    sprintf(sql, "delete from %snetmag where hypid = %d",
            OutputDB, ISChypid);
    if ((res_set = PQexec(conn,sql)) == NULL) {
        PrintPgsqlError("DeleteISChypid: netmag:");
    }
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "DeleteISChypid: netmag: %d",
                PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
/*
 *  delete hypid from stamag
 */
    sprintf(sql, "delete from %sstamag where hypid = %d",
            OutputDB, ISChypid);
    if ((res_set = PQexec(conn, sql)) == NULL) {
        PrintPgsqlError("DeleteISChypid: stamag:");
    }
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "DeleteISChypid: stamag: %d",
                PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
/*
 *  delete hypid from readingmag
 */
    sprintf(sql, "delete from %sreadingmag where hypid = %d",
            OutputDB, ISChypid);
    if ((res_set = PQexec(conn, sql)) == NULL) {
        PrintPgsqlError("DeleteISChypid: readingmag:");
    }
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "DeleteISChypid: readingmag: %d",
                PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
/*
 *  delete hypid from ampmag
 */
    sprintf(sql, "delete from %sampmag where hypid = %d",
            OutputDB, ISChypid);
    if ((res_set = PQexec(conn, sql)) == NULL) {
        PrintPgsqlError("DeleteISChypid: ampmag:");
    }
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "DeleteISChypid: ampmag: %d",
                PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
/*
 *  delete hypid from ms_zh
 */
    sprintf(sql, "delete from %sms_zh where hypid = %d",
            OutputDB, ISChypid);
    if ((res_set = PQexec(conn, sql)) == NULL) {
        PrintPgsqlError("DeleteISChypid: ms_zh:");
    }
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "DeleteISChypid: ms_zh: %d",
                PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
}

/*
 *  Title:
 *     ReplaceISCPrime
 *  Synopsis:
 *     Changes prime hypocentre of event without removing old one.
 *  Input Arguments:
 *     ep  - pointer to event info
 *     hp  - pointer to preferred hypocentre
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     Locator, Synthetic
 *  Calls:
 *     PrintPgsqlError
 */
void ReplaceISCPrime(EVREC *ep, HYPREC *hp)
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[1536], errmsg[1536];
/*
 *  if event is not in OutputDB, put it there
 */
    if (strcmp(InputDB, OutputDB) && !ep->OutputDBPreferredOrid) {
        if (verbose) fprintf(logfp, "        WriteISCEvent (%.2f)\n", secs(&t0));
        sprintf(sql, "insert into %sevent select * from %sevent \
                       where evid = %d", OutputDB, InputDB, ep->evid);
        if (verbose > 2) fprintf(logfp, "            ReplaceISCPrime: %s\n", sql);
        if ((res_set = PQexec(conn, sql)) == NULL)
            PrintPgsqlError("ReplaceISCPrime:");
        else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
            sprintf(errmsg, "ReplaceISCPrime: %d", PQresultStatus(res_set));
            PrintPgsqlError(errmsg);
        }
        sprintf(sql, "insert into %shypocenter select * from %shypocenter \
                       where hypid = %d", OutputDB, InputDB, hp->hypid);
        if (verbose > 2) fprintf(logfp, "            ReplaceISCPrime: %s\n", sql);
        if ((res_set = PQexec(conn, sql)) == NULL)
            PrintPgsqlError("ReplaceISCPrime:");
        else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
            sprintf(errmsg, "ReplaceISCPrime: %d", PQresultStatus(res_set));
            PrintPgsqlError(errmsg);
        }
        PQclear(res_set);
    }
/*
 *  Update event table
 */
    sprintf(sql, "update %sevent set prime_hyp = %d where evid = %d",
            OutputDB, hp->hypid, ep->evid);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("ReplaceISCPrime:");
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "ReplaceISCPrime: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
/*
 *  Update old prime
 */
    sprintf(sql, "update %shypocenter set prime = NULL where isc_evid = %d",
            OutputDB, ep->evid);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("ReplaceISCPrime: ");
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "ReplaceISCPrime: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
/*
 *  Update new prime
 */
    sprintf(sql, "update %shypocenter set prime = 'P' where hypid = %d",
            OutputDB, hp->hypid);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("ReplaceISCPrime: ");
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "ReplaceISCPrime: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
    if (verbose)
        fprintf(logfp, "    ReplaceISCPrime: prime: %d evid: %d\n",
                hp->hypid, ep->evid);
}

/*
 *  Title:
 *     ReplaceISCAssociation
 *  Synopsis:
 *     Rolls back associations to previous prime.
 *  Input Arguments:
 *     ep  - pointer to event info
 *     p[] - array of phase structures.
 *     hp  - pointer to prefered hypocentre
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     Locator, Synthetic
 *  Calls:
 *     PrintPgsqlError
 */
void ReplaceISCAssociation(EVREC *ep, PHAREC p[], HYPREC *hp)
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[2048], errmsg[2048];
    int hypid = 0;
    char author[VALLEN];
    double timeres = 0.;
    char  phase_fixed[2], phase[11];
    short timeres_ind, phase_fixed_ind;
    int i, j, k, n;
/*
 *  delete previous assocs
 */
    strcpy(author, "");
    if (streq(InputDB, OutputDB)) {
        hypid = ep->PreferredOrid;
        strcpy(author, InAgency);
    }
    else {
        hypid = ep->OutputDBPreferredOrid;
        strcpy(author, OutAgency);
    }
    if (!hypid)
        return;
/*
 *  Delete current associations and put back old associations
 */
    DeleteISCAssociation(hypid, author);
    if (verbose)
        fprintf(logfp, "    ReplaceISCAssociation: insert associations to hypid=%d\n",
                hp->hypid);
    for (i = 0; i < ep->numPhase; i++) {
/*
 *      Want phase_fixed flag to remain for next pass.
 */
        phase_fixed_ind = 0;
        if (p[i].phase_fixed) {
            strcpy(phase_fixed, "F");
            phase_fixed_ind = 1;
        }
        timeres_ind = 1;
        if (p[i].timeres == NULLVAL)       timeres_ind = 0;
        else if (isnan(p[i].timeres))      timeres_ind = 0;
        else if (p[i].timeres >= 1000000)  timeres = 999999;
        else if (p[i].timeres <= -1000000) timeres = -999999;
        else                               timeres = p[i].timeres;
/*
 *      populate association table
 */
        sprintf(sql,"insert into %sassociation \
                    (hypid, phid, phase, sta, delta, esaz, seaz,     \
                     timeres, phase_fixed, author, reporter, lddate) \
                     values (%d, %d,",
                OutputDB, hp->hypid, p[i].phid);
/*
 *      fix for apostrophes in phasenames (e.g. P'P'df)
 */
        if (strstr(p[i].phase, "'")) {
            n = (int)strlen(p[i].phase);
            for (k = 0, j = 0; j < n; j++) {
                if (p[i].phase[j] == '\'')
                    phase[k++] = '\'';
                phase[k++] = p[i].phase[j];
            }
            phase[k] = '\0';
        }
        else
            strcpy(phase, p[i].phase);
        if (phase[0]) sprintf(sql, "%s '%s', ", sql, phase);
        else          sprintf(sql, "%s NULL, ", sql);
        sprintf(sql, "%s '%s', %f, %f, %f,",
                sql, p[i].sta, p[i].delta, p[i].esaz, p[i].seaz);
        if (timeres_ind) sprintf(sql, "%s %f,", sql, timeres);
        else             sprintf(sql, "%s NULL,", sql);
        if (phase_fixed_ind) sprintf(sql, "%s '%s',", sql, phase_fixed);
        else                 sprintf(sql ,"%s NULL,", sql);
        sprintf(sql, "%s '%s', %d, now())", sql, OutAgency, p[i].repid);
        if (verbose > 2)
            fprintf(logfp, "            ReplaceISCAssociation: %4d: %s\n", i, sql);
        if ((res_set = PQexec(conn, sql)) == NULL)
            PrintPgsqlError("ReplaceISCAssociation:");
        else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
            sprintf(errmsg, "ReplaceISCAssociation: %d", PQresultStatus(res_set));
            PrintPgsqlError(errmsg);
        }
        PQclear(res_set);
    }
}

/*
 *  Title:
 *      GetISCNextID
 *  Synopsis:
 *      Gets next unique id from DB sequence in the isc account.
 *  Input Arguments:
 *      sequence - sequence name
 *  Output Arguments:
 *      nextid   - unique id
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     WriteISCHypocenter, WriteISCNetmag, WriteISCStamag,
 *     WriteISCReadingmag, WriteISCAmplitudemag, put_ms_zh
 *  Calls:
 *     PrintPgsqlError
 *
 */
static int GetISCNextID(char *sequence, int *nextid)
{
    extern char NextidDB[VALLEN];           /* get new ids from this account */
    PGresult *res_set = (PGresult *)NULL;
    char sql[MAXBUF];
    sprintf(sql, "select nextval('%s%s')", NextidDB, sequence);
    if (verbose > 2) fprintf(logfp, "            GetISCNextID: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL) {
        PrintPgsqlError("GetISCNextID: error: nextid:");
        PQclear(res_set);
        return 1;
    }
    else if (PQntuples(res_set) == 1) {
        *nextid = atoi(PQgetvalue(res_set, 0, 0));
    }
    PQclear(res_set);
    if (verbose > 2) fprintf(logfp, "            new id=%d\n", *nextid);
    return 0;
}

#endif
#endif /* ISCDB */

/*  EOF  */
