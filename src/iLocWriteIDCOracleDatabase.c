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
#ifdef ORASQL

#include "iLoc.h"
extern int verbose;                                        /* verbose level */
extern FILE *logfp;
extern FILE *errfp;
extern int errorcode;
extern struct timeval t0;
extern char OutAgency[VALLEN];      /* author for new hypocentres and assocs */
extern char InputDB[VALLEN];       /* read data from this DB account, if any */
extern char OutputDB[VALLEN];    /* write results to this DB account, if any */
extern char NextidDB[VALLEN];               /* get new ids from this account */
extern dpiConn *conn;                          /* Oracle database connection */
extern dpiContext *gContext;
extern int MinNetmagSta;                 /* min number of stamags for netmag */

/*
 * Functions:
 *    WriteEventToIDCOraDatabase
 */

/*
 * Local functions
 */
static int WriteIDCOrigin(EVREC *ep, SOLREC *sp, FE *fep);
static int WriteIDCOrigerr(EVREC *ep, SOLREC *sp);
static int WriteIDCMag(SOLREC *sp, STAMAG **stamag, int evid);
static int WriteIDCAssoc(SOLREC *sp, PHAREC p[]);
static int DeleteIDCorid(int evid, int orid, char *account);
static int GetIDClastid(char *keyname, int *nextid);

/*
 *  Title:
 *     WriteEventToIDCOraDatabase
 *  Synopsis:
 *     Writes solution to IDC NIOB PostgreSQL database.
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
int WriteEventToIDCOraDatabase(EVREC *ep, SOLREC *sp, PHAREC p[],
        STAMAG **stamag, FE *fep)
{
    if (verbose)
        fprintf(logfp, "        WriteEventToIDCOraDatabase: evid=%d\n", ep->evid);
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
    if (WriteIDCOrigerr(ep, sp))
        return 1;
/*
 *  populate assoc table
 */
    if (verbose) fprintf(logfp, "        WriteIDCAssoc\n");
    if (WriteIDCAssoc(sp, p))
        return 1;
/*
 *  populate stamag and netmag tables
 */
    if (sp->nstamag) {
        if (verbose) fprintf(logfp, "        WriteIDCMag\n");
        if (WriteIDCMag(sp, stamag, ep->evid))
            return 1;
    }
    if (verbose)
        fprintf(logfp, "        WriteEventToIDCOraDatabase (%.2f)\n", secs(&t0));
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
 *     WriteEventToIDCOraDatabase
 *  Calls:
 *     PrintOraError, DropSpace, GetIDClastid, EpochToJdate,
 *     GregionNumber, GregToSreg
 */
