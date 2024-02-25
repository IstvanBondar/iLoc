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
extern double MaxHypocenterDepth;
extern int DoCorrelatedErrors;
extern struct timeval t0;
extern double NAsearchRadius;       /* search radius around preferred origin */
extern double NAsearchDepth;    /* search radius (km) around preferred depth */
extern double NAsearchOT;           /* search radius (s) around preferred OT */
extern double NAlpNorm;                /* p-value for norm to compute misfit */
extern int NAiterMax;                            /* max number of iterations */
extern int NAinitialSample;                        /* size of initial sample */
extern int NAnextSample;                       /* size of subsequent samples */
extern int NAcells;                       /* number of cells to be resampled */
extern long iseed;                                     /* random number seed */
extern int WriteNAResultsToFile;
extern char PhaseWithoutResidual[MAXNUMPHA][PHALEN];   /* no-residual phases */
extern int PhaseWithoutResidualNum;          /* number of no-residual phases */
extern double SigmaThreshold;

/*
 * Functions:
 *    SetNASearchSpace
 *    NASearch
 */

/*
 * Local functions
 *    na_initialize
 *    na_initial_sample
 *    na_sample
 *    na_restart
 *    NNcalc_dlist
 *    NNupdate_dlist
 *    NNaxis_intersect
 *    na_misfits
 *    na_deviate
 *    findnearest
 *    transform2raw
 *    tolatlon
 *    jumble
 *    na_sas_table
 *    na_sobol
 *    sobseq
 *    selecti
 *    indexx
 *    irandomvalue
 *    lranq1
 *    dranq1
 *    ranfib
 *    WriteNAModels
 *    NAForwardProblem
 *    dosamples
 */
static int na_initialize(NASPACE *nasp, double *xcur, SOBOL *sas);
static int na_initial_sample(double *na_models[], NASPACE *nasp, SOBOL *sas);
static int na_sample(double *na_models[], NASPACE *nasp, int ntot,
        int *mfitord, double *xcur, int *restartNA, int nclean, double *dlist,
        SOBOL *sas, int *nupd);
static int na_restart(double *na_models[], int nd, int ind, double *x);
static int NNcalc_dlist(int dim, double *dlist, double *na_models[], int nd,
        int ntot, double *x);
static int NNupdate_dlist(int idnext, int id, double *dlist,
        double *na_models[], int ntot, double *x);
static void NNaxis_intersect(int id, double *dlist, double *na_models[],
        int ntot, int nodex, NASPACE *nasp, double *x1, double *x2);
static void na_misfits(double *misfit, int nsample, int ntot, double *mfitmin,
        double *mfitminc, double *mfitmean, int *mopt, double *work, int *ind,
        int *iwork, int *mfitord);
static double na_deviate(double x1, double x2, int i, SOBOL *sas);
static int findnearest(double *xcur, double *na_models[], int ntot, int nd);
static void transform2raw(double *model_sca, NASPACE *nasp, double *model_raw);
static void tolatlon(double *model_raw, NASPACE *nasp);
static void jumble(int *iarr, double *arr, int n);
static void na_sas_table(SOBOL *sas);
static int na_sobol(int n, double *x, int mode, int init, SOBOL *sas);
static void sobseq(int n, double *x);
static double selecti(int k, int n, double *arr, int *ind);
static void indexx(int n, double *arr, int *indx);
static int irandomvalue(int lo, int hi);
static unsigned long lranq1(unsigned long seed);
static double dranq1(unsigned long seed);
static double ranfib(int init, unsigned long seed);
static void WriteNAModels(double *na_models[], NASPACE *nasp, double *misfit);
static double dosamples(int i, int ntot, double *na_model, NASPACE *nasp,
        int np, int nsta, SOLREC *sp, READING *rdindx, PHAREC *pgs,
        TT_TABLE *TTtables, TT_TABLE *LocalTTtable, EC_COEF *ec,
        short int **topo, STAREC stalist[], double **distmatrix,
        VARIOGRAM *variogramp, FILE *fp, int is2nderiv);
static double NAForwardProblem(int nsta, NASPACE *nasp, double *model,
        SOLREC *sp, READING *rdindx, PHAREC pgs[], TT_TABLE *TTtables,
        TT_TABLE *LocalTTtable, EC_COEF *ec, short int **topo, STAREC stalist[],
        double **distmatrix, VARIOGRAM *variogramp, char *buf, int is2nderiv,
        int isprint);

/*
 *  Title:
 *     SetNASearchSpace
 *  Synopsis:
 *     Sets NA search limits around initial hypocentre. By default, it
 *     searches in 4D, regardless whether there is depth resolution.
 *  Input Arguments:
 *     sp   - pointer to current solution
 *  Output Arguments:
 *     nasp - NA search parameter structure
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     Locator
 */
int SetNASearchSpace(SOLREC *sp, NASPACE *nasp)
{
    int i;
    nasp->lat = sp->lat;
    nasp->lon = sp->lon;
    nasp->ot = sp->time;
    nasp->depth = sp->depth;
    nasp->epifix = sp->epifix;
    nasp->otfix = sp->timfix;
    nasp->depfix = sp->depfix;
/*
 *  tighten search space for anthropogenic events
 */
    if (sp->FixedDepthType == 4) {
        if (NAsearchOT > 20.) NAsearchOT = 20.;
        if (NAsearchRadius > 2.) NAsearchRadius = 2.;
    }
/*
 *  search space dimensions
 */
    nasp->nd = 4;
    if (nasp->epifix) nasp->nd -= 2;
    if (nasp->otfix)  nasp->nd--;
    if (nasp->depfix) nasp->nd--;
    if (verbose)
        fprintf(logfp, "NASearchSpace: otfix=%d epifix=%d depfix=%d\n",
                        nasp->otfix, nasp->epifix, nasp->depfix);
    if (nasp->nd < 1) return 1;
    i = 0;
/*
 *  search in delta-azimuth instead of lat, lon
 */
    if (!nasp->epifix) {
        nasp->range[i][0] = 0.;
        nasp->range[i][1] = NAsearchRadius;
        i++;
        nasp->range[i][0] = 0.;
        nasp->range[i][1] = 360.;
        i++;
    }
/*
 *  search range for origin time
 */
    if (!nasp->otfix) {
        nasp->range[i][0] = sp->time - NAsearchOT;
        nasp->range[i][1] = sp->time + NAsearchOT;
        i++;
    }
/*
 *  search range for depth
 */
    if (!nasp->depfix) {
        if ((sp->depth - NAsearchDepth) > 0.)
            nasp->range[i][0] = sp->depth - NAsearchDepth;
        else
            nasp->range[i][0] = 0.;
        if ((sp->depth + NAsearchDepth) < MaxHypocenterDepth)
             nasp->range[i][1] = sp->depth + NAsearchDepth;
        else nasp->range[i][1] = MaxHypocenterDepth;
    }
/*
 *  set scale factor to parameter range
 */
    nasp->scale[0] = -1.;
    for (i = 0; i < nasp->nd; i++) {
        nasp->scale[i+1] = nasp->range[i][1] - nasp->range[i][0];
        if (nasp->scale[i+1] < 0.001) {
            fprintf(errfp, "Invalid range: %d, %f!\n", i+1, nasp->scale[i+1]);
            fprintf(logfp, "Invalid range: %d, %f!\n", i+1, nasp->scale[i+1]);
            return 1;
        }
    }
/*
 *  NA residual statistics set to L1, L2 or L1.x norm
 */
    if (NAlpNorm < 1 || NAlpNorm > 2) {
        fprintf(errfp, "Invalid lp-norm: %.2f!\n", NAlpNorm);
        fprintf(logfp, "Invalid lp-norm: %.2f!\n", NAlpNorm);
        return 1;
    }
    nasp->lpnorm = NAlpNorm;
    return 0;
}

/*
 *  Title:
 *     NASearch
 *  Synopsis:
 *     Neigbourhood Algorithm search to find a hypocentre guess for
 *        the linearized inversion.
 *
 *     Sambridge, M., Geophysical inversion with a neighbourhood
 *        algorithm. I. Searching the parameter space,
 *        Geophys. J. Int., 138, 479-494, 1999.
 *     Sambridge M. and B.L.N. Kennett, Seismic event location:
 *        non-linear inversion using a neighbourhood algorithm,
 *        Pageoph, 158, 241-257, 2001.
 *
 *     Main subroutine NA - sampling a parameter space
 *        using a Neighbourhood algorithm
 *        M. Sambridge, (RSES, ANU) Last revision Sept. 1999.
 *     Transcripted to C by Istvan Bondar (ISC), March 2009
 *        Several simplifications are made to the original code, such as
 *        Monte Carlo option is no longer supported;
 *        quasi-random sequences are always generated by Sobol's method;
 *        NAD files are no longer supported;
 *        simplified verbose/debugging.
 *     Forward modeling
 *        uses all P and S-type phases
 *        searches in 4D (lat, lon, OT, depth) by default
 *        reidentifies phases w.r.t. each trial hypocentre
 *        accounting for correlated errors may be turned off for speed
 *  Input Arguments:
 *     nsta       - number of stations
 *     sp         - pointer to current solution
 *     p          - array of phase structures
 *     TTtables  - pointer to travel-time tables
 *     ec         - pointer to ellipticity correction coefficient structure
 *     topo       - ETOPO bathymetry/elevation matrix
 *     stalist    - array of starec structures
 *     distmatrix - station separation matrix
 *     variogramp - pointer to generic variogram model
 *     staorder   - array of staorder structures (nearest-neighbour order)
 *     nasp       - NA search parameter structure
 *     filename   - pathname for grid search results
 *  Output Arguments:
 *     sp         - pointer to current solution
 *     p          - array of phase structures
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     Locator
 *  Calls:
 *     AllocateFloatMatrix, Free, FreeFloatMatrix, GetDataCovarianceMatrix,
 *     GetdUGapSgap, ProjectionMatrix, SortPhasesForNA, EpochToHuman,
 *     PrintSolution, PrintDefiningPhases, na_initialize, na_initial_sample,
 *     na_sample, transform2raw, NAForwardProblem, na_misfits, tolatlon,
 *     WriteNAModels, do_samples
 */
