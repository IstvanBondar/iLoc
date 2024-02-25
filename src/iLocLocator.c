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
#include "iLoc.h"

extern int verbose;
extern FILE *logfp;
extern FILE *errfp;
extern int errorcode;
#ifdef PGSQL
extern PGconn *conn;                       /* PostgreSQL database connection */
#endif
#ifdef MYSQLDB
extern MYSQL *mysql;                            /* MySQL database connection */
#endif
#ifdef ORASQL
extern dpiConn *conn;                          /* Oracle database connection */
extern dpiContext *gContext;
#endif
extern struct timeval t0;
extern int MinIterations;                        /* min number of iterations */
extern int MaxIterations;                        /* max number of iterations */
extern int MinNdefPhases;                            /* min number of phases */
extern double DefaultDepth;         /* used if seed hypocentre depth is NULL */
extern double Moho;                                            /* Moho depth */
extern double Conrad;                                        /* Conrad depth */
extern int DoCorrelatedErrors;              /* account for correlated errors */
extern int AllowDamping;                 /* allow damping in LSQR iterations */
extern int UpdateDB;                             /* write result to database */
extern char InputDB[VALLEN];       /* read data from this DB account, if any */
extern char OutputDB[VALLEN];    /* write results to this DB account, if any */
extern char ISFOutputFile[FILENAMELEN];               /* output ISF filename */
extern double MaxHypocenterDepth;                    /* max hypocenter depth */
extern double MaxShallowDepthError;      /* max error for crustal free-depth */
extern double MaxDeepDepthError;            /* max error for deep free-depth */
extern int DoGridSearch;         /* perform grid search for initial location */
extern int WriteNAResultsToFile;                    /* write results to file */
extern double NAsearchRadius;       /* search radius around preferred origin */
extern double NAsearchDepth;    /* search radius (km) around preferred depth */
extern double NAsearchOT;           /* search radius (s) around preferred OT */
extern double NAlpNorm;                /* p-value for norm to compute misfit */
extern int NAiterMax;                            /* max number of iterations */
extern int NAinitialSample;                        /* size of initial sample */
extern int NAnextSample;                       /* size of subsequent samples */
extern int NAcells;                       /* number of cells to be resampled */
extern long iseed;                                     /* random number seed */
extern char InAgency[VALLEN];                     /* author for input assocs */
extern int UseLocalTT;                           /* use local TT predictions */
extern double MaxLocalTTDelta;           /* use local TT up to this distance */
extern char LocalVmodelFile[FILENAMELEN];       /* pathname for local vmodel */
extern double PrevLat;                                 /* previous epicentre */
extern double PrevLon;                                 /* previous epicentre */
extern int UpdateLocalTT;                         /* static/dynamic local TT */
extern int MinNetmagSta;                 /* min number of stamags for netmag */
extern int MagnitudesOnly;                      /* calculate magnitudes only */

/*
 * Functions:
 *    Locator
 *    Synthetic
 *    ResidualsForFixedHypocenter
 *    LocateEvent
 *    GetPhaseList
 *    FreePhaseList
 *    Readings
 */

/*
 * Local functions
 *    GetNdef
 *    GetResiduals
 *    BuildGd
 *    ProjectGd
 *    WeightGd
 *    ConvergenceTestValue
 *    ConvergenceTest
 *    WxG
 */
static int GetNdef(int numPhase, PHAREC p[], int nsta, STAREC stalist[],
        double *toffset);
static int GetResiduals(SOLREC *sp, READING *rdindx, PHAREC p[], EC_COEF *ec,
        TT_TABLE *TTtables, TT_TABLE *LocalTTtable, short int **topo,
        int iszderiv, int *has_depdpres, int *ndef, int *ischanged, int iter,
        int ispchange, int prevndef, int *nunp, char **phundef, double **dcov,
        double **w, int is2nderiv);
static double BuildGd(int ndef, SOLREC *sp, PHAREC p[], int fixdepthfornow,
        double **g, double *d);
static int ProjectGd(int ndef, int m, double **g, double *d, double **w,
        double *dnorm, double *wrms);
static int WxG(int j, int ndef, double **w, double **g);
static void WeightGd(int ndef, int m, int numPhase, PHAREC p[],
        double **g, double *d, double *dnorm, double *wrms);
static double ConvergenceTestValue(double gtdnorm, double gnorm, double dnorm);
static int ConvergenceTest(int iter, int m, int *nds, double *sol,
        double *oldsol, double wrms, double *modelnorm, double *convgtest,
        double *oldcvgtst, double *step, int *isdiv);


