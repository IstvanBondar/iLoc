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

/*
 * Functions:
 *    GetStalist
 *    StarecCompare
 *    GetDistanceMatrix
 *    GetStationIndex
 *    GetDataCovarianceMatrix
 *    ReadVariogram
 *    FreeVariogram
 */

/*
 *  Title:
 *     GetStalist
 *  Synopsis:
 *     Populates and sorts STAREC structure (stalist) with distinct stations.
 *  Input Arguments:
 *     numPhase - number of associated phases
 *     p[]     - array of phase structures
 *  Ouput Arguments:
 *     nsta    - number of distinct stations
 *  Return:
 *     stalist[] - array of starec structures
 *  Called by:
 *     Locator
 *  Calls:
 *     StarecCompare
 */
STAREC *GetStalist(int numPhase, PHAREC p[], int *nsta)
{
    STAREC *stalist = (STAREC *)NULL;
    char prev_prista[STALEN];
    int i, j, n = 0;
    strcpy(prev_prista, "");
/*
 *  count number of distinct stations
 */
    for (i = 0; i < numPhase; i++) {
        if (streq(p[i].prista, prev_prista)) continue;
        strcpy(prev_prista, p[i].prista);
        n++;
    }
/*
 *  allocate memory for stalist
 */
    if ((stalist = (STAREC *)calloc(n, sizeof(STAREC))) == NULL) {
        fprintf(logfp, "GetStalist: cannot allocate memory\n");
        fprintf(errfp, "GetStalist: cannot allocate memory\n");
        errorcode = 1;
        return (STAREC *)NULL;
    }
/*
 *  populate stalist
 */
    strcpy(prev_prista, "");
    for (j = 0, i = 0; i < numPhase; i++) {
        if (streq(p[i].prista, prev_prista)) continue;
        strcpy(prev_prista, p[i].prista);
        strcpy(stalist[j].fdsn, p[i].fdsn);
        strcpy(stalist[j].sta, p[i].sta);
        strcpy(stalist[j].altsta, p[i].prista);
        strcpy(stalist[j].agency, p[i].agency);
        strcpy(stalist[j].deploy, p[i].deploy);
        strcpy(stalist[j].lcn, p[i].lcn);
        stalist[j].lat = p[i].StaLat;
        stalist[j].lon = p[i].StaLon;
        stalist[j].elev = p[i].StaElev;
        j++;
    }
    qsort(stalist, n, sizeof(STAREC), StarecCompare);
    *nsta = n;
    return stalist;
}

/*
 *
 * StarecCompare: compares two stalist records based on sta
 *
 */
int StarecCompare(const void *sta1, const void *sta2)
{
    return strcmp(((STAREC *)sta1)->altsta, ((STAREC *)sta2)->altsta);
}

/*
 *  Title:
 *     GetDistanceMatrix
 *  Synopsis:
 *     Calculates station separations in km
 *  Input Arguments:
 *     nsta      - number of distinct stations
 *     stalist[] - array of starec structures
 *  Return:
 *     distmatrix
 *  Called by:
 *     Locator
 *  Calls:
 *     AllocateFloatMatrix, DistAzimuth
 */
double **GetDistanceMatrix(int nsta, STAREC stalist[])
{
    double **distmatrix = (double **)NULL;
    int i, j;
    double d = 0., esaz = 0., seaz = 0.;
/*
 *  memory allocation
 */
    if ((distmatrix = AllocateFloatMatrix(nsta, nsta)) == NULL) {
        fprintf(logfp, "GetDistanceMatrix: cannot allocate memory\n");
        fprintf(errfp, "GetDistanceMatrix: cannot allocate memory\n");
        errorcode = 1;
        return (double **)NULL;
    }
/*
 *  populate distmatrix; station separations in km
 */
    for (i = 0; i < nsta; i++) {
        distmatrix[i][i] = 0.;
        for (j = i + 1; j < nsta; j++) {
            d = DEG2KM * DistAzimuth(stalist[j].lat, stalist[j].lon,
                                     stalist[i].lat, stalist[i].lon,
	                                 &seaz, &esaz);
            distmatrix[i][j] = distmatrix[j][i] = d;
        }
    }
    if (verbose > 2) {
        fprintf(logfp, "    distance matrix (%d x %d):\n", nsta, nsta);
        for (i = 0; i < nsta; i++) {
            fprintf(logfp, "      %-6s ", stalist[i].altsta);
            for (j = 0; j < nsta; j++)
                fprintf(logfp, "%7.1f ", distmatrix[i][j]);
            fprintf(logfp, "\n");
        }
    }
    return distmatrix;
}