int NASearch(int nsta, SOLREC *sp, PHAREC p[], TT_TABLE *TTtables,
        TT_TABLE *LocalTTtable, EC_COEF *ec, short int **topo, STAREC stalist[],
        double **distmatrix, VARIOGRAM *variogramp, STAORDER staorder[],
        NASPACE *nasp, char *filename, int is2nderiv)
{
    FILE *fp = (FILE *)NULL;
    PHAREC *pgs = (PHAREC *)NULL;          /* phase records for grid search */
    READING *rdindx = (READING *)NULL;                          /* readings */
    double *misfit = (double *)NULL;
    double mfitmin = 0., mfitmean = 0., mfitminc = 0.;
    double du = 1., gap = 360., sgap = 360., dummy = 0.;
    double *esaz = (double *)NULL;
    double **na_models = (double **)NULL;
    double xcur[NA_MAXND], model_opt[NA_MAXND];
    double *dlist = (double *)NULL;
    double *work_NA2 = (double *)NULL;
    int *iwork_NA1 = (int *)NULL;
    int *iwork_NA2 = (int *)NULL;
    int *mfitord = (int *)NULL;
    int ntotal = 0, nsamp = 0, nclean = 500;
    int restartNA = 1, mopt = 0, prev_rdid = -1;
    int ntot = 0, ncald = 0, nupd = 0, nc = 0, nu = 0, ksta = 0;
    int iter = 0, i, j, k, ns = 0, nd = 0, np = 0, nrd = 0, prank = 0;
    int DoCorrelatedErrors_cf;
    int verbose_cf = verbose;
    char timestr[25], buf[64];
    char prevsta[STALEN];
    SOBOL sas;
    ntotal = NAinitialSample + 1 + NAnextSample * NAiterMax;
    nsamp = max(NAnextSample, NAinitialSample + 1);
    nd = nasp->nd;
    sas.n = nd * NAcells;
/*
 *  sanity checks
 */
    if (iseed < 0) iseed = -iseed;
    if (nd > NA_MAXND) {
        fprintf(errfp, "NA: model parameters %d > %d!\n", nd, NA_MAXND);
        fprintf(logfp, "NA: model parameters %d > %d!\n", nd, NA_MAXND);
        return 1;
    }
    if (NAcells > NAnextSample || NAcells > NAinitialSample ) {
        fprintf(errfp, "NA: number of cells to resampled %d > (%d, %d)!\n",
                NAcells, NAinitialSample, NAnextSample);
        fprintf(logfp, "NA: number of cells to resampled %d > (%d, %d)!\n",
                NAcells, NAinitialSample, NAnextSample);
        return 1;
    }
/*
 *  count P, S, I and H, O type phases that could be used in location
 */
    np = nrd = 0;
    for (i = 0; i < sp->numPhase; i++) {
        p[i].timedef = p[i].azimdef = p[i].slowdef = 0;
/*
 *      skip phases that don't get residuals (amplitudes etc)
 */
        for (j = 0; j < PhaseWithoutResidualNum; j++) {
            if (streq(p[i].phase, PhaseWithoutResidual[j]))
                break;
        }
        if (j != PhaseWithoutResidualNum)
            continue;
        p[i].timedef = 1;
        if (streq(p[i].phase, "I") ||
            streq(p[i].phase, "H") || streq(p[i].phase, "O"))
            p[i].azimdef = 1;
        np++;
        if (p[i].rdid != prev_rdid) nrd++;
        prev_rdid = p[i].rdid;
    }
    if (np < nasp->nd) {
        fprintf(logfp, "insufficient number of observations ");
        fprintf(logfp, "(%d) to perform grid search!\n", np);
        return 1;
    }
   if (WriteNAResultsToFile) {
        if ((fp = fopen(filename, "w")) == NULL) {
            fprintf(logfp, "NA: cannot open %s\n", filename);
            return 1;
        }
    }
/*
 *  memory allocations
 */
    rdindx = (READING *)calloc(nrd, sizeof(READING));
    esaz = (double *)calloc(np + 2, sizeof(double));
    if ((pgs = (PHAREC *)calloc(np, sizeof(PHAREC))) == NULL) {
        fprintf(errfp, "NASearch: cannot allocate memory!\n");
        fprintf(logfp, "NASearch: cannot allocate memory!\n");
        Free(rdindx); Free(esaz);
        errorcode = 1;
        return 1;
    }
/*
 *  make a copy of P, S, I and H, O type phases that could be used in location
 */
    strcpy(prevsta, "");
    j = ksta = 0;
    for (i = 0; i < sp->numPhase; i++) {
        if (p[i].timedef || p[i].azimdef) {
            memmove(&pgs[j], &p[i], sizeof(PHAREC));
            if (strcmp(p[i].prista, prevsta))
                esaz[ksta++] = p[i].esaz;
            strcpy(prevsta, p[i].prista);
            j++;
        }
    }
    sp->numPhase = prank = np;
    sp->nreading = nrd;
    Readings(np, nrd, pgs, rdindx);
/*
 *  disable correlated errors for reasonably balanced networks
 */
    du = GetdUGapSgap(ksta, esaz, &gap, &sgap);
    Free(esaz);
    DoCorrelatedErrors_cf = DoCorrelatedErrors;
    if (np > 30 && du < 0.7)
        DoCorrelatedErrors = 0;
    if (DoCorrelatedErrors) {
/*
 *      reorder phaserecs by staorder, rdid, time so that covariance matrices
 *      for various phases will become block-diagonal
 */
        SortPhasesForNA(np, nsta, pgs, stalist, staorder);
        Readings(np, nrd, pgs, rdindx);
    }
/*
 *  memory allocations
 */
    sas.mdeg = (int *)calloc(sas.n, sizeof(int));
    sas.pol = (unsigned long *)calloc(sas.n, sizeof(unsigned long));
    sas.iv = AllocateLongMatrix(sas.n, NA_MAXBIT);
    misfit = (double *)calloc(ntotal, sizeof(double));
    work_NA2 = (double *)calloc(ntotal, sizeof(double));
    iwork_NA1 = (int *)calloc(ntotal, sizeof(int));
    iwork_NA2 = (int *)calloc(ntotal, sizeof(int));
    mfitord = (int *)calloc(ntotal, sizeof(int));
    na_models = AllocateFloatMatrix(ntotal, NA_MAXND);
    if ((dlist = (double *)calloc(ntotal, sizeof(double))) == NULL) {
        fprintf(errfp, "NASearch: cannot allocate memory!\n");
        fprintf(logfp, "NASearch: cannot allocate memory!\n");
        Free(pgs);
        Free(iwork_NA1); Free(iwork_NA2); Free(work_NA2);
        Free(misfit); Free(mfitord);
        FreeLongMatrix(sas.iv);
        Free(sas.pol); Free(sas.mdeg);
        FreeFloatMatrix(na_models);
        Free(rdindx);
        errorcode = 1;
        DoCorrelatedErrors = DoCorrelatedErrors_cf;
        return 1;
    }
    mfitmin = 1e6;
/*
 *  verbose
 */
    if (verbose) {
        fprintf(logfp, "Grid search around hypocentre:\n");
        EpochToHuman(timestr, nasp->ot);
        fprintf(logfp, "  OT = %s ", timestr);
        fprintf(logfp, "Lat = %7.3f Lon = %8.3f Depth = %.1f\n",
                nasp->lat, nasp->lon, nasp->depth);
        fprintf(logfp, "  Number of phases = %d\n", np);
        fprintf(logfp, "  Number of model parameters = %d\n", nd);
        fprintf(logfp, "    NA options\n");
        fprintf(logfp, "      Initial sample size = %d\n", NAinitialSample);
        fprintf(logfp, "      Sample size = %d\n", NAnextSample);
        fprintf(logfp, "      Number of iterations = %d\n", NAiterMax);
        fprintf(logfp, "      Number of cells resampled = %d\n", NAcells);
        fprintf(logfp, "      Random seed value = %ld\n", iseed);
        fprintf(logfp, "      SAS Quasi-random sequence used\n");
        fprintf(logfp, "      Starting models generated randomly\n");
        if (DoCorrelatedErrors != DoCorrelatedErrors_cf) {
            fprintf(logfp, "      Temporarily disabled correlated errors");
            fprintf(logfp, " (np = %d > 30 && dU = %4.2f < 0.7)\n", np, du);
        }
    }
/*
 *  initialize NA routines
 */
    if (na_initialize(nasp, xcur, &sas)) {
        Free(pgs);
        Free(iwork_NA1); Free(iwork_NA2); Free(work_NA2);
        Free(misfit); Free(mfitord);
        FreeLongMatrix(sas.iv);
        Free(sas.pol); Free(sas.mdeg);
        FreeFloatMatrix(na_models);
        Free(rdindx); Free(dlist);
        DoCorrelatedErrors = DoCorrelatedErrors_cf;
        return 1;
    }
    if (WriteNAResultsToFile) {
/*
 *      print search limits and starting point to file
 */
        fprintf(fp, "# epifix = %d epirange = %.2f\n", nasp->epifix, NAsearchRadius);
        fprintf(fp, "# ot_fix = %d ot_range = %.2f\n", nasp->otfix, NAsearchOT);
        fprintf(fp, "# depfix = %d deprange = %.2f\n", nasp->depfix, NAsearchDepth);
        fprintf(fp, "# %8.4f %9.4f %15.3f %8.4f\n",
                     nasp->lat, nasp->lon, nasp->ot, nasp->depth);
        fprintf(fp, "#  i   j   lat       lon         ot          depth     ");
        fprintf(fp, "misfit       norm     penalty    np ndef rank\n");
    }
/*
 *  optimization loop
 */
    ntot = ncald = nupd = nc = nu = 0;
    ns = NAinitialSample + 1;
    for (iter = 0; iter <= NAiterMax; iter++) {
        if (iter) {
/*
 *          generate new sample with nearest neighbour resampling
 */
            nc = na_sample(na_models, nasp, ntot, mfitord,
                           xcur, &restartNA, nclean, dlist, &sas, &nu);
            ncald += nc;
            nupd += nu;
            ns = NAnextSample;
        }
        else {
/*
 *          generate starting models
 */
            na_initial_sample(na_models, nasp, &sas);
        }
/*
 *      calculate model misfits
 */
        verbose = 0;
        for (i = 0; i < ns; i++) {
            misfit[ntot + i] = dosamples((int)i, ntot, na_models[ntot + i],
                                    nasp, np, nsta, sp, rdindx, pgs,
                                    TTtables, LocalTTtable, ec, topo,
                                    stalist, distmatrix, variogramp, fp,
                                    is2nderiv);
        }
        verbose = verbose_cf;
/*
 *      misfit statistics
 */
        na_misfits(misfit, ns, ntot, &mfitmin, &mfitminc, &mfitmean, &mopt,
                   work_NA2, iwork_NA1, iwork_NA2, mfitord);
/*
 *      save best model
 */
        transform2raw(na_models[mopt], nasp, model_opt);
        if (!nasp->epifix)
            tolatlon(model_opt, nasp);
        ntot += ns;
        if (WriteNAResultsToFile) {
/*
 *          print best model to file
 */
            k = 0;
            fprintf(fp, "%4d %3d ", iter, -1);
            if (nasp->epifix)
                fprintf(fp, "%8.4f %9.4f ", nasp->lat, nasp->lon);
            else {
                fprintf(fp, "%8.4f %9.4f ", model_opt[k], model_opt[k+1]);
                k += 2;
            }
            if (nasp->otfix)
                fprintf(fp, "%15.3f ", nasp->ot);
            else {
                fprintf(fp, "%15.3f ", model_opt[k]);
                k++;
            }
            if (nasp->depfix) fprintf(fp, "%8.4f ", nasp->depth);
            else              fprintf(fp, "%8.4f ", model_opt[k]);
            fprintf(fp, "%10.4f\n", misfit[mopt]);
        }
        if (verbose > 4) {
/*
 *          high level verbose
 */
            fprintf(logfp, "        NA iteration %d\n", iter);
            fprintf(logfp, "          Total number of samples = %d\n", ntot);
            fprintf(logfp, "          Minimum misfit = %.4f\n", mfitmin);
            fprintf(logfp, "          Minimum misfit in this iteration = %.4f\n",
                    mfitminc);
            fprintf(logfp, "          Mean misfit in this iteration = %.4f\n",
                    mfitmean);
            fprintf(logfp, "          Index of best fitting model = %d\n",
                    mopt);
            fprintf(logfp, "          Best fitting model so far\n");
            k = 0;
            fprintf(logfp, "          ");
            if (nasp->epifix)
                fprintf(logfp, "%7.3f %8.3f ", nasp->lat, nasp->lon);
            else {
                fprintf(logfp, "%7.3f %8.3f ", model_opt[k], model_opt[k+1]);
                k += 2;
            }
            if (nasp->otfix)
                fprintf(logfp, "%15.3f ", nasp->ot);
            else {
                fprintf(logfp, "%15.3f ", model_opt[k]);
                k++;
            }
            if (nasp->depfix)
                fprintf(logfp, "%7.3f ", nasp->depth);
            else
                fprintf(logfp, "%7.3f ", model_opt[k]);
            fprintf(logfp, "\n");
        }
    }
    if (WriteNAResultsToFile) fclose(fp);
    if (verbose) {
        fprintf(logfp, "  NA summary\n");
        fprintf(logfp, "    Total number of samples = %d\n", ntot);
        fprintf(logfp, "    Total number of full dlist evaluations = %d\n",
                ncald);
        fprintf(logfp, "    Total number of partial dlist updates = %d\n",
                nupd);
        fprintf(logfp, "    Lowest misfit found = %.4f\n", mfitmin);
    }
    if (mfitmin < 999) {
        if (verbose) {
            fprintf(logfp, "    Mean misfit over all models = %.4f\n",
                    mfitmean);
            fprintf(logfp, "    Index of best fitting model = %d\n", mopt);
            fprintf(logfp, "    Best fitting model\n");
            k = 0;
            fprintf(logfp, "    ");
            if (nasp->epifix)
                fprintf(logfp, "%7.3f %8.3f ", nasp->lat, nasp->lon);
            else {
                fprintf(logfp, "%7.3f %8.3f ", model_opt[k], model_opt[k+1]);
                k += 2;
            }
            if (nasp->otfix)
                EpochToHuman(timestr, nasp->ot);
            else {
                EpochToHuman(timestr, model_opt[k]);
                k++;
            }
            fprintf(logfp, "%s ", timestr);
            if (nasp->depfix)
                fprintf(logfp, "%7.3f ", nasp->depth);
            else
                fprintf(logfp, "%7.3f ", model_opt[k]);
            fprintf(logfp, "\n");
            if (verbose > 4)
                WriteNAModels(na_models, nasp, misfit);
        }
/*
 *      save best model in SOLREC and PHAREC
 */
        NAForwardProblem(nsta, nasp, model_opt, sp, rdindx, pgs,
                TTtables, LocalTTtable, ec, topo,
                stalist, distmatrix, variogramp, buf, is2nderiv, 1);
        if (verbose > 2) {
            PrintSolution(sp, 0);
            PrintDefiningPhases(np, pgs);
        }
        j = 0;
    }
    else {
        if (verbose) fprintf(logfp, "    No acceptable model was found!\n");
        j = 1;
    }
/*
 *  free memory
 */
    FreeFloatMatrix(na_models);
    Free(iwork_NA1); Free(iwork_NA2); Free(work_NA2);
    Free(misfit); Free(mfitord);
    FreeLongMatrix(sas.iv);
    Free(sas.pol); Free(sas.mdeg);
    Free(rdindx); Free(pgs); Free(dlist);
    DoCorrelatedErrors = DoCorrelatedErrors_cf;
    na_sobol(sas.n, &dummy, 1, 2, &sas);
    return j;
}