/*
 *  Title:
 *     Locator
 *  Synopsis:
 *     Prepares for iterative linearised least-squares inversion.
 *        sets starting hypocentre
 *           if hypocenter is fixed then just calculate residuals and return
 *        calculates station separations and nearest-neighbour station order
 *        sets locator option according to instructions:
 *           option = 0 free depth
 *           option = 1 fix to region-dependent default depth
 *           option = 2 fix depth to value provided by analyst (only on request)
 *           option = 3 fix depth to median of reported depths (only on request)
 *           option = 4 fix location (only on request)
 *           option = 5 fix depth and location (only on request)
 *           option = 6 fix hypocentre (only on request)
 *        performs phase identification w.r.t. starting hypocentre
 *        performs NA grid search to get initial guess for linearised inversion
 *        reidentifies phases according to best NA hypocentre
 *        tests for depth resolution; fixes depth if necessary
 *        locates event
 *        if convergent solution is obtained:
 *           discards free-depth solution if depth error is too large
 *              fixes depth and start over again
 *           performs depth-phase stack if there is depth-phase depth resolution
 *           calculates residuals for all reported phases
 *           calculates location quality metrics
 *           calculates magnitudes
 *           writes results to database and/or ISF file
 *        else:
 *           reverts to previous preferred hypocentre
 *           calculates residuals for all reported phases
 *        reports results
 *
 *  Input arguments:
 *     isf       - ISF text file input?
 *     db        - database connection
 *                    0=none, 1=ISCPG, 2=SC3PG, 3=IDCPG, 4=SC3MYSQL, 5=IDCORA
 *     e         - pointer to event info
 *     h         - array of hypocentres
 *     s         - pointer to current solution
 *     p         - array of phase structures
 *     ismbQ     - use mb magnitude attenuation table? (0/1)
 *     mbQ       - pointer to mb Q(d,h) table
 *     ec        - pointer to ellipticity correction coefficient structure
 *     TTtables  - array of travel-time tables
 *     LocalTTtables - pointer to local travel-time tables
 *     variogram - pointer to generic variogram model
 *     gres      - grid spacing in default depth grid
 *     ngrid     - number of grid points in default depth grid
 *     DepthGrid - pointer to default depth grid (lat, lon, depth)
 *     fe        - pointer to FE structure
 *     GrnDepth  - pointer to geographic region default depths
 *     topo      - ETOPO bathymetry/elevation matrix
 *     isfout    - file pointer to ISF output file
 *     magbloc   - reported magnitudes from ISF input file
 *  Output arguments:
 *     e         - pointer to event info
 *     s         - pointer to current solution
 *     p         - array of phase structures
 *     total     - number of successful locations
 *     fail      - number of failed locations
 *     opt       - locator option counter
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     main
 *  Calls:
 *     gettimeofday, InitializeEvent, PrintHypocenter, Synthetic,
 *     InitialHypocenter, EpochToHuman, GetStalist, GetDistanceMatrix,
 *     HierarchicalCluster, InitialSolution, GetDefaultDepth, GetDeltaAzimuth,
 *     IdentifyPhases, DuplicatePhases, TravelTimeResiduals, DepthPhaseCheck,
 *     DepthResolution, DepthPhaseStack, SetNASearchSpace, NASearch,
 *     ReIdentifyPhases, LocateEvent, FreeFloatMatrix, IdentifyPFAKE,
 *     RemovePFAKE, LocationQuality, NetworkMagnitudes, GregionNumber,
 *     PrintPhases, PrintSolution, ResidualsForFixedHypocenter,
 *     WriteEventToSC3database, WriteEventToISCdatabase, WriteISF,
 *     RemoveISCHypocenter, ReplaceISCPrime, ReplaceISCAssociation, DistAzimuth
 */
 int Locator(int isf, int db, int *total, int *fail, int *opt,
        EVREC *e, HYPREC h[], SOLREC *s, PHAREC p[], int ismbQ, MAGQ *mbQ,
        EC_COEF *ec, TT_TABLE *TTtables, TT_TABLE *LocalTTtables[],
        VARIOGRAM *variogram, double gres, int ngrid, double **DepthGrid,
        FE *fe, double *GrnDepth, short int **topo, FILE *isfout, char *magbloc)
{
    HYPREC starthyp;                                   /* median hypocenter */
    SOLREC grds;                                         /* solution record */
    HYPQUAL hq;                        /* hypocenter quality metrics record */
    NASPACE nasp;                   /* Neighbourhood algorithm search space */
    double **distmatrix = (double **)NULL;               /* distance matrix */
    STAMAG **stamag = (STAMAG **)NULL;         /* station magnitude records */
    STAMAG **rdmag = (STAMAG **)NULL;          /* reading magnitude records */
    MSZH *mszh = (MSZH *)NULL;  /* MS vertical|horizontal magnitude records */
    STAREC *stalist = (STAREC *)NULL;                /* unique station list */
    STAORDER *staorder = (STAORDER *)NULL;              /* NN station order */
    READING *rdindx = (READING *)NULL;                          /* Readings */
    TT_TABLE *LocalTTtable = *LocalTTtables;
#ifdef PGSQL
    PGresult *res_set = (PGresult *)NULL;
#endif
#ifdef ORASQL
    dpiStmt *stmt;
    const char *query = "set transaction name 'IDCDB'";
#endif
    HYPREC temp;
    char timestr[25], gregname[255];
    char filename[FILENAMELEN];
    int iszderiv = 0, firstpass = 1, DoGridSearch_cf = DoGridSearch;
    int option = 0, grn = 0, i, isgridsearch, istimefix;
    int hasDepthResolution = 0, has_depdpres = 0, isdefdep = 0;
    int nsta = 0, ndef = 0, ntimedef = 0, is2nderiv = 1;
    double mediandepth, medianot, medianlat, medianlon, epidist, x, y;
    isgridsearch = DoGridSearch;
    s->hypid = 0;
/*
 *
 *  Initialize event
 *
 */
    gettimeofday(&t0, NULL);
    if (InitializeEvent(e, h)) {
        fprintf(logfp, "ABORT: event initialization failed!\n");
        return 1;
    }
/*
 *  Print hypocentres (first is preferred one)
 */
    PrintHypocenter(e, h);
/*
 *  Check that there are enough phases to start with.
 */
    if (!e->FixedHypocenter && e->numPhase < MinNdefPhases) {
        fprintf(logfp, "WARNING: insufficient number of phases (%d), ",
                e->numPhase);
        fprintf(logfp, "fixing the hypocentre\n");
        fprintf(errfp, "insufficient number of phases (%d), ", e->numPhase);
        fprintf(errfp, "fixing the hypocentre\n");
        e->FixedHypocenter = 1;
    }
/*
 *  Allocate memory for rdindx
 */
    if ((rdindx = (READING *)calloc(e->numReading, sizeof(READING))) == NULL) {
        fprintf(logfp, "ABORT: cannot allocate memory for rdindx!\n");
        fprintf(errfp, "Locator: cannot allocate memory for rdindx!\n");
        errorcode = 1;
        return 1;
    }
    Readings(e->numPhase, e->numReading, p, rdindx);
/*
 *
 *  If hypocenter is fixed then just calculate residuals and magnitudes
 *
 */
    if (e->FixedHypocenter) {
        fprintf(logfp, "Calculate residuals for fixed hypocentre\n");
        s->numUnknowns = 0;
        s->hypofix = 1;
        Synthetic(e, &h[0], s, rdindx, p, ec, TTtables, LocalTTtables,
                  topo, db, isf);
/*
 *      Magnitudes
 */
        mszh = (MSZH *)calloc(s->nreading, sizeof(MSZH));
        stamag = (STAMAG **)calloc(MAXMAG, sizeof(STAMAG *));
        stamag[0] = (STAMAG *)calloc(MAXMAG * s->nreading, sizeof(STAMAG));
        for (i = 1; i < MAXMAG; i++)
            stamag[i] = stamag[i - 1] + s->nreading;
        rdmag = (STAMAG **)calloc(MAXMAG, sizeof(STAMAG *));
        if ((rdmag[0] = (STAMAG *)calloc(MAXMAG * s->nreading, sizeof(STAMAG))) == NULL) {
            fprintf(logfp, "Cannot allocate memory for station magnitudes!\n");
            fprintf(errfp, "Cannot allocate memory for station magnitudes!\n");
            Free(rdmag);
            Free(stamag[0]);
            Free(stamag);
            Free(mszh);
            errorcode = 1;
            return 1;
        }
        else {
            for (i = 1; i < MAXMAG; i++)
                rdmag[i] = rdmag[i - 1] + s->nreading;
            NetworkMagnitudes(s, rdindx, p, stamag, rdmag, mszh, ismbQ, mbQ);
        }
        opt[6]++;
        Free(rdindx);
/*
 *      Print results to log file.
 */
        grn = GregionNumber(s->lat, s->lon, fe);
        Gregion(grn, gregname);
        PrintPhases(s->numPhase, p);
        for (i = 0; i < MAXMAG; i++) {
            if (s->mag[i].numDefiningMagnitude < MinNetmagSta) continue;
            fprintf(logfp, "    %s=%.2f +/- %.2f nsta=%d\n",
                    s->mag[i].magtype, s->mag[i].magnitude,
                    s->mag[i].uncertainty, s->mag[i].numDefiningMagnitude);
        }
/*
 *      Write event to ISF2 file if required.
 */
        if (ISFOutputFile[0]) {
            s->timfix = 1;
            s->epifix = 1;
            s->depfix = 1;
            i = s->hypid;
            if (i == 0) s->hypid = *total;
            if (*total == 0) {
/*
 *              Write event header
 */
                fprintf(isfout, "BEGIN IMS2.0\n");
                fprintf(isfout, "DATA_TYPE BULLETIN IMS1.0:short with ISF2.1 extensions\n");
            }
            WriteISF(isfout, e, s, h, p, stamag, rdmag, grn, magbloc);
            s->hypid = i;
        }
        Free(rdmag[0]);
        Free(rdmag);
        Free(stamag[0]);
        Free(stamag);
        Free(mszh);
        return 0;
    }
/*
 *
 *  If only magnitudes need to be calculated w.r.t. preferred origin
 *
 */
    if (MagnitudesOnly) {
        fprintf(logfp, "Calculate magnitudes for the preferred hypocentre\n");
        s->numUnknowns = 0;
        ResidualsForFixedHypocenter(e, &h[0], s, rdindx, p, ec,
                                    TTtables, LocalTTtables, topo);
/*
 *      Magnitudes
 */
        mszh = (MSZH *)calloc(s->nreading, sizeof(MSZH));
        stamag = (STAMAG **)calloc(MAXMAG, sizeof(STAMAG *));
        stamag[0] = (STAMAG *)calloc(MAXMAG * s->nreading, sizeof(STAMAG));
        for (i = 1; i < MAXMAG; i++)
            stamag[i] = stamag[i - 1] + s->nreading;
        rdmag = (STAMAG **)calloc(MAXMAG, sizeof(STAMAG *));
        if ((rdmag[0] = (STAMAG *)calloc(MAXMAG * s->nreading, sizeof(STAMAG))) == NULL) {
            fprintf(logfp, "Cannot allocate memory for station magnitudes!\n");
            fprintf(errfp, "Cannot allocate memory for station magnitudes!\n");
            Free(rdmag);
            Free(stamag[0]);
            Free(stamag);
            Free(mszh);
            errorcode = 1;
            return 1;
        }
        else {
            for (i = 1; i < MAXMAG; i++)
                rdmag[i] = rdmag[i - 1] + s->nreading;
            NetworkMagnitudes(s, rdindx, p, stamag, rdmag, mszh, ismbQ, mbQ);
        }
        opt[6]++;
        *total += 1;
        Free(rdindx);
/*
 *      Print results to log file.
 */
        grn = GregionNumber(s->lat, s->lon, fe);
        Gregion(grn, gregname);
        PrintPhases(s->numPhase, p);
        for (i = 0; i < MAXMAG; i++) {
            if (s->mag[i].numDefiningMagnitude < MinNetmagSta) continue;
            fprintf(logfp, "    %s=%.2f +/- %.2f nsta=%d\n",
                    s->mag[i].magtype, s->mag[i].magnitude,
                    s->mag[i].uncertainty, s->mag[i].numDefiningMagnitude);
        }
#ifdef DB
        if (UpdateDB && db) {
            if (strcmp(InputDB, OutputDB) && !e->OutputDBPreferredOrid) {
                fprintf(errfp, "WARNING: no event found in %s!\n", OutputDB);
                fprintf(logfp, "WARNING: no event found in %s!\n", OutputDB);
            }
            else {
#ifdef PGSQL
#ifdef ISCDB
/*
 *              Write magnitudes to ISC PostgreSQL database
 */
                if (db == 1) {
                    res_set = PQexec(conn, "begin");
                    PQclear(res_set);
                    if (verbose)
                        fprintf(logfp, "        UpdateMagnitudesInISCPgsqlDatabase\n");
                    if (UpdateMagnitudesInISCPgsqlDatabase(e, s, p, stamag, rdmag, mszh)) {
                        fprintf(errfp, "Locator: could not update magnitudes %d!\n", e->evid);
                        fprintf(logfp, "Locator: could not update magnitudes %d!\n", e->evid);
                        res_set = PQexec(conn, "rollback");
                        PQclear(res_set);
                    }
                    else {
                        res_set = PQexec(conn, "commit");
                        PQclear(res_set);
                    }
                }
#endif /* ISC */
#ifdef SC3DB
/*
 *              Write magnitudes to SeisComp3 NIAB PostgreSQL database
 */
                if (db == 2) {
                    res_set = PQexec(conn, "begin");
                    PQclear(res_set);
                    if (verbose)
                        fprintf(logfp, "        UpdateMagnitudesInSC3NIABDatabase\n");
                    if (UpdateMagnitudesInSC3NIABDatabase(e, s, p, stamag, rdmag)) {
                        fprintf(errfp, "Locator: could not update magnitudes %d!\n", e->evid);
                        fprintf(logfp, "Locator: could not update magnitudes %d!\n", e->evid);
                        res_set = PQexec(conn, "rollback");
                        PQclear(res_set);
                    }
                    else {
                        res_set = PQexec(conn, "commit");
                        PQclear(res_set);
                    }
                }
#endif /* SC3 NIAB */
#ifdef IDCDB
/*
 *              Write magnitudes to IDC NIAB PostgreSQL database
 */
                if (db == 3) {
                    res_set = PQexec(conn, "begin");
                    PQclear(res_set);
                    if (verbose)
                        fprintf(logfp, "        UpdateMagnitudesInIDCNIABDatabase\n");
                    if (UpdateMagnitudesInIDCNIABDatabase(e, s, stamag)) {
                        fprintf(errfp, "Locator: could not update magnitudes %d!\n", e->evid);
                        fprintf(logfp, "Locator: could not update magnitudes %d!\n", e->evid);
                        res_set = PQexec(conn, "rollback");
                        PQclear(res_set);
                    }
                    else {
                        res_set = PQexec(conn, "commit");
                        PQclear(res_set);
                    }
                }
#endif /* IDC NIAB */
#endif /* PGSQL */
#ifdef MYSQLDB
#ifdef SC3DB
/*
 *              Write magnitudes to SeisComp3 MySQL database
 */
                if (db == 4) {
                    mysql_query(mysql, "start transaction");
                    if (verbose)
                        fprintf(logfp, "        UpdateMagnitudesInSC3MysqlDatabase\n");
                    if (UpdateMagnitudesInSC3MysqlDatabase(e, s, p, stamag, rdmag)) {
                        fprintf(errfp, "Locator: could not update magnitudes %d!\n", e->evid);
                        fprintf(logfp, "Locator: could not update magnitudes %d!\n", e->evid);
                        mysql_query(mysql, "rollback");
                    }
                    else {
                        mysql_query(mysql, "commit");
                    }
                }
#endif /* SC3 MYSQL */
#endif /* MYSQLDB */
#ifdef ORASQL
#ifdef IDCDB
/*
 *              Write magnitudes to IDC Oracle database
 */
                if (db == 5) {
                    if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
                         return PrintOraError(stmt, "Locator: prepareStmt");
                    if (dpiStmt_execute(stmt, 0, NULL) < 0)
                        return PrintOraError(stmt, "Locator: execute");
                    dpiStmt_release(stmt);
                    if (verbose)
                        fprintf(logfp, "        UpdateMagnitudesInIDCOraDatabase\n");
                    if (UpdateMagnitudesInIDCOraDatabase(e, s, stamag)) {
                        fprintf(errfp, "Locator: could not update magnitudes %d!\n", e->evid);
                        fprintf(logfp, "Locator: could not update magnitudes %d!\n", e->evid);
                        dpiConn_rollback(conn);
                    }
                    else
                        dpiConn_commit(conn);
                }
#endif /* IDC */
#endif /* ORASQL */
            }
            fprintf(logfp, "Locator: updated magnitudes for event %d (%.2f)\n",
                    e->evid, secs(&t0));
        }
        else {
            if (UpdateDB) {
                fprintf(errfp, "Locator: no database connection!\n");
                fprintf(logfp, "Locator: no database connection!\n");
            }
        }
#endif /* DB */
/*
 *      Write event to ISF2 file if required.
 */
        if (ISFOutputFile[0]) {
            i = s->hypid;
            if (i == 0) s->hypid = *total;
            if (*total == 1) {
/*
 *              Write event header
 */
                fprintf(isfout, "BEGIN IMS2.0\n");
                fprintf(isfout, "DATA_TYPE BULLETIN IMS1.0:short with ISF2.1 extensions\n");
            }
            WriteISF(isfout, e, s, h, p, stamag, rdmag, grn, magbloc);
            s->hypid = i;
        }
        Free(rdmag[0]);
        Free(rdmag);
        Free(stamag[0]);
        Free(stamag);
        Free(mszh);
        return 0;
/*
 *  end of MagnitudesOnly block
 */
    }
/*
 *
 *  Prepare for event location
 *
 */
/*
 *  set starting hypocentre to median of all hypocentre parameters
 */
    InitialHypocenter(e, h, &starthyp);
    EpochToHuman(timestr, starthyp.time);
    fprintf(logfp, "Median hypocentre:\n");
    fprintf(logfp, "  OT = %s Lat = %7.3f Lon = %8.3f Depth = %.1f\n",
            timestr, starthyp.lat, starthyp.lon, starthyp.depth);
/*
 *  get unique station list
 */
    if ((stalist = GetStalist(e->numPhase, p, &nsta)) == NULL) {
        fprintf(logfp, "ABORT: cannot generate station list!\n");
        fprintf(errfp, "cannot get station list!\n");
        Free(rdindx);
        return 1;
    }
    s->nsta = nsta;
/*
 *  correlated errors
 */
    if (DoCorrelatedErrors) {
        if ((staorder = (STAORDER *)calloc(nsta, sizeof(STAORDER))) == NULL) {
            fprintf(logfp, "ABORT: staorder: cannot allocate memory\n");
            fprintf(errfp, "staorder: cannot allocate memory\n");
            Free(stalist);
            Free(rdindx);
            return 1;
        }
/*
 *      calculate station separations
 */
        if ((distmatrix = GetDistanceMatrix(nsta, stalist)) == NULL) {
            fprintf(logfp, "ABORT: GetDistanceMatrix failed!\n");
            fprintf(errfp, "cannot get distmatrix!\n");
            Free(stalist); Free(staorder);
            Free(rdindx);
            return 1;
        }
/*
 *      nearest-neighbour station order
 */
        if (HierarchicalCluster(nsta, distmatrix, staorder)) {
            fprintf(logfp, "ABORT: HierarchicalCluster failed!\n");
            fprintf(errfp, "HierarchicalCluster failed!\n");
            Free(stalist); Free(staorder);
            FreeFloatMatrix(distmatrix);
            Free(rdindx);
            return 1;
        }
    }
/*
 *  Option loop: set options
 *       option = 0 free depth
 *       option = 1 fix to region-dependent default depth
 *       option = 2 fixed depth by analyst (only on request)
 *       option = 3 fix depth to median of reported depths (only on request)
 *       option = 4 fix location (only on request).
 *       option = 5 fix depth and location (only on request).
 */
    istimefix = s->timfix;
    for (option = 0; option < 2; option++) {
again:
        errorcode = 0;
/*
 *      Fixed depth instruction
 */
        if (e->FixedDepth)
            option = 2;
/*
 *      Default depth instruction
 */
        if (e->FixDepthToDefault)
            option = 1;
/*
 *      Fix on median depth instruction
 */
        if (e->FixDepthToMedian)
            option = 3;
/*
 *      Fixed location instruction
 */
        if (e->FixedEpicenter) {
/*
 *          depending on e.FixedDepth solve for OT only or OT and depth
 */
            if (e->FixedDepth) option = 5;
            else               option = 4;
        }
        fprintf(logfp, "Option %d\n", option);
/*
 *      Number of model parameters
 */
        if      (option == 0) s->numUnknowns = 4;
        else if (option == 4) s->numUnknowns = 2;
        else if (option == 5) s->numUnknowns = 1;
        else                  s->numUnknowns = 3;
        s->numUnknowns -= e->FixedOT;
/*
 *      depth derivatives are only needed when depth is a free parameter
 */
        if (option == 0 || option == 4) iszderiv = 1;
        else                            iszderiv = 0;
/*
 *      depth fix type (option 1 is set by GetDefaultDepth)
 */
        if (option == 0 || option == 4)
            s->FixedDepthType = 0;
        if (option == 2 || option == 5) {
            if (e->FixDepthToUser)
                s->FixedDepthType = 8;
            else if (e->FixDepthToDefault)
                s->FixedDepthType = 5;
            else if (e->FixDepthToDepdp)
                s->FixedDepthType = 3;
            else if (e->FixDepthToMedian) {
                s->FixedDepthType = 6;
                isdefdep = 1;
            }
            else if (e->FixDepthToZero)
                s->FixedDepthType = 4;
            else
                s->FixedDepthType = 2;
        }
        if (option == 3) {
            s->FixedDepthType = 6;
            isdefdep = 1;
        }
/*
 *      initialize the solution structure
 */
        if (firstpass) {
            if (InitialSolution(s, e, &starthyp)) {
                fprintf(logfp, "    WARNING: InitialSolution failed!\n");
                continue;
            }
            mediandepth = s->depth;
            medianot = s->time;
            medianlat = s->lat;
            medianlon = s->lon;
        }
        if (option == 1) {
/*
 *          either no depth resolution or the locator did not converge
 *              fix depth to region-dependent default depth
 */
            s->depth = mediandepth;
            s->depth = GetDefaultDepth(s, ngrid, gres, DepthGrid, fe,
                                       GrnDepth, &isdefdep);
/*
 *          if there is a large depth difference, fall back to the
 *              initial hypocentre and recalcaluate the default depth
 */
            if (fabs((s->depth - mediandepth)) > 20.) {
                fprintf(logfp, "Large depth difference, ");
                fprintf(logfp, "fall back to median hypocentre\n");
                s->time = medianot;
                s->lat = medianlat;
                s->lon = medianlon;
                s->depth = mediandepth;
                s->depth = GetDefaultDepth(s, ngrid, gres, DepthGrid, fe,
                                           GrnDepth, &isdefdep);
            }
/*
 *          adjust origin time according to depth change if OT is not fixed
 */
            if (!s->timfix)
                s->time += (s->depth - mediandepth) / 10.;
/*
 *          if there is still a large depth difference, do NA again
 */
            if (fabs((s->depth - mediandepth)) > 20.)
                firstpass = 1;
        }
        if (option == 0 || option == 4) s->depfix = 0;
        else                            s->depfix = 1;
        EpochToHuman(timestr, s->time);
        fprintf(logfp, "Initial hypocentre:\n");
        fprintf(logfp, "  OT = %s Lat = %7.3f Lon = %8.3f Depth = %.1f\n",
                timestr, s->lat, s->lon, s->depth);
/*
 *      generate local TT tables if necessary
 */
        if (UpdateLocalTT) {
            epidist = DistAzimuth(PrevLat, PrevLon, s->lat, s->lon, &x, &y);
            epidist *= DEG2KM;
            PrevLat = s->lat;
            PrevLon = s->lon;
            if (epidist > EPIWALK) {
                fprintf(logfp, "Generate local TT tables...\n");
                UseLocalTT = 1;
                if (LocalTTtable != NULL)
                    FreeLocalTTtables(LocalTTtable);
                if ((LocalTTtable = GenerateLocalTTtables(LocalVmodelFile,
                                                  s->lat, s->lon)) == NULL) {
                    fprintf(logfp, "Cannot generate local TT tables!\n");
                    UseLocalTT = 0;
                }
                *LocalTTtables = LocalTTtable;
            }
        }
/*
 *      delta, esaz and seaz for each phase
 */
        GetDeltaAzimuth(s, p, 1);
/*
 *      phase identification
 */
        ndef = IdentifyPhases(s, rdindx, p, ec, TTtables, LocalTTtable, topo,
                              &is2nderiv);
/*
 *      deal with duplicate picks
 */
        if (DuplicatePhases(s, p, ec, TTtables, LocalTTtable, topo))
            continue;
/*
 *
 *      Neighbourhood algorithm search to get initial hypocentre guess
 *          - may be executed only once
 *          - search in 4D (lat, lon, OT, depth)
 *          - reidentify phases w.r.t. each trial hypocentre
 *          - ignore correlated error structure for the sake of speed
 */
        if (isgridsearch && firstpass) {
            fprintf(logfp, "Neighbourhood algorithm (%.4f)\n", secs(&t0));
            memmove(&grds, s, sizeof(SOLREC));
/*
 *          set up search space for NA
 */
            if (SetNASearchSpace(&grds, &nasp)) {
                fprintf(logfp, "    WARNING: SetNASearchSpace failed!\n");
            }
            else {
/*
 *              Neighbourhood algorithm
 */
                if (WriteNAResultsToFile)
                    sprintf(filename, "%d.%d.gsres", e->evid, option);
                if (NASearch(nsta, &grds, p, TTtables, LocalTTtable, ec,
                              topo, stalist, distmatrix, variogram, staorder,
                              &nasp, filename, is2nderiv)) {
                    fprintf(logfp, "    WARNING: NASearch failed!\n");
                    memmove(&grds, s, sizeof(SOLREC));
                }
                else {
/*
 *                  store the best hypo from NA in the solution record
 */
                    s->lat = grds.lat;
                    s->lon = grds.lon;
                    s->time = grds.time;
                    s->depth = grds.depth;
                    EpochToHuman(timestr, s->time);
                    epidist = DistAzimuth(medianlat, medianlon, s->lat, s->lon,
                                          &x, &y);
                    epidist *= DEG2KM;
                    PrevLat = s->lat;
                    PrevLon = s->lon;
                    fprintf(logfp, "Best fitting hypocentre from grid search:\n");
                    fprintf(logfp, "  OT = %s Lat = %7.3f Lon = %8.3f ",
                            timestr, s->lat, s->lon);
                    fprintf(logfp, "Depth = %.1f\n", s->depth);
                    fprintf(logfp, "Distance from initial guess = %.1f km\n",
                            epidist);
/*
 *                  reidentify phases w.r.t. best hypocentre
 */
                    fprintf(logfp, "Reidentify phases after NA\n");
                    if (UpdateLocalTT && epidist > EPIWALK) {
                        UseLocalTT = 1;
                        if (LocalTTtable != NULL)
                            FreeLocalTTtables(LocalTTtable);
                        if ((LocalTTtable =
                             GenerateLocalTTtables(LocalVmodelFile,
                                                  s->lat, s->lon)) == NULL) {
                            fprintf(logfp, "Cannot generate local TT tables!\n");
                            UseLocalTT = 0;
                        }
                        *LocalTTtables = LocalTTtable;
                    }
                    GetDeltaAzimuth(s, p, 0);
                    ReIdentifyPhases(s, rdindx, p, ec, TTtables, LocalTTtable,
                                     topo, is2nderiv);
                    DuplicatePhases(s, p, ec, TTtables, LocalTTtable, topo);
                }
                fprintf(logfp, "NA (%.4f) done\n", secs(&t0));
            }
        }
/*
 *      disable further grid searches
 */
        firstpass = 0;
/*
 *      set ttime, residual, dtdh, and dtdd for each phase
 */
        if (verbose > 4) fprintf(logfp, "Locator: TravelTimeResiduals\n");
        if (TravelTimeResiduals(s, p, "use", ec, TTtables, LocalTTtable, topo,
                                iszderiv, is2nderiv))
            continue;
/*
 *      number of initial defining observations
 */
        ndef = 0;
        for (i = 0; i < s->numPhase; i++) {
            if (p[i].timedef) ndef++;
            if (p[i].azimdef) ndef++;
            if (p[i].slowdef) ndef++;
        }
        if (ndef < s->numUnknowns) {
            fprintf(logfp, "Insufficient number (%d) of phases left\n", ndef);
            errorcode = 7;
            continue;
        }
/*
 *      pointless to try free-depth solution with just a few phases
 */
        if (ndef <= s->numUnknowns + 1 && (option == 0 || option == 4)) {
            fprintf(logfp, "Not enough phases for free-depth solution!\n");
            if (fabs(s->depth - mediandepth) > 20.) firstpass = 1;
            continue;
        }
/*
 *      depth-phase depth resolution
 *          (ndepassoc >= MinDepthPhases && nagent >= MindDepthPhaseAgencies)
 *          also flag first arriving defining P for a reading
 */
        i = (option == 0 || option == 4) ? 1 : 0;
        has_depdpres = DepthPhaseCheck(s, rdindx, p, i);
/*
 *      recount number of defining observations as DepthPhaseCheck
 *          may make depth phases non-defining
 */
        ndef = 0;
        for (i = 0; i < s->numPhase; i++) {
            if (p[i].timedef) ndef++;
            if (p[i].azimdef) ndef++;
            if (p[i].slowdef) ndef++;
        }
        if (ndef < s->numUnknowns) {
            fprintf(logfp, "Insufficient number (%d) of phases left\n", ndef);
            errorcode = 7;
            continue;
        }
/*
 *      depth resolution
 *          (has_depdpres || nlocal >= MinLocalStations ||
 *           nsdef >= MinSPpairs || ncoredef >= MinCorePhases)
 */
        i = (option == 0 || option == 4) ? 1 : 0;
        hasDepthResolution = DepthResolution(s, rdindx, p, i);
        if (has_depdpres) hasDepthResolution = 1;
/*
 *      pointless to try free-depth solution without depth resolution
 */
        if (!hasDepthResolution && (option == 0 || option == 4)) {
            fprintf(logfp, "No depth resolution for free-depth solution!\n");
            if (option == 4) {
                s->depth = GetDefaultDepth(s, ngrid, gres, DepthGrid, fe,
                                           GrnDepth, &isdefdep);
//                s->numUnknowns = 1;
//                s->depfix = 1;
                e->FixedDepth = 1;
                goto again;
            }
            else {
                if (fabs(s->depth - mediandepth) > 20.) firstpass = 1;
                continue;
            }
        }
/*
 *
 *      locate event
 *
 */
        fprintf(logfp, "Event location\n");
        if (LocateEvent(option, nsta, has_depdpres, s, rdindx, p, ec,
                        TTtables, LocalTTtable, stalist, distmatrix,
                        variogram, staorder, topo, is2nderiv)) {
/*
 *          divergent solution
 */
            if (isgridsearch) {
/*
 *              if grid search was enabled:
 *                  disable it, reinitialize and give it one more try
 */
                isgridsearch = 0;
                firstpass = 1;
                s->timfix = istimefix;
                option--;
                if (option > 0) option = 0;
                fprintf(logfp, "Try again without the grid search\n");
            }
            else if (option == 0)
/*
 *              reinitialize solution for the fixed depth option
 */
                firstpass = 1;
            else {
/*
 *              give up
 */
                fprintf(logfp, "locator failed!\n");
                fprintf(errfp, "locator failed!\n");
            }
            continue;
        }
/*
 *
 *      converged?
 *
 */
        if (s->converged) {
/*
 *          Discard free-depth solution if depth error is too large
 */
            if (option == 0 && (
                     (s->depth > 0. && s->depth <= Moho &&
                      s->error[3] > MaxShallowDepthError) ||
                     (s->depth >  Moho && s->error[3] > MaxDeepDepthError))) {
                fprintf(logfp, "Discarded free-depth solution!\n");
                fprintf(logfp, "     depth = %5.1f depth error = %.1f\n",
                                s->depth, s->error[3]);
                firstpass = 1;
                continue;
            }
/*
 *          We're done.
 */
            else
                break;
        }
    }
/*
 *  End of option loop
 */
    DoGridSearch = DoGridSearch_cf;
    if (verbose)
        fprintf(logfp, "End of option loop (%.4f)\n", secs(&t0));
/*
 *  Free memory
 */
    Free(stalist);
    if (DoCorrelatedErrors) {
        FreeFloatMatrix(distmatrix);
        Free(staorder);
    }
/*
 *
 *  Convergence: calculate all residuals, magnitudes etc.
 *
 */
    if (s->converged) {
        fprintf(logfp, "Convergent solution, final touches\n");
        epidist = DistAzimuth(medianlat, medianlon, s->lat, s->lon, &x, &y);
        epidist *= DEG2KM;
        x = s->time - medianot;
        y = s->depth - mediandepth;
        fprintf(logfp, "    Solution - initial guess: ");
        fprintf(logfp, "Distance = %.1f km, OTdiff = %.1f s, Depthdiff = %.1f km\n",
                        epidist, x, y);
/*
 *      Calculate depth-phase depth if possible
 */
        s->depdp = s->depdp_error = NULLVAL;
        has_depdpres = DepthPhaseCheck(s, rdindx, p, 1);
        if (has_depdpres) {
            DepthPhaseStack(s, p, TTtables, topo);
            if (s->depdp != NULLVAL)
                fprintf(logfp, "    ndp = %d, depth=%.1f, depth error=%.1f\n",
                        s->ndp, s->depdp, s->depdp_error);
        }
/*
 *      Temporarily reidentify PFAKES so that they get residuals.
 */
        IdentifyPFAKE(s, p, ec, TTtables, LocalTTtable, topo);
/*
 *      Calculate residuals for all phases
 */
        TravelTimeResiduals(s, p, "all", ec, TTtables, LocalTTtable, topo, 0, 0);
/*
 *      Remove phase codes from temporarily reidentified PFAKES.
 */
        RemovePFAKE(s, p);
/*
 *      Calculate location quality metrics
 */
        if (!LocationQuality(s->numPhase, p, &hq)) {
            s->azimgap = hq.FullNetwork.gap;
            s->sgap = hq.FullNetwork.sgap;
            s->mindist = hq.FullNetwork.mindist;
            s->maxdist = hq.FullNetwork.maxdist;
        }
/*
 *      Magnitudes
 */
        mszh = (MSZH *)calloc(s->nreading, sizeof(MSZH));
        stamag = (STAMAG **)calloc(MAXMAG, sizeof(STAMAG *));
        stamag[0] = (STAMAG *)calloc(MAXMAG * s->nreading, sizeof(STAMAG));
        for (i = 1; i < MAXMAG; i++)
            stamag[i] = stamag[i - 1] + s->nreading;
        rdmag = (STAMAG **)calloc(MAXMAG, sizeof(STAMAG *));
        if ((rdmag[0] = (STAMAG *)calloc(MAXMAG * s->nreading,
                                         sizeof(STAMAG))) == NULL) {
            fprintf(logfp, "Cannot allocate memory for station magnitudes!\n");
            fprintf(errfp, "Cannot allocate memory for station magnitudes!\n");
            Free(rdmag);
            Free(stamag[0]);
            Free(stamag);
            Free(mszh);
        }
        else {
            for (i = 1; i < MAXMAG; i++)
                rdmag[i] = rdmag[i - 1] + s->nreading;
            NetworkMagnitudes(s, rdindx, p, stamag, rdmag, mszh, ismbQ, mbQ);
        }
        Free(rdindx);
/*
 *      Print final solution, phases and residuals to log file.
 */
        grn = GregionNumber(s->lat, s->lon, fe);
        Gregion(grn, gregname);
        PrintPhases(s->numPhase, p);
        fprintf(logfp, "final : ");
        PrintSolution(s, grn);
        fprintf(logfp, "    nsta=%d ndefsta=%d nreading=%d nass=%d ",
                s->nsta, s->ndefsta, s->nreading, s->nass);
        fprintf(logfp, "ndef=%d (T=%d A=%d S=%d) nrank=%d\n",
                s->ndef, s->ntimedef, s->nazimdef, s->nslowdef, s->prank);
        fprintf(logfp, "    sgap=%5.1f ", hq.FullNetwork.sgap);
        if (s->smajax != NULLVAL)
            fprintf(logfp, "smajax=%.1f sminax=%.1f strike=%.1f",
                    s->smajax, s->sminax, s->strike);
        if (s->error[0] != NULLVAL)
            fprintf(logfp, " stime=%.3f", s->error[0]);
        if (s->error[3] != NULLVAL)
            fprintf(logfp, " sdepth=%.1f", s->error[3]);
        if (s->sdobs != NULLVAL)
            fprintf(logfp, " sdobs=%.3f", s->sdobs);
        fprintf(logfp, "\n");
        if (s->depdp != NULLVAL)
            fprintf(logfp, "    depdp=%.2f +/- %.2f ndp=%d\n",
                    s->depdp, s->depdp_error, s->ndp);
        if (e->FixedEpicenter)
            fprintf(logfp, "    epicentre fixed to %s epicentre\n",
                    e->EpicenterAgency);
        if (e->FixedOT)
            fprintf(logfp, "    origin time fixed to %s origin time\n",
                    e->OTAgency);
        fprintf(logfp, "    etype=%s\n", e->etype);
        if (s->FixedDepthType == 8)
            fprintf(logfp, "    depth fixed by editor\n");
        else if (s->FixedDepthType == 1)
            fprintf(logfp, "    airquake, depth fixed to surface\n");
        else if (s->FixedDepthType == 2)
            fprintf(logfp, "    depth fixed to %s depth\n", e->DepthAgency);
        else if (s->FixedDepthType == 3)
            fprintf(logfp, "    depth fixed to depth-phase depth\n");
        else if (s->FixedDepthType == 4)
            fprintf(logfp, "    anthropogenic event, depth fixed to surface\n");
        else if (s->FixedDepthType == 5)
            fprintf(logfp, "    depth fixed to default depth grid depth\n");
        else if (s->FixedDepthType == 6) {
            if (!isdefdep) {
                fprintf(logfp, "    no default depth grid point exists, ");
                fprintf(logfp, "depth fixed to median reported depth\n");
            }
            else {
                fprintf(logfp, "    depth fixed to median reported depth\n");
            }
        }
        else if (s->FixedDepthType == 7) {
            fprintf(logfp, "    no default depth grid point exists, ");
            fprintf(logfp, "depth fixed to GRN-dependent depth\n");
        }
        else
            fprintf(logfp, "    free-depth solution\n");
        fprintf(logfp, "    GT5cand=%d (nsta=%d ndef=%d sgap=%5.1f ",
                hq.GT5candidate, hq.LocalNetwork.ndefsta,
                hq.LocalNetwork.ndef, hq.LocalNetwork.sgap);
        fprintf(logfp, "dU=%5.3f numStaWithin10km=%d)\n",
                hq.LocalNetwork.du, hq.numStaWithin10km);
        for (i = 0; i < MAXMAG; i++) {
            if (s->mag[i].numDefiningMagnitude < MinNetmagSta) continue;
            fprintf(logfp, "    %s=%.2f +/- %.2f nsta=%d\n",
                    s->mag[i].magtype, s->mag[i].magnitude,
                    s->mag[i].uncertainty, s->mag[i].numDefiningMagnitude);
        }
/*
 *      Update counters
 */
        opt[option]++;
        *total += 1;
/*
 *      Write solution to database
 *      Supported database schemas:
 *          ISC PostgreSQL
 *          IDC Oracle
 *          IDC NIAB PostgreSQL
 *          SeisComp3 NIAB PostgreSQL
 *          SeisComp3 MySQL
 *
 */
#ifdef DB
        if (UpdateDB && db) {
#ifdef PGSQL
#ifdef ISCDB
/*
 *          Write solution to ISC PostgreSQL database if required.
 */
            if (db == 1) {
                res_set = PQexec(conn, "begin");
                PQclear(res_set);
                if (verbose)
                    fprintf(logfp, "        WriteEventToISCPgsqlDatabase\n");
                if (WriteEventToISCPgsqlDatabase(e, s, p, &hq, stamag, rdmag,
                                            mszh, fe)) {
                    fprintf(errfp, "Locator: could not update event %d!\n", e->evid);
                    fprintf(logfp, "Locator: could not update event %d!\n", e->evid);
                    res_set = PQexec(conn, "rollback");
                    PQclear(res_set);
                }
                else {
                    res_set = PQexec(conn, "commit");
                    PQclear(res_set);
                }
            }
#endif /* ISC */
#ifdef SC3DB
/*
 *          Write solution to SeisComp3 NIAB PostgreSQL database if required.
 */
            if (db == 2) {
                res_set = PQexec(conn, "begin");
                PQclear(res_set);
                if (verbose)
                    fprintf(logfp, "        WriteEventToSC3NIABDatabase\n");
                if (WriteEventToSC3NIABDatabase(e, s, p, hq.GT5candidate,
                                            stamag, rdmag, gregname)) {
                    fprintf(errfp, "Locator: could not update event %d!\n", e->evid);
                    fprintf(logfp, "Locator: could not update event %d!\n", e->evid);
                    res_set = PQexec(conn, "rollback");
                    PQclear(res_set);
                }
                else {
                    res_set = PQexec(conn, "commit");
                    PQclear(res_set);
                }
            }
#endif /* SC3 NIAB */
#ifdef IDCDB
/*
 *          Write solution to IDC NIAB PostgreSQL database if required.
 */
            if (db == 3) {
                res_set = PQexec(conn, "begin");
                PQclear(res_set);
                if (verbose)
                    fprintf(logfp, "        WriteEventToIDCNIABDatabase\n");
                if (WriteEventToIDCNIABDatabase(e, s, p, stamag, fe)) {
                    fprintf(errfp, "Locator: could not update event %d!\n", e->evid);
                    fprintf(logfp, "Locator: could not update event %d!\n", e->evid);
                    res_set = PQexec(conn, "rollback");
                    PQclear(res_set);
                }
                else {
                    res_set = PQexec(conn, "commit");
                    PQclear(res_set);
                }
            }
#endif /* IDC NIAB */
#endif /* PGSQL */
#ifdef MYSQLDB
#ifdef SC3DB
/*
 *          Write solution to SeisComp3 MySQL database if required.
 */
            if (db == 4) {
                mysql_query(mysql, "start transaction");
                if (verbose)
                    fprintf(logfp, "        WriteEventToSC3MysqlDatabase\n");
                if (WriteEventToSC3MysqlDatabase(e, s, p, hq.GT5candidate,
                                            stamag, rdmag, gregname)) {
                    fprintf(errfp, "Locator: could not update event %d!\n", e->evid);
                    fprintf(logfp, "Locator: could not update event %d!\n", e->evid);
                    mysql_query(mysql, "rollback");
                }
                else
                    mysql_query(mysql, "commit");
            }
#endif /* SC3 MYSQL */
#endif /* MYSQLDB */
#ifdef ORASQL
#ifdef IDCDB
/*
 *          Write solution to IDC Oracle database if required.
 */
            if (db == 5) {
                if (dpiConn_prepareStmt(conn, 0, query, strlen(query), NULL, 0, &stmt) < 0)
                     return PrintOraError(stmt, "Locator: prepareStmt");
                if (dpiStmt_execute(stmt, 0, NULL) < 0)
                    return PrintOraError(stmt, "Locator: execute");
                dpiStmt_release(stmt);
                if (verbose)
                    fprintf(logfp, "        WriteEventToIDCOraDatabase\n");
                if (WriteEventToIDCOraDatabase(e, s, p, stamag, fe)) {
                    fprintf(errfp, "Locator: could not update event %d!\n", e->evid);
                    fprintf(logfp, "Locator: could not update event %d!\n", e->evid);
                    if (dpiConn_rollback(conn) < 0)
                         return PrintOraError(stmt, "Locator: rollback;");
                }
                else
                    dpiConn_commit(conn);
            }
#endif /* IDC */
#endif /* ORASQL */
            fprintf(logfp, "Locator: updated event %d (%.2f)\n", e->evid, secs(&t0));
        }
        else {
            if (UpdateDB) {
                fprintf(errfp, "Locator: no database connection!\n");
                fprintf(logfp, "Locator: no database connection!\n");
            }
        }
#endif /* DB */
/*
 *      Write event to ISF2 file if required.
 */
        if (ISFOutputFile[0]) {
            i = s->hypid;
            if (i == 0) s->hypid = *total;
            if (*total == 1) {
/*
 *              Write event header
 */
                fprintf(isfout, "BEGIN IMS2.0\n");
                fprintf(isfout, "DATA_TYPE BULLETIN IMS1.0:short with ISF2.1 extensions\n");
            }
            WriteISF(isfout, e, s, h, p, stamag, rdmag, grn, magbloc);
            s->hypid = i;
        }
        Free(rdmag[0]);
        Free(rdmag);
        Free(stamag[0]);
        Free(stamag);
        Free(mszh);
    }
/*
 *
 *  Non-convergence: roll back to previous preferred origin
 *
 */
    else {
        *fail += 1;
/*
 *      no need to do anything if data read from ISF file
 */
        if (isf) {
            Free(rdindx);
            fprintf(logfp, "Finished event %d (%.2f)\n", e->evid, secs(&t0));
            return 1;
        }
/*
 *      Get rid of any starting point from instructions.
 */
        e->StartDepth = NULLVAL;
        e->StartLat   = NULLVAL;
        e->StartLon   = NULLVAL;
        e->StartOT    = NULLVAL;
/*
 *      Check if InAgency solution is the previous preferred origin
 */
        if (streq(h[0].agency, InAgency)) {
            if (e->numHypo > 1) {
/*
 *              revert to previous preferred origin
 */
                fprintf(logfp, "Set preferred origin to %s\n", h[1].agency);
                swap(h[1], h[0]);
            }
            else {
/*
 *              ISC-specific: ISC is the only hypocenter; generate warning
 */
                fprintf(logfp, "WARNING: Search event %d could not ", e->evid);
                fprintf(logfp, "converge and should be banished\n");
                fprintf(logfp, "FAILURE\n");
                Free(rdindx);
                return 1;
            }
        }
/*
 *      Reset solution to previous preferred origin and calculate residuals
 */
        fprintf(logfp, "Calculate residuals w.r.t. previous preferred origin\n");
        ResidualsForFixedHypocenter(e, &h[0], s, rdindx, p, ec,
                                    TTtables, LocalTTtables, topo);
        PrintPhases(s->numPhase, p);
        Free(rdindx);
        fprintf(logfp, "FAILURE\n");
/*
 *      update DB
 */
#ifdef PGSQL
#ifdef ISCDB
/*
 *
 *      ISC-specific: Restore previous prime in ISC PostgreSQL database
 *
 */
        if (UpdateDB && db == 1) {
            res_set = PQexec(conn, "BEGIN");
            PQclear(res_set);
            if (verbose)
                fprintf(logfp, "    Locator: prime: %d new: %d evid: %d\n",
                                e->PreferredOrid, h[0].hypid, e->evid);
/*
 *          Remove ISC hypocentre
 */
            if (e->ISChypid) {
                fprintf(logfp, "Removing ISC hypocentre\n");
                RemoveISCHypocenter(e);
            }
/*
 *          change prime if necessary
 */
            if (e->PreferredOrid != h[0].hypid) {
                if (verbose) fprintf(logfp, "    ReplaceISCPrime\n");
                ReplaceISCPrime(e, &h[0]);
            }
            if (verbose) fprintf(logfp, "    ReplaceISCAssociation\n");
            ReplaceISCAssociation(e, p, &h[0]);
            res_set = PQexec(conn, "COMMIT");
            PQclear(res_set);
        }
        else {
            if (UpdateDB) {
                fprintf(errfp, "Locator: no database connection!\n");
                fprintf(logfp, "Locator: no database connection!\n");
            }
        }
#endif /* ISC */
#endif /* PGSQL */
        return 1;
    }
    fprintf(logfp, "Finished event %d (%.2f)\n", e->evid, secs(&t0));
    return 0;
}