/*
 *  Title:
 *     GetStationIndex
 *  Synopsis:
 *     Returns index of a station in the stalist array
 *  Input Arguments:
 *     nsta      - number of distinct stations
 *     stalist[] - array of starec structures
 *     sta       - station to find
 *  Return:
 *     station index or -1 on error
 *  Called by:
 *     GetNdef, SortPhasesForNA
 */
int GetStationIndex(int nsta, STAREC stalist[], char *sta)
{
    int klo = 0, khi = nsta - 1, k = 0, i;
    if (nsta > 2) {
        while (khi - klo > 1) {
            k = (khi + klo) >> 1;
            if ((i = strcmp(stalist[k].altsta, sta)) == 0)
                return k;
            else if (i > 0)
                khi = k;
            else
                klo = k;
        }
        if (khi == 1) {
            k = 0;
            if (streq(sta, stalist[k].altsta)) return k;
            else return -1;
        }
        if (klo == nsta - 2) {
            k = nsta - 1;
            if (streq(sta, stalist[k].altsta)) return k;
            else return -1;
        }
    }
    else if (nsta == 2) {
        if (streq(sta, stalist[0].altsta)) return 0;
        else if (streq(sta, stalist[1].altsta)) return 1;
        else return -1;
    }
    else {
        if (streq(sta, stalist[0].altsta)) return 0;
        else return -1;
    }
    return -1;
}

/*
 *  Title:
 *     GetDataCovarianceMatrix
 *  Synopsis:
 *     Constructs full data covariance matrix from variogram (model errors)
 *     and prior phase variances (measurement errors)
 *  Input Arguments:
 *     nsta       - number of distinct stations
 *     numPhase   - number of associated phases
 *     nd         - number of defining phases
 *     p[]        - array of phase structures
 *     stalist[]  - array of starec structures
 *     distmatrix - matrix of station separations
 *     variogramp - pointer to generic variogram model
 *  Return:
 *     data covariance matrix
 *  Called by:
 *     LocateEvent, NASearch
 *  Calls:
 *     AllocateFloatMatrix, FreeFloatMatrix, GetStationIndex,
 *     SplineInterpolation
 */