/*
 *  Title:
 *     dosamples
 *  Synopsis:
 *     Calculates the misfit of a sample model
 *     Forward modeling uses all defining phases and
 *        accounts for correlated errors.
 *  Input Arguments:
 *     i          - sample index
 *     ntot       - number of collected samples
 *     na_model   - sample model
 *     nasp       - NA search parameter structure
 *     np         - number of P and S type phases
 *     nsta       - number of stations
 *     sp         - pointer to current solution
 *     rdindx     - array of reading structures
 *     pgs        - array of phase structures
 *     TTtables  - pointer to travel-time tables
 *     ec         - pointer to ellipticity correction coefficient structure
 *     topo       - ETOPO bathymetry/elevation matrix
 *     stalist    - array of starec structures
 *     distmatrix - station separation matrix
 *     variogramp - pointer to generic variogram model
 *     fp         - file pointer to grid search results
 *  Return:
 *     misfit    - Lp-norm misfit of the sample model
 *  Called by:
 *     NASearch
 *  Calls:
 *     NAForwardProblem, tolatlon, transform2raw, Free
 */
static double dosamples(int i, int ntot, double *na_model, NASPACE *nasp,
        int np, int nsta, SOLREC *sp, READING *rdindx, PHAREC *pgs,
        TT_TABLE *TTtables, TT_TABLE *LocalTTtable, EC_COEF *ec,
        short int **topo, STAREC stalist[], double **distmatrix,
        VARIOGRAM *variogramp, FILE *fp, int is2nderiv)
{
    SOLREC s;                                           /* solution record */
    PHAREC *pset = (PHAREC *)NULL;        /* phase records for grid search */
    double model_raw[NA_MAXND];
    double misfit = 9999.;
    char buf[64];
    int j, k;
    j = ntot + i;
    if ((pset = (PHAREC *)calloc(np, sizeof(PHAREC))) == NULL) {
        fprintf(errfp, "dosamples: cannot allocate memory!\n");
        fprintf(logfp, "dosamples: cannot allocate memory!\n");
        return misfit;
    }
/*
 *  make a copy of defining phases and the solution
 *  in order to not to interfere with phase identifications
 */
    for (k = 0; k < np; k++) {
        memmove(&pset[k], &pgs[k], sizeof(PHAREC));
    }
    memmove(&s, sp, sizeof(SOLREC));
/*
 *  calculate misfit value for each model
 */
    transform2raw(na_model, nasp, model_raw);
    if (!nasp->epifix)
        tolatlon(model_raw, nasp);
    misfit = NAForwardProblem(nsta, nasp, model_raw, &s, rdindx, pset,
                     TTtables, LocalTTtable, ec, topo, stalist, distmatrix,
                     variogramp, buf, is2nderiv, 0);
    if (WriteNAResultsToFile) {
/*
 *      print results to file
 */
        k = 0;
        fprintf(fp, "%4d %3d ", j, i);
        if (nasp->epifix)
            fprintf(fp, "%8.4f %9.4f ", nasp->lat, nasp->lon);
        else {
            fprintf(fp, "%8.4f %9.4f ", model_raw[k], model_raw[k+1]);
            k += 2;
        }
        if (nasp->otfix)
            fprintf(fp, "%15.3f ", nasp->ot);
        else {
            fprintf(fp, "%15.3f ", model_raw[k]);
            k++;
        }
        if (nasp->depfix) fprintf(fp, "%8.4f ", nasp->depth);
        else              fprintf(fp, "%8.4f ", model_raw[k]);
        fprintf(fp, "%10.4f %s\n", misfit, buf);
    }
    Free(pset);
    return misfit;
}