/*
 *  Title:
 *     Synthetic
 *  Synopsis:
 *     Calculate residuals w.r.t. a hypocentre.
 *     Sets preferred hypocentre in DB if necessary.
 *  Input Arguments:
 *     ep        - pointer to event info
 *     hp        - pointer to hypocentre
 *     sp        - pointer to current solution
 *     rdindx    - array of reading structures
 *     p         - array of phase structures
 *     ec        - ellipticity correction coefs
 *     TTtables  - pointer to travel-time tables
 *     LocalTTtables - pointer to local travel-time tables
 *     topo      - ETOPO bathymetry/elevation matrix
 *     db        - database connection exists?
 *     isf       - ISF text file input?
 *  Called by:
 *     Locator
 *  Calls:
 *     ResidualsForFixedHypocenter, ReplaceISCPrime, ReplaceISCAssociation,
 *     PrintPhases
 */
void Synthetic(EVREC *ep, HYPREC *hp, SOLREC *sp, READING *rdindx, PHAREC p[],
               EC_COEF *ec, TT_TABLE *TTtables, TT_TABLE *LocalTTtables[],
               short int **topo, int db, int isf)
{
/*
 *  Set solution to favourite hypocenter and calculate residuals
 */
    fprintf(logfp, "Calculating residuals for a fixed hypocentre\n");
    ResidualsForFixedHypocenter(ep, hp, sp, rdindx, p, ec,
                                TTtables, LocalTTtables, topo);
    if (isf) return;
/*
 *  Change association rows in database.
 */
#ifdef PGSQL
#ifdef ISCDB
    if (UpdateDB && db == 1) {
/*
 *      change prime if necessary
 */
        if (ep->PreferredOrid != hp->hypid) {
            if (verbose) fprintf(logfp, "    ReplaceISCPrime\n");
            ReplaceISCPrime(ep, hp);
        }
        if (verbose) fprintf(logfp, "    ReplaceISCAssociation\n");
        ReplaceISCAssociation(ep, p, hp);
    }
    else {
        if (UpdateDB) {
            fprintf(errfp, "Synthetic: no database connection!\n");
            fprintf(logfp, "Synthetic: no database connection!\n");
        }
    }
#endif
#endif /* PGSQL */
}


