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
#ifdef IDCDB
#ifdef PGSQL

#include "iLoc.h"
extern int verbose;                                        /* verbose level */
extern FILE *logfp;
extern FILE *errfp;
extern int errorcode;
extern struct timeval t0;
extern char OutAgency[VALLEN];      /* author for new hypocentres and assocs */
extern PGconn *conn;
extern int MinNetmagSta;                 /* min number of stamags for netmag */

/*
 * Functions:
 *    WriteEventToIDCNIABDatabase
 */

/*
 * Local functions
 */
static int WriteIDCOrigin(EVREC *ep, SOLREC *sp, FE *fep);
static void WriteIDCOrigerr(EVREC *ep, SOLREC *sp);
static int WriteIDCMag(SOLREC *sp, STAMAG **stamag, int evid);
static void WriteIDCAssoc(SOLREC *sp, PHAREC p[]);
static void DeleteIDCorid(int evid, int orid);
static int GetIDClastid(char *keyname, int *nextid);

/*
 *  Title:
 *     WriteEventToIDCNIABDatabase
 *  Synopsis:
 *     Writes solution to IDC NIAB PostgreSQL database.
 *     Populates origin, origerr, assoc, netmag and stamag tables.
 *  Input Arguments:
 *     ep     - pointer to event info
 *     sp     - pointer to current solution
 *     p[]    - array of phase structures
 *     stamag - array of station magnitude structures
 *     fep    - pointer to Flinn_Engdahl region number structure
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     Locator
 *  Calls:
 *     WriteIDCOrigin, WriteIDCOrigerr, WriteIDCNetmag, WriteIDCStamag,
 *     WriteIDCAssoc
 *
 */
int WriteEventToIDCNIABDatabase(EVREC *ep, SOLREC *sp, PHAREC p[],
        STAMAG **stamag, FE *fep)
{
    if (verbose)
        fprintf(logfp, "        WriteEventToIDCNIABDatabase: evid=%d\n", ep->evid);
/*
 *  populate origin table
 */
    if (verbose) fprintf(logfp, "        WriteIDCOrigin\n");
    if (WriteIDCOrigin(ep, sp, fep))
        return 1;
/*
 *  set preferred orid
 */
    sp->hypid = ep->PreferredOrid = ep->ISChypid;
/*
 *  populate origerr table
 */
    if (verbose) fprintf(logfp, "        WriteIDCOrigerr\n");
    WriteIDCOrigerr(ep, sp);
/*
 *  populate assoc table
 */
    if (verbose) fprintf(logfp, "        WriteIDCAssoc\n");
    WriteIDCAssoc(sp, p);
/*
 *  populate stamag and netmag tables
 */
    if (sp->nstamag) {
        if (verbose) fprintf(logfp, "        WriteIDCMag\n");
        if (WriteIDCMag(sp, stamag, ep->evid))
            return 1;
    }
/*
 *  set preferred orid
 */
    ep->PreferredOrid = ep->ISChypid;
    if (verbose)
        fprintf(logfp, "        WriteEventToIDCNIABDatabase (%.2f)\n", secs(&t0));
    return 0;
}

/*
 *  Title:
 *     WriteIDCOrigin
 *  Synopsis:
 *     Populates hypocenter table.
 *     Gets a new hypid if previous prime was not IDC.
 *     Deletes previous prime if it was IDC.
 *     Sets depfix flag:
 *            F: Free-depth solution
 *            A: Depth fixed by IDC Analyst
 *            S: Anthropogenic event; depth fixed to surface
 *            G: Depth fixed to IDC default depth grid
 *            R: Depth fixed to IDC default region depth
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
 *     WriteEventToIDCNIABDatabase
 *  Calls:
 *     PrintPgsqlError, DropSpace, GetIDClastid, EpochToJdate,
 *     GregionNumber, GregToSreg
 */
static int WriteIDCOrigin(EVREC *ep, SOLREC *sp, FE *fep)
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[2048], psql[2048], errmsg[2048], depfix[2];
    int nextid = 0, grn = 0, srn = 0, jdate;
/*
 *  depth determination type
 */
    if      (sp->FixedDepthType == 8) strcpy(depfix, "A"); /* analyst */
    else if (sp->FixedDepthType == 1) strcpy(depfix, "B"); /* beyond limit */
    else if (sp->FixedDepthType == 2) strcpy(depfix, "H"); /* hypocentre */
    else if (sp->FixedDepthType == 3) strcpy(depfix, "D"); /* depdp */
    else if (sp->FixedDepthType == 4) strcpy(depfix, "S"); /* surface */
    else if (sp->FixedDepthType == 5) strcpy(depfix, "G"); /* depth grid */
    else if (sp->FixedDepthType == 6) strcpy(depfix, "M"); /* median depth */
    else if (sp->FixedDepthType == 7) strcpy(depfix, "R"); /* GRN depth */
    else                              strcpy(depfix, "F"); /* free depth */