/*
 *  Title:
 *     NAForwardProblem
 *  Synopsis:
 *     returns misfit value (defined by lpnorm) for a given model
 *  Input Arguments:
 *     nsta       - number of stations
 *     nasp       - NA search parameter structure
 *     model      - sample model
 *     sp         - pointer to current solution
 *     rdindx     - array of reading structures
 *     pgs        - array of phase structures
 *     TTtables  - pointer to travel-time tables
 *     ec         - pointer to ellipticity correction coefficient structure
 *     topo       - ETOPO bathymetry/elevation matrix
 *     stalist    - array of starec structures
 *     distmatrix - station separation matrix
 *     variogramp - pointer to generic variogram model
 *  Return:
 *     misfit    - Lp-norm misfit of the sample model
 *  Called by:
 *     NASearch, do_samples
 *  Calls:
 *     GetDeltaAzimuth, ReIdentifyPhases, DuplicatePhases,
 *     GetDataCovarianceMatrix, AllocateFloatMatrix, FreeFloatMatrix,
 *     GetTravelTimePrediction, Free
 */
static double NAForwardProblem(int nsta, NASPACE *nasp, double *model,
        SOLREC *sp, READING *rdindx, PHAREC pgs[], TT_TABLE *TTtables,
        TT_TABLE *LocalTTtable, EC_COEF *ec, short int **topo, STAREC stalist[],
        double **distmatrix, VARIOGRAM *variogramp, char *buf, int is2nderiv,
        int isprint)
{
    double z = 0., totnp = 0.;
    double misfit = 9999., sum = 0., norm = 0., penal = 0.;
    int i, k, prank = 0, ndef = 0, np = 0;
    double **dcov = (double **)NULL;
    double **w = (double **)NULL;
    double *d = (double *)NULL;
    double *temp = (double *)NULL;
/*
 *  current hypocenter
 */
    i = 0;
    if (!nasp->epifix) {
        sp->lat = model[i++];
        sp->lon = model[i++];
    }
    if (!nasp->otfix)
        sp->time = model[i++];
    if (!nasp->depfix)
        sp->depth = model[i];
/*
 *  identify phases according to the current hypocenter
 */
    np = sp->numPhase;
    totnp = 2. * (double)np;
    GetDeltaAzimuth(sp, pgs, 0);
    ReIdentifyPhases(sp, rdindx, pgs, ec, TTtables, LocalTTtable, topo, is2nderiv);
    DuplicatePhases(sp, pgs, ec, TTtables, LocalTTtable, topo);
    ndef = 0;
    for (i = 0; i < np; i++) {
        if (pgs[i].timedef) ndef++;
        if (pgs[i].azimdef) ndef++;
        if (pgs[i].slowdef) ndef++;
    }
    prank = ndef;
    sprintf(buf, "%10.4f %10.4f %4d %4d %4d", misfit, misfit, np, ndef, prank);
    if (ndef < nasp->nd)
        return misfit;
    d = (double *)calloc(ndef, sizeof(double));
    if ((temp = (double *)calloc(ndef, sizeof(double))) == NULL) {
        Free(d);
        return misfit;
    }
/*
 *  correlated errors
 */
    if (DoCorrelatedErrors) {
/*
 *      construct data covariance matrix
 */
        if ((dcov = GetDataCovarianceMatrix(nsta, np, ndef, pgs,
                         stalist, distmatrix, variogramp)) == NULL) {
            Free(d); Free(temp);
            return misfit;
        }
/*
 *      projection matrix
 */
        if ((w = AllocateFloatMatrix(ndef, ndef)) == NULL) {
            FreeFloatMatrix(dcov);
            Free(d); Free(temp);
            return misfit;
        }
        if (ProjectionMatrix(np, pgs, ndef, 95., dcov, w,
                              &prank, 0, (char **)NULL, 1)) {
            FreeFloatMatrix(dcov);
            FreeFloatMatrix(w);
            Free(d); Free(temp);
            return misfit;
        }
        if (prank < nasp->nd) {
            FreeFloatMatrix(dcov);
            FreeFloatMatrix(w);
            Free(d); Free(temp);
            return misfit;
        }
    }
/*
 *  loop over phases
 */
    k = 0;
    for (i = 0; i < np; i++) {
        if (pgs[i].timedef) {
/*
 *          residual = observed - predicted travel time
 */
            d[k] = pgs[i].timeres;
            k++;
        }
    }
    for (i = 0; i < np; i++) {
        if (pgs[i].azimdef) {
/*
 *          residual = observed - predicted azimuth
 */
            pgs[i].azimres = pgs[i].azim - pgs[i].seaz;
            d[k] = DEG_TO_RAD * pgs[i].azimres;
            k++;
        }
    }
    for (i = 0; i < np; i++) {
        if (pgs[i].slowdef) {
/*
 *          residual = observed - predicted slowness
 */
            pgs[i].slowres = pgs[i].slow - pgs[i].dtdd;
            d[k] = pgs[i].slowres / DEG2KM;
            k++;
        }
    }
    if (DoCorrelatedErrors) {
/*
 *      project residuals
 */
        for (i = 0; i < ndef; i++) {
            temp[i] = 0.;
            for (k = 0; k < ndef; k++) {
                temp[i] += w[i][k] * d[k];
            }
            if (fabs(temp[i]) < ZERO_TOL) temp[i] = 0.;
        }
        for (i = 0; i < ndef; i++) {
            d[i] = temp[i];
        }
    }
    else {
/*
 *      weight residuals
 */
        k = 0;
        for (i = 0; i < np; i++) {
            if (!pgs[i].timedef || pgs[i].deltim < DEPSILON)
                continue;
            d[k] /= pgs[i].deltim;
            k++;
        }
        for (i = 0; i < np; i++) {
            if (!pgs[i].azimdef || pgs[i].delaz < DEPSILON)
                continue;
            d[k] /= pgs[i].delaz;
            k++;
        }
        for (i = 0; i < np; i++) {
            if (!pgs[i].slowdef || pgs[i].delslo < DEPSILON)
                continue;
            d[k] /= pgs[i].delslo;
            k++;
        }
    }
/*
 *  Lp-norm misfit
 */
    sum = 0.;
    for (i = 0; i < ndef; i++) {
        if (nasp->lpnorm - 1. < 0.01)
            z = fabs(d[i]);
        else
            z = pow(fabs(d[i]), nasp->lpnorm);
        sum += z;
    }
//    misfit = sum / (double)(prank - nasp->nd);
/*
 *  penalize low-ndef solutions when calculating misfit
 *
 *
 *                ||d||             NP - Ndef
 *     misfit = --------- + alpha * ---------
 *              Nrank - M              NP
 *
*/
    z = (double)max(prank - nasp->nd, 1);
    norm = sum / z;
    penal = 4.0 * (totnp - (double)ndef) / totnp;
    misfit = norm + penal;
    sprintf(buf, "%10.4f %10.4f %4d %4d %4d", norm, penal, (int)totnp, ndef, prank);
    FreeFloatMatrix(dcov);
    FreeFloatMatrix(w);
    Free(d);
    Free(temp);
    return misfit;
}

/*
 *
 *  na_initialize - performs minor initialization tasks for NA algorithm
 *
 */
static int na_initialize(NASPACE *nasp, double *xcur, SOBOL *sas)
{
    double rval[2], dummy = 0.;
    int nd = nasp->nd, i;
/*
 *  initialize 1-D Sobol sequence to control initial dimension in NA walk
 */
    sobseq(-1, rval);
/*
 *  initialize pseudo-random number generator
 */
    ranfib(1, iseed);
/*
 *  generate coefficients for quasi-random multidimensional SAS sequence
 */
    na_sas_table(sas);
/*
 *  initialize n-D Sobol sequence
 */
    if (na_sobol(sas->n, &dummy, 1, 1, sas))
        return 1;
/*
 *  normalize parameter ranges by a priori model covariances
 */
    if (nasp->scale[0] == 0.0) {
/*
 *      unit a priori model covariances
 */
        for (i = 0; i < nd; i++) {
            nasp->ranget[i][0] = nasp->range[i][0];
            nasp->ranget[i][1] = nasp->range[i][1];
            nasp->scale[i+1] = 1.;
        }
    }
    else if (nasp->scale[0] == -1.) {
/*
 *      use parameter range as a priori model covariances
 */
        for (i = 0; i < nd; i++) {
            nasp->ranget[i][0] = 0.;
            nasp->ranget[i][1] = 1.;
            nasp->scale[i+1] = nasp->range[i][1] - nasp->range[i][0];
        }
    }
    else {
/*
 *      use scale array as a priori model covariances
 */
        for (i = 0; i < nd; i++) {
            nasp->ranget[i][0] = 0.;
            nasp->ranget[i][1] = 1.;
            if (nasp->scale[i+1] > 0.)
                nasp->ranget[i][1] = (nasp->range[i][1] - nasp->range[i][0]) /
                                      nasp->scale[i+1];
        }
    }
/*
 *  initialize current point to mid-point of parameter space
 */
    for (i = 0; i < nd; i++)
        xcur[i] = (nasp->ranget[i][0] + nasp->ranget[i][1]) / 2.;
/*
 *  verbose
 */
    if (verbose) {
        fprintf(logfp, "      Parameter ranges\n");
        fprintf(logfp, "        Number       Minimum         Maximum   ");
        fprintf(logfp, "prior_Cov Scaled min  Scaled max\n");
        for (i = 0; i < nd; i++)
            fprintf(logfp, "    %7d  %15.4f %15.4f %10.4f %10.4f %10.4f\n",
                    i + 1, nasp->range[i][0], nasp->range[i][1],
                    nasp->scale[i+1], nasp->ranget[i][0], nasp->ranget[i][1]);
    }
    return 0;
}