static int WriteIDCOrigin(EVREC *ep, SOLREC *sp, FE *fep)
{
    dpiStmt *stmt;
    const char *query;
    char sql[2048], psql[2048], depfix[2];
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
    if (ep->OutputDBISChypid) {
/*
 *      previous preferred origin in OutputDB is OutAgency; delete old solution
 */
        DeleteIDCorid(ep->evid, ep->ISChypid, OutputDB);
    }
    else if (ep->ISChypid) {
/*
 *      previous preferred origin is OutAgency; delete old solution
 */
        DeleteIDCorid(ep->evid, ep->ISChypid, InputDB);
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
    sprintf(psql, "insert into %sorigin                           \
                   (evid, orid, srn, grn, etype, auth, algorithm, \
                    time, jdate, lat, lon, depth, dtype, depdp,   \
                    ndp, nass, ndef, lddate)",
            OutputDB);
    sprintf(psql, "%s values (%d, %d, %d, %d, '%s', '%s', 'iLoc', ",
            psql, ep->evid, ep->ISChypid, srn, grn, ep->etype, OutAgency);
    sprintf(psql, "%s %f, %d, %f, %f, %f, '%s', ",
            psql, sp->time, jdate, sp->lat, sp->lon, sp->depth, depfix);
    if (sp->depdp == NULLVAL)
        sprintf(psql, "%s NULL, NULL, ", psql);
    else
        sprintf(psql, "%s %f, %d, ", psql, sp->depdp, sp->ndp);
    sprintf(psql, "%s %d, %d, sysdate)", psql, sp->nass, sp->ndef);
    DropSpace(psql, sql);
    query = sql;
    if (verbose > 2)
        fprintf(logfp, "            WriteIDCOrigin: %s\n", query);
    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
         return PrintOraError(stmt, "WriteIDCOrigin: prepareStmt");
    if (dpiStmt_execute(stmt, 0, NULL) < 0)
        return PrintOraError(stmt, "WriteIDCOrigin: execute");
    dpiStmt_release(stmt);
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
 *     WriteEventToIDCOraDatabase
 *  Calls:
 *     PrintOraError, DropSpace
 */
static int WriteIDCOrigerr(EVREC *ep, SOLREC *sp)
{
    extern double ConfidenceLevel;                       /* from config file */
    dpiStmt *stmt;
    const char *query;
    char sql[2048], psql[2048];
    char conf[5];
    if      (fabs(ConfidenceLevel - 90.) < DEPSILON) strcpy(conf, "90");
    else if (fabs(ConfidenceLevel - 95.) < DEPSILON) strcpy(conf, "95");
    else if (fabs(ConfidenceLevel - 98.) < DEPSILON) strcpy(conf, "98");
    else                                             strcpy(conf, "NULL");
    sprintf(psql, "insert into %sorigerr                                    \
                   (orid, stt, stx, sty, stz, sxx, sxy, syy, syz, sxz, szz, \
                    sdobs, smajax, sminax, strike, stime, sdepth, conf,     \
                    lddate) values (%d, ",
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
    sprintf(psql, "%s '%s', sysdate)", psql, conf);
    DropSpace(psql, sql);
    query = sql;
    if (verbose > 2)
        fprintf(logfp, "            WriteIDCOrigerr: %s\n", query);
    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
         return PrintOraError(stmt, "WriteIDCOrigerr: prepareStmt");
    if (dpiStmt_execute(stmt, 0, NULL) < 0)
        return PrintOraError(stmt, "WriteIDCOrigerr: execute");
    dpiStmt_release(stmt);
    return 0;
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
 *     PrintOraError, DropSpace, GetIDClastid
 */
static int WriteIDCMag(SOLREC *sp, STAMAG **stamag, int evid)
{
    dpiStmt *stmt;
    const char *query;
    char sql[2048], psql[2048];
    STAMAG *smag = (STAMAG *)NULL;
    int i, k, nextid = 0;
    for (i = 0; i < MAXMAG; i++) {
        smag = stamag[i];
        if (sp->mag[i].numDefiningMagnitude >= MinNetmagSta) {
/*
 *          populate netmag table
 */
            if (GetIDClastid("magid", &nextid))
                return 1;
            sp->mag[i].magid = nextid;
            sprintf(psql, "insert into %snetmag                    \
                           (evid, orid, magid, magtype, magnitude, \
                            nsta, uncertainty, auth, lddate) values ",
                    OutputDB);
            sprintf(psql, "%s (%d, %d, %d, '%s', %f, %d, %f, '%s', sysdate)",
                    psql, evid, sp->hypid, sp->mag[i].magid,
                    sp->mag[i].magtype, sp->mag[i].magnitude,
                    sp->mag[i].numDefiningMagnitude,
                    sp->mag[i].uncertainty, OutAgency);
            DropSpace(psql, sql);
            query = sql;
            if (verbose > 2)
                fprintf(logfp, "            WriteIDCNetmag: %s\n", query);
            if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
                 return PrintOraError(stmt, "WriteIDCNetmag: prepareStmt");
            if (dpiStmt_execute(stmt, 0, NULL) < 0)
                return PrintOraError(stmt, "WriteIDCNetmag: execute");
            dpiStmt_release(stmt);
/*
 *          populate stamag table with station magnitudes belonging to netmag
 */
            for (k = 0; k < sp->mag[i].numDefiningMagnitude; k++) {
                if (smag[k].mtypeid == sp->mag[i].mtypeid) {
                    sprintf(psql, "insert into %sstamag            \
                                   (evid, orid, magid, magtype,    \
                                    magnitude,  magdef, sta, auth, \
                                    lddate)                        \
                                   values (%d, %d, %d, '%s', %f,   \
                                          '%d', '%s', '%s', sysdate)",
                            OutputDB, evid, sp->hypid, sp->mag[i].magid,
                            smag[k].magtype, smag[k].magnitude,
                            smag[k].magdef, smag[k].sta, OutAgency);
                    DropSpace(psql, sql);
                    query = sql;
                    if (verbose > 2)
                        fprintf(logfp, "            WriteIDCNetmag: %s\n", query);
                    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
                         return PrintOraError(stmt, "WriteIDCNetmag: prepareStmt");
                    if (dpiStmt_execute(stmt, 0, NULL) < 0)
                        return PrintOraError(stmt, "WriteIDCNetmag: execute");
                    dpiStmt_release(stmt);
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
                sprintf(psql, "insert into %sstamag                    \
                               (evid, orid, magid, magtype, magnitude, \
                                magdef, sta, auth, lddate)             \
                               values (%d, %d, %d, '%s', %f, '%d',     \
                                      '%s', '%s', sysdate)",
                        OutputDB, evid, sp->hypid, nextid, smag[k].magtype,
                        smag[k].magnitude, smag[k].magdef, smag[k].sta,
                        OutAgency);
                DropSpace(psql, sql);
                if (verbose > 2)
                    fprintf(logfp, "            WriteIDCNetmag: %s\n", query);
                if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
                     return PrintOraError(stmt, "WriteIDCNetmag: prepareStmt");
                if (dpiStmt_execute(stmt, 0, NULL) < 0)
                    return PrintOraError(stmt, "WriteIDCNetmag: execute");
                dpiStmt_release(stmt);
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
 *     WriteEventToIDCOraDatabase
 *  Calls:
 *     PrintOraError, DropSpace
 */
static int WriteIDCAssoc(SOLREC *sp, PHAREC p[])
{
    dpiStmt *stmt;
    const char *query;
    char sql[2048], psql[2048];
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
        sprintf(psql, "insert into %sassoc                        \
                      (orid, arid, phase, sta, delta, esaz, seaz, \
                       timeres, timedef, wgt, azres, azdef,       \
                       slores, slodef, vmodel, lddate)            \
                       values (%d, %d, ",
                OutputDB, sp->hypid, p[i].phid);
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
        sprintf(psql, "%s '%s', sysdate)", psql, p[i].vmod);
        DropSpace(psql, sql);
        query = sql;
        if (verbose > 2) fprintf(logfp, "            WriteIDCAssoc: %s\n", query);
        if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
             return PrintOraError(stmt, "WriteIDCAssoc: prepareStmt");
        if (dpiStmt_execute(stmt, 0, NULL) < 0)
            return PrintOraError(stmt, "WriteIDCAssoc: execute");
        dpiStmt_release(stmt);
    }
    return 0;
}

/*
 *  Title:
 *     UpdateMagnitudesInIDCOraDatabase
 *  Synopsis:
 *     Writes magnitudes to IDC Oracle database.
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
int UpdateMagnitudesInIDCOraDatabase(EVREC *ep, SOLREC *sp, STAMAG **stamag)
{
    dpiStmt *stmt;
    const char *query;
    char sql[MAXBUF];
    if (verbose)
        fprintf(logfp, "        UpdateMagnitudesInIDCOraDatabase");
/*
 *  delete orid from netmag
 */
    sprintf(sql, "delete from %snetmag where evid = %d and orid = %d",
            OutputDB, ep->evid, sp->hypid);
    query = sql;
    if (verbose > 2) fprintf(logfp, "            UpdateMagnitudes: %s\n", query);
    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
         return PrintOraError(stmt, "UpdateMagnitudes: prepareStmt");
    if (dpiStmt_execute(stmt, 0, NULL) < 0)
        return PrintOraError(stmt, "UpdateMagnitudes: execute");
    dpiStmt_release(stmt);
/*
 *  delete orid from stamag
 */
    sprintf(sql, "delete from %sstamag where evid = %d and orid = %d",
            OutputDB, ep->evid, sp->hypid);
    query = sql;
    if (verbose > 2) fprintf(logfp, "            UpdateMagnitudes: %s\n", query);
    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
         return PrintOraError(stmt, "UpdateMagnitudes: prepareStmt");
    if (dpiStmt_execute(stmt, 0, NULL) < 0)
        return PrintOraError(stmt, "UpdateMagnitudes: execute");
    dpiStmt_release(stmt);
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
 *     PrintOraError
 */
static int DeleteIDCorid(int evid, int orid, char *account)
{
    dpiStmt *stmt;
    const char *query;
    char sql[MAXBUF];
/*
 *  delete orid from origin
 */
    sprintf(sql, "delete from %sorigin where evid = %d and orid = %d",
            account, evid, orid);
    query = sql;
    if (verbose > 2) fprintf(logfp, "            DeleteIDCorid: %s\n", query);
    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
         return PrintOraError(stmt, "DeleteIDCorid: prepareStmt");
    if (dpiStmt_execute(stmt, 0, NULL) < 0)
        return PrintOraError(stmt, "DeleteIDCorid: execute");
    dpiStmt_release(stmt);
/*
 *  delete orid from origerr
 */
    sprintf(sql, "delete from %sorigerr where orid = %d", account, orid);
    query = sql;
    if (verbose > 2) fprintf(logfp, "            DeleteIDCorid: %s\n", query);
    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
         return PrintOraError(stmt, "DeleteIDCorid: prepareStmt");
    if (dpiStmt_execute(stmt, 0, NULL) < 0)
        return PrintOraError(stmt, "DeleteIDCorid: execute");
    dpiStmt_release(stmt);
/*
 *  delete orid from netmag
 */
    sprintf(sql, "delete from %snetmag where evid = %d and orid = %d",
            account, evid, orid);
    query = sql;
    if (verbose > 2) fprintf(logfp, "            DeleteIDCorid: %s\n", query);
    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
         return PrintOraError(stmt, "DeleteIDCorid: prepareStmt");
    if (dpiStmt_execute(stmt, 0, NULL) < 0)
        return PrintOraError(stmt, "DeleteIDCorid: execute");
    dpiStmt_release(stmt);
/*
 *  delete orid from stamag
 */
    sprintf(sql, "delete from %sstamag where evid = %d and orid = %d",
            account, evid, orid);
    query = sql;
    if (verbose > 2) fprintf(logfp, "            DeleteIDCorid: %s\n", query);
    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
         return PrintOraError(stmt, "DeleteIDCorid: prepareStmt");
    if (dpiStmt_execute(stmt, 0, NULL) < 0)
        return PrintOraError(stmt, "DeleteIDCorid: execute");
    dpiStmt_release(stmt);
/*
 *  delete orid from assoc
 */
    sprintf(sql, "delete from %sassoc where orid = %d", account, orid);
    query = sql;
    if (verbose > 2) fprintf(logfp, "            DeleteIDCorid: %s\n", query);
    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
         return PrintOraError(stmt, "DeleteIDCorid: prepareStmt");
    if (dpiStmt_execute(stmt, 0, NULL) < 0)
        return PrintOraError(stmt, "DeleteIDCorid: execute");
    dpiStmt_release(stmt);
    if (dpiConn_commit(conn) < 0)
        return PrintOraError(stmt, "DeleteIDCorid: commit;");
    if (verbose)
        fprintf(logfp, "    DeleteIDCorid: %d, %d hypocentre deleted.\n",
                evid, orid);
    return 0;
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
 *     PrintOraError
 *
 */
static int GetIDClastid(char *keyname, int *nextid)
{
    dpiStmt *stmt;
    dpiData *data;
    dpiNativeTypeNum nativeTypeNum;
    const char *query;
    uint32_t numQueryColumns, bufferRowIndex;
    char sql[MAXBUF], psql[MAXBUF];
    int id, found = 0;
    sprintf(psql, "select keyvalue from %slastid \
                   where keyname = '%s' for update of keyvalue",
            NextidDB, keyname);
    DropSpace(psql, sql);
    query = sql;
    if (verbose > 2) fprintf(logfp, "            GetIDClastid: %s\n", query);
    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
         return PrintOraError(stmt, "GetIDClastid: prepareStmt");
    if (dpiStmt_execute(stmt, 0, &numQueryColumns) < 0)
        return PrintOraError(stmt, "GetIDClastid: execute");
    do {
        if (dpiStmt_fetch(stmt, &found, &bufferRowIndex) < 0)
            return PrintOraError(stmt, "GetIDClastid: fetch");
        if (!found) break;
        if (dpiStmt_getQueryValue(stmt, 1, &nativeTypeNum, &data) < 0)
            return PrintOraError(stmt, "GetIDClastid: getQueryValue");
        id = (int)data->value.asInt64;
        *nextid = id + 1;
    } while (found);
    dpiStmt_release(stmt);
    if (verbose > 2) fprintf(logfp, "            new id=%d\n", *nextid);
    sprintf(sql, "update %slastid set keyvalue = keyvalue+1 where keyname = '%s'",
            NextidDB, keyname);
    query = sql;
    if (verbose > 2) fprintf(logfp, "            GetIDClastid: %s\n", query);
    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
         return PrintOraError(stmt, "GetIDClastid: prepareStmt");
    if (dpiStmt_execute(stmt, 0, NULL) < 0)
        return PrintOraError(stmt, "GetIDClastid: execute");
    dpiStmt_release(stmt);
    return 0;
}

#endif
#endif /* IDCDB */

/*  EOF  */