/*
 *  Flinn-Engdahl grn and srn
 */
    grn = GregionNumber(sp->lat, sp->lon, fep);
    srn = GregToSreg(grn);
/*
 *  jdate
 */
    jdate = EpochToJdate(sp->time);
    if (ep->ISChypid) {
/*
 *      previous prime is OutAgency; delete old solution
 */
        DeleteIDCorid(ep->evid, ep->ISChypid);
    }
    else {
/*
 *      previous prime is not OutAgency; get a new orid
 */
        if (GetIDClastid("orid", &nextid))
            return 1;
        if (verbose > 2) fprintf(logfp, "            new orid=%d\n", nextid);
        ep->ISChypid = nextid;
    }
/*
 *  populate origin table
 */
    sprintf(psql, "insert into origin                             \
                   (evid, orid, srn, grn, etype, auth, algorithm, \
                    time, jdate, lat, lon, depth, dtype, depdp,   \
                    ndp, nass, ndef, lddate)");
    sprintf(psql, "%s values (%d, %d, %d, %d, '%s', '%s', 'iLoc', ",
            psql, ep->evid, ep->ISChypid, srn, grn, ep->etype, OutAgency);
    sprintf(psql, "%s %f, %d, %f, %f, %f, '%s', ",
            psql, sp->time, jdate, sp->lat, sp->lon, sp->depth, depfix);
    if (sp->depdp == NULLVAL)
        sprintf(psql, "%s NULL, NULL, ", psql);
    else
        sprintf(psql, "%s %f, %d, ", psql, sp->depdp, sp->ndp);
    sprintf(psql, "%s %d, %d, now())", psql, sp->nass, sp->ndef);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            WriteIDCOrigin: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("WriteIDCOrigin:");
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "WriteIDCOrigin: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
    if (verbose) fprintf(logfp, "    WriteIDCOrigin: done\n");
    return 0;
}

/*
 *  Title:
 *     WriteIDCOrigerr
 *  Synopsis:
 *     Populates hypoc_err table.
 *  Input Arguments:
 *     ep  - pointer to event info
 *     sp  - pointer to current solution
 *  Called by:
 *     WriteEventToIDCNIABDatabase
 *  Calls:
 *     PrintPgsqlError, DropSpace
 */
static void WriteIDCOrigerr(EVREC *ep, SOLREC *sp)
{
    extern double ConfidenceLevel;                       /* from config file */
    PGresult *res_set = (PGresult *)NULL;
    char sql[2048], psql[2048], errmsg[2048];
    char conf[5];
    if      (fabs(ConfidenceLevel - 90.) < DEPSILON) strcpy(conf, "90");
    else if (fabs(ConfidenceLevel - 95.) < DEPSILON) strcpy(conf, "95");
    else if (fabs(ConfidenceLevel - 98.) < DEPSILON) strcpy(conf, "98");
    else                                             strcpy(conf, "NULL");
    sprintf(psql, "insert into origerr                                      \
                   (orid, stt, stx, sty, stz, sxx, sxy, syy, syz, sxz, szz, \
                    sdobs, smajax, sminax, strike, stime, sdepth, conf,     \
                    lddate) values (%d, ",
            ep->ISChypid);
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
 *      sxz, syz
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
    if (sp->error[0] == NULLVAL)
        sprintf(psql, "%s NULL, ", psql);
    else if (fabs(sp->error[0]) > 999.99)
        sprintf(psql, "%s %f, ", psql, 999.999);
    else
        sprintf(psql, "%s %f, ", psql, sp->error[0]);
    if (sp->error[3] == NULLVAL)
        sprintf(psql, "%s NULL, ", psql);
    else if (fabs(sp->error[3]) > 999.99)
        sprintf(psql, "%s %f, ", psql, 999.999);
    else
        sprintf(psql, "%s %f, ", psql, sp->error[3]);
    sprintf(psql, "%s '%s', now())", psql, conf);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp,"            WriteIDCOrigerr: %s\n",sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("WriteIDCOrigerr:");
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "WriteIDCOrigerr: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
}

/*
 *  Title:
 *     WriteIDCMag
 *  Synopsis:
 *     Populates netmag and stamag tables
 *  Input Arguments:
 *     sp   - pointer to current solution
 *     evid - evid
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     WriteEventToIDCOraDatabase
 *  Uses:
 *     PrintPgsqlError, DropSpace, GetIDClastid
 */