/*
 *
 *  na_initial_sample - generates initial sample for NA algorithm
 *
 *        Assumes n-dimensional Sobol sequence has been initialized
 *        Assumes ranfib has been initialized
 *        Will generate a minimum of two samples
 */
static int na_initial_sample(double *na_models[], NASPACE *nasp, SOBOL *sas)
{
    int i, j, k, nd = nasp->nd;
    double a = 0., b = 0.;
/*
 *  Generate initial random sample using quasi random sequences
 */
    if (verbose > 3) fprintf(logfp, "                initial sample:\n");
    for (i = 0; i < NAinitialSample; i++) {
        j = 0;
        if (i == 0) {
/*
 *          include initial hypocentre in the initial sample
 */
            if (!nasp->epifix) {
                na_models[i][j++] = 0.;
                na_models[i][j++] = 0.;
            }
            if (!nasp->otfix)
                na_models[i][j++] = 0.5;
            if (!nasp->depfix)
                na_models[i][j] = (nasp->depth - nasp->range[j][0]) /
                                  (nasp->range[j][1] - nasp->range[j][0]);
        }
        else {
/*
 *          generate the rest of the trial hypocentres
 */
            if (!nasp->epifix) {
                na_sobol(j, &a, 1, 0, sas);
                na_models[i][j] = Sqrt(a) * nasp->ranget[j][1];
                j++;
            }
            for (; j < nd; j++) {
                na_sobol(j, &a, 1, 0, sas);
                b = 1. - a;
                na_models[i][j] = b * nasp->ranget[j][0] +
                                  a * nasp->ranget[j][1];
            }
        }
        if (verbose > 3) {
            k = 0;
            fprintf(logfp, "                  %4d ", i);
            if (nasp->epifix)
                fprintf(logfp, "%7.3f %8.3f ", nasp->lat, nasp->lon);
            else {
                fprintf(logfp, "%7.3f %8.3f ",
                        na_models[i][k], na_models[i][k+1]);
                k += 2;
            }
            if (nasp->otfix)
                fprintf(logfp, "%15.3f ", nasp->ot);
            else {
                fprintf(logfp, "%15.3f ", na_models[i][k]);
                k++;
            }
            if (nasp->depfix)
                fprintf(logfp, "%7.3f", nasp->depth);
            else
                fprintf(logfp, "%7.3f", na_models[i][k]);
            fprintf(logfp, "\n");
        }
    }
    return 0;
}

/*
 *  na_sample - generates a new sample of models using the Neighbourhood
 *              algorithm by distributing nsample new models in ncells cells.
 *
 *  Comments:
 *       If xcur is changed between calls then restartNA must be set to true.
 *       restartNA must also be set to true on the first call.
 *
 *       Calls are made to various NA_routines.
 *
 *                      M. Sambridge
 *                      Last updated Sept. 1999.
 *
 */
static int na_sample(double *na_models[], NASPACE *nasp, int ntot,
                     int *mfitord, double *xcur, int *restartNA, int nclean,
                     double *dlist, SOBOL *sas, int *nu)
{
    static int id = 0, ic = 0;
    int idnext = 0, cell = 0, icount = 0, nc = 0, nup = 0;
    int nrem = 0, nsampercell = 0, i, j, is = 0, iw = 0, resetlist = 0;
    int ind_cell = 0, ind_nextcell = 0, ind_lastcell = 0, mopt = 0;
    int nodex = 0, nnode = 0, nd = 0, kd = 0;
    double x1 = 0., x2 = 0., xdum[NA_MAXND];
/*
 *  initializations
 */
    if (verbose > 4) fprintf(logfp, "                next sample:\n");
    nd = nasp->nd;
    idnext = irandomvalue(0, nd - 1);
    ic++;
    if (!(ic % nclean)) resetlist = 1;
    mopt = mfitord[cell];
    ind_nextcell = mopt;
    ind_lastcell = 0;
    nrem = NAnextSample % NAcells;
    if (nrem == 0) nsampercell = NAnextSample / NAcells;
    else           nsampercell = 1 + NAnextSample / NAcells;
/*
 *  loop over samples
 */
    for (is = 0; is < NAnextSample; is++) {
        ind_cell = ind_nextcell;
        icount++;
/*
 *      reset walk to chosen model
 */
        if (ind_cell != ind_lastcell)
            *restartNA = na_restart(na_models, nd, ind_cell, xcur);
        if (*restartNA) {
            *restartNA = 0;
            resetlist = 1;
        }
        for (iw = 0; iw < nd; iw++) {
/*
 *          reset dlist and nodex for new axis
 */
            if (resetlist) {
                nodex = NNcalc_dlist(idnext, dlist, na_models, nd, ntot, xcur);
                nc++;
                resetlist = 0;
            }
/*
 *          update dlist and nodex for new axis
 */
            else {
                nodex = NNupdate_dlist(idnext, id, dlist, na_models, ntot,
                                       xcur);
                nup++;
            }
            id = idnext;
/*
 *          calculate intersection of current Voronoi cell with current 1D axis
 */
            NNaxis_intersect(id, dlist, na_models, ntot, nodex, nasp, &x1, &x2);
/*
 *          generate new node in Voronoi cell of input point
 */
            kd = id + cell * nd;
            xcur[id] = na_deviate(x1, x2, kd, sas);
/*
 *          check Voronoi boundaries
 */
            if (verbose > 4) {
                for (i = 0; i < nd; i++) xdum[i] = xcur[i];
                xdum[id] = x1;
                nnode = findnearest(xdum, na_models, ntot, nd);
                fprintf(logfp, "                Nearest node to x1 = %d\n",
                        nnode);
                xdum[id] = x2;
                nnode = findnearest(xdum, na_models, ntot, nd);
                fprintf(logfp, "                Nearest node to x2 = %d\n",
                        nnode);
            }
/*
 *          increment axis
 */
            idnext++;
            if (idnext == nd) idnext = 0;
        }
/*
 *      put new sample in list
 */
        j = ntot + is;
        for (i = 0; i < nd; i++) na_models[j][i] = xcur[i];
/*
 *      check nearest node
 */
        if (verbose > 4) {
            nnode = findnearest(xcur, na_models, ntot, nd);
            fprintf(logfp, "               Nearest node to new model %d = %d\n",
                    j, nnode);
            if (nnode != nodex) {
                fprintf(logfp, "              Node outside Voronoi cell!\n");
                fprintf(logfp, "                original cell = %d\n", nodex);
                fprintf(logfp, "                     new cell = %d\n", nnode);
                fprintf(logfp, "                j = %d, iw = %d id = %d\n",
                        j, iw, id);
            }
        }
        ind_lastcell = ind_cell;
        if (icount == nsampercell) {
            icount = 0;
            cell++;
            ind_nextcell = mfitord[cell];
            if (cell == nrem) nsampercell--;
        }
    }
    *nu = nup;
    return nc;
}

/*
 *
 * na_restart - resets NA walk to start from input model
 *
 */
static int na_restart(double *na_models[], int nd, int ind, double *x)
{
    int i, restartNA = 1;
    for (i = 0; i < nd; i++)
        x[i] = na_models[ind][i];
    return restartNA;
}

/*
 *
 *  NNcalc_dlist - calculates square of distance from all base points to
 *                 new axis (defined by dimension dim through point x).
 *                 Updates the nearest node and distance to the point x.
 *
 *     This is a full update of dlist, i.e. not using a previous dlist.
 *
 */
static int NNcalc_dlist(int dim, double *dlist, double *na_models[],
                        int nd, int ntot, double *x)
{
    int nodex = 0, i, j;
    double dmin = 1e6, dsum = 0., d = 0.;
    for (i = 0; i < ntot; i++) {
        dsum = 0.;
        for (j = 0; j < dim; j++) {
            d = x[j] - na_models[i][j];
            dsum += d * d;
        }
        for (j = dim + 1; j < nd; j++) {
            d = x[j] - na_models[i][j];
            dsum += d * d;
        }
        dlist[i] = dsum;
        d = x[dim] - na_models[i][dim];
        dsum += d * d;
        if (dsum < dmin) {
            dmin = dsum;
            nodex = i;
        }
    }
    return nodex;
}

/*
 *
 * NNupdate_dlist - calculates square of distance from all base points to
 *                  new axis, assuming dlist contains square of all distances
 *                  to previous axis dimlast. It also updates the nearest node
 *                  to the point x through which the axes pass.
 *
 */
static int NNupdate_dlist(int idnext, int id, double *dlist,
                          double *na_models[], int ntot, double *x)
{
    int nodex = 0, i;
    double dmin = 1e6, d1 = 0., d2 = 0.;
    for (i = 0; i < ntot; i++) {
        d1 = x[id] - na_models[i][id];
        d1 = dlist[i] + d1 * d1;
        if (d1 < dmin) {
            dmin = d1;
            nodex = i;
        }
        d2 = x[idnext] - na_models[i][idnext];
        dlist[i] = d1 - d2 * d2;
    }
    return nodex;
}