/*
 *  Title:
 *     ResidualsForFixedHypocenter
 *  Synopsis:
 *     Calculate residuals w.r.t. a fixed hypocentre.
 *  Input Arguments:
 *     ep        - pointer to event info
 *     hp        - pointer to hypocentre
 *     sp        - pointer to current solution
 *     rdindx    - array of reading structures
 *     p[]       - array of phase structures
 *     ec        - ellipticity correction coefs
 *     TTtables - pointer to travel-time tables
 *     LocalTTtables - pointer to local travel-time tables
 *     topo      - ETOPO bathymetry/elevation matrix
 *  Called by:
 *     Locator, Synthetic
 *  Calls:
 *     InitialSolution, GetDeltaAzimuth, IdentifyPhases, IdentifyPFAKE,
 *     TravelTimeResiduals, RemovePFAKE
 */
void ResidualsForFixedHypocenter(EVREC *ep, HYPREC *hp, SOLREC *sp,
        READING *rdindx, PHAREC p[], EC_COEF *ec, TT_TABLE *TTtables,
        TT_TABLE *LocalTTtables[], short int **topo)
{
    TT_TABLE *LocalTTtable = *LocalTTtables;
    int is2nderiv = 0;
    double epidist, x, y;
/*
 *  Set solution to fixed hypo
 */
    if (verbose) fprintf(logfp, "    InitialSolution\n");
    if (hp->depth == NULLVAL || hp->depth < 0.)
        hp->depth = DefaultDepth;
    InitialSolution(sp, ep, hp);
    if (UpdateLocalTT) {
        epidist = DistAzimuth(PrevLat, PrevLon, sp->lat, sp->lon, &x, &y);
        epidist *= DEG2KM;
        PrevLat = sp->lat;
        PrevLon = sp->lon;
        if (epidist > EPIWALK) {
/*
 *      generate local TT tables if necessary
 */
            fprintf(logfp, "Generate local TT tables...\n");
            UseLocalTT = 1;
            if (LocalTTtable != NULL)
                FreeLocalTTtables(LocalTTtable);
            if ((LocalTTtable = GenerateLocalTTtables(LocalVmodelFile,
                                                  sp->lat, sp->lon)) == NULL) {
                fprintf(logfp, "Cannot generate local TT tables!\n");
                UseLocalTT = 0;
            }
            *LocalTTtables = LocalTTtable;
        }
    }
/*
 *  Set delta and seaz for each phase
 */
    if (verbose) fprintf(logfp, "    GetDeltaAzimuth\n");
    GetDeltaAzimuth(sp, p, 1);
/*
 *  Identify phases
 */
    if (verbose) fprintf(logfp, "    IdentifyPhases\n");
    IdentifyPhases(sp, rdindx, p, ec, TTtables, LocalTTtable, topo, &is2nderiv);
/*
 *  Temporarily reidentify PFAKES so that they get residuals
 */
    if (verbose) fprintf(logfp, "    IdentifyPFAKE\n");
    IdentifyPFAKE(sp, p, ec, TTtables, LocalTTtable, topo);
/*
 *  Calculate residuals (no need for dtdd and dtdh)
 */
    if (verbose) fprintf(logfp, "    TravelTimeResiduals\n");
    TravelTimeResiduals(sp, p, "all", ec, TTtables, LocalTTtable, topo, 0, 0);
/*
 *  Remove ISC phase codes from temporarily reidentified PFAKES
 */
    if (verbose) fprintf(logfp, "    RemovePFAKE\n");
    RemovePFAKE(sp, p);
}