static int WriteIDCMag(SOLREC *sp, STAMAG **stamag, int evid)
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[2048], psql[2048], errmsg[2048];
    STAMAG *smag = (STAMAG *)NULL;
    int i, k, nextid = 0, ampid;
    for (i = 0; i < MAXMAG; i++) {
        smag = stamag[i];
        if (sp->mag[i].numDefiningMagnitude >= MinNetmagSta) {
/*
 *          populate netmag table
 */
            if (GetIDClastid("magid", &nextid))
                return 1;
            sp->mag[i].magid = nextid;
            sprintf(psql, "insert into netmag                      \
                           (evid, orid, magid, magtype, magnitude, \
                            nsta, uncertainty, auth, lddate) values ");
            sprintf(psql, "%s (%d, %d, %d, '%s', %f, %d, %f, '%s', now())",
                    psql, evid, sp->hypid, sp->mag[i].magid,
                    sp->mag[i].magtype, sp->mag[i].magnitude,
                    sp->mag[i].numDefiningMagnitude,
                    sp->mag[i].uncertainty, OutAgency);
            DropSpace(psql, sql);
            if (verbose > 2) fprintf(logfp, "            WriteIDCNetmag: %s\n", sql);
            if ((res_set = PQexec(conn, sql)) == NULL)
                PrintPgsqlError("WriteIDCNetmag:");
            else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
                sprintf(errmsg, "WriteIDCNetmag: %d", PQresultStatus(res_set));
                PrintPgsqlError(errmsg);
            }
            PQclear(res_set);
/*
 *          populate stamag table with station magnitudes belonging to netmag
 */
            for (k = 0; k < sp->mag[i].numDefiningMagnitude; k++) {
                if (smag[k].mtypeid == sp->mag[i].mtypeid) {
                    if (GetIDClastid("ampid", &ampid))
                        return 1;
                    sprintf(psql, "insert into stamag             \
                                   (evid, orid, magid, ampid,     \
                                    magtype,  magnitude, magdef,  \
                                    sta, auth, lddate)            \
                                   values (%d, %d, %d, %d, '%s',  \
                                           %f, '%d', '%s', '%s', now())",
                            evid, sp->hypid, sp->mag[i].magid, ampid,
                            smag[k].magtype, smag[k].magnitude,
                            smag[k].magdef, smag[k].sta, OutAgency);
                    DropSpace(psql, sql);
                    if (verbose > 2)
                       fprintf(logfp, "            WriteIDCNetmag: %s\n", sql);
                    if ((res_set = PQexec(conn, sql)) == NULL)
                        PrintPgsqlError("WriteIDCNetmag:");
                    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
                        sprintf(errmsg, "WriteIDCNetmag: %d",
                                PQresultStatus(res_set));
                        PrintPgsqlError(errmsg);
                    }
                    PQclear(res_set);
                }
            }
        }
        else {
/*
 *          write stamags if any, even if no network magnitudes were calculated
 *          netmag requires at least MinNetmagSta station magnitudes
 */
            for (k = 0; k < sp->mag[i].numDefiningMagnitude; k++) {
                if (GetIDClastid("magid", &nextid))
                    return 1;
                if (GetIDClastid("ampid", &ampid))
                    return 1;
                sprintf(psql, "insert into stamag                      \
                               (evid, orid, magid, ampid, magtype,     \
                                magnitude, magdef, sta, auth, lddate)  \
                               values (%d, %d, %d, %d, '%s', %f, '%d', \
                                       '%s', '%s', now())",
                        evid, sp->hypid, nextid, ampid, smag[k].magtype,
                        smag[k].magnitude, smag[k].magdef, smag[k].sta,
                        OutAgency);
                DropSpace(psql, sql);
                if (verbose > 2)
                    fprintf(logfp, "            WriteIDCNetmag: %s\n", sql);
                if ((res_set = PQexec(conn, sql)) == NULL)
                    PrintPgsqlError("WriteIDCNetmag:");
                else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
                    sprintf(errmsg, "WriteIDCNetmag: %d", PQresultStatus(res_set));
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
 *     WriteIDCAssoc
 *  Synopsis:
 *     Populates association table.
 *  Input Arguments:
 *     sp  - pointer to current solution
 *     p[] - array of phase structures
 *  Called by:
 *     WriteEventToIDCNIABDatabase
 *  Calls:
 *     PrintPgsqlError, DropSpace
 */
static void WriteIDCAssoc(SOLREC *sp, PHAREC p[])
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[2048], psql[2048], errmsg[2048];
    int i, j, k, n;
    char phase[11];
    double timeres = 0., slowres = 0., azimres = 0;
    int slowres_ind, timeres_ind, azimres_ind;
/*
 *
 *  populate assoc table
 *
 */
    for (i = 0; i < sp->numPhase; i++) {
        timeres_ind = 1;
        if      (p[i].timeres == NULLVAL)   timeres_ind = 0;
        else if (isnan(p[i].timeres))       timeres_ind = 0;
        else if (p[i].timeres >= 1000000.)  timeres = 999999.;
        else if (p[i].timeres <= -1000000.) timeres = -999999.;
        else                                timeres = p[i].timeres;
        azimres_ind = 1;
        if      (p[i].azimres == NULLVAL)   azimres_ind = 0;
        else if (isnan(p[i].azimres))       azimres_ind = 0;
        else if (p[i].azimres >= 1000000.)  azimres = 999999.;
        else if (p[i].azimres <= -1000000.) azimres = -999999.;
        else                                azimres = p[i].azimres;
        slowres_ind = 1;
        if      (p[i].slowres == NULLVAL)   slowres_ind = 0;
        else if (isnan(p[i].slowres))       slowres_ind = 0;
        else if (p[i].slowres >= 1000000.)  slowres = 999999.;
        else if (p[i].slowres <= -1000000.) slowres = -999999.;
        else                                slowres = p[i].slowres;
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
/*
 *      insert association record
 */
        sprintf(psql, "insert into assoc                          \
                      (orid, arid, phase, sta, delta, esaz, seaz, \
                       timeres, timedef, wgt, azres, azdef,       \
                       slores, slodef, vmodel, lddate)            \
                       values (%d, %d, ",
                sp->hypid, p[i].phid);
        if (phase[0])      sprintf(psql, "%s '%s', ", psql, phase);
        else               sprintf(psql, "%s NULL, ", psql);
        sprintf(psql, "%s '%s', %f, %f, %f, ",
                psql, p[i].sta, p[i].delta, p[i].esaz, p[i].seaz);
        if (timeres_ind)  sprintf(psql, "%s %f, ", psql, timeres);
        else              sprintf(psql, "%s NULL, ", psql);
        if (p[i].timedef) sprintf(psql, "%s 'd', ", psql);
        else              sprintf(psql, "%s 'n', ", psql);
        if (p[i].deltim == NULLVAL) sprintf(psql, "%s NULL, ", psql);
        else              sprintf(psql, "%s %f, ", psql, 1. / p[i].deltim);
        if (azimres_ind)  sprintf(psql, "%s %f, ", psql, azimres);
        else              sprintf(psql, "%s NULL, ", psql);
        if (p[i].azimdef) sprintf(psql, "%s 'd', ", psql);
        else              sprintf(psql, "%s 'n', ", psql);
        if (slowres_ind)  sprintf(psql, "%s %f, ", psql, slowres);
        else              sprintf(psql, "%s NULL, ", psql);
        if (p[i].slowdef) sprintf(psql, "%s 'd', ", psql);
        else              sprintf(psql, "%s 'n', ", psql);
        sprintf(psql, "%s '%s', now())", psql, p[i].vmod);
        DropSpace(psql, sql);
        if (verbose > 2) fprintf(logfp, "            WriteIDCAssoc: %s\n", sql);
        if ((res_set = PQexec(conn, sql)) == NULL)
            PrintPgsqlError("WriteIDCAssoc:");
        else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
            sprintf(errmsg, "WriteIDCAssoc: %d", PQresultStatus(res_set));
            PrintPgsqlError(errmsg);
        }
        PQclear(res_set);
    }
}