/*
 *
 * NNaxis_intersect - find intersections of current Voronoi cell
 *                    with current 1-D axis.
 *
 *     Input:
 *        x(nd)     : point on axis
 *        id        : dimension index (defines axis)
 *        dlist     : set of distances of base points to axis
 *        na_models(nd,ntot) : set of base points
 *        nd        : number of dimensions
 *        ntot      : number of base points
 *        nodex     : index of base node closest to x
 *        xmin      : start point along axis
 *        xmax      : end point along axis
 *
 *     Output:
 *        x1        :intersection of first Voronoi boundary
 *        x2        :intersection of second Voronoi boundary
 *
 *     Comment:
 *          This method uses a simple formula to exactly calculate
 *          the intersections of the Voronoi cells with the 1-D axis.
 *          It makes use of the perpendicluar distances of all nodes
 *          to the current axis contained in the array dlist.
 *
 *          The method involves a loop over ensemble nodes for
 *          each new intersection found. For an axis intersected
 *          by ni Voronoi cells the run time is proportional to ni*ne.
 *
 *          It is assumed that the input point x(nd) lies in
 *          the Vcell of nodex, i.e. nodex is the closest node to x(nd).
 *
 *     Note: If the intersection points are outside of either
 *           axis range then the axis range is returned, i.e.
 *
 *                    x1 is set to max(x1,xmin) and
 *                    x2 is set to min(x2,xmin) and
 *
 *                                     M. Sambridge, RSES, June 1998
 *
 */
static void NNaxis_intersect(int id, double *dlist, double *na_models[],
                             int ntot, int nodex, NASPACE *nasp,
                             double *x1, double *x2)
{
    int i;
    double lo, hi, x0, dp0, xmin, xmax;
    double xc = 0., dpc = 0., dx = 0., xi = 0.;
    lo = xmin = nasp->ranget[id][0];
    hi = xmax = nasp->ranget[id][1];
    x0 = na_models[nodex][id];
    dp0 = dlist[nodex];
/*
 *  find intersection of current Voronoi cell with 1D axis
 */
    for (i = 0; i < ntot; i++) {
        if (i == nodex) continue;
        xc = na_models[i][id];
        dpc = dlist[i];
/*
 *      calculate intersection between nodes nodex and i and the 1D axis
 */
        dx = x0 - xc;
        if (fabs(dx) > DEPSILON) {
            xi = 0.5 * (x0 + xc +(dp0 - dpc) / dx);
            if (xmin < xi && xi < xmax) {
                if (xi > lo && x0 > xc)
                    lo = xi;
                if (xi < hi && x0 < xc)
                    hi = xi;
            }
        }
    }
    *x1 = lo;
    *x2 = hi;
}

/*
 *
 *   NA_deviate - generates a random deviate according to
 *                a given distribution using a 1-D SAS sequence.
 *
 *   Comments:
 *         This routine generates a random number between x1 and x2.
 *         The parameter i is the sequence number from
 *         which the quasi random deviate is drawn.
 *
 *      This version is for resample mode and simply generates
 *      a deviate between input values x1 and x2.
 *
 */
static double na_deviate(double x1, double x2, int i, SOBOL *sas)
{
    double x = 0., deviate = 0.;
    na_sobol(i, &x, 1, 0, sas);
    deviate = x1 + (x2 - x1) * x;
    return deviate;
}

/*
 *
 * findnearest - finds nearest model to input point
 *
 */
static int findnearest(double *xcur, double *na_models[], int ntot, int nd)
{
    int i, j, nnode = 0;
    double dmin = 1e6, dsum = 0., d = 0.;
    for (i = 0; i < ntot; i++) {
        dsum = 0.;
        for (j = 0; j < nd; j++) {
            d = xcur[j] - na_models[i][j];
            dsum += d * d;
        }
        if (dsum < dmin) {
            dmin = dsum;
            nnode = i;
        }
    }
    return nnode;
}

/*
 *
 * na_misfits - calculate performance statistics for NA algorithm
 *
 */
static void na_misfits(double *misfit, int ns, int ntot, double *mfitmin,
                       double *mfitminc, double *mfitmean, int *mopt,
                       double *work, int *ind, int *iwork, int *mfitord)
{
    int ibest = 0, i, n;
    double mean = 0., minc = 1e6;
/*
 *  current sample stats
 */
    n = ntot + ns;
    for (i = ntot; i < n; i++) {
        mean += misfit[i];
        if (misfit[i] < minc) {
            minc = misfit[i];
            ibest = i;
        }
    }
    mean /= (double)ns;
    if (minc < *mfitmin) {
        *mfitmin = minc;
        *mopt = ibest;
    }
    *mfitmean = mean;
    *mfitminc = minc;
/*
 *  entire population: find first ncells model with lowest misfit
 */
    if (NAcells == 1)
        mfitord[0] = *mopt;
    else {
        for (i = 0; i < n; i++) {
            ind[i] = i;
            work[i] = misfit[i];
        }
        jumble(ind, work, n);
        selecti(NAcells, n, work, ind);
        for (i = 0; i < NAcells; i++)
            iwork[i] = ind[i];
        indexx(NAcells, work, ind);
        for (i = 0; i < NAcells; i++)
            mfitord[i] = iwork[ind[i]];
    }
}

/*
 *
 * transform2raw - transforms model from scaled to raw units.
 *
 *  Input:
 *       nd            : dimension of parameter space
 *       model_sca(nd) : model in scaled co-ordinates
 *       range(2,nd)   : min and max of parameter space in raw co-ordinates
 *       scales(nd+1)  : range scale factors
 *  Output:
 *       model_raw(nd) : model in scaled co-ordinates
 *  Comments:
 *       This routine transforms a model in dimensionless scaled
 *       co-ordinates to input (raw) units.
 *                                             M. Sambridge, March 1998
 */
static void transform2raw(double *model_sca, NASPACE *nasp, double *model_raw)
{
    int i, nd;
    double a = 0., b = 0.;
    nd = nasp->nd;
    if (nasp->scale[0] == 0.0) {
/*
 *      unit a priori model covariances
 */
        for (i = 0; i < nd; i++)
            model_raw[i] = model_sca[i];
    }
    else if (nasp->scale[0] == -1.) {
/*
 *      use parameter range as a priori model covariances
 */
        for (i = 0; i < nd; i++) {
            b = model_sca[i];
            a = 1. - b;
            model_raw[i] = a * nasp->range[i][0] + b * nasp->range[i][1];
        }
    }
    else {
/*
 *      use scale array as a priori model covariances
 */
        for (i = 0; i < nd; i++)
            model_raw[i] = nasp->range[i][0] + nasp->scale[i+1] * model_sca[i];
    }
}

/*
 *
 * tolatlon - transforms model from raw (delta, azim) to geographic coords
 *
 */
static void tolatlon(double *model_raw, NASPACE *nasp)
{
    double lat = 0., lon = 0.;
    PointAtDeltaAzimuth(nasp->lat, nasp->lon, model_raw[0], model_raw[1],
                        &lat, &lon);
    model_raw[0] = lat;
    model_raw[1] = lon;
}

/*
 *
 *  jumble - randomly re-arranges input array
 *
 */
static void jumble(int *iarr, double *arr, int n)
{
    double rn = 0., rval = 0., temp = 0.;
    int j, k, itemp = 0;
    rn = (double)n;
    for (j = 0; j < n; j++) {
        rval = ranfib(0, 0L);
        k = (int)(rval * rn);
        if (k < n) {
            swap(arr[j], arr[k]);
            swapi(iarr[j], iarr[k]);
        }
    }
}

/*
 *
 * irandomvalue - generates a random integer between lo and hi.
 *
 *      Uses Sobol-Antonov_Saleev quasi-sequence
 *      Assumes that random sequence has been initialized.
 *
 *                                             M. Sambridge, Aug. 1997
 */
static int irandomvalue(int lo, int hi)
{
    int irnum = 0;
    double rval[2];
    sobseq(1, rval);
    irnum = lo + (int)rval[0] * ( hi - lo + 1);
    return irnum;
}

/*
 *
 *  Numerical recipes routine adapted to give ind
 *
 */
static double selecti(int k, int n, double *arr, int *ind)
{
    int i = 0, ia = 0, ir = 0, j = 0, l = 0, mid = 0, itemp = 0;
    double a = 0., temp = 0.;
    ir = n - 1;
    for (;;) {
        if (ir <= l + 1) {
            if (ir == l + 1 && arr[ir] < arr[l]) {
                swap(arr[l], arr[ir]);
                swapi(ind[l], ind[ir]);
            }
            return arr[k];
        }
        else {
            mid = (l + ir) >> 1;
            swap(arr[mid], arr[l+1]);
            swapi(ind[mid], ind[l+1]);
            if (arr[l] > arr[ir]) {
                swap(arr[l], arr[ir]);
                swapi(ind[l], ind[ir]);
            }
            if (arr[l+1] > arr[ir]) {
                swap(arr[l+1], arr[ir]);
                swapi(ind[l+1], ind[ir]);
            }
            if (arr[l] > arr[l+1]) {
                swap(arr[l], arr[l+1]);
                swapi(ind[l], ind[l+1]);
            }
            i = l + 1;
            j = ir;
            a = arr[l+1];
            ia = ind[l+1];
            for (;;) {
                do i++; while (arr[i] < a);
                do j--; while (arr[j] > a);
                if (j < i) break;
                swap(arr[i], arr[j]);
                swapi(ind[i], ind[j]);
            }
            arr[l+1] = arr[j];
            arr[j] = a;
            ind[l+1] = ind[j];
            ind[j] = ia;
            if (j >= k) ir = j - 1;
            if (j <= k) l = i;
        }
    }
}

#define NSTACK 64
/*
 *
 *  Numerical recipes routine
 *
 */