/*
 *  Title:
 *     LocateEvent
 *  Synopsis:
 *     Iterative linearised least-squares inversion of travel-times to
 *         obtain a solution for the hypocentre.
 *     Bondár, I., and K. McLaughlin, 2009,
 *        Seismic location bias and uncertainty in the presence of correlated
 *        and non-Gaussian travel-time errors,
 *        Bull. Seism. Soc. Am., 99, 172-193.
 *     Bondár, I., and D. Storchak, 2011,
 *        Improved location procedures at the International Seismological
 *        Centre,
 *        Geophys. J. Int., doi: 10.1111/j.1365-246X.2011.05107.x.
 *
 *     If DoCorrelatedErrors is true, it projects Gm = d into the
 *         eigensystem defined by the full data covariance matrix,
 *         i.e. in the system where the full data covariance matrix
 *         becomes diagonal
 *     otherwise assumes independent errors and weights Gm = d with
 *         the a priori estimates of measurement error variances.
 *     WGm = Wd is solved with singular value decomposition.
 *     Damping is applied if condition number is large.
 *     Convergence test is based on the Paige-Saunder convergence test value
 *         and the history of model and data norms.
 *     Formal uncertainties (model covariance matrix) are scaled
 *         to <ConfidenceLevel>% confidence level.
 *     Free-depth solutions:
 *         Depth is fixed in the first MinIterations-1 iterations.
 *         Depth remains fixed if number of airquakes/deepquakes exceeds 2;
 *             depthfixtype is set to 'B'.
 *         Phases are reidentified if depth crosses Moho or Conrad
 *             discontinuities.
 *     Correlated errors:
 *         The data covariance and projection matrices are calculated once.
 *         The data covariance matrix is recalculated if defining phases were
 *             renamed during an iteration.
 *         The projection matrix is recalculated if defining phases were
 *            renamed or defining phases were made non-defining during an
 *            iteration.
 *  Input arguments:
 *     option       - locator option
 *                    option 0 = free depth
 *                    option 1 = depth fixed to default regional depth
 *                    option 2 = fixed depth by analyst
 *                    option 3 = depth fixed to median depth
 *                    option 4 = fixed location (lat, lon)
 *                    option 5 = fixed depth and location
 *     nsta         - number of distinct stations
 *     has_depdpres - do we have depth resolution by depth phases?
 *     sp           - pointer to current solution
 *     rdindx       - array of reading structures
 *     p            - array of phase structures
 *     ec           - pointer to ellipticity correction coefficient structure
 *     TTtables     - pointer to travel-time tables
 *     LocalTTtables - pointer to local travel-time tables
 *     stalist      - array of starec structures
 *     distmatrix   - station separation matrix
 *     variogramp   - pointer to generic variogram model
 *     staorder     - array of staorder structures (nearest-neighbour order)
 *     gres         - grid spacing in default depth grid
 *     ngrid        - number of grid points in default depth grid
 *     DepthGrid    - pointer to default depth grid (lat, lon, depth)
 *     topo         - ETOPO bathymetry/elevation matrix
 *  Output arguments:
 *     sp           - pointer to current solution.
 *     p            - array of phase structures
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     Locator
 *  Calls:
 *     GetNdef, SortPhasesForNA, DepthPhaseCheck, PrintPhases,
 *     GetDeltaAzimuth, ReIdentifyPhases, DuplicatePhases, GetResiduals,
 *     AllocateFloatMatrix, GetDataCovarianceMatrix, ProjectionMatrix,
 *     FreeFloatMatrix, BuildGd, ProjectGd, WeightGd, SVDdecompose,
 *     SVDthreshold, SVDrank, SVDnorm, SVDsolve, ConvergenceTestValue,
 *     ConvergenceTest, PointAtDeltaAzimuth, PrintSolution, PrintDefiningPhases,
 *     SortPhasesFromDatabase, SVDModelCovarianceMatrix, Uncertainties
 */