/*
 *  Title:
 *     UpdateMagnitudesInIDCNIABDatabase
 *  Synopsis:
 *     Writes magnitudes to IDC NIAB database.
 *     Populates netmag, stamag tables.
 *  Input Arguments:
 *     ep        - pointer to event info
 *     sp        - pointer to current solution
 *     p[]       - array of phase structures
 *     stamag - array of station magnitude structures
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     Locator
 *  Calls:
 *     WriteIDCMag
 */
int UpdateMagnitudesInIDCNIABDatabase(EVREC *ep, SOLREC *sp, STAMAG **stamag)
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[MAXBUF], errmsg[MAXBUF];
    if (verbose)
        fprintf(logfp, "        UpdateMagnitudesInIDCNIABDatabase");
/*
 *  delete orid from netmag
 */
    sprintf(sql, "delete from netmag where evid = %d and orid = %d",
            ep->evid, sp->hypid);
    if ((res_set = PQexec(conn,sql)) == NULL) {
        PrintPgsqlError("UpdateMagnitudesInIDCNIABDatabase: netmag:");
    }
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "UpdateMagnitudesInIDCNIABDatabase: netmag: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
/*
 *  delete orid from stamag
 */
    sprintf(sql, "delete from stamag where evid = %d and orid = %d",
            ep->evid, sp->hypid);
    if ((res_set = PQexec(conn, sql)) == NULL) {
        PrintPgsqlError("UpdateMagnitudesInIDCNIABDatabase: stamag:");
    }
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "UpdateMagnitudesInIDCNIABDatabase: stamag: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
/*
 *  populate stamag and netmag tables
 */
    if (sp->nstamag) {
        if (verbose) fprintf(logfp, "        WriteIDCMag\n");
        if (WriteIDCMag(sp, stamag, ep->evid))
            return 1;
    }
    return 0;
}