static void indexx(int n, double *arr, int *indx)
{
    int M = 7;
    int i = 0, indxt = 0, ir = 0, itemp = 0, j = 0, k = 0, l = 0;
    int jstack = -1;
    double a = 0.;
    int istack[NSTACK];
    ir = n - 1;
    for (j = 0; j < n; j++) indx[j] = j;
    for (;;) {
        if (ir - l < M) {
            for (j = l + 1; j <= ir; j++) {
                indxt = indx[j];
                a = arr[indxt];
                for (i = j - 1; i >= l; i--) {
                    if (arr[indx[i]] <= a) break;
                    indx[i+1] = indx[i];
                }
                indx[i+1] = indxt;
            }
            if (jstack < 0)
                break;
            ir = istack[jstack--];
            l = istack[jstack--];
        }
        else {
            k = (l + ir) >> 1;
            swapi(indx[k], indx[l+1]);
            if (arr[indx[l]] > arr[indx[ir]])
                swapi(indx[l], indx[ir]);
            if (arr[indx[l+1]] > arr[indx[ir]])
                swapi(indx[l+1], indx[ir]);
            if (arr[indx[l]] > arr[indx[l+1]])
                swapi(indx[l], indx[l+1]);
            i = l + 1;
            j = ir;
            indxt = indx[l+1];
            a = arr[indxt];
            for (;;) {
                do i++; while (arr[indx[i]] < a);
                do j--; while (arr[indx[j]] > a);
                if (j < i) break;
                swapi(indx[i], indx[j]);
            }
            indx[l+1] = indx[j];
            indx[j] = indxt;
            jstack += 2;
            if (jstack >= NSTACK) {
                fprintf(errfp, "NSTACK too small in index!\n");
                fprintf(logfp, "NSTACK too small in index!\n");
            }
            if (ir - i + 1 >= j - l) {
                istack[jstack] = ir;
                istack[jstack-1] = i;
                ir = j - 1;
            }
            else {
                istack[jstack] = j - 1;
                istack[jstack-1] = l;
                l = i;
            }
        }
    }
}

/*
 *
 *  na_sas_table - uses a pseudo random number generator to build
 *                 the initializing data for the quasi random SAS sequence.
 *
 *  Input:
 *  nt            Number of sequences to be generated
 *
 *  Comments:
 *  This routine generates initializing data for multiple
 *  Sobol-Antonov-Saleev quasi-random sequences. For each
 *  sequence a degree and order of the primitive polynomial
 *  are required, and here they are determined by a particular
 *  formula (below).
 *
 *  For each degree and order pair (q,p) q initializing integers are
 *  required for each sequence, (M1, M2, ..., Mq), where
 *  Mi may be any odd integer less than 2**i. So
 *  for the i-th term, there are 2**(i-1) possible values. We write,
 *
 *             Nq = 2**(i-1).
 *
 *  Since each initializing datum is independent, the total number
 *  of possible sequences for degree q is the product,
 *
 *             Ntotal =  N1 x N2 x N3 x ... Nq,
 *
 *  which gives,
 *
 *             Ntotal = prod (for i=1,...,q) 2**(i-1),
 *             Ntotal = 2**[sum(for i=1,...,q) (i-1))]
 *             Ntotal = 2**(q*(q-1)/2)
 *
 *  Which gives,
 *
 *     q           Ntotal      Nq   Number of primitive polynomials (Np)
 *     1                1       1            1
 *     2                2       2            1
 *     3                8       4            2
 *     4               64       8            2
 *     5             1024      16            6
 *     6            32768      32            6
 *     7          2097152      64           18
 *     8        268435456     128           16
 *     9     6.8719476E10     256           48
 *    10     3.5184372E13     512           60
 *
 *  Note the number of possible primitive polynomial orders (Np)
 *  and their values are defined the degree.
 *  All possible values of polynomial order for degrees up to 10 are
 *  contained in the array pporder.
 *  (A table can also be found on p 302 of Numerical Recipes in
 *  Fortran 2d Ed. Press et al 1992)
 *  The product of Np and Ntotal is the total number of possible
 *  sequences for that degree.
 *
 *  When generating a large number of independent sequences using randomly
 *  generated initializing data it is prudent to use only higher degrees
 *  because for, say degree 4 there are only 64 possible sequences for
 *  each of the two polynomial order values, and so
 *  if more than 64 are generated some will be duplicates and hence
 *  will produce identical (and not independent) sequences.
 *
 *  Array pporder contains all possible primitive polynomial orders
 *  for each degree up to 10.
 *
 *  The particular formula used here to choose degree and polynomial
 *  order for each independent sequence is to cycle through each degree
 *  (starting from 5) and take ntotal/10 sequences from that degree.
 *  Once the degree is chosen, the polynomial order cycles through
 *  its possible values (given by array pporder). The objective here
 *  is to minimize the likelihood of repeated trials with different
 *  random seeds reproducing the same sequence. Remember there are
 *  a finite number of sequences for each degree (see above).
 *
 *  Calls ranfib and assumes that this pseudo random number generator
 *  has been initialized.
 *
 *                      M. Sambridge, Aug. 1999
 *
 */
static void na_sas_table(SOBOL *sas)
{
    static double ntot[NA_MAXDEG] = {
        1., 2., 8., 64., 1024., 32768., 2097152.,
        268435456., 6.8719476E10, 3.5184372E13
    };
    static int nprim[NA_MAXDEG] = {
        1, 1, 2, 2, 6, 6, 18, 16, 48, 60
    };
    static unsigned long pporder[NA_MAXDEG][60] = {
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        },
        { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        },
        { 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        },
        { 1, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        },
        { 2, 4, 7, 11, 13, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        },
        { 1, 13, 16, 19, 22, 25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        },
        {   1,   4,   7,   8,  14,  19,  21,  28,  31,  32,  37,  41,
           42,  50,  55,  56,  59,  62,   0,   0,   0,   0,   0,   0,
            0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
            0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
            0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
        },
        {  14,  21,  22,  34,  47,  49,  50,  52,  56,  67,  70,  84,
           97, 103, 115, 122,   0,   0,   0,   0,   0,   0,   0,   0,
            0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
            0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
            0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
        },
        {   8,  13,  16,  22,  25,  44,  47,  52,  55,  59,  62,  67,
           74,  81,  82,  87,  91,  94, 103, 104, 109, 122, 124, 137,
          138, 143, 145, 152, 157, 167, 173, 176, 181, 182, 185, 191,
          194, 199, 218, 220, 227, 229, 230, 234, 236, 241, 244, 253,
            0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
        },
        {   4,  13,  19,  22,  50,  55,  64,  69,  98, 107, 115, 121,
          127, 134, 140, 145, 152, 158, 161, 171, 181, 194, 199, 203,
          208, 227, 242, 251, 253, 265, 266, 274, 283, 289, 295, 301,
          316, 319, 324, 346, 352, 361, 367, 382, 395, 398, 400, 412,
          419, 422, 426, 428, 433, 446, 454, 457, 472, 493, 505, 508
        }
    };
    int i, is = 0, m = 0, j = 0, k = 0, n = 0, md = 0;
    double rval = 0.;
    if (verbose > 3)
        fprintf(logfp, "            Initializing SAS table\n");
    for (i = 0; i < sas->n; i++) {
        sas->mdeg[i] = 0;
        sas->pol[i] = 0L;
        for (k = 0; k < NA_MAXBIT; k++) sas->iv[i][k] = 0;
    }
    for (i = 0, md = 6; md < NA_MAXDEG; md++) {
        n = (int)(ntot[md] / 100.);
        for (k = 0; k < n; k++) {
            is = (k + 1) % nprim[md];
            sas->mdeg[i] = md;
            sas->pol[i] = pporder[md][is];
            sas->iv[i][0] = 1;
            for (j = 1; j <= md; j++) {
                rval = ranfib(0, 0L);
                m = 1 << j;                         /* m = 2**j */
                sas->iv[i][j] = 2 * (1 + (unsigned long)(m * rval)) - 1;
            }
            i++;
            if (i == sas->n) break;
        }
        if (i == sas->n) break;
    }
}

/*
 *
 * na_sobol - Adaptation of numerical recipes routine for
 *            generating a Sobol-Antonov-Saleev sequence in n-dimensions
 *
 *     If mode = 0:
 *         an n-dimensional vector of independent quasi random deviates
 *         is generated. The value of n should not be changed after
 *         initialization.
 *
 *     If mode .ne. 0:
 *         n independent sequences are initialized but each call generates
 *         the next value of the nth-sequence, i.e. not all n are generated
 *         at once and different numbers of deviates can be generated
 *         from each sequence. After initialization n represents the
 *         sequence number for the next deviate and may be any value
 *         between 1 and the n used for initialization.
 *
 *     The input parameter mode is used at initialization and must
 *     not be changed after initialization.
 *
 *                  M. Sambridge, RSES, July 1998.
 *
 */