int LocateEvent(int option, int nsta, int has_depdpres, SOLREC *sp,
        READING *rdindx, PHAREC p[], EC_COEF *ec, TT_TABLE *TTtables,
        TT_TABLE *LocalTTtable, STAREC stalist[], double **distmatrix,
        VARIOGRAM *variogramp, STAORDER staorder[], short int **topo,
        int is2nderiv)
{
    int i, j, k, m, iter, iserr = 0, nds[3], isconv = 0, isdiv = 0;
    int iszderiv = 0, fixdepthfornow = 0, nairquakes = 0, ndeepquakes = 0;
    int prank = 0, dpok = 0, ndef = 0, nd = 0, nr = 0, nunp = 0;
    int ischanged = 0, ispchange = 0, ntimedef = 0;
    double **g = (double **)NULL;
    double *d = (double *)NULL;
    double *sv = (double *)NULL;
    double svundamped[4];
    double **v = (double **)NULL;
    double **dcov = (double **)NULL;
    double **w = (double **)NULL;
    double mcov[4][4], sol[4], oldsol[4], modelnorm[3], convgtest[3];
    double toffset = 0., torg = 0., delta = 0., azim = 0.;
    double svth = 0., damp = 0., dmax = 0., step = 0., prev_depth = 0.;
    double urms = 0., wrms = 0., scale = 0.;
    double gtd = 0., gtdnorm = 0., dnorm = 0., gnorm = 0., mnorm = 0.;
    double cnvgtst = 0., oldcvgtst = 0., cond = 0.;
    char *phundef[MAXNUMPHA], phbuf[MAXNUMPHA * PHALEN];
    for (i = 0; i < MAXNUMPHA; i++) phundef[i] = phbuf + i * PHALEN;
    dpok = has_depdpres;
    prev_depth = sp->depth;
/*
 *  find number of defining observations and earliest arrival time
 */
    nd = GetNdef(sp->numPhase, p, nsta, stalist, &toffset);
    prank = nd;
    if (nd < 0) return 1;
    if (nd <= sp->numUnknowns) {
        if (verbose)
            fprintf(logfp, "LocateEvent: insufficient number of phases (%d)!\n",
                    nd);
        errorcode = 5;
        return 1;
    }
/*
 *  reduced origin time
 */
    torg = sp->time - toffset;
/*
 *  initializations
 */
    for (i = 0; i < 4; i++) {
        sol[i] = 0.;
        oldsol[i] = 0.;
    }
    i = 0;
    if (!sp->timfix)
        sol[i++] = torg;
    if (!sp->epifix) {
        sol[i++] = sp->lon;
        sol[i++] = sp->lat;
    }
    if (!sp->depfix)
        sol[i] = sp->depth;
    nairquakes = ndeepquakes = 0;
    ischanged = ispchange = isconv = isdiv = 0;
    iserr = 1;
    step = oldcvgtst = 1.;
    for (i = 0; i < 4; i++) {
        sp->error[i] = sp->covar[i][i] = NULLVAL;
        for (j = i + 1; j < 4; j++)
            sp->covar[i][j] = sp->covar[j][i] = NULLVAL;
    }
/*
 *  convergence history
 */
    for (j = 0; j < 3; j++) {
        modelnorm[j] = 0.;
        convgtest[j] = 0.;
        nds[j] = 0;
    }
    nds[0] = nd;
/*
 *  reorder phaserecs by staorder, rdid, time so that covariance matrices
 *  for various phases will become block-diagonal
 */
    if (DoCorrelatedErrors) {
        SortPhasesForNA(sp->numPhase, nsta, p, stalist, staorder);
        Readings(sp->numPhase, sp->nreading, p, rdindx);
        dpok = DepthPhaseCheck(sp, rdindx, p, 0);
        if (verbose > 3)
            PrintPhases(sp->numPhase, p);
    }
/*
 *
 * iteration loop
 *
 */
    if (verbose)
        fprintf(logfp, "Start iteration loop (%.4f)\n", secs(&t0));
    for (iter = 0; iter < MaxIterations; iter++) {
        if (verbose) fprintf(logfp, "iteration = %d\n", iter);
/*
 *      number of model parameters
 */
        m = sp->numUnknowns;
        iszderiv = 1;
/*
 *      check if necessary to fix depth if free depth solution
 */
        if (option == 0 || option == 4) {
            fixdepthfornow = 0;
/*
 *          fix depth for the first MinIterations - 1 iterations
 */
            if (iter < MinIterations - 1) {
                fixdepthfornow = 1;
                m--;
            }
/*
 *          do not allow airquakes
 */
            else if (sp->depth < 0.) {
                if (verbose)
                    fprintf(logfp, "    airquake, fixing depth to 0\n");
                nairquakes++;
                sp->depth = 0.;
                fixdepthfornow = 1;
                m--;
            }
/*
 *          do not allow deepquakes
 */
            else if (sp->depth > MaxHypocenterDepth) {
                if (verbose)
                    fprintf(logfp, "    deepquake, fixing depth to max depth\n");
                ndeepquakes++;
                sp->depth = MaxHypocenterDepth;
                fixdepthfornow = 1;
                m--;
            }
        }
/*
 *      various fix depth instructions
 */
        else
            fixdepthfornow = 1;
/*
 *      enough of airquakes!
 */
        if (nairquakes > 2 || ndeepquakes > 2) {
            fixdepthfornow = 1;
            m = sp->numUnknowns - 1;
            sp->FixedDepthType = 1;
        }
/*
 *      no need for z derivatives when depth is fixed
 */
        if (fixdepthfornow)
            iszderiv = 0;
/*
 *      set delta, esaz and seaz for each phase
 */
        GetDeltaAzimuth(sp, p, 0);
/*
 *      Reidentify phases iff depth crosses Moho or Conrad
 */
        if ((sp->depth > Moho && prev_depth <= Moho) ||
            (sp->depth < Moho && prev_depth >= Moho) ||
            (sp->depth > Conrad && prev_depth <= Conrad) ||
            (sp->depth < Conrad && prev_depth >= Conrad)) {
            if (verbose) {
                fprintf(logfp, "    depth: %.2f prev_depth: %.2f; ",
                        sp->depth, prev_depth);
                fprintf(logfp, "reidentifying phases\n");
            }
            ispchange = ReIdentifyPhases(sp, rdindx, p, ec, TTtables,
                                         LocalTTtable, topo, is2nderiv);
            DuplicatePhases(sp, p, ec, TTtables, LocalTTtable, topo);
        }
/*
 *      get residuals w.r.t. current solution
 */
        if (GetResiduals(sp, rdindx, p, ec, TTtables, LocalTTtable, topo,
                         iszderiv, &dpok, &ndef, &ischanged, iter, ispchange,
                         nd, &nunp, phundef, dcov, w, is2nderiv))
            break;
        if (ndef <= sp->numUnknowns) {
            if (verbose) {
                fprintf(logfp, "Insufficient number (%d) of phases left; ",
                        ndef);
            }
            errorcode = 7;
            break;
        }
        for (j = 2; j > 0; j--) nds[j] = nds[j-1];
        nds[0] = ndef;
/*
 *      initial memory allocations
 */
        if (iter == 0) {
            nd = ndef;
            prank = ndef;
            g = AllocateFloatMatrix(nd, 4);
            v = AllocateFloatMatrix(4, 4);
            d = (double *)calloc(nd, sizeof(double));
            if ((sv = (double *)calloc(4, sizeof(double))) == NULL) {
                fprintf(logfp, "LocateEvent: cannot allocate memory!\n");
                fprintf(errfp, "LocateEvent: cannot allocate memory!\n");
                errorcode = 1;
                break;
            }
/*
 *          account for correlated error structure
 */
            if (DoCorrelatedErrors) {
/*
 *              construct data covariance matrix
 */
                if ((dcov = GetDataCovarianceMatrix(nsta, sp->numPhase, nd, p,
                                 stalist, distmatrix, variogramp)) == NULL)
                    break;
/*
 *              projection matrix
 */
                if ((w = AllocateFloatMatrix(nd, nd)) == NULL) break;
                if (ProjectionMatrix(sp->numPhase, p, nd, 95., dcov, w,
                                     &prank, nunp, phundef, 1))
                    break;
            }
        }
/*
 *      rest of the iterations:
 *          check if set of time-defining phases or their names were changed
 */
        else if (ispchange || ndef > nd) {
/*
 *          change in defining phase names or increase in defining phases:
 *              reallocate memory for G and d
 */
            if (nd != ndef) {
                FreeFloatMatrix(g);
                Free(d);
                isconv = 0;
                nd = ndef;
                g = AllocateFloatMatrix(nd, 4);
                if ((d = (double *)calloc(nd, sizeof(double))) == NULL)
                    break;
            }
            if (DoCorrelatedErrors) {
/*
 *              recalculate the data covariance and projection matrices
 */
                if (verbose) {
                    fprintf(logfp, "    Changes in defining phasenames, ");
                    fprintf(logfp, "recalculating projection matrix\n");
                }
                FreeFloatMatrix(dcov);
                FreeFloatMatrix(w);
                if ((dcov = GetDataCovarianceMatrix(nsta, sp->numPhase, nd, p,
                                 stalist, distmatrix, variogramp)) == NULL)
                    break;
/*
 *              projection matrix
 */
                if ((w = AllocateFloatMatrix(nd, nd)) == NULL) break;
                if (ProjectionMatrix(sp->numPhase, p, nd, 95., dcov, w,
                                     &prank, nunp, phundef, 1))
                    break;
            }
            else
                prank = ndef;
        }
        else if (ischanged) {
/*
 *          reduced number of defining phases:
 *              recalculate the projection matrix for phases that were changed
 */
            isconv = 0;
            nd = ndef;
            if (DoCorrelatedErrors) {
                if (verbose) {
                    fprintf(logfp, "    Changes in defining phasenames, ");
                    fprintf(logfp, "recalculating projection matrix\n");
                }
/*
 *              readjust row index pointers to covariance matrix
 */
                for (k = 0, i = 0; i < sp->numPhase; i++) {
                    if (p[i].timedef) {
                        p[i].CovIndTime = k;
                        k++;
                   }
                }
                for (i = 0; i < sp->numPhase; i++) {
                    if (p[i].azimdef) {
                        p[i].CovIndAzim = k;
                        k++;
                    }
                }
                for (i = 0; i < sp->numPhase; i++) {
                    if (p[i].slowdef) {
                        p[i].CovIndSlow = k;
                        k++;
                    }
                }
                if (ProjectionMatrix(sp->numPhase, p, nd, 95., dcov, w,
                                     &prank, nunp, phundef, ispchange))
                    break;
            }
            else
                prank = ndef;
        }
        if (prank < sp->numUnknowns) {
            fprintf(logfp, "Insufficient number of independent phases (%d, %d)!\n",
                    prank, sp->numUnknowns);
            errorcode = 6;
            break;
        }
/*
 *      build G matrix and d vector
 */
        urms = BuildGd(nd, sp, p, fixdepthfornow, g, d);
        if (DoCorrelatedErrors) {
/*
 *          project Gm = d into eigensystem
 */
            if (ProjectGd(nd, m, g, d, w, &dnorm, &wrms))
                break;
        }
        else {
/*
 *          independent observations: weight Gm = d by measurement errors
 */
            WeightGd(nd, m, sp->numPhase, p, g, d, &dnorm, &wrms);
        }
/*
 *      finish if convergent or divergent solution
 *          for the last iteration we only need urms and wrms
 */
        if (isconv || isdiv)
            break;
/*
 *      transpose(G) * d matrix norm
 */
        gtdnorm = 0.;
        for (i = 0; i < nd; i++) {
            gtd = 0.0;
            for (j = 0; j < m; j++)
                gtd += g[i][j] * d[i];
            gtdnorm += gtd * gtd;
        }
/*
 *      SVD of G (G is overwritten by U matrix!)
 */
        if (SVDdecompose(nd, m, g, sv, v))
            break;
        for (j = 0; j < m; j++) svundamped[j] = sv[j];
/*
 *      condition number, G matrix norm, rank and convergence test value
 */
        svth = SVDthreshold(nd, m, sv);
        nr = SVDrank(nd, m, sv, svth);
        gnorm = SVDnorm(m, sv, svth, &cond);
        if (nr < m) {
            fprintf(logfp, "Singular G matrix (%d < %d)!\n", nr, m);
            fprintf(errfp, "Singular G matrix (%d < %d)!\n", nr, m);
            errorcode = 9;
            break;
        }
        if (cond > 30000.) {
            fprintf(logfp, "Abnormally ill-conditioned problem (cond=%.0f)!\n",
                    cond);
            fprintf(errfp, "Abnormally ill-conditioned problem (cond=%.0f)!\n",
                    cond);
            errorcode = 10;
            break;
        }
        cnvgtst = ConvergenceTestValue(gtdnorm, gnorm, dnorm);
/*
 *      If damping is enabled, apply damping if condition number is large.
 *      Apply of 1% largest singular value for moderately ill-conditioned,
 *               5% for more severely ill-conditioned and
 *              10% for highly ill-conditioned problems.
 */
        if (AllowDamping && cond > 30.) {
            damp = 0.01;
            if (cond > 300.)  damp = 0.05;
            if (cond > 3000.) damp = 0.1;
            for (j = 1; j < nr; j++)
                sv[j] += sv[0] * damp;
            if (verbose) {
                fprintf(logfp, "    Large condition number (%.3f): ", cond);
                fprintf(logfp, "%.0f%% damping is applied.\n", 100. * damp);
            }
        }
/*
 *      solve Gm = d
 */
        if (SVDsolve(nd, m, g, sv, v, d, sol, svth))
            break;
/*
 *      model norm
 */
        for (mnorm = 0., j = 0; j < m; j++)
            mnorm += sol[j] * sol[j];
        mnorm = Sqrt(mnorm);
/*
 *      scale down hypocenter perturbations if they are very large
 */
        dmax = 1000.;
        if (mnorm > dmax) {
            scale = dmax / mnorm;
            for (j = 0; j < m; j++)
                sol[j] *= scale;
            mnorm = dmax;
            if (verbose) {
                fprintf(logfp, "    Large perturbation: ");
                fprintf(logfp, "%.g scaling is applied.\n", scale);
            }
        }
/*
 *      convergence test
 */
        for (j = 2; j > 0; j--) {
            modelnorm[j] = modelnorm[j-1];
            convgtest[j] = convgtest[j-1];
        }
        modelnorm[0] = mnorm;
        convgtest[0] = cnvgtst;
        if (iter > MinIterations - 1)
            isconv = ConvergenceTest(iter, m, nds, sol, oldsol, wrms,
                            modelnorm, convgtest, &oldcvgtst, &step, &isdiv);
/*
 *      update hypocentre coordinates
 */
        prev_depth = sp->depth;
        if (verbose) {
            fprintf(logfp, "    iteration = %d: ", iter);
            if     (isconv) fprintf(logfp, "    converged!\n");
            else if (isdiv) fprintf(logfp, "    diverged!\n");
            else            fprintf(logfp, "\n");
            fprintf(logfp, "    ||Gt*d|| = %.5f ||G|| = %.5f ", gtdnorm, gnorm);
            fprintf(logfp, "||d|| = %.5f ||m|| = %.5f\n", dnorm, mnorm);
            fprintf(logfp, "    convgtst = %.5f condition number = %.3f\n",
                    cnvgtst, cond);
            fprintf(logfp, "    eigenvalues: ");
            for (i = 0; i < m; i++) fprintf(logfp, "%g ", sv[i]);
            fprintf(logfp, "\n    unweighted RMS residual = %8.4f\n", urms);
            fprintf(logfp, "      weighted RMS residual = %8.4f\n", wrms);
            fprintf(logfp, "    ndef = %d rank = %d m = %d ischanged = %d\n",
                    nd, prank, m, ischanged);
        }
        i = 0;
        if (verbose) fprintf(logfp, "    ");
        if (!sp->timfix) {
            if (verbose) fprintf(logfp, "dOT = %g ", sol[i]);
            torg += sol[i++];
            sp->time = torg + toffset;
        }
        if (!sp->epifix) {
            azim = RAD_TO_DEG * atan2(sol[i], sol[i+1]);
            delta = Sqrt(sol[i] * sol[i] + sol[i+1] * sol[i+1]);
            delta = RAD_TO_DEG * (delta / (EARTH_RADIUS - sp->depth));
            PointAtDeltaAzimuth(sp->lat, sp->lon, delta, azim, &sp->lat, &sp->lon);
            if (verbose) fprintf(logfp, "dx = %g dy = %g ", sol[i], sol[i+1]);
            i += 2;
        }
        if (!fixdepthfornow) {
            if (verbose) fprintf(logfp, "dz = %g ", -sol[i]);
            sp->depth -= sol[i];
        }
        if (verbose) {
            fprintf(logfp, "\n");
            PrintSolution(sp, 0);
        }
        if (verbose > 1)
            PrintDefiningPhases(sp->numPhase, p);
        if (verbose)
            fprintf(logfp, "iteration = %d (%.4f) done\n", iter, secs(&t0));
    }
/*
 *
 *  end of iteration loop
 *
 */
    if (verbose)
        fprintf(logfp, "    end of iteration loop (%.4f)\n", secs(&t0));
/*
 *  Sort phase structures so that they ordered by delta, prista, rdid, time
 */
    SortPhasesFromDatabase(sp->numPhase, p);
    Readings(sp->numPhase, sp->nreading, p, rdindx);
    if (verbose > 3)
        PrintPhases(sp->numPhase, p);
/*
 *
 *  max number of iterations reached
 *
 */
    if (iter >= MaxIterations) {
        fprintf(logfp, "    maximum number of iterations is reached!\n");
        errorcode = 8;
        isdiv = 1;
    }
/*
 *
 *  convergent solution
 *      calculate model covariance matrix
 *
 */
    else if (isconv) {
        fprintf(logfp, "    convergent solution after %d iterations\n", iter);
        iserr = 0;
        sp->urms = urms;
        sp->wrms = wrms;
        sp->prank = prank;
        sp->ndef = nd;
        if (!sp->FixedDepthType && sp->depth < DEPSILON) {
            fixdepthfornow = 1;
            sp->FixedDepthType = 1;
            sp->depth = 0.;
            m--;
        }
        if (!sp->FixedDepthType && sp->depth > MaxHypocenterDepth - DEPSILON) {
            fixdepthfornow = 1;
            sp->FixedDepthType = 1;
            sp->depth = MaxHypocenterDepth;
            m--;
        }
        sp->numUnknowns = m;
        sp->depfix = fixdepthfornow;
        SVDModelCovarianceMatrix(m, svth, svundamped, v, mcov);
        if (!sp->timfix) {
            sp->covar[0][0] = mcov[0][0];                /* stt */
            if (!sp->epifix) {
                sp->covar[0][1] = mcov[0][1];            /* stx */
                sp->covar[0][2] = mcov[0][2];            /* sty */
                sp->covar[1][0] = mcov[1][0];            /* sxt */
                sp->covar[1][1] = mcov[1][1];            /* sxx */
                sp->covar[1][2] = mcov[1][2];            /* sxy */
                sp->covar[2][0] = mcov[2][0];            /* syt */
                sp->covar[2][1] = mcov[2][1];            /* syx */
                sp->covar[2][2] = mcov[2][2];            /* syy */
                if (!fixdepthfornow) {
                    sp->covar[0][3] = mcov[0][3];        /* stz */
                    sp->covar[1][3] = mcov[1][3];        /* sxz */
                    sp->covar[2][3] = mcov[2][3];        /* syz */
                    sp->covar[3][0] = mcov[3][0];        /* szt */
                    sp->covar[3][1] = mcov[3][1];        /* szx */
                    sp->covar[3][2] = mcov[3][2];        /* szy */
                    sp->covar[3][3] = mcov[3][3];        /* szz */
                }
            }
            else {
                if (!fixdepthfornow) {
                    sp->covar[0][3] = mcov[0][1];        /* stz */
                    sp->covar[3][0] = mcov[1][0];        /* szt */
                    sp->covar[3][3] = mcov[1][1];        /* szz */
                }
            }
        }
        else {
            if (!sp->epifix) {
                sp->covar[1][1] = mcov[0][0];            /* sxx */
                sp->covar[1][2] = mcov[0][1];            /* sxy */
                sp->covar[2][1] = mcov[1][0];            /* syx */
                sp->covar[2][2] = mcov[1][1];            /* syy */
                if (!fixdepthfornow) {
                    sp->covar[1][3] = mcov[0][2];        /* sxz */
                    sp->covar[2][3] = mcov[1][2];        /* syz */
                    sp->covar[3][1] = mcov[2][0];        /* szx */
                    sp->covar[3][2] = mcov[2][1];        /* szy */
                    sp->covar[3][3] = mcov[2][2];        /* szz */
                }
            }
            else {
                if (!fixdepthfornow)
                    sp->covar[3][3] = mcov[0][0];        /* szz */
            }
        }
/*
 *      location uncertainties
 */
        Uncertainties(sp, p);
    }
/*
 *
 *  divergent solution
 *
 */
    else if (isdiv) {
        fprintf(logfp, "    divergent solution\n");
        errorcode = 4;
    }
/*
 *  abnormal exit
 */
    else {
        fprintf(logfp, "    abnormal exit from iteration loop!\n");
        fprintf(errfp, "    abnormal exit from iteration loop!\n");
        isdiv = 1;
    }
    sp->converged = isconv;
    sp->diverging = isdiv;
/*
 *  free memory allocated to various arrays
 */
    if (DoCorrelatedErrors) {
        FreeFloatMatrix(w);
        FreeFloatMatrix(dcov);
    }
    Free(sv);
    Free(d);
    FreeFloatMatrix(v);
    FreeFloatMatrix(g);
    return iserr;
}

/*
 *  Title:
 *     GetNdef
 *  Synopsis:
 *     Finds number of defining phases and the earliest arrival time.
 *  Input Arguments:
 *     numPhase   - number of associated phases
 *     p[]       - array of phase structures
 *     nsta      - number of distinct stations
 *     stalist[] - array of starec structures
 *  Output Arguments:
 *     toffset   - earliest arrival time
 *  Return:
 *     nd - number of defining phases at valid stations or -1 on error
 *  Called by:
 *     LocateEvent
 *  Calls:
 *     GetStationIndex
 */
static int GetNdef(int numPhase, PHAREC p[], int nsta, STAREC stalist[],
        double *toffset)
{
    int i, nd = 0, sind = -1;
    double toff = 1e+32;
    for (i = 0; i < numPhase; i++) {
        if (!p[i].timedef) continue;
        if ((sind = GetStationIndex(nsta, stalist, p[i].prista)) < 0) {
            fprintf(logfp, "GetNdef: %s invalid station!\n", p[i].prista);
            fprintf(errfp, "GetNdef: %s invalid station!\n", p[i].prista);
            errorcode = 11;
            return -1;
        }
        p[i].prevtimedef = p[i].timedef;
        strcpy(p[i].prevphase, p[i].phase);
        if (p[i].time < toff)
            toff = p[i].time;
        nd++;
    }
    for (i = 0; i < numPhase; i++) {
        if (!p[i].azimdef) continue;
        p[i].prevazimdef = p[i].azimdef;
        strcpy(p[i].prevphase, p[i].phase);
        nd++;
    }
    for (i = 0; i < numPhase; i++) {
        if (!p[i].slowdef) continue;
        p[i].prevslowdef = p[i].slowdef;
        strcpy(p[i].prevphase, p[i].phase);
        nd++;
    }
    if (toff < 1e+32) *toffset = toff;
    return nd;
}

/*
 *  Title:
 *     GetPhaseList
 *  Synopsis:
 *     populates PHASELIST for defining phases
 *     PHASELIST contains:
 *          phase name
 *          number of observations for this phase
 *          permutation vector that renders the data covariance matrix
 *              block-diagonal (phase by phase)
 *     returns number of distinct defining phases
 *  Input Arguments:
 *     numPhase - number of associated phases
 *     p[]     - array of phase structures
 *  Output Arguments:
 *     plist - PHASELIST structure
 *  Return:
 *     nphases or 0 on error
 *  Called by:
 *     ProjectionMatrix
 */