/*
 *  Title:
 *     DeleteIDCorid
 *  Synopsis:
 *     Removes existing OutAgency hypocentre from database.
 *  Input Arguments:
 *     evid - evid
 *     orid - orid
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     WriteIDCOrigin
 *  Calls:
 *     PrintPgsqlError
 */
static void DeleteIDCorid(int evid, int orid)
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[MAXBUF], errmsg[MAXBUF];
/*
 *  delete orid from origin
 */
    sprintf(sql, "delete from origin where evid = %d and orid = %d",
            evid, orid);
    if ((res_set = PQexec(conn, sql)) == NULL) {
        PrintPgsqlError("DeleteIDCorid: origin:");
    }
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "DeleteIDCorid: origin: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
/*
 *  delete orid from origerr
 */
    sprintf(sql, "delete from origerr where orid = %d", orid);
    if ((res_set = PQexec(conn, sql)) == NULL) {
        PrintPgsqlError("DeleteIDCorid: origerr:");
    }
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "DeleteIDCorid: origerr: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
/*
 *  delete orid from netmag
 */
    sprintf(sql, "delete from netmag where evid = %d and orid = %d",
            evid, orid);
    if ((res_set = PQexec(conn,sql)) == NULL) {
        PrintPgsqlError("DeleteIDCorid: netmag:");
    }
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "DeleteIDCorid: netmag: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
/*
 *  delete orid from stamag
 */
    sprintf(sql, "delete from stamag where evid = %d and orid = %d",
            evid, orid);
    if ((res_set = PQexec(conn, sql)) == NULL) {
        PrintPgsqlError("DeleteIDCorid: stamag:");
    }
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "DeleteIDCorid: stamag: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
/*
 *  delete orid from assoc
 */
    sprintf(sql, "delete from assoc where orid = %d", orid);
    if (verbose > 2) fprintf(logfp, "            DeleteIDCorid: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL)
        PrintPgsqlError("DeleteIDCorid: assoc:");
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "DeleteIDCorid: assoc: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
    if (verbose)
        fprintf(logfp, "    DeleteIDCorid: %d, %d hypocentre deleted.\n",
                evid, orid);
}

/*
 *  Title:
 *      GetIDClastid
 *  Synopsis:
 *      Gets next unique id from DB sequence in the isc account.
 *  Input Arguments:
 *      sequence - sequence name
 *  Output Arguments:
 *      nextid   - unique id
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     WriteIDCOrigin, WriteIDCNetmag, WriteIDCStamag
 *  Calls:
 *     PrintPgsqlError
 *
 */
static int GetIDClastid(char *keyname, int *nextid)
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[MAXBUF], errmsg[2048];
    int id;
    sprintf(sql, "select keyvalue from lastid \
                   where keyname = '%s' for update of lastid",
            keyname);
    if (verbose > 2) fprintf(logfp, "            GetIDClastid: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL) {
        PrintPgsqlError("GetIDClastid: error: nextid:");
        PQclear(res_set);
        return 1;
    }
    else if (PQntuples(res_set) == 1) {
        id = atoi(PQgetvalue(res_set, 0, 0));
        *nextid = id + 1;
    }
    PQclear(res_set);
    if (verbose > 2) fprintf(logfp, "            new id=%d\n", *nextid);
    sprintf(sql, "update lastid set keyvalue = keyvalue+1 where keyname = '%s'",
            keyname);
    if (verbose > 2) fprintf(logfp, "            GetIDClastid: %s\n", sql);
    if ((res_set = PQexec(conn, sql)) == NULL) {
        PrintPgsqlError("GetIDClastid: ");
        PQclear(res_set);
        return 1;
    }
    else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "GetIDClastid: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
    return 0;
}

#endif
#endif /* IDCDB */

/*  EOF  */