static int na_sobol(int n, double *x, int mode, int init, SOBOL *sas)
{
    int j, k, m;
    unsigned long i, im, ipp;
    static double fac;
    static unsigned long in, *inn, *ix;

    if (init == 1) {
/*
 *      initialization of Sobol-Antonov-Saleev sequences
 */
        if (n > sas->n) {
            fprintf(errfp, "na_sobol: requested number of sequences %d > %d!\n",
                    n, sas->n);
            fprintf(logfp, "na_sobol: requested number of sequences %d > %d!\n",
                    n, sas->n);
            return 1;
        }
        if (verbose > 3)
            fprintf(logfp, "            initializing SAS sequence\n");
/*
 *      memory allocations
 */
        inn = (unsigned long *)calloc(n, sizeof(unsigned long));
        if ((ix = (unsigned long *)calloc(n, sizeof(unsigned long))) == NULL) {
            fprintf(errfp, "na_sobol: cannot allocate memory!\n");
            fprintf(logfp, "na_sobol: cannot allocate memory!\n");
            Free(inn);
            return 1;
        }
        if (mode == 0)
            in = 0;
        else {
            for (k = 0; k < n; k++) inn[k] = 0;
        }
        fac = 1.0 / (1 << NA_MAXBIT);
        for (k = 0; k < sas->n; k++) {
            ix[k] = 0;
            for (j = 0; j < sas->mdeg[k]; j++)
                sas->iv[k][j] <<= (NA_MAXBIT-j-1);
            for (j = sas->mdeg[k]; j < NA_MAXBIT; j++) {
                ipp = sas->pol[k];
                i = sas->iv[k][j - sas->mdeg[k]];
                i ^= (i >> sas->mdeg[k]);
                for (m = sas->mdeg[k] - 1; m >= 1; m--) {
                    if (ipp & 1) i ^= sas->iv[k][j-m];
                    ipp >>= 1;
                }
                sas->iv[k][j] = i;
            }
        }
    }
    else if (init == 2) {
/*
 *      free memory
 */
        Free(inn);
        Free(ix);
    }
    else {
/*
 *      calculate next vector in the sequence
 */
        if (mode == 0) {
            im = in++;
            for (j = 0; j < NA_MAXBIT; j++) {
                if (!(im & 1)) break;
                im >>= 1;
            }
            if (j >= NA_MAXBIT) {
                fprintf(errfp, "MAXBIT too small in na_sobol!\n");
                fprintf(logfp, "MAXBIT too small in na_sobol!\n");
                j = NA_MAXBIT - 1;
            }
            im = j * sas->n;
            m = min(n, sas->n);
            for (k = 0; k < m; k++) {
                ix[k] ^= sas->iv[k][j];
                x[k] = ix[k] * fac;
            }
        }
/*
 *      generate next quasi deviate in the n-th sequence
 */
        else {
            k = n;
            im = inn[k]++;
            for (j = 0; j < NA_MAXBIT; j++) {
                if (!(im & 1)) break;
                im >>= 1;
            }
            if (j >= NA_MAXBIT) {
                fprintf(errfp, "MAXBIT too small in na_sobol!\n");
                fprintf(logfp, "MAXBIT too small in na_sobol!\n");
                j = NA_MAXBIT - 1;
            }
            im = j * sas->n;
            ix[k] ^= sas->iv[k][j];
            *x = ix[k] * fac;
        }
    }
    return 0;
}

/*
 *
 * sobseq - Numerical recipes routine
 *
 */
#define MAXDIM 6
static void sobseq(int n, double *x)
{
    int j, k, m;
    unsigned long i, im, ipp;
    static int mdeg[MAXDIM] = { 1, 2, 3, 3, 4, 4 };
    static unsigned long in;
    static unsigned long ix[MAXDIM], *iu[NA_MAXBIT];
    static unsigned long ip[MAXDIM] = { 0, 1, 1, 2, 1, 4 };
    static unsigned long iv[MAXDIM*NA_MAXBIT] =
        { 1,1,1,1,1,1,3,1,3,3,1,1,5,7,7,3,3,5,15,11,5,15,13,9 };
    static double fac;
/*
 *  initialize
 */
    if (n < 0) {
        for (k = 25; k < MAXDIM*NA_MAXBIT; k++) iv[k] = 0;
        for (k = 0; k < MAXDIM; k++) ix[k] = 0;
        in = 0;
        if (iv[0] != 1) return;
        fac = 1.0 / (1 << NA_MAXBIT);
        for (j = 0, k = 0; j < NA_MAXBIT; j++, k += MAXDIM) iu[j] = &iv[k];
        for (k = 0; k < MAXDIM; k++) {
            for (j = 0; j < mdeg[k]; j++) iu[j][k] <<= (NA_MAXBIT-j-1);
            for (j = mdeg[k]; j < NA_MAXBIT; j++) {
                ipp = ip[k];
                i = iu[j-mdeg[k]][k];
                i ^= (i >> mdeg[k]);
                for (m = mdeg[k] - 1; m >= 1; m--) {
                    if (ipp & 1) i ^= iu[j-m][k];
                    ipp >>= 1;
                }
                iu[j][k] = i;
            }
        }
    }
/*
 *  calculate next vector in the sequence
 */
    else {
        im = in++;
        for (j = 0; j < NA_MAXBIT; j++) {
            if (!(im & 1)) break;
            im >>= 1;
        }
        if (j >= NA_MAXBIT) {
            fprintf(errfp, "MAXBIT too small in sobseq!\n");
            fprintf(logfp, "MAXBIT too small in sobseq!\n");
            j = NA_MAXBIT - 1;
        }
        im = j * MAXDIM;
        m = min(n, MAXDIM);
        for (k = 0; k < m; k++) {
            ix[k] ^= iv[im+k];
            x[k] = ix[k] * fac;
        }
    }
}

/*
 * random number generator - Numerical Recipes routine
 */
static double ranfib(int init, unsigned long seed)
{
    static int inext, inextp;
    static double dtab[55];
    int k;
    double d = 0.;
    if (init) {
/*
 *      initialize random number generator
 */
        dranq1(seed);
        for (k = 0; k < 55; k++) {
            dtab[k] = dranq1(0L);
            inext = 0;
            inextp = 31;
        }
    }
    else {
        if (++inext == 55)  inext = 0;
        if (++inextp == 55) inextp = 0;
        d = dtab[inext] - dtab[inextp];
        if (d < 0.) d += 1.;
        dtab[inext] = d;
    }
    return d;
}

static double dranq1(unsigned long seed)
{
    return 5.42101086242752217E-20 * (double)lranq1(seed);
}

static unsigned long lranq1(unsigned long seed)
{
    static unsigned long v = 1L;
/*
 *  initialize random number generator
 */
    if (seed) {
        v = seed ^ 4101842887655102017L;
        v ^= v >> 21; v ^= v << 35; v ^= v >> 4;
        return v * 2685821657736338717L;
    }
/*
 *  generate next random number
 */
    v ^= v >> 21; v ^= v << 35; v ^= v >> 4;
    return v * 2685821657736338717L;
}

/*
 *
 * report entire ensemble of models
 *
 */
static void WriteNAModels(double *na_models[], NASPACE *nasp, double *misfit)
{
    char timestr[25];
    int i, k, ns = 0, np = 0, it, jj = 0, mopt = 0;
    double mean = 0., minfit = 0., minfitc = 0.;
    double model_raw[NA_MAXND];
    fprintf(logfp, "    Parameter space search using a Neighbourhood");
    fprintf(logfp, " algorithm\n      Misfit Lp-norm : %4.2f\n", nasp->lpnorm);
    ns = NAinitialSample;
    minfit = misfit[0];
    for (it = 0; it <= NAiterMax; it++) {
        minfitc = misfit[np];
        mean = 0.;
        for (i = 0; i < ns; i++) {
            jj = np + i;
            if (misfit[jj] < minfit) {
                minfit = misfit[jj];
                mopt = jj;
            }
            if (misfit[jj] < minfitc)
                minfitc = misfit[jj];
            mean += misfit[jj];
        }
        mean /= (double)ns;
        fprintf(logfp, "    iteration %5d,  misfit: mean = %10.4f min = %9.4f ",
                it, mean, minfitc);
        fprintf(logfp, "best so far = %9.4f\n", minfit);
        for (i = 0; i < ns; i++) {
            jj = np + i;
            transform2raw(na_models[jj], nasp, model_raw);
            if (!nasp->epifix)
                tolatlon(model_raw, nasp);
            k = 0;
            fprintf(logfp, "      %4d ", jj);
            if (nasp->epifix)
                fprintf(logfp, "%7.3f %8.3f ", nasp->lat, nasp->lon);
            else {
                fprintf(logfp, "%7.3f %8.3f ", model_raw[k], model_raw[k+1]);
                k += 2;
            }
            if (nasp->otfix)
                fprintf(logfp, "%15.3f ", nasp->ot);
            else {
                fprintf(logfp, "%15.3f ", model_raw[k]);
                k++;
            }
            if (nasp->depfix)
                fprintf(logfp, "%7.3f ", nasp->depth);
            else
                fprintf(logfp, "%7.3f ", model_raw[k]);
            fprintf(logfp, "%9.4f\n", misfit[jj]);
        }
        np += ns;
        ns = NAnextSample;
    }
/*
 *  best model
 */
    transform2raw(na_models[mopt], nasp, model_raw);
    if (!nasp->epifix)
        tolatlon(model_raw, nasp);
    if (!nasp->otfix) {
        k = (nasp->epifix) ? 0 : 2;
        EpochToHuman(timestr, model_raw[k]);
    }
    else
        EpochToHuman(timestr, nasp->ot);
    fprintf(logfp, "    Best model:\n");
    fprintf(logfp, "       id   lat      lon     origin time");
    fprintf(logfp, "             depth    misfit\n");
    k = 0;
    fprintf(logfp, "      %5d ", mopt);
    if (nasp->epifix)
        fprintf(logfp, "%7.3f %8.3f ", nasp->lat, nasp->lon);
    else {
        fprintf(logfp, "%7.3f %8.3f ", model_raw[k], model_raw[k+1]);
        k += 2;
    }
    if (!nasp->otfix) k++;
    fprintf(logfp, "%s ", timestr);
    if (nasp->depfix)
        fprintf(logfp, "%7.3f ", nasp->depth);
    else
        fprintf(logfp, "%7.3f ", model_raw[k]);
    fprintf(logfp, "%9.4f\n", misfit[mopt]);
}