int GetPhaseList(int numPhase, PHAREC p[], PHASELIST plist[])
{
    int i, j, k, nphases = 0, isfound = 0;
    for (j = 0; j < MAXTTPHA; j++) {
        strcpy(plist[j].phase, "");
        plist[j].nTime = 0;
        plist[j].nAzim = 0;
        plist[j].nSlow = 0;
        plist[j].indTime = (int *)NULL;
        plist[j].indAzim = (int *)NULL;
        plist[j].indSlow = (int *)NULL;
    }
/*
 *  get number of defining observations for each phase
 */
    for (i = 0; i < numPhase; i++) {
        if (p[i].timedef) {
/*
 *          find phase in plist
 */
            isfound = 0;
            for (j = 0; j < nphases; j++) {
                if (streq(p[i].phase, plist[j].phase)) {
                    plist[j].nTime++;
                    isfound = 1;
                    break;
                }
            }
/*
 *          new phase; add it to plist
 */
            if (!isfound) {
                strcpy(plist[j].phase, p[i].phase);
                plist[j].nTime = 1;
                nphases++;
            }
        }
    }
    for (i = 0; i < numPhase; i++) {
        if (p[i].azimdef) {
/*
 *          find phase in plist
 */
            isfound = 0;
            for (j = 0; j < nphases; j++) {
                if (streq(p[i].phase, plist[j].phase)) {
                    plist[j].nAzim++;
                    isfound = 1;
                    break;
                }
            }
/*
 *          new phase; add it to plist
 */
            if (!isfound) {
                strcpy(plist[j].phase, p[i].phase);
                plist[j].nAzim = 1;
                nphases++;
            }
        }
    }
    for (i = 0; i < numPhase; i++) {
        if (p[i].slowdef) {
/*
 *          find phase in plist
 */
            isfound = 0;
            for (j = 0; j < nphases; j++) {
                if (streq(p[i].phase, plist[j].phase)) {
                    plist[j].nSlow++;
                    isfound = 1;
                    break;
                }
            }
/*
 *          new phase; add it to plist
 */
            if (!isfound) {
                strcpy(plist[j].phase, p[i].phase);
                plist[j].nSlow = 1;
                nphases++;
            }
        }
    }
/*
 *  allocate memory
 */
    for (j = 0; j < nphases; j++) {
        if (verbose > 3)
           fprintf(logfp, "GetPhaseList: phase=%s nTime=%d nAzim=%d nSlow=%d\n",
                plist[j].phase, plist[j].nTime, plist[j].nAzim, plist[j].nSlow);
        if (plist[j].nSlow)
            plist[j].indSlow = (int *)calloc(plist[j].nSlow, sizeof(int));
        if (plist[j].nAzim) {
            if ((plist[j].indAzim = (int *)calloc(plist[j].nAzim, sizeof(int))) == NULL) {
                FreePhaseList(nphases, plist);
                fprintf(logfp, "GetPhaseList: cannot allocate memory\n");
                fprintf(errfp, "GetPhaseList: cannot allocate memory\n");
                errorcode = 1;
                return 0;
            }
        }
        if (plist[j].nTime) {
            if ((plist[j].indTime = (int *)calloc(plist[j].nTime, sizeof(int))) == NULL) {
                FreePhaseList(nphases, plist);
                fprintf(logfp, "GetPhaseList: cannot allocate memory\n");
                fprintf(errfp, "GetPhaseList: cannot allocate memory\n");
                errorcode = 1;
                return 0;
            }
        }
    }
/*
 *  build permutation vectors
 */
    for (j = 0; j < nphases; j++) {
        for (k = 0, i = 0; i < numPhase; i++) {
            if (p[i].timedef) {
                if (streq(p[i].phase, plist[j].phase)) {
                    plist[j].indTime[k] = p[i].CovIndTime;
                    k++;
                }
            }
        }
        for (k = 0, i = 0; i < numPhase; i++) {
            if (p[i].azimdef) {
                if (streq(p[i].phase, plist[j].phase)) {
                    plist[j].indAzim[k] = p[i].CovIndAzim;
                    k++;
                }
            }
        }
        for (k = 0, i = 0; i < numPhase; i++) {
            if (p[i].slowdef) {
                if (streq(p[i].phase, plist[j].phase)) {
                    plist[j].indSlow[k] = p[i].CovIndSlow;
                    k++;
                }
            }
        }
    }
    return nphases;
}

/*
 *  Title:
 *     FreePhaseList
 *  Synopsis:
 *     frees memory allocated to PHASELIST structure
 *  Input Arguments:
 *     nphases number of distinct defining phases
 *     plist - PHASELIST structure
 *  Called by:
 *     ProjectionMatrix
 */
void FreePhaseList(int nphases, PHASELIST plist[])
{
    int j;
    for (j = 0; j < nphases; j++) {
        strcpy(plist[j].phase, "");
        if (plist[j].nTime) Free(plist[j].indTime);
        if (plist[j].nAzim) Free(plist[j].indAzim);
        if (plist[j].nSlow) Free(plist[j].indSlow);
        plist[j].nTime = plist[j].nAzim = plist[j].nSlow = 0;
    }
}

/*
 *  Title:
 *     GetResiduals
 *  Synopsis:
 *     Sets residuals for defining phases.
 *     Flags first arriving P and remove orphan depth phases
 *     Makes a phase non-defining if its residual is larger than
 *         SigmaThreshold times the prior measurement error and
 *         deletes corresponding row and column in the data covariance and
 *         projection matrices.
 *  Input Arguments:
 *     sp        - pointer to current solution
 *     p[]       - array of phase structures
 *     ec        - pointer to ellipticity correction coefficient structure
 *     TTtables - pointer to travel-time tables
 *     topo      - ETOPO bathymetry/elevation matrix
 *     iszderiv  - calculate dtdh [0/1]?
 *     iter      - iteration number
 *     ispchange - change in phase names?
 *     prevndef  - number of defining phases from previous iteration
 *     dcov      - data covariance matrix from previous iteration
 *     w         - projection matrix from previous iteration
 *  Output Arguments:
 *     has_depdpres - do we have depth-phase depth resolution?
 *     ndef      - number of defining phases
 *     ischanged - change in the set of defining phases? [0/1]
 *     nunp      - number of distinct phases made non-defining
 *     phundef   - list of distinct phases made non-defining
 *     dcov      - data covariance matrix
 *     w         - projection matrix
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     LocateEvent
 *  Calls:
 *     DepthPhaseCheck, TravelTimeResiduals
 */
static int GetResiduals(SOLREC *sp, READING *rdindx, PHAREC p[], EC_COEF *ec,
        TT_TABLE *TTtables, TT_TABLE *LocalTTtable, short int **topo,
        int iszderiv, int *has_depdpres, int *ndef, int *ischanged, int iter,
        int ispchange, int prevndef, int *nunp, char **phundef, double **dcov,
        double **w, int is2nderiv)
{
    int i, j, k = 0, m = 0, kp = 0, nd = 0, nund = 0, isdiff = 0, isfound = 0;
    extern double SigmaThreshold;                        /* from config file */
    double thres = 0.;
/*
 *  flag first arriving P and remove orphan depth phases
 */
    k = DepthPhaseCheck(sp, rdindx, p, 0);
    *has_depdpres = k;
/*
 *  set ttime, residual, dtdh, and dtdd for defining phases
 */
    if (TravelTimeResiduals(sp, p, "use", ec, TTtables, LocalTTtable, topo,
                            iszderiv, is2nderiv))
        return 1;
/*
 *  see if set of time defining phases has changed
 */
    for (i = 0; i < sp->numPhase; i++) {
        if (p[i].timedef) {
            thres = SigmaThreshold * p[i].deltim;
/*
 *          make phase non-defining if its residual is larger than
 *          SigmaThreshold times the prior measurement error
 */
            if (fabs(p[i].timeres) > thres)
                p[i].timedef = 0;
            else nd++;
        }
        if (p[i].timedef < p[i].prevtimedef) {
            isdiff = 1;
            nund++;
            if (verbose > 4)
                fprintf(logfp, "        %-6s %-8s %10.3f time made non-defining\n",
                        p[i].sta, p[i].phase, p[i].timeres);
/*
 *          delete corresponding row and column in dcov and w
 */
            if (iter && !ispchange && DoCorrelatedErrors) {
                isfound = 0;
                for (j = 0; j < kp; j++)
                    if (streq(phundef[j], p[i].phase)) isfound = 1;
                if (!isfound) strcpy(phundef[kp++], p[i].phase);
/*
 *             index of phase in dcov and w
 */
                k = p[i].CovIndTime;
                for (j = k; j < prevndef - 1; j++) {
                    for (m = 0; m < prevndef; m++) {
                        dcov[j][m] = dcov[j+1][m];
                        w[j][m] = w[j+1][m];
                    }
                }
                for (j = k; j < prevndef - 1; j++) {
                    for (m = 0; m < prevndef; m++) {
                        dcov[m][j] = dcov[m][j+1];
                        w[m][j] = w[m][j+1];
                    }
                }
            }
        }
        p[i].prevtimedef = p[i].timedef;
    }
/*
 *  see if set of azimuth defining phases has changed
 */
    for (i = 0; i < sp->numPhase; i++) {
        if (p[i].azimdef) {
            thres = SigmaThreshold * p[i].delaz;
/*
 *          make phase non-defining if its residual is larger than
 *          SigmaThreshold times the prior measurement error
 */
            if (fabs(p[i].azimres) > thres)
                p[i].azimdef = 0;
            else nd++;
        }
        if (p[i].azimdef < p[i].prevazimdef) {
            isdiff = 1;
            nund++;
            if (verbose > 4)
                fprintf(logfp, "        %-6s %-8s %10.3f azimuth made non-defining\n",
                        p[i].sta, p[i].phase, p[i].azimres);
/*
 *          delete corresponding row and column in dcov and w
 */
            if (iter && !ispchange && DoCorrelatedErrors) {
                isfound = 0;
                for (j = 0; j < kp; j++)
                    if (streq(phundef[j], p[i].phase)) isfound = 1;
                if (!isfound) strcpy(phundef[kp++], p[i].phase);
/*
 *             index of phase in dcov and w
 */
                k = p[i].CovIndAzim;
                for (j = k; j < prevndef - 1; j++) {
                    for (m = 0; m < prevndef; m++) {
                        dcov[j][m] = dcov[j+1][m];
                        w[j][m] = w[j+1][m];
                    }
                }
                for (j = k; j < prevndef - 1; j++) {
                    for (m = 0; m < prevndef; m++) {
                        dcov[m][j] = dcov[m][j+1];
                        w[m][j] = w[m][j+1];
                    }
                }
            }
        }
        p[i].prevazimdef = p[i].azimdef;
    }
/*
 *  see if set of slowness defining phases has changed
 */
    for (i = 0; i < sp->numPhase; i++) {
        if (p[i].slowdef) {
            thres = SigmaThreshold * p[i].delslo;
/*
 *          make phase non-defining if its residual is larger than
 *          SigmaThreshold times the prior measurement error
 */
            if (fabs(p[i].slowres) > thres)
                p[i].slowdef = 0;
            else nd++;
        }
        if (p[i].slowdef < p[i].prevslowdef) {
            isdiff = 1;
            nund++;
            if (verbose > 4)
                fprintf(logfp, "        %-6s %-8s %10.3f slowness made non-defining\n",
                        p[i].sta, p[i].phase, p[i].slowres);
/*
 *          delete corresponding row and column in dcov and w
 */
            if (iter && !ispchange && DoCorrelatedErrors) {
                isfound = 0;
                for (j = 0; j < kp; j++)
                    if (streq(phundef[j], p[i].phase)) isfound = 1;
                if (!isfound) strcpy(phundef[kp++], p[i].phase);
/*
 *             index of phase in dcov and w
 */
                k = p[i].CovIndSlow;
                for (j = k; j < prevndef - 1; j++) {
                    for (m = 0; m < prevndef; m++) {
                        dcov[j][m] = dcov[j+1][m];
                        w[j][m] = w[j+1][m];
                    }
                }
                for (j = k; j < prevndef - 1; j++) {
                    for (m = 0; m < prevndef; m++) {
                        dcov[m][j] = dcov[m][j+1];
                        w[m][j] = w[m][j+1];
                    }
                }
            }
        }
        p[i].prevslowdef = p[i].slowdef;
    }
    if (verbose > 2)
        fprintf(logfp, "    GetResiduals: %d phases made non-defining\n", nund);
    *ndef = nd;
    *nunp = kp;
    *ischanged = isdiff;
    return 0;
}

/*
 *  Title:
 *     BuildGd
 *  Synopsis:
 *     Builds G matrix and d vector for equation Gm = d.
 *     G is the matrix of partial derivates of travel-times,
 *     d is the vector of residuals.
 *  Input arguments
 *     ndef - number of defining observations
 *     sp   - pointer to current solution
 *     p[]  - array of phase structures
 *     fixdepthfornow - fix depth for this iteration?
 *  Output arguments
 *     g    - G matrix G(N x 4)
 *     d    - residual vector d(N)
 *  Return:
 *     urms - unweighted rms residual
 *  Called by:
 *     LocateEvent
 */
static double BuildGd(int ndef, SOLREC *sp, PHAREC p[], int fixdepthfornow,
        double **g, double *d)
{
    int i, j, k, im = 0;
    double urms = 0., depthcorr = 0., esaz = 0., acorr = 0., azcorr = 0.;
	depthcorr = DEG_TO_RAD * (EARTH_RADIUS - sp->depth);
	acorr = DEG_TO_RAD * EARTH_RADIUS;
/*
 *  build G matrix and d vector
 */
    if (verbose > 2)
        fprintf(logfp, "        G matrix and d vector:\n");
/*
 *  G matrix of partial derivates of travel-times
 */
    for (k = 0, i = 0; i < sp->numPhase; i++) {
        if (p[i].timedef) {
            for (j = 0; j < 4; j++) g[k][j] = 0.;
            im = 0;
            if (!sp->timfix)       /* Time */
                g[k][im++] = 1.;
            if (!sp->epifix) {     /* E, N */
                esaz = DEG_TO_RAD * p[i].esaz;
                g[k][im++] = -(p[i].dtdd / depthcorr) * sin(esaz);
                g[k][im++] = -(p[i].dtdd / depthcorr) * cos(esaz);
            }
            if (!fixdepthfornow)   /* Up */
                g[k][im++] = -p[i].dtdh;
/*
 *          d vector and unweighted rms residual
 */
            d[k] = p[i].timeres;
            urms += d[k] * d[k];
            if (verbose > 2) {
                fprintf(logfp, "          %6d %-6s %-9s (T) ",
                        k, p[i].prista, p[i].phase);
                for (j = 0; j < im; j++) fprintf(logfp, "%12.6f ", g[k][j]);
                fprintf(logfp, " , %12.6f\n", d[k]);
            }
            k++;
        }
    }
/*
 *  G matrix of partial derivates of azimuths
 */
    for (i = 0; i < sp->numPhase; i++) {
        if (p[i].azimdef) {
            for (j = 0; j < 4; j++) g[k][j] = 0.;
            im = 0;
            if (!sp->timfix)       /* Time */
                g[k][im++] = 0.;
            if (!sp->epifix) {     /* E, N */
                esaz = DEG_TO_RAD * p[i].esaz;
	            azcorr = sin(DEG_TO_RAD * p[i].delta) * acorr;
	            if (fabs(azcorr) < 0.0001) {
	                if (azcorr < 0.) azcorr = -0.0001;
	                else             azcorr = 0.0001;
	            }
                g[k][im++] = -cos(esaz) / azcorr;
                g[k][im++] = sin(esaz) / azcorr;
            }
            if (!fixdepthfornow)   /* Up */
                g[k][im++] = 0.;
/*
 *          d vector and unweighted rms residual
 */
            d[k] = DEG_TO_RAD * p[i].azimres;
            urms += d[k] * d[k];
            if (verbose > 2) {
                fprintf(logfp, "          %6d %-6s %-9s (A) ",
                        k, p[i].prista, p[i].phase);
                for (j = 0; j < im; j++) fprintf(logfp, "%12.6f ", g[k][j]);
                fprintf(logfp, " , %12.6f\n", d[k]);
            }
            k++;
        }
    }
/*
 *  G matrix of partial derivates of slownesses
 */
    for (i = 0; i < sp->numPhase; i++) {
        if (p[i].slowdef) {
            for (j = 0; j < 4; j++) g[k][j] = 0.;
            im = 0;
            if (!sp->timfix)       /* Time */
                g[k][im++] = 0.;
            if (!sp->epifix) {     /* E, N */
                esaz = DEG_TO_RAD * p[i].esaz;
                g[k][im++] = -(p[i].d2tdd / depthcorr) * sin(esaz);
                g[k][im++] = -(p[i].d2tdd / depthcorr) * cos(esaz);
            }
            if (!fixdepthfornow)   /* Up */
                g[k][im++] = -p[i].d2tdh;
/*
 *          d vector and unweighted rms residual
 */
            d[k] = p[i].slowres / DEG2KM;
            urms += d[k] * d[k];
            if (verbose > 2) {
                fprintf(logfp, "          %6d %-6s %-9s (S) ",
                        k, p[i].prista, p[i].phase);
                for (j = 0; j < im; j++) fprintf(logfp, "%12.6f ", g[k][j]);
                fprintf(logfp, " , %12.6f\n", d[k]);
            }
            k++;
        }
    }
    urms = Sqrt(urms / (double)ndef);
    if (verbose > 2) fprintf(logfp, "        urms = %12.6f\n", urms);
    return urms;
}