double **GetDataCovarianceMatrix(int nsta, int numPhase, int nd, PHAREC p[],
                                STAREC stalist[], double **distmatrix,
                                VARIOGRAM *variogramp)
{
    int i, j, k, m, sind1 = 0, sind2 = 0;
    double stasep = 0., var = 0., dydx = 0., d2ydx = 0.;
    double **dcov = (double **)NULL;
/*
 *  allocate memory for dcov
 */
    if ((dcov = AllocateFloatMatrix(nd, nd)) == NULL) {
        fprintf(logfp, "GetDataCovarianceMatrix: cannot allocate memory\n");
        fprintf(errfp, "GetDataCovarianceMatrix: cannot allocate memory\n");
        errorcode = 1;
        return (double **)NULL;
    }
/*
 *  construct data covariance matrix from variogram and prior measurement
 *  error variances
 */
    for (k = 0, i = 0; i < numPhase; i++) {
/*
 *      arrival time
 */
        if (p[i].timedef) {
            if ((sind1 = GetStationIndex(nsta, stalist, p[i].prista)) < 0) {
                FreeFloatMatrix(dcov);
                return (double **)NULL;
            }
/*
 *          prior picking error variances add to the diagonal
 */
            dcov[k][k] = variogramp->sill + p[i].deltim * p[i].deltim;
/*
 *          RSTT provides path-dependent uncertainties (model + pick error)
 */
            if (p[i].rsttTotalErr > 0)
                dcov[k][k] = p[i].rsttTotalErr * p[i].rsttTotalErr;
            p[i].CovIndTime = k;
            if (verbose > 4) {
                fprintf(logfp, "                i=%d k=%d sind1=%d ", i, k, sind1);
                fprintf(logfp, "sta=%s phase=%s var=%.3f\n",
                        p[i].prista, p[i].phase, dcov[k][k]);
            }
/*
 *          covariances
 */
            for (m = k + 1, j = i + 1; j < numPhase; j++) {
                if (!p[j].timedef) continue;
/*
 *              different phases have different ray paths so they do not correlate
 */
                if (strcmp(p[i].phase, p[j].phase)) {
                    m++;
                    continue;
                }
                if ((sind2 = GetStationIndex(nsta, stalist, p[j].prista)) < 0) {
                    FreeFloatMatrix(dcov);
                    return (double **)NULL;
                }
/*
 *              station separation
 */
                var = 0.;
                stasep = distmatrix[sind1][sind2];
                if (stasep < variogramp->maxsep) {
/*
 *                  interpolate variogram
 */
                    var = SplineInterpolation(stasep, variogramp->n, variogramp->x,
                                variogramp->y, variogramp->d2y, 0, &dydx, &d2ydx);
/*
 *                  covariance: sill - variogram
 */
                    var = variogramp->sill - var;
                }
                dcov[k][m] = var;
                dcov[m][k] = var;
                if (verbose > 4) {
                    fprintf(logfp, "                  j=%d m=%d sind2=%d ",
                            j, m, sind2);
                    fprintf(logfp, "sta=%s phase=%s ", p[j].prista, p[j].phase);
                    fprintf(logfp, "stasep=%.1f var=%.3f\n", stasep, var);
                }
                m++;
            }
            k++;
        }
    }
    for (i = 0; i < numPhase; i++) {
/*
 *      azimuth
 */
        if (p[i].azimdef) {
            if ((sind1 = GetStationIndex(nsta, stalist, p[i].prista)) < 0) {
                FreeFloatMatrix(dcov);
                return (double **)NULL;
            }
/*
 *          prior picking error variances add to the diagonal
 */
            dcov[k][k] = variogramp->sill + p[i].delaz * p[i].delaz;
            p[i].CovIndAzim = k;
            if (verbose > 4) {
                fprintf(logfp, "                i=%d k=%d sind1=%d ", i, k, sind1);
                fprintf(logfp, "sta=%s phase=%s\n", p[i].prista, p[i].phase);
            }
/*
 *          covariances
 */
            for (m = k + 1, j = i + 1; j < numPhase; j++) {
                if (!p[j].azimdef) continue;
/*
 *              different phases have different ray paths so they do not correlate
 */
                if (strcmp(p[i].phase, p[j].phase)) {
                    m++;
                    continue;
                }
                if ((sind2 = GetStationIndex(nsta, stalist, p[j].prista)) < 0) {
                    FreeFloatMatrix(dcov);
                    return (double **)NULL;
                }
/*
 *              station separation
 */
                var = 0.;
                stasep = distmatrix[sind1][sind2];
                if (stasep < variogramp->maxsep) {
/*
 *                  interpolate variogram
 */
                    var = SplineInterpolation(stasep, variogramp->n, variogramp->x,
                                variogramp->y, variogramp->d2y, 0, &dydx, &d2ydx);
/*
 *                  covariance: sill - variogram
 */
                    var = variogramp->sill - var;
                }
                dcov[k][m] = var;
                dcov[m][k] = var;
                if (verbose > 4) {
                    fprintf(logfp, "                  j=%d m=%d sind2=%d ",
                            j, m, sind2);
                    fprintf(logfp, "sta=%s phase=%s ", p[j].prista, p[j].phase);
                    fprintf(logfp, "stasep=%.1f var=%.3f\n", stasep, var);
                }
                m++;
            }
            k++;
        }
    }
    for (i = 0; i < numPhase; i++) {
/*
 *      slowness
 */
        if (p[i].slowdef) {
            if ((sind1 = GetStationIndex(nsta, stalist, p[i].prista)) < 0) {
                FreeFloatMatrix(dcov);
                return (double **)NULL;
            }
/*
 *          prior picking error variances add to the diagonal
 */
            dcov[k][k] = variogramp->sill + p[i].delslo * p[i].delslo;
            p[i].CovIndSlow = k;
            if (verbose > 4) {
                fprintf(logfp, "                i=%d k=%d sind1=%d ", i, k, sind1);
                fprintf(logfp, "sta=%s phase=%s\n", p[i].prista, p[i].phase);
            }
/*
 *          covariances
 */
            for (m = k + 1, j = i + 1; j < numPhase; j++) {
                if (!p[j].slowdef) continue;
/*
 *              different phases have different ray paths so they do not correlate
 */
                if (strcmp(p[i].phase, p[j].phase)) {
                    m++;
                    continue;
                }
                if ((sind2 = GetStationIndex(nsta, stalist, p[j].prista)) < 0) {
                    FreeFloatMatrix(dcov);
                    return (double **)NULL;
                }
/*
 *              station separation
 */
                var = 0.;
                stasep = distmatrix[sind1][sind2];
                if (stasep < variogramp->maxsep) {
/*
 *                  interpolate variogram
 */
                    var = SplineInterpolation(stasep, variogramp->n, variogramp->x,
                                variogramp->y, variogramp->d2y, 0, &dydx, &d2ydx);
/*
 *                  covariance: sill - variogram
 */
                    var = variogramp->sill - var;
                }
                dcov[k][m] = var;
                dcov[m][k] = var;
                if (verbose > 4) {
                    fprintf(logfp, "                  j=%d m=%d sind2=%d ",
                            j, m, sind2);
                    fprintf(logfp, "sta=%s phase=%s ", p[j].prista, p[j].phase);
                    fprintf(logfp, "stasep=%.1f var=%.3f\n", stasep, var);
                }
                m++;
            }
            k++;
        }
    }
    if (verbose > 2) {
        fprintf(logfp, "        Data covariance matrix C(%d x %d):\n", nd, nd);
        for (k = 0, i = 0; i < numPhase; i++) {
            if (p[i].timedef) {
                fprintf(logfp, "          %4d %4d %-6s %-8s T ",
                        i, k, p[i].prista, p[i].phase);
                for (j = 0; j < nd; j++) fprintf(logfp, "%6.3f ", dcov[k][j]);
                fprintf(logfp, "\n");
                k++;
            }
        }
        for (i = 0; i < numPhase; i++) {
            if (p[i].azimdef) {
                fprintf(logfp, "          %4d %4d %-6s %-8s A ",
                        i, k, p[i].prista, p[i].phase);
                for (j = 0; j < nd; j++) fprintf(logfp, "%6.3f ", dcov[k][j]);
                fprintf(logfp, "\n");
                k++;
            }
        }
        for (i = 0; i < numPhase; i++) {
            if (p[i].slowdef) {
                fprintf(logfp, "          %4d %4d %-6s %-8s S ",
                        i, k, p[i].prista, p[i].phase);
                for (j = 0; j < nd; j++) fprintf(logfp, "%6.3f ", dcov[k][j]);
                fprintf(logfp, "\n");
                k++;
            }
        }
    }
    return dcov;
}

/*
 *  Title:
 *     ReadVariogram
 *  Synopsis:
 *     Reads generic variogram from file and stores it in VARIOGRAM structure.
 *  Input Arguments:
 *     fname - pathname of variogram file
 *  Output Arguments:
 *     variogramp - pointer to VARIOGRAM structure
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     ReadAuxDataFiles
 *  Calls:
 *     SplineCoeffs, Free, SkipComments
 */
int ReadVariogram(char *fname, VARIOGRAM *variogramp)
{
    FILE *fp;
    char buf[LINLEN];
    int i, n = 0;
    double sill = 0., maxsep = 0.;
    double *tmp = (double *)NULL;
    char *s;
/*
 *  open variogram file and get number of phases
 */
    if ((fp = fopen(fname, "r")) == NULL) {
        fprintf(logfp, "ReadVariogram: cannot open %s\n", fname);
        fprintf(errfp, "ReadVariogram: cannot open %s\n", fname);
        errorcode = 2;
        return 1;
    }
/*
 *  number of samples
 */
    fgets(buf, LINLEN, fp);
    SkipComments(buf, fp);
    sscanf(buf, "%d", &n);
    variogramp->n = n;
/*
 *  sill variance
 */
    SkipComments(buf, fp);
    sscanf(buf, "%lf", &sill);
    variogramp->sill = sill;
/*
 *  max station separation to be considered
 */
    SkipComments(buf, fp);
    sscanf(buf, "%lf", &maxsep);
    variogramp->maxsep = maxsep;
/*
 *  memory allocations
 */
    tmp = (double *)calloc(n, sizeof(double));
    variogramp->x = (double *)calloc(n, sizeof(double));
    variogramp->y = (double *)calloc(n, sizeof(double));
    if ((variogramp->d2y = (double *)calloc(n, sizeof(double))) == NULL) {
        fprintf(logfp, "ReadVariogram: cannot allocate memory!\n");
        fprintf(errfp, "ReadVariogram: cannot allocate memory!\n");
        Free(variogramp->y);
        Free(variogramp->x);
        Free(tmp);
        errorcode = 1;
        return 1;
    }
/*
 *  variogram: x = distance [km], y = gamma(x) [s**2]
 */
    for (i = 0; i < n; i++) {
       SkipComments(buf, fp);
       s = strtok(buf, " ");
       variogramp->x[i] = atof(s);
       s = strtok(NULL, " ");
       variogramp->y[i] = atof(s);
    }
    fclose(fp);
/*
 *  second derivatives of the natural SplineCoeffs interpolating function
 */
    SplineCoeffs(n, variogramp->x, variogramp->y, variogramp->d2y, tmp);
    Free(tmp);
    return 0;
}

/*
 *  Title:
 *     FreeVariogram
 *  Synopsis:
 *     frees memory allocated to VARIOGRAM structure
 *  Input Arguments:
 *     variogramp - pointer to VARIOGRAM structure
 *  Called by:
 *     main, ReadAuxDataFiles
 *  Calls:
 *     Free
 */
void FreeVariogram(VARIOGRAM *variogramp)
{
    Free(variogramp->d2y);
    Free(variogramp->y);
    Free(variogramp->x);
}