/*
 *  Title:
 *     ProjectGd
 *  Synopsis:
 *     Projects G matrix and d vector into eigensystem.
 *  Input arguments:
 *     ndef  - number of defining observations
 *     m     - number of model parameters
 *     g     - G matrix G(N x M)
 *     d     - residual vector d(N)
 *     w     - W projection matrix W(N x N)
 *  Output arguments:
 *     g     - projected G matrix G(N x M)
 *     d     - projected residual vector d(N)
 *     dnorm - data norm (sum of squares of weighted residuals)
 *     wrms  - weighted rms residual
 *  Returns:
 *     0/1 on success/error
 *  Called by:
 *     LocateEvent
 *  Calls:
 *     WxG
 */
static int ProjectGd(int ndef, int m, double **g, double *d, double **w,
        double *dnorm, double *wrms)
{
    int i, j, k;
    double *temp = (double *)NULL;
    double wssq = 0.;
/*
 *  allocate memory for temporary storage
 */
    if ((temp = (double *)calloc(ndef, sizeof(double))) == NULL) {
        fprintf(logfp, "ProjectGd: cannot allocate memory\n");
        fprintf(errfp, "ProjectGd: cannot allocate memory\n");
        errorcode = 1;
        return 1;
    }
/*
 *  WG(NxM) = W(NxN) * G(NxM)
 */
#ifdef GCD
/*
 *  use GCD (Mac OS) to parallelize the matrix multiplication
 *  each model dimension is processed concurrently
 */
    if (ndef > 100) {
        if (verbose > 3) fprintf(logfp, "ProjectGd: GCD is on\n");
        dispatch_apply(m, dispatch_get_global_queue(0, 0),
                       ^(size_t j){ WxG((int)j, ndef, w, g); });
    }
    else {
        for (j = 0; j < m; j++) {
            WxG(j, ndef, w, g);
        }
    }
#else
/*
 *  single core
 */
    for (j = 0; j < m; j++) {
        WxG(j, ndef, w, g);
    }
#endif
    if (errorcode) {
        Free(temp);
        return 1;
    }
/*
 *  Wd(N) = W(NxN) * d(N) and sum of squares of weighted residuals
 */
    for (i = 0; i < ndef; i++) {
        temp[i] = 0.;
        for (k = 0; k < ndef; k++)
            temp[i] += w[i][k] * d[k];
        if (fabs(temp[i]) < ZERO_TOL) temp[i] = 0.;
    }
    wssq = 0.;
    for (i = 0; i < ndef; i++) {
        d[i] = temp[i];
        wssq += d[i] * d[i];
    }
    Free(temp);
    *dnorm = wssq;
    *wrms = Sqrt(wssq / (double)ndef);
    if (verbose > 2) {
        fprintf(logfp, "        WG(%d x %d) matrix and Wd vector:\n", ndef, m);
        for (i = 0; i < ndef; i++) {
            fprintf(logfp, "          %6d ", i);
            for (j = 0; j < m; j++) fprintf(logfp, "%12.6f ", g[i][j]);
            fprintf(logfp, " , %12.6f\n", d[i]);
        }
        fprintf(logfp, "        wrms = %12.6f\n", *wrms);
    }
    return 0;
}

/*
 *  Title:
 *     WxG
 *  Synopsis:
 *     W * G matrix multiplication
 *  Input arguments:
 *     j     - model dimension index
 *     ndef  - number of defining observations
 *     w     - W projection matrix W(N x N)
 *     g     - G matrix G(N x M)
 *  Output arguments:
 *     g     - projected G matrix G(N x M)
 *  Returns:
 *     0/1 on success/error
 *  Called by:
 *     ProjectGd
 */
static int WxG(int j, int ndef, double **w, double **g)
{
    int i, k;
    double *temp = (double *)NULL;
    if ((temp = (double *)calloc(ndef, sizeof(double))) == NULL) {
        fprintf(logfp, "WxG: cannot allocate memory\n");
        fprintf(errfp, "WxG: cannot allocate memory\n");
        errorcode = 1;
        return 1;
    }
    for (k = 0; k < ndef; k++)
        temp[k] = g[k][j];
    for (i = 0; i < ndef; i++) {
        g[i][j] = 0.;
        for (k = 0; k < ndef; k++)
            g[i][j] += w[i][k] * temp[k];
        if (fabs(g[i][j]) < ZERO_TOL) g[i][j] = 0.;
    }
    Free(temp);
    return 0;
}


/*
 *  Title:
 *     WeightGd
 *  Synopsis:
 *     Independence assumption: weight Gm = d by measurement errors.
 *  Input arguments
 *     ndef    - number of defining observations
 *     m       - number of model parameters
 *     numPhase - number of associated phases
 *     p[]     - array of phase structures
 *     g       - G matrix G(N x M)
 *     d       - residual vector d(N)
 *  Output arguments
 *     g     - weighted G matrix G(N x M)
 *     d     - weighted residual vector d(N)
 *     dnorm - data norm (sum of squares of weighted residuals)
 *     wrms  - weighted rms residual
 *  Called by:
 *     LocateEvent
 */
static void WeightGd(int ndef, int m, int numPhase, PHAREC p[],
                      double **g, double *d, double *dnorm, double *wrms)
{
    int i, j, k;
    double wssq = 0., weight = 0.;
    for (k = 0, i = 0; i < numPhase; i++) {
        if (!p[i].timedef) continue;
        if (p[i].deltim < DEPSILON)
            weight = 1.;
        else
            weight = 1. / p[i].deltim;
        for (j = 0; j < m; j++)
            g[k][j] *= weight;
        d[k] *= weight;
        wssq += d[k] * d[k];
        k++;
    }
    for (k = 0, i = 0; i < numPhase; i++) {
        if (!p[i].azimdef) continue;
        if (p[i].delaz < DEPSILON)
            weight = 1.;
        else
            weight = 1. / p[i].delaz;
        for (j = 0; j < m; j++)
            g[k][j] *= weight;
        d[k] *= weight;
        wssq += d[k] * d[k];
        k++;
    }
    for (k = 0, i = 0; i < numPhase; i++) {
        if (!p[i].slowdef) continue;
        if (p[i].delslo < DEPSILON)
            weight = 1.;
        else
            weight = 1. / p[i].delslo;
        for (j = 0; j < m; j++)
            g[k][j] *= weight;
        d[k] *= weight;
        wssq += d[k] * d[k];
        k++;
    }
    *dnorm = wssq;
    *wrms = Sqrt(wssq / (double)ndef);
    if (verbose > 2) {
        fprintf(logfp, "        WG(%d x %d) matrix and Wd vector:\n", ndef, m);
        for (i = 0; i < ndef; i++) {
            fprintf(logfp, "          %6d ", i);
            for (j = 0; j < m; j++) fprintf(logfp, "%12.6f ", g[i][j]);
            fprintf(logfp, " , %12.6f\n", d[i]);
        }
    }
}

/*
 *  Title:
 *     ConvergenceTestValue
 *  Synopsis:
 *     Convergence test value of Paige and Saunders (1982)
 *
 *     Paige, C. and Saunders, M., 1982,
 *         LSQR: An Algorithm for Sparse Linear Equations and
 *         Sparse Least Squares,
 *         ACM Trans. Math. Soft. 8, 43-71.
 *
 *               ||transpose(G) * d||
 *     cvgtst = ----------------------
 *                  ||G|| * ||d||
 *
 *  Input arguments:
 *     gtdnorm  - ||transpose(G) * d||
 *     gnorm    - G matrix norm (Sum(sv^2))
 *     dnorm    - d vector norm (Sum(d^2))
 *  Return:
 *     cnvgtst  - Paige-Saunders convergence test number
 *  Called by:
 *     LocateEvent
 */
static double ConvergenceTestValue(double gtdnorm, double gnorm, double dnorm)
{
    double cnvgtst = 0., gd = 0.;
    gd = gnorm * dnorm;
    if (gtdnorm > DEPSILON && gd < DEPSILON)
        cnvgtst = 999.;
    else
        cnvgtst = gtdnorm / gd;
    return cnvgtst;
}

/*
 *  Title:
 *     ConvergenceTest
 *  Synopsis:
 *     Convergence/divergence is decided based on
 *         the Paige-Saunder convergence test value and
 *         the history of model and data norms.
 *  Input Arguments:
 *     iter        - iteration number
 *     m           - number of model parameters
 *     nds[]       - current and past number of defining observations
 *     sol[]       - current solution vector
 *     oldsol[]    - previous solution vector
 *     wrms        - weighted RMS residual
 *     modelnorm[] - current and past model norms
 *     convgtest[] - current and past convergence test values
 *     oldcvgtst   - previous convergence test value
 *     step        - step length
 *  Output Arguments:
 *     oldsol[]    - previous solution vector
 *     oldcvgtst   - previous convergence test value
 *     step        - step length
 *     isdiv       - divergent solution [0/1]
 *  Returns:
 *     isconv - convergent solution [0/1]
 *  Called by:
 *     LocateEvent
 */
static int ConvergenceTest(int iter, int m, int *nds, double *sol,
        double *oldsol, double wrms, double *modelnorm, double *convgtest,
        double *oldcvgtst, double *step, int *isdiv)
{
    double dm01 = 0., dm12 = 0., dc01 = 0., dc12 = 0.;
    double sc = *step, oldcvg = 0.;
    int i, convergent = 0, divergent = 0;
    oldcvg = *oldcvgtst;
    if (modelnorm[0] > 0. && convgtest[0] > 0.) {
/*
 *      indicators of increasing/decreasing model norms and convergence tests
 */
        if (modelnorm[1] <= 0. || modelnorm[2] <= 0.)
            dm01 = dm12 = 1.05;
        else {
            dm01 = modelnorm[0] / modelnorm[1];
            dm12 = modelnorm[1] / modelnorm[2];
        }
        if (convgtest[1] <= 0. || convgtest[2] <= 0.)
            dc01 = convgtest[0];
        else {
            dc01 = convgtest[0] / convgtest[1];
            dc12 = convgtest[1] / convgtest[2];
            dc01 = fabs(dc12 - dc01);
        }
        dc12 = fabs(convgtest[0] - convgtest[2]);
/*
 *      divergent solution if increasing model norm
 */
        if (dm12 > 1.1 && dm01 > dm12 &&
            iter > MinIterations + 2 && modelnorm[0] > 500)
            divergent = 1;
/*
 *      convergent solution if vanishing
 *      convergence test value or model norm or weighted RMS residual
 */
        else if (nds[0] == nds[1] &&
                (convgtest[0] < CONV_TOL || modelnorm[0] < 0.1 || wrms < 0.01))
            convergent = 1;
/*
 *      convergent solution if vanishing convergence test value
 */
        else if ((convgtest[0] < 1.01 * oldcvg && convgtest[0] < CONV_TOL) ||
                 (iter > 3 * MaxIterations / 4 &&
                     (convgtest[0] < sqrt(CONV_TOL) ||
                      dc01 < CONV_TOL ||
                      dc12 < sqrt(CONV_TOL))
                ))
            convergent = 1;
    }
    else
        convergent = 1;
    if (iter == MaxIterations - 1)
        convergent = 0;
/*
 *  Apply step-length weighting if convergence test value is increasing.
 *  Steps are applied in half-lengths of previous solution vector
 */
    if (iter > MinIterations + 2 && sc > 0.05 &&
        (convgtest[0] > *oldcvgtst || convgtest[0] - convgtest[2] == 0.)) {
        sc *= 0.5;
        if (sc != 0.5) {
            for (i = 0; i < m; i++) {
                if (fabs(oldsol[i]) < ZERO_TOL) oldsol[i] = sol[i];
                sol[i] = sc * oldsol[i];
            }
        }
        else {
            for (i = 0; i < m; i++) {
                sol[i] = sc * sol[i];
                oldsol[i] = sol[i];
            }
        }
    }
    else {
        sc = 1;
        *oldcvgtst = convgtest[0];
    }
    *step = sc;
    *isdiv = divergent;
    return convergent;
}

/*
 *  Title:
 *     Readings
 *  Synopsis:
 *     records starting index and number of phases in a reading
 *  Input Arguments:
 *     numPhase  - number of associated phases
 *     nreading - number of Readings
 *     p        - array of phase structures
 *  Output Arguments:
 *     rdindx   - array of reading structures
 *  Called by:
 *     Locator, LocateEvent, NASearch
 */
void Readings(int numPhase, int nreading, PHAREC p[], READING *rdindx)
{
    int i, j = 0, rdid;
    for (i = 0; i < nreading; i++) {
        rdindx[i].start = j;
        rdindx[i].npha = 0;
        rdid = p[j].rdid;
        for (; j < numPhase; j++) {
            if (p[j].rdid != rdid)
                break;
            rdindx[i].npha++;
        }
    }
}
