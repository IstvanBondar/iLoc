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
extern int numPhaseTT;                                   /* number of phases */
extern char PhaseTT[MAXTTPHA][PHALEN];                         /* phase list */
extern int numLocalPhaseTT;                        /* number of local phases */
extern char LocalPhaseTT[MAXLOCALTTPHA][PHALEN];         /* local phase list */
extern char TTimeTable[];                                      /* model name */
extern double PSurfVel;             /* Pg velocity for elevation corrections */
extern double SSurfVel;             /* Sg velocity for elevation corrections */
extern double MaxHypocenterDepth;    /* max hypocenter depth from model file */
extern char PhaseWithoutResidual[MAXNUMPHA][PHALEN];      /* from model file */
extern int PhaseWithoutResidualNum;       /* number of 'non-residual' phases */
extern int EtopoNlon;                /* number of longitude samples in ETOPO */
extern int EtopoNlat;                 /* number of latitude samples in ETOPO */
extern double EtopoRes;                                 /* cellsize in ETOPO */
extern int UseRSTTPnSn;                       /* use RSTT Pn/Sn predictions? */
extern int UseRSTTPgLg;                       /* use RSTT Pg/Lg predictions? */
extern int UseRSTT;                                 /* use RSTT predictions? */
extern int UseLocalTT;                           /* use local TT predictions */
extern double MaxLocalTTDelta;           /* use local TT up to this distance */

/*
 * Functions:
 *    ReadTTtables
 *    FreeTTtables
 *    FreeLocalTTtables
 *    ReadEtopo1
 *    GetPhaseIndex
 *    GetLocalPhaseIndex
 *    GetTravelTimePrediction
 *    GetTravelTimeTableValue
 *    TravelTimeResiduals
 *    GetEtopoCorrection
 */

/*
 * Local functions:
 *    GetEtopoElevation
 *    TravelTimeCorrections
 *    GetBounceCorrection
 *    GetElevationCorrection
 *    GetLastLag
 *    GetGeoidCorrection
 *    HeightAboveMeanSphere
 *    GetTTResidual
 *    isRSTT
 */
static double GetEtopoElevation(double lat, double lon, short int **topo);
static void TravelTimeCorrections(SOLREC *sp, PHAREC *pp, EC_COEF *ec,
        short int **topo);
static double GetBounceCorrection(SOLREC *sp, PHAREC *pp, short int **topo,
        double *tcorw);
static double GetElevationCorrection(PHAREC *pp);
static int GetLastLag(char phase[]);
static double GetGeoidCorrection(double lat, PHAREC *pp);
static double HeightAboveMeanSphere(double lat);
static double GetTTResidual(PHAREC *pp, int all, SOLREC *sp, EC_COEF *ec,
        TT_TABLE *tt_tables, TT_TABLE *localtt_tables, short int **topo,
        int iszderiv, int is2nderiv);
static int isRSTT(PHAREC *pp, double depth);

/*
 *  Title:
 *     ReadTTtables
 *  Synopsis:
 *     Read travel-time tables from files in dirname.
 *  Input Arguments:
 *     dirname - directory pathname for TT tables
 *  Return:
 *     tt_tables - pointer to TT_TABLE structure or NULL on error
 *  Called by:
 *     ReadAuxDataFiles
 *  Calls:
 *     SkipComments, FreeTTtables, AllocateFloatMatrix
 */
TT_TABLE *ReadTTtables(char *dirname)
{
    FILE *fp;
    TT_TABLE *tt_tables = (TT_TABLE *)NULL;
    char fname[MAXBUF], buf[LINLEN], *s;
    int ndists = 0, ndepths = 0, i, j, k, m, ind = 0, isdepthphase = 0;
/*
 *  memory allocation
 */
    tt_tables = (TT_TABLE *)calloc(numPhaseTT, sizeof(TT_TABLE));
    if (tt_tables == NULL) {
        fprintf(logfp, "ReadTTtables: cannot allocate memory\n");
        fprintf(errfp, "ReadTTtables: cannot allocate memory\n");
        errorcode = 1;
        return (TT_TABLE *) NULL;
    }
/*
 *  read TT table files
 *      numPhaseTT and PhaseTT are specified in iloc.h and iloc_main.c
 */
    for (ind = 0; ind < numPhaseTT; ind++) {
/*
 *      initialize tt_tables for this phase
 */
        isdepthphase = 0;
        if (PhaseTT[ind][0] == 'p' || PhaseTT[ind][0] == 's')
            isdepthphase = 1;
        strcpy(tt_tables[ind].phase, PhaseTT[ind]);
        tt_tables[ind].ndel = 0;
        tt_tables[ind].ndep = 0;
        tt_tables[ind].isbounce = isdepthphase;
        tt_tables[ind].deltas = (double *)NULL;
        tt_tables[ind].depths = (double *)NULL;
        tt_tables[ind].tt = (double **)NULL;
        tt_tables[ind].dtdd = (double **)NULL;
        tt_tables[ind].dtdh = (double **)NULL;
        tt_tables[ind].bpdel = (double **)NULL;
/*
 *      open TT table file for this phase
 */
        if (isdepthphase)
            sprintf(fname, "%s/%s.little%s.tab",
                    dirname, TTimeTable, PhaseTT[ind]);
        else
            sprintf(fname, "%s/%s.%s.tab",
                    dirname, TTimeTable, PhaseTT[ind]);
        if ((fp = fopen(fname, "r")) == NULL) {
            if (verbose > 3)
                fprintf(errfp, "ReadTTtables: cannot open %s\n", fname);
            errorcode = 2;
            continue;
        }
/*
 *      number of distance and depth samples
 */
        fgets(buf, LINLEN, fp);
        SkipComments(buf, fp);
        sscanf(buf, "%d%d", &ndists, &ndepths);
        tt_tables[ind].ndel = ndists;
        tt_tables[ind].ndep = ndepths;
/*
 *      memory allocations
 */
        tt_tables[ind].deltas = (double *)calloc(ndists, sizeof(double));
        tt_tables[ind].depths = (double *)calloc(ndepths, sizeof(double));
        if (isdepthphase)
            tt_tables[ind].bpdel = AllocateFloatMatrix(ndists, ndepths);
        tt_tables[ind].tt = AllocateFloatMatrix(ndists, ndepths);
        tt_tables[ind].dtdd = AllocateFloatMatrix(ndists, ndepths);
        if ((tt_tables[ind].dtdh = AllocateFloatMatrix(ndists, ndepths)) == NULL) {
            FreeTTtables(tt_tables);
            fclose(fp);
            errorcode = 1;
            return (TT_TABLE *) NULL;
        }
/*
 *      delta samples (broken into lines of 25 values)
 */
        m = ceil((double)ndists / 25.);
        for (i = 0, k = 0; k < m - 1; k++) {
            SkipComments(buf, fp);
            s = strtok(buf, " ");
            tt_tables[ind].deltas[i++] = atof(s);
            for (j = 1; j < 25; j++) {
                s = strtok(NULL, " ");
                tt_tables[ind].deltas[i++] = atof(s);
            }
        }
        if (i < ndists) {
            SkipComments(buf, fp);
            s = strtok(buf, " ");
            tt_tables[ind].deltas[i++] = atof(s);
            for (j = i; j < ndists; j++) {
                s = strtok(NULL, " ");
                tt_tables[ind].deltas[j] = atof(s);
            }
        }
/*
 *      depth samples
 */
        SkipComments(buf, fp);
        s = strtok(buf, " ");
        tt_tables[ind].depths[0] = atof(s);
        for (i = 1; i < ndepths; i++) {
            s = strtok(NULL, " ");
            tt_tables[ind].depths[i] = atof(s);
        }
/*
 *      travel-times (ndists rows, ndepths columns)
 */
        for (i = 0; i < ndists; i++) {
            SkipComments(buf, fp);
            s = strtok(buf, " ");
            tt_tables[ind].tt[i][0] = atof(s);
            for (j = 1; j < ndepths; j++) {
                s = strtok(NULL, " ");
                tt_tables[ind].tt[i][j] = atof(s);
            }
        }
/*
 *      dtdd (horizontal slowness)
 */
        for (i = 0; i < ndists; i++) {
            SkipComments(buf, fp);
            s = strtok(buf, " ");
            tt_tables[ind].dtdd[i][0] = atof(s);
            for (j = 1; j < ndepths; j++) {
                s = strtok(NULL, " ");
                tt_tables[ind].dtdd[i][j] = atof(s);
            }
        }
/*
 *      dtdh (vertical slowness)
 */
        for (i = 0; i < ndists; i++) {
            SkipComments(buf, fp);
            s = strtok(buf, " ");
            tt_tables[ind].dtdh[i][0] = atof(s);
            for (j = 1; j < ndepths; j++) {
                s = strtok(NULL, " ");
                tt_tables[ind].dtdh[i][j] = atof(s);
            }
        }
/*
 *      depth phase bounce point distances
 */
        if (isdepthphase) {
            for (i = 0; i < ndists; i++) {
                SkipComments(buf, fp);
                s = strtok(buf, " ");
                tt_tables[ind].bpdel[i][0] = atof(s);
                for (j = 1; j < ndepths; j++) {
                    s = strtok(NULL, " ");
                    tt_tables[ind].bpdel[i][j] = atof(s);
                }
            }
        }
        fclose(fp);
    }
    return tt_tables;
}

/*
 *  Title:
 *     FreeTTtables
 *  Synopsis:
 *     Frees memory allocated to TT_TABLE structure
 *  Input Arguments:
 *     tt_tables - TT table structure
 *  Called by:
 *     ReadAuxDataFiles, ReadTTtables, main
 *  Calls:
 *     FreeFloatMatrix
 */
void FreeTTtables(TT_TABLE *tt_tables)
{
    int i, ndists = 0;
    for (i = 0; i < numPhaseTT; i++) {
        if ((ndists = tt_tables[i].ndel) == 0) continue;
        FreeFloatMatrix(tt_tables[i].dtdh);
        FreeFloatMatrix(tt_tables[i].dtdd);
        FreeFloatMatrix(tt_tables[i].tt);
        if (PhaseTT[i][0] == 'p' || PhaseTT[i][0] == 's')
            FreeFloatMatrix(tt_tables[i].bpdel);
        Free(tt_tables[i].depths);
        Free(tt_tables[i].deltas);
    }
    Free(tt_tables);
}

/*
 *  Title:
 *     FreeLocalTTtables
 *  Synopsis:
 *     Frees memory allocated to local TT_TABLE structure
 *  Input Arguments:
 *     tt_tables - TT table structure
 *  Called by:
 *     ReadAuxDataFiles, ReadTTtables, main
 *  Calls:
 *     FreeFloatMatrix
 */
void FreeLocalTTtables(TT_TABLE *tt_tables)
{
    int i, ndists = 0;
    for (i = 0; i < numLocalPhaseTT; i++) {
        if ((ndists = tt_tables[i].ndel) == 0) continue;
        FreeFloatMatrix(tt_tables[i].dtdh);
        FreeFloatMatrix(tt_tables[i].dtdd);
        FreeFloatMatrix(tt_tables[i].tt);
        if (tt_tables[i].isbounce)
            FreeFloatMatrix(tt_tables[i].bpdel);
        Free(tt_tables[i].depths);
        Free(tt_tables[i].deltas);
    }
    Free(tt_tables);
}

/*
 *  Title:
 *      ReadEtopo1
 *  Synopsis:
 *      Reads ETOPO1 topography file and store it in global short int topo
 *      array
 *      ETOPO1:
 *         etopo1_bed_g_i2.bin
 *         Amante, C. and B. W. Eakins,
 *           ETOPO1 1 Arc-Minute Global Relief Model: Procedures, Data Sources
 *           and Analysis.
 *           NOAA Technical Memorandum NESDIS NGDC-24, 19 pp, March 2009.
 *         NCOLS         21601
 *         NROWS         10801
 *         XLLCENTER     -180.000000
 *         YLLCENTER     -90.000000
 *         CELLSIZE      0.01666666667
 *         NODATA_VALUE  -32768
 *         BYTEORDER     LSBFIRST
 *         NUMBERTYPE    2_BYTE_INTEGER
 *         ZUNITS        METERS
 *         MIN_VALUE     -10898
 *         MAX_VALUE     8271
 *         1'x1' resolution, 21601 lons, 10801 lats
 *      Resampled ETOPO1 versions:
 *         etopo2_bed_g_i2.bin
 *           grdfilter -I2m etopo1_bed.grd -Fg10 -D4 -Getopo2_bed.grd
 *             Gridline node registration used
 *             x_min: -180 x_max: 180 x_inc: 0.0333333 name: nx: 10801
 *             y_min: -90 y_max: 90 y_inc: 0.0333333 name: ny: 5401
 *             z_min: -10648.7 z_max: 7399.13 name: m
 *             scale_factor: 1 add_offset: 0
 *         etopo5_bed_g_i2.bin
 *           grdfilter -I5m etopo1_bed.grd -Fg15 -D4 -Getopo5_bed.grd
 *             Gridline node registration used
 *             x_min: -180 x_max: 180 x_inc: 0.0833333 nx: 4321
 *             y_min: -90 y_max: 90 y_inc: 0.0833333 ny: 2161
 *             z_min: -10515.5 z_max: 6917.75 name: m
 *             scale_factor: 1 add_offset: 0
 *  ETOPO parameters are specified in config.txt file:
 *     EtopoFile - pathname for ETOPO file
 *     EtopoNlon - number of longitude samples in ETOPO
 *     EtopoNlat - number of latitude samples in ETOPO
 *     EtopoRes  - cellsize in ETOPO
 *  Input Arguments:
 *     filename - filename pathname
 *  Return:
 *     topo - ETOPO bathymetry/elevation matrix or NULL on error
 *  Called by:
 *     ReadAuxDataFiles
 *  Calls:
 *     AllocateShortMatrix, FreeShortMatrix
 */
short int **ReadEtopo1(char *filename)
{
    FILE *fp;
    short int **topo = (short int **)NULL;
    unsigned long n, m;
/*
 *  open etopo file
 */
    if ((fp = fopen(filename, "rb")) == NULL) {
        fprintf(stderr, "Cannot open %s!\n", filename);
        errorcode = 2;
        return (short int **)NULL;
    }
/*
 *  allocate memory
 */
    if ((topo = AllocateShortMatrix(EtopoNlat, EtopoNlon)) == NULL)
        return (short int **)NULL;
/*
 *  read etopo file
 */
    n = EtopoNlat * EtopoNlon;
    if ((m = fread(topo[0], sizeof(short int), n, fp)) != n) {
        fprintf(stderr, "Corrupted %s!\n", filename);
        fclose(fp);
        FreeShortMatrix(topo);
        return (short int **)NULL;
    }
    fclose(fp);
    return topo;
}

/*
 *  Title:
 *      GetEtopoElevation
 *  Synopsis:
 *      Returns ETOPO1 topography in kilometers for a lat, lon pair.
 *  ETOPO parameters are specified in config.txt file:
 *     EtopoFile - pathname for ETOPO file
 *     EtopoNlon - number of longitude samples in ETOPO
 *     EtopoNlat - number of latitude samples in ETOPO
 *     EtopoRes  - cellsize in ETOPO
 *  Input Arguments:
 *      lat, lon - latitude, longitude in degrees
 *      topo     - ETOPO bathymetry/elevation matrix
 *  Returns:
 *      elevation above sea level [km]
 *      topography above sea level is taken positive,
 *                 below sea level negative.
 *  Called by:
 *     GetEtopoCorrection
 */
static double GetEtopoElevation(double lat, double lon, short int **topo)
{
    int i, j, m, k1, k2;
    double a1, a2, lat2, lon2, lat1, lon1;
    double top, topo1, topo2, topo3, topo4;
/*
 *  bounding box
 */
    i = (int)((lon + 180.) / EtopoRes);
    j = (int)((90. - lat) / EtopoRes);
    lon1 = (double)(i) * EtopoRes - 180.;
    lat1 = 90. - (double)(j) * EtopoRes;
    lon2 = (double)(i + 1) * EtopoRes - 180.;
    lat2 = 90. - (double)(j + 1) * EtopoRes;
    k1 = i;
    k2 = i + 1;
    m = j;
    a1 = (lon2 - lon) / (lon2 - lon1);
    a2 = (lat2 - lat) / (lat2 - lat1);
/*
 *  take care of grid boundaries
 */
    if (i < 0 || i > EtopoNlon - 2) {
        k1 = EtopoNlon - 1;
        k2 = 0;
    }
    if (j < 0) {
        m = 0;
        a2 = 0.;
    }
    if (j > EtopoNlat - 2) {
        m = EtopoNlat - 2;
        a2 = 1.;
    }
/*
 *  interpolate
 */
    topo1 = (double)topo[m][k1];
    topo2 = (double)topo[m+1][k1];
    topo3 = (double)topo[m][k2];
    topo4 = (double)topo[m+1][k2];
    top = (1. - a1) * (1. - a2) * topo1 + a1 * (1. - a2) * topo3 +
          (1. - a1) * a2 * topo2 + a1 * a2 * topo4;
    return top / 1000.;
}

/*
 *  Title:
 *     GetPhaseIndex
 *  Synopsis:
 *	   Returns index of tt_table struct array for a given phase
 *  Input Arguments:
 *     phase - phase
 *  Return:
 *     phase index or -1 on error
 *  Called by:
 *     GetTravelTimePrediction, DepthPhaseStack
 */
int GetPhaseIndex(char *phase)
{
    int i;
    for (i = 0; i < numPhaseTT; i++) {
        if (streq(phase, PhaseTT[i])) return i;
    }
    return -1;
}

/*
 *  Title:
 *     GetLocalPhaseIndex
 *  Synopsis:
 *	   Returns index of tt_table struct array for a given phase
 *  Input Arguments:
 *     phase - phase
 *  Return:
 *     phase index or -1 on error
 *  Called by:
 *     GetTravelTimePrediction, DepthPhaseStack
 */
int GetLocalPhaseIndex(char *phase)
{
    int i;
    for (i = 0; i < numLocalPhaseTT; i++) {
        if (streq(phase, LocalPhaseTT[i])) return i;
    }
    if (streq(phase, "Lg")) return 22;
    return -1;
}

/*
 *  Title:
 *     GetTravelTimeTableValue
 *  Synopsis:
 *	   Returns TT table values for a given phase, depth and delta.
 *     Bicubic spline interpolation is used to get interpolated values.
 *     Horizontal and vertical slownesses are calculated if requested.
 *     Horizontal and vertical second time derivatives, needed for defining
 *         slownesses, are calculated if requested.
 *     Bounce point distance is calculated for depth phases.
 *  Input Arguments:
 *     tt_tablep - TT table structure for phase
 *     depth     - depth
 *     delta     - delta
 *     iszderiv  - do we need dtdh [0/1]?
 *     is2nderiv - do we need d2tdd and d2tdh [0/1]?
 *  Output Arguments:
 *     dtdd  - interpolated dtdd (horizontal slowness, s/deg)
 *     dtdh  - interpolated dtdh (vertical slowness, s/km)
 *     bpdel - bounce point distance (deg) if depth phase
 *     d2tdd - interpolated second horizontal time derivative
 *     d2tdh - interpolated second vertical time derivative
 *  Return:
 *     TT table value for a phase at depth and delta or -1. on error
 *  Called by:
 *     GetTravelTimePrediction
 *  Calls:
 *     FloatBracket, SplineCoeffs, SplineInterpolation
 */
double GetTravelTimeTableValue(TT_TABLE *tt_tablep, double depth, double delta,
              int iszderiv, double *dtdd, double *dtdh, double *bpdel,
              int is2nderiv, double *d2tdd, double *d2tdh)
{
    int i, j, k, m, ilo, ihi, jlo, jhi, idel, jdep, ndep, ndel;
    int exactdelta = 0, exactdepth = 0, isdepthphase = 0;
    double ttim = -1., dydx = 0., d2ydx = 0.;
    double  x[DELTA_SAMPLES],  z[DEPTH_SAMPLES], d2y[DELTA_SAMPLES];
    double tx[DELTA_SAMPLES], tz[DEPTH_SAMPLES];
    double dx[DELTA_SAMPLES], dz[DEPTH_SAMPLES];
    double hx[DELTA_SAMPLES], hz[DEPTH_SAMPLES];
    double px[DELTA_SAMPLES], pz[DEPTH_SAMPLES], tmp[DELTA_SAMPLES];
    ndep = tt_tablep->ndep;
    ndel = tt_tablep->ndel;
    isdepthphase = tt_tablep->isbounce;
    *bpdel = 0.;
    *dtdd  = 0.;
    *dtdh  = 0.;
    *d2tdd  = 0.;
    *d2tdh  = 0.;
/*
 *  check if travel time table exists
 */
    if (ndel == 0)
        return ttim;
/*
 *  check for out of range depth or delta
 */
    if (depth < tt_tablep->depths[0] || depth > tt_tablep->depths[ndep - 1] ||
        delta < tt_tablep->deltas[0] || delta > tt_tablep->deltas[ndel - 1]) {
        return ttim;
    }
/*
 *  delta range
 */
    FloatBracket(delta, ndel, tt_tablep->deltas, &ilo, &ihi);
    if (fabs(delta - tt_tablep->deltas[ilo]) < DEPSILON) {
        idel = ilo;
        if (ilo > 0) ilo--;
        ihi = ilo + DELTA_SAMPLES;
        if (ihi > ndel - 1) {
            ihi = ndel;
            ilo = ihi - DELTA_SAMPLES;
        }
        exactdelta = 1;
    }
    else if (fabs(delta - tt_tablep->deltas[ihi]) < DEPSILON) {
        idel = ihi;
        if (ihi <= ndel - 1) ihi++;
        ilo = ihi - DELTA_SAMPLES;
        if (ilo < 0) {
            ilo = 0;
            ihi = ilo + DELTA_SAMPLES;
        }
        exactdelta = 1;
    }
    else if (ndel <= DELTA_SAMPLES) {
        ilo = 0;
        ihi = ndel;
        idel = ilo;
    }
    else {
        idel = ilo;
        ilo = idel - DELTA_SAMPLES / 2 + 1;
        ihi = idel + DELTA_SAMPLES / 2 + 1;
        if (ilo < 0) {
            ilo = 0;
            ihi = ilo + DELTA_SAMPLES;
        }
        if (ihi > ndel - 1) {
            ihi = ndel;
            ilo = ihi - DELTA_SAMPLES;
        }
    }
/*
 *  depth range
 */
    FloatBracket(depth, ndep, tt_tablep->depths, &jlo, &jhi);
    if (fabs(depth - tt_tablep->depths[jlo]) < DEPSILON) {
        jdep = jlo;
        if (jlo > 0) jlo--;
        jhi = jlo + DEPTH_SAMPLES;
        if (jhi > ndep - 1) {
            jhi = ndep;
            jlo = jhi - DEPTH_SAMPLES;
        }
        exactdepth = 1;
    }
    else if (fabs(depth - tt_tablep->depths[jhi]) < DEPSILON) {
        jdep = jhi;
        if (jhi <= ndep - 1) jhi++;
        jlo = jhi - DEPTH_SAMPLES;
        if (jlo < 0) {
            jlo = 0;
            jhi = jlo + DEPTH_SAMPLES;
        }
        exactdepth = 1;
    }
    else if (ndep <= DEPTH_SAMPLES) {
        jlo = 0;
        jhi = ndep;
        jdep = jlo;
    }
    else {
        jdep = jlo;
        jlo = jdep - DEPTH_SAMPLES / 2 + 1;
        jhi = jdep + DEPTH_SAMPLES / 2 + 1;
        if (jlo < 0) {
            jlo = 0;
            jhi = jlo + DEPTH_SAMPLES;
        }
        if (jhi > ndep - 1) {
            jhi = ndep;
            jlo = jhi - DEPTH_SAMPLES;
        }
    }
    if (exactdelta && exactdepth) {
        if (is2nderiv == 0) {
            ttim  = tt_tablep->tt[idel][jdep];
            *dtdd  = tt_tablep->dtdd[idel][jdep];
            if (iszderiv)     *dtdh  = tt_tablep->dtdh[idel][jdep];
            if (isdepthphase) *bpdel = tt_tablep->bpdel[idel][jdep];
            return ttim;
        }
    }
/*
 *  bicubic spline interpolation
 */
    for (k = 0, j = jlo; j < jhi; j++) {
/*
 *      no need for spline interpolation if exact delta
 */
        if (exactdelta && (is2nderiv == 0)) {
            if (tt_tablep->tt[idel][j] < 0) continue;
            z[k] = tt_tablep->depths[j];
            tz[k] = tt_tablep->tt[idel][j];
            if (isdepthphase) pz[k] = tt_tablep->bpdel[idel][j];
            dz[k] = tt_tablep->dtdd[idel][j];
            if (iszderiv)     hz[k] = tt_tablep->dtdh[idel][j];
            k++;
        }
/*
 *      spline interpolation in delta
 */
        else {
            for (m = 0, i = ilo; i < ihi; i++) {
                if (tt_tablep->tt[i][j] < 0) continue;
                x[m] = tt_tablep->deltas[i];
                tx[m] = tt_tablep->tt[i][j];
                if (isdepthphase) px[m] = tt_tablep->bpdel[i][j];
                dx[m] = tt_tablep->dtdd[i][j];
                if (iszderiv)     hx[m] = tt_tablep->dtdh[i][j];
                m++;
            }
            if (m < MIN_SAMPLES) continue;
            SplineCoeffs(m, x, tx, d2y, tmp);
            z[k] = tt_tablep->depths[j];
            tz[k] = SplineInterpolation(delta, m, x, tx, d2y, 0, &dydx, &d2ydx);
            if (isdepthphase) {
                SplineCoeffs(m, x, px, d2y, tmp);
                pz[k] = SplineInterpolation(delta, m, x, px, d2y, 0, &dydx,
                                            &d2ydx);
            }
            SplineCoeffs(m, x, dx, d2y, tmp);
            dz[k] = SplineInterpolation(delta, m, x, dx, d2y, 0, &dydx, &d2ydx);
            if (iszderiv) {
                SplineCoeffs(m, x, hx, d2y, tmp);
                hz[k] = SplineInterpolation(delta, m, x, hx, d2y, 0, &dydx,
                                            &d2ydx);
            }
            k++;
        }
    }
/*
 *  no valid data
 */
    if (k == 0) return ttim;
/*
 *  insufficient data for spline interpolation
 */
    if (k < MIN_SAMPLES) return ttim;
/*
 *  spline interpolation in depth
 */
    SplineCoeffs(k, z, tz, d2y, tmp);
    ttim = SplineInterpolation(depth, k, z, tz, d2y, 1, &dydx, &d2ydx);
    if (is2nderiv && d2ydx > -999.)
        *d2tdh = d2ydx;
    if (isdepthphase) {
        SplineCoeffs(k, z, pz, d2y, tmp);
        *bpdel = SplineInterpolation(depth, k, z, pz, d2y, 0, &dydx, &d2ydx);
    }
    SplineCoeffs(k, z, dz, d2y, tmp);
    *dtdd = SplineInterpolation(depth, k, z, dz, d2y, 0, &dydx, &d2ydx);
    if (iszderiv) {
        SplineCoeffs(k, z, hz, d2y, tmp);
        *dtdh = SplineInterpolation(depth, k, z, hz, d2y, 0, &dydx, &d2ydx);
    }
   if (is2nderiv) {
/*
 *      get d2t/dd2 from the transpose t matrix
 */
        for (k = 0, i = ilo; i < ihi; i++) {
            for (m = 0, j = jlo; j < jhi; j++) {
                if (tt_tablep->tt[i][j] < 0)
                    continue;
                z[m] = tt_tablep->depths[j];
                dz[m] = tt_tablep->dtdd[i][j];
                m++;
            }
            if (m < MIN_SAMPLES)
                continue;
/*
 *          spline interpolation in depth
 */
            x[k] = tt_tablep->deltas[i];
            SplineCoeffs(m, z, dz, d2y, tmp);
            dx[k] = SplineInterpolation(delta, m, z, dz, d2y, 0, &dydx, &d2ydx);
            k++;
        }
        if (k == 0) return ttim;
        if (k < MIN_SAMPLES) return ttim;
/*
 *      Spline interpolation in delta
 */
        SplineCoeffs(k, x, dx, d2y, tmp);
        SplineInterpolation(delta, k, x, dx, d2y, 1, &dydx, &d2ydx);
        if (dydx > -999.)
            *d2tdd = dydx;
    }
    return ttim;
}


/*
 *  Title:
 *     TravelTimeResiduals
 *  Synopsis:
 *     Calculates time residuals.
 *     Uses phase dependent information from <vmodel>_model.txt file.
 *        PhaseWithoutResidual - list of phases that don't get residuals, i.e.
 *                         never used in the location (e.g. amplitude phases)
 *     If mode is et to 'all' it attempts to get time residuals for all
 *        associated phases (final call),
 *     otherwise considers only time-defining phases.
 *  Input Arguments:
 *     sp        - pointer to current solution
 *     p         - array of phase structures
 *     mode      - "all" if require residuals for all phases
 *                 "use" if only want residuals for time-defining phases
 *     ec        - pointer to ellipticity correction coefficient structure
 *     tt_tables - pointer to travel-time tables
 *     localtt_tables - pointer to local travel-time tables, if any
 *     topo      - ETOPO bathymetry/elevation matrix
 *     iszderiv  - calculate dtdh [0/1]?
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     Locator, GetResiduals, ResidualsForFixedHypocenter
 *  Calls:
 *     GetTTResidual
 */
int TravelTimeResiduals(SOLREC *sp, PHAREC p[], char mode[4], EC_COEF *ec,
        TT_TABLE *tt_tables, TT_TABLE *localtt_tables, short int **topo,
        int iszderiv, int is2nderiv)
{
    int i, all = 0;
    if (verbose > 4)
        fprintf(logfp, "TravelTimeResiduals: %s iszderiv=%d is2nderiv=%d\n",
                mode, iszderiv, is2nderiv);
/*
 *  set flag to enable/disable residual calculation for unused phases
 */
    if      (streq(mode, "all")) all = 1;
    else if (streq(mode, "use")) all = 0;
    else {
        fprintf(logfp, "TravelTimeResiduals: unrecognised mode %s\n", mode);
        return 1;
    }
/*
 *  can't calculate residuals unless have a depth for the source
 */
    if (sp->depth == NULLVAL) {
        fprintf(logfp, "TravelTimeResiduals: depthless hypocentre\n");
        return 1;
    }
/*
 *  won't get residuals if source has gone too deep
 */
    if (sp->depth > MaxHypocenterDepth) {
        fprintf(logfp, "TravelTimeResiduals: solution too deep %f > %f \n",
                sp->depth, MaxHypocenterDepth);
        return 1;
    }
/*
 *  calculate time residual for associated/defining phases
 */
#ifdef GCD
/*
 *  use GCD (Mac OS) to parallelize the calculation of time residuals
 *  each phase is processed concurrently
 */
    if (sp->numPhase > 500) {
        if (verbose > 3) fprintf(logfp, "TravelTimeResiduals: GCD is on\n");
        dispatch_apply(sp->numPhase, dispatch_get_global_queue(0, 0),
                       ^(size_t i){ p[i].timeres = GetTTResidual(&p[i], all,
                                    sp, ec, tt_tables, localtt_tables, topo,
                                    iszderiv, is2nderiv); });
    }
    else {
        for (i = 0; i < sp->numPhase; i++) {
            p[i].timeres = GetTTResidual(&p[i], all, sp, ec, tt_tables,
                                localtt_tables, topo, iszderiv, is2nderiv);
        }
    }
#else
/*
 *  single core
 */
    for (i = 0; i < sp->numPhase; i++) {
        p[i].timeres = GetTTResidual(&p[i], all, sp, ec, tt_tables,
                            localtt_tables, topo, iszderiv, is2nderiv);
    }
#endif
/*
 *  clear current GreatCircle object and the pool of CrustalProfile objects
 */
    if (UseRSTT)
        slbm_shell_clear();
    return 0;
}

/*
 *  Title:
 *     GetTTResidual
 *  Synopsis:
 *     Calculates the time residual for a single phase.
 *     Uses phase dependent information from <vmodel>_model.txt file.
 *        PhaseWithoutResidual - list of phases that don't get residuals, i.e.
 *                         never used in the location (e.g. amplitude phases)
 *  Input Arguments:
 *     pp        - pointer to phase struct
 *     all       - 1 if require residuals for all phases
 *                 0 if only want residuals for time-defining phases
 *     sp        - pointer to current solution
 *     ec        - pointer to ellipticity correction coefficient structure
 *     tt_tables - pointer to travel-time tables
 *     localtt_tables - pointer to local travel-time tables, if any
 *     topo      - ETOPO bathymetry/elevation matrix
 *     iszderiv  - calculate dtdh [0/1]?
 *  Return:
 *     resid - time residual for a phase
 *  Called by:
 *     TravelTimeResiduals
 *  Calls:
 *     GetTravelTimePrediction, ResidualsForReportedPhases
 */
static double GetTTResidual(PHAREC *pp, int all, SOLREC *sp, EC_COEF *ec,
        TT_TABLE *tt_tables, TT_TABLE *localtt_tables, short int **topo,
        int iszderiv, int is2nderiv)
{
    double obtime = 0., resid = NULLVAL;
    int j, isfirst = 0;
/*
 *  azimuth residual
 */
    if (pp->azim != NULLVAL) {
        pp->azimres = pp->azim - pp->seaz;
        if (verbose > 2) {
            fprintf(logfp, "        %-6s %-8s ", pp->sta, pp->phase);
            fprintf(logfp, "delta=%8.3f azim=%7.2f seaz=%7.2f azimres=%7.2f\n",
                    pp->delta, pp->azim, pp->seaz, pp->azimres);
        }
    }
/*
 *  timeless or codeless phases don't have residuals
 */
    if (!all && (!pp->timedef || !pp->phase[0]))
        return resid;
    if (pp->time == NULLVAL)
        return resid;
    if (all) {
/*
 *      try to get residuals for all associated phases
 */
        if (pp->phase[0]) {
/*
 *          check for phases that don't get residuals (amplitudes etc)
 */
            for (j = 0; j < PhaseWithoutResidualNum; j++)
                if (streq(pp->phase, PhaseWithoutResidual[j]))
                    break;
            if (j != PhaseWithoutResidualNum)
                return resid;
        }
        else {
/*
 *          unidentified phase;
 *          try to get residual for the reported phase name
 */
            ResidualsForReportedPhases(sp, pp, ec, tt_tables, localtt_tables,
                                       topo);
        }
    }
/*
 *  observed travel time
 */
    obtime = pp->time - sp->time;
    if (verbose > 3)
        fprintf(logfp, "%9d %9d %-6s %-8s obtime: %9.4f\n",
                pp->rdid, pp->phid, pp->sta, pp->phase, obtime);
/*
 *  allow using first-arriving TT tables to deal with crossover distances?
 */
    if (all && !pp->timedef)
/*
 *      do not use first-arriving TT tables
 */
        isfirst = -1;
    else if (streq(pp->phase, "I") || streq(pp->phase, "H") || streq(pp->phase, "O"))
        isfirst = -1;
    else
/*
 *      allow
 */
        isfirst = 0;
/*
 *  predicted TT with corrections; partial derivatives if requested
 */
    if (GetTravelTimePrediction(sp, pp, ec, tt_tables, localtt_tables, topo,
                                iszderiv, isfirst, is2nderiv)) {
/*
 *      no valid TT prediction
 */
        if (all && !pp->phase_fixed)
            strcpy(pp->phase, "");
        pp->ttime = resid = NULLVAL;
        pp->dtdd = pp->dtdh = pp->d2tdd = pp->d2tdh = 0.;
    }
    else {
/*
 *      time residual
 */
        resid = obtime - pp->ttime;
        if (pp->slow != NULLVAL)
            pp->slowres = pp->slow - pp->dtdd;
    }
    if (verbose > 2) {
        fprintf(logfp, "        %-6s %-8s ", pp->sta, pp->phase);
        fprintf(logfp, "delta=%8.3f slow=%9.2f dtdd=%9.2f slowres=%9.2f\n",
                pp->delta, pp->slow, pp->dtdd, pp->slowres);
        fprintf(logfp, "        %-6s %-8s ", pp->sta, pp->phase);
        fprintf(logfp, "delta=%8.3f obst=%9.4f predt=", pp->delta, obtime);
        if (pp->ttime != NULLVAL) fprintf(logfp, "%9.4f ", pp->ttime);
        else                      fprintf(logfp, "%7s ", "");
        if (resid != NULLVAL) fprintf(logfp, "tres=%10.5f", resid);
        fprintf(logfp, "\n");
    }
    return resid;
}

/*
 *  Title:
 *     GetTravelTimePrediction
 *  Synopsis:
 *     Returns the travel-time prediction with elevation, ellipticity and
 *         optional bounce-point corrections for a phase.
 *     Horizontal and vertical slownesses are calculated if requested.
 *     The first two indices (0 and 1) in the TT table structures are
 *        reserved for the composite first-arriving P and S TT tables.
 *        The isfirst flags controls the use of these tables.
 *        1: in this case the phaseids are ignored and GetTravelTimePrediction
 *           returns the TT for the first-arriving P or S (never actually used)
 *        0: phaseids are respected, but first_arriving P or S tables can
 *           be used to get a valid TT table value at local/regional crossover
 *           distances without reidentifying the phase during the subsequent
 *           iterations of the location algorithm;
 *       -1: do not use them at all (the behaviour in phase id routines).
 *  Input Arguments:
 *     sp        - pointer to current solution.
 *     pp        - pointer to a phase record.
 *     ec        - pointer to ellipticity correction coefficient structure
 *     tt_tables - pointer to travel-time tables
 *     localtt_tables - pointer to local travel-time tables, if any
 *     topo      - ETOPO bathymetry/elevation matrix
 *     iszderiv  - calculate dtdh [0/1]?
 *     isfirst   - use first arriving composite tables?
 *                    1: ignore phaseid and use them
 *                    0: don't use but allow for fix at crossover distances
 *                   -1: don't use (used in phase id routines)
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     TravelTimeResiduals, ResidualsForReportedPhases, PhaseIdentification,
 *     SameArrivalTime, IdentifyPFAKE
 *  Calls:
 *     GetPhaseIndex, GetLocalPhaseIndex, GetTravelTimeTableValue,
 *     TravelTimeCorrections
 */
int GetTravelTimePrediction(SOLREC *sp, PHAREC *pp, EC_COEF *ec,
        TT_TABLE *tt_tables, TT_TABLE *localtt_tables, short int **topo,
        int iszderiv, int isfirst, int is2nderiv)
{
    int pind = 0, isdepthphase = 0, rstt_phase = 0;
    double ttim = 0., dtdd = 0., dtdh = 0., bpdel = 0., d2tdd = 0., d2tdh = 0.;
    double dtdlat = 0., dtdlon = 0., mperr = 0., merr = 0., perr = 0.;
    double lat, lon, depth, slat, slon, elev;
    char phase[PHALEN];
    strcpy(pp->vmod, "null");
/*
 *  invalid depth
 */
    if (sp->depth < 0. || sp->depth > MaxHypocenterDepth || sp->depth == NULLVAL) {
        fprintf(logfp, "GetTravelTimePrediction: invalid depth! depth=%.2f phase=%s\n",
                sp->depth, pp->phase);
        return 1;
    }
/*
 *  invalid delta
 */
    if (pp->delta < 0. || pp->delta > 180. || pp->delta == NULLVAL) {
        fprintf(logfp, "GetTravelTimePrediction: invalid delta! delta=%.2f phase=%s\n",
                pp->delta, pp->phase);
        return 1;
    }
/*
 *  get travel-time table index for phase
 */
    if (isfirst == 1) {
/*
 *      use composite first-arriving P or S travel-time tables
 */
        if      (toupper(pp->phase[0]) == 'P') pind = 0;
        else if (toupper(pp->phase[0]) == 'S') pind = 1;
        else                                   pind = -1;
    }
    else {
/*
 *      use travel-time tables specified for the phase
 */
        if (UseLocalTT && pp->delta <= MaxLocalTTDelta)
            pind = GetLocalPhaseIndex(pp->phase);
        else
            pind = GetPhaseIndex(pp->phase);
    }
    if (pind < 0) {
        if (verbose > 1)
            fprintf(logfp, "GetTravelTimePrediction: unknown phase %s!\n",
                    pp->phase);
        return 1;
    }
/*
 *  if depth phase, we need dtdd for bounce point correction
 */
    isdepthphase = 0;
    if (pp->phase[0] == 'p' || pp->phase[0] == 's')
        isdepthphase = 1;
    if (UseLocalTT && pp->delta <= MaxLocalTTDelta) {
/*
 *      use local travel-time tables
 */
        strcpy(pp->vmod, "local");
        ttim = GetTravelTimeTableValue(&localtt_tables[pind], sp->depth,
                    pp->delta, iszderiv, &dtdd, &dtdh, &bpdel,
                    is2nderiv, &d2tdd, &d2tdh);
/*
 *      couldn't get valid TT table value
 *         if we are allowed to use composite first-arriving travel-time
 *         tables, try them to deal with local/regional crossover ranges
 *         without renaming the phase
 */
        if (ttim < 0 && isfirst == 0) {
            if (streq(pp->phase, "Pg") || streq(pp->phase, "Pb") ||
                streq(pp->phase, "Pn") || streq(pp->phase, "P")) {
                ttim = GetTravelTimeTableValue(&localtt_tables[0], sp->depth,
                            pp->delta, iszderiv, &dtdd, &dtdh, &bpdel,
                            is2nderiv, &d2tdd, &d2tdh);
            }
            if (streq(pp->phase, "Sg") || streq(pp->phase, "Sb") ||
                streq(pp->phase, "Sn") || streq(pp->phase, "S")  ||
                streq(pp->phase, "Lg")) {
                ttim = GetTravelTimeTableValue(&localtt_tables[1], sp->depth,
                            pp->delta, iszderiv, &dtdd, &dtdh, &bpdel,
                            is2nderiv, &d2tdd, &d2tdh);
            }
        }
        if (verbose > 3) {
            fprintf(logfp, "sta=%-6s phase=%-8s delta=%.2f ",
                    pp->sta, pp->phase, pp->delta);
            fprintf(logfp, "tt=%.3f dtdd=%.3f dtdh=%.3f ", ttim, dtdd, dtdh);
            fprintf(logfp, "d2tdd=%.3f d2tdh=%.3f model=%s duplicate=%d\n",
                    d2tdd, d2tdh, pp->vmod, pp->duplicate);
        }
    }
    else if (UseRSTT) {
        lat = DEG_TO_RAD * sp->lat;
        lon = DEG_TO_RAD * sp->lon;
        depth = sp->depth;
        slat = DEG_TO_RAD * pp->StaLat;
        slon = DEG_TO_RAD * pp->StaLon;
        elev = -pp->StaElev / 1000.;
        if      (streq(pp->phase, "Pb")) strcpy(phase, "Pg");
        else if (streq(pp->phase, "Sb")) strcpy(phase, "Lg");
        else if (streq(pp->phase, "Sg")) strcpy(phase, "Lg");
        else                             strcpy(phase, pp->phase);
/*
 *      decide if the phase belongs to RSTT domain
 */
        if ((rstt_phase = isRSTT(pp, sp->depth)) == 0 ||
            slbm_shell_createGreatCircle(phase, &lat, &lon, &depth,
                                         &slat, &slon, &elev)) {
/*
 *          not RSTT, use ak135
 */
            strcpy(pp->vmod, "ak135");
/*
 *          get travel-time prediction from TT table
 */
            ttim = GetTravelTimeTableValue(&tt_tables[pind], sp->depth,
                        pp->delta, iszderiv, &dtdd, &dtdh, &bpdel,
                        is2nderiv, &d2tdd, &d2tdh);
/*
 *          couldn't get valid TT table value
 *             if we are allowed to use composite first-arriving travel-time
 *             tables, try them to deal with local/regional crossover ranges
 *             without renaming the phase
 */
            if (ttim < 0 && isfirst == 0) {
                if (pp->delta < 23) {
                    if (streq(pp->phase, "Pg") || streq(pp->phase, "Pb") ||
                        streq(pp->phase, "Pn") || streq(pp->phase, "P")) {
                        ttim = GetTravelTimeTableValue(&tt_tables[0], sp->depth,
                                    pp->delta, iszderiv, &dtdd, &dtdh, &bpdel,
                                    is2nderiv, &d2tdd, &d2tdh);
                    }
                    if (streq(pp->phase, "Sg") || streq(pp->phase, "Sb") ||
                        streq(pp->phase, "Sn") || streq(pp->phase, "S")  ||
                        streq(pp->phase, "Lg")) {
                        ttim = GetTravelTimeTableValue(&tt_tables[1], sp->depth,
                                    pp->delta, iszderiv, &dtdd, &dtdh, &bpdel,
                                    is2nderiv, &d2tdd, &d2tdh);
                    }
                }
            }
            if (verbose > 3) {
                fprintf(logfp, "sta=%-6s phase=%-8s delta=%.2f ",
                        pp->sta, pp->phase, pp->delta);
                fprintf(logfp, "tt=%.3f dtdd=%.3f dtdh=%.3f ", ttim, dtdd, dtdh);
                fprintf(logfp, "d2tdd=%.3f d2tdh=%.3f model=%s duplicate=%d\n",
                        d2tdd, d2tdh, pp->vmod, pp->duplicate);
            }
        }
        else {
/*
 *          get travel-time prediction from RSTT
 */
            strcpy(pp->vmod, "RSTT");
            if (slbm_shell_getTravelTime(&ttim)) ttim = -999.;
            if (ttim >= 0.) {
/*
 *              path-dependent uncertainty (total error = model + pick error)
 */
                if (slbm_shell_getTTUncertainty_useRandErr(&mperr)) mperr = NULLVAL;
                if (mperr < NULLVAL) {
/*
 *                  get pick error
 */
                    if (slbm_shell_getTTUncertainty(&merr)) merr = NULLVAL;
                    if (merr < NULLVAL) {
                        merr = max(0.25, merr);
                        perr = sqrt(fabs(mperr * mperr - merr * merr));
                        if (pp->phase[0] == 'P')
                            perr = max(0.8, perr);
                        else
                            perr = max(1.2, perr);
                        mperr = sqrt(fabs(merr * merr + perr * perr));
                    }
                }
/*
 *              derivatives
 */
                if (slbm_shell_get_dtt_dlat(&dtdlat)) dtdlat = NULLVAL;
                if (slbm_shell_get_dtt_dlon(&dtdlon)) dtdlon = NULLVAL;
                if (dtdlat < NULLVAL && dtdlon < NULLVAL)
                    dtdd = sqrt(dtdlat * dtdlat + dtdlon * dtdlon) / RAD_TO_DEG;
                if (iszderiv) {
                    if (slbm_shell_get_dtt_ddepth(&dtdh)) dtdh = 0.;
                }
            }
            if (verbose > 3) {
                fprintf(logfp, "sta=%-6s phase=%-8s delta=%.2f ",
                        pp->sta, pp->phase, pp->delta);
                fprintf(logfp, "tt=%.3f dtdd=%.3f dtdh=%.3f ", ttim, dtdd, dtdh);
                fprintf(logfp, "d2tdd=%.3f d2tdh=%.3f model=%s ",
                        d2tdd, d2tdh, pp->vmod);
                fprintf(logfp, "toterr=%.3f moderr=%.3f pickerr=%.3f duplicate=%d\n",
                        mperr, merr, perr, pp->duplicate);
            }
        }
    }
    else {
/*
 *      get travel-time table value from global tables
 */
        strcpy(pp->vmod, "ak135");
        ttim = GetTravelTimeTableValue(&tt_tables[pind], sp->depth,
                    pp->delta, iszderiv, &dtdd, &dtdh, &bpdel,
                    is2nderiv, &d2tdd, &d2tdh);
/*
 *      couldn't get valid TT table value
 *         if we are allowed to use composite first-arriving travel-time
 *         tables, try them to deal with local/regional crossover ranges
 *         without renaming the phase
 */
        if (ttim < 0 && isfirst == 0) {
            if (pp->delta < 23) {
                if (streq(pp->phase, "Pg") || streq(pp->phase, "Pb") ||
                    streq(pp->phase, "Pn") || streq(pp->phase, "P")) {
                    ttim = GetTravelTimeTableValue(&tt_tables[0], sp->depth,
                                pp->delta, iszderiv, &dtdd, &dtdh, &bpdel,
                                is2nderiv, &d2tdd, &d2tdh);
                }
                if (streq(pp->phase, "Sg") || streq(pp->phase, "Sb") ||
                    streq(pp->phase, "Sn") || streq(pp->phase, "S")  ||
                    streq(pp->phase, "Lg")) {
                    ttim = GetTravelTimeTableValue(&tt_tables[1], sp->depth,
                                pp->delta, iszderiv, &dtdd, &dtdh, &bpdel,
                                is2nderiv, &d2tdd, &d2tdh);
                }
            }
        }
        if (verbose > 3) {
            fprintf(logfp, "sta=%-6s phase=%-8s delta=%.2f ",
                    pp->sta, pp->phase, pp->delta);
            fprintf(logfp, "tt=%.3f dtdd=%.3f dtdh=%.3f ", ttim, dtdd, dtdh);
            fprintf(logfp, "d2tdd=%.3f d2tdh=%.3f model=%s duplicate=%d\n",
                    d2tdd, d2tdh, pp->vmod, pp->duplicate);
        }
    }
    if (ttim < 0) {
        if (verbose > 4) {
            fprintf(logfp, "        GetTravelTimePrediction: can't get TT for %s! ",
                    pp->phase);
            fprintf(logfp, "(depth=%.2f delta=%.2f)\n", sp->depth, pp->delta);
        }
        return 1;
    }
/*
 *  model predictions
 */
    pp->ttime = ttim;
    pp->dtdd = dtdd;
    if (iszderiv)      pp->dtdh = dtdh;
    if (isdepthphase) pp->bpdel = bpdel;
    if (is2nderiv) {
        pp->d2tdd = d2tdd;
        pp->d2tdh = d2tdh;
    }
    pp->rsttPickErr = perr;
    pp->rsttTotalErr = mperr;
/*
 *  elevation, ellipticity and bounce point corrections
 *  Note: RSTT includes all these
 */
    if (UseRSTT) {
        if (!rstt_phase)
            TravelTimeCorrections(sp, pp, ec, topo);
    }
    else
        TravelTimeCorrections(sp, pp, ec, topo);
    return 0;
}

/*
 *  Title:
 *     isRSTT
 *  Synopsis:
 *     Decides whether phase belongs to RSTT domain:
 *         delta < MAX_RSTT_DIST && phase is
 *         Pn/Sn or first-arriving Pg/Pb/Sg/Sb if crustal phases to be used)
 *  Input Arguments:
 *     pp        - pointer to a phase record.
 *  Return:
 *     1 if phase qualifies for RSTT, 0 otherwise
 *  Called by:
 *     GetTravelTimePrediction
 */
static int isRSTT(PHAREC *pp, double depth)
{
    if (UseRSTT == 0)
        return 0;
/*
 *  delta must be less than MAX_RSTT_DIST
 */
    if (pp->delta > MAX_RSTT_DIST)
        return 0;
/*
 *  check if RSTT phase
 */
    if (!(streq(pp->phase, "Pn") || streq(pp->phase, "Sn") ||
          streq(pp->phase, "Pg") || streq(pp->phase, "Lg") ||
          streq(pp->phase, "Sg") ||
          streq(pp->phase, "Pb") || streq(pp->phase, "Sb"))) {
        return 0;
    }
    if ((streq(pp->phase, "Pn") || streq(pp->phase, "Sn")) && !UseRSTTPnSn)
        return 0;
    if ((streq(pp->phase, "Pg") || streq(pp->phase, "Lg") ||
         streq(pp->phase, "Sg") ||
         streq(pp->phase, "Pb") || streq(pp->phase, "Sb")) && !UseRSTTPgLg)
        return 0;
/*
 *  RSTT doesn't calculate direct waves for events below the crust
 */
    if ((streq(pp->phase, "Pg") || streq(pp->phase, "Lg") ||
         streq(pp->phase, "Sg") ||
         streq(pp->phase, "Pb") || streq(pp->phase, "Sb")) &&
         pp->delta < 0.75 && depth > 40)
        return 0;
    return 1;
}

/*
 *  Title:
 *     TravelTimeCorrections
 *  Synopsis:
 *     Applies travel-time corrections to a predicted TT table value.
 *     Approximate geoid correction is calculated for Jeffreys-Bullen;
 *     otherwise the ak135 (Kennett and Gudmundsson, 1996) ellipticity
 *         correction is used.
 *     Bounce point correction is applied for depth phases, and for pwP,
 *        water depth correction is also calculated.
 *  Input Arguments:
 *     sp   - pointer to current solution.
 *     pp   - pointer to a phase structure.
 *     ec   - pointer to ellipticity correction coefficient structure
 *     topo - ETOPO bathymetry/elevation matrix
 *  Called by:
 *     GetTravelTimePrediction
 *  Calls:
 *     GetEllipticityCorrection, GetElevationCorrection, GetBounceCorrection
 */
static void TravelTimeCorrections(SOLREC *sp, PHAREC *pp, EC_COEF *ec,
                          short int **topo)
{
    double geoid_corr = 0., elev_corr = 0., ellip_corr = 0.;
    double bounce_corr = 0., water_corr = 0.;
    double f = (1. - FLATTENING) * (1. - FLATTENING);
    double ecolat = 0.;
/*
 *  ak135 ellipticity corrections (Kennett and Gudmundsson, 1996)
 */
    if (sp->lat != NULLVAL) {
/*
 *      ellipticity correction using Dziewonski and Gilbert (1976)
 *      formulation for ak135/iasp91
 */
        ecolat = PI2 - atan(f * tan(DEG_TO_RAD * sp->lat));
        ellip_corr = GetEllipticityCorrection(ec, pp->phase, ecolat, pp->delta,
                                              sp->depth, pp->esaz);
        pp->ttime += ellip_corr;
        if (verbose > 4)
            fprintf(logfp, "            %-6s ellip_corr=%.3f\n",
                    pp->sta, ellip_corr);
    }
/*
 *  elevation correction
 */
    elev_corr = GetElevationCorrection(pp);
    pp->ttime += elev_corr;
    if (verbose > 4)
        fprintf(logfp, "            %-6s elev_corr=%.3f\n",
                pp->sta, elev_corr);
/*
 *  depth phase bounce point correction
 */
    if (pp->phase[0] == 'p' || pp->phase[0] == 's') {
        bounce_corr = GetBounceCorrection(sp, pp, topo, &water_corr);
        pp->ttime += bounce_corr;
        if (verbose > 4)
            fprintf(logfp, "            %-8s bounce_corr=%.3f\n",
                    pp->phase, bounce_corr);
/*
 *      water depth correction for pwP
 */
        if (streq(pp->phase, "pwP")) {
            pp->ttime += water_corr;
            if (verbose > 4)
                fprintf(logfp, "            %-8s bounce_corr=%.3f\n",
                        pp->phase, water_corr);
        }
    }
}

/*
 *  Title:
 *     GetBounceCorrection
 *  Synopsis:
 *     Returns the correction for topography/bathymetry at the bounce point
 *        for a depth phase, as well as the water depth correction for pwP.
 *     Adopted from Bob Engdahl's libtau extensions.
 *  Input Arguments:
 *     sp   - pointer to current solution.
 *     pp   - pointer to a phase structure.
 *     topo - ETOPO bathymetry/elevation matrix
 *  Output Arguments:
 *     tcorw - water travel time correction (water column)
 *  Return:
 *     tcorc - crust travel time correction (topography)
 *  Called by:
 *     TravelTimeCorrections
 *  Calls:
 *     PointAtDeltaAzimuth, GetEtopoCorrection
 */
static double GetBounceCorrection(SOLREC *sp, PHAREC *pp,
                              short int **topo, double *tcorw)
{
    int ips = 0;
    double tcor = 0., bp2 = 0., bpaz = 0., bplat = 0., bplon = 0.;
    *tcorw = 0.;
/*
 *  get geographic coordinates of bounce point
 */
    bp2 = pp->dtdd;
    bpaz = pp->esaz;
    if (bp2 < 0.)    bpaz += 180.;
    if (bpaz > 360.) bpaz -= 360.;
    PointAtDeltaAzimuth(sp->lat, sp->lon, pp->bpdel, bpaz, &bplat, &bplon);
/*
 *  get topography/bathymetry correction for upgoing part of depth phase
 */
    if      (strncmp(pp->phase, "pP", 2) == 0)  ips = 1;
    else if (strncmp(pp->phase, "pwP", 3) == 0) ips = 1;
    else if (strncmp(pp->phase, "pS", 2) == 0)  ips = 2;
    else if (strncmp(pp->phase, "sP", 2) == 0)  ips = 2;
    else if (strncmp(pp->phase, "sS", 2) == 0)  ips = 3;
    else                                        ips = 4;
    tcor = GetEtopoCorrection(ips, bp2, bplat, bplon, topo, tcorw);
    return tcor;
}

/*
 *  Title:
 *     GetEtopoCorrection
 *  Synopsis:
 *     Calculates bounce point correction for depth phases.
 *     Calculates water depth correction for pwP if water column > 1.5 km.
 *     Uses Bob Engdahl's topography equations.
 *  Input Arguments:
 *     ips   - 1 if pP* wave, 2 if sP* or pS* wave, 3 if sS* wave
 *     rayp  - horizontal slowness [s/deg]
 *     bplat - latitude of surface reflection point
 *     bplon - longitude of surface reflection point
 *     topo  - ETOPO bathymetry/elevation matrix
 *  Output Arguments:
 *     tcorw - water travel time correction (water column)
 *     wdep  - water column depth
 *  Return:
 *     tcorc - crust travel time correction (topography)
 *  Called by:
 *     GetBounceCorrection
 *  Calls:
 *     GetEtopoElevation
 */
double GetEtopoCorrection(int ips, double rayp, double bplat, double bplon,
              short int **topo, double *tcorw)
{
    double watervel = 1.5;                    /* P velocity in water [km/s] */
    double delr = 0., term = 0., term1 = 0., term2 = 0.;
    double bp2 = 0., tcorc = 0.;
    *tcorw = 0.;
/*
 *  get topography/bathymetry elevation
 */
    delr = GetEtopoElevation(bplat, bplon, topo);
    if (fabs(delr) < DEPSILON) return tcorc;
    bp2 = fabs(rayp) * RAD_TO_DEG / EARTH_RADIUS;
    if (ips == 1) {
/*
 *      pP*
 */
        term = PSurfVel * PSurfVel * bp2 * bp2;
        if (term > 1.) term = 1.;
        tcorc = 2. * delr * Sqrt(1. - term) / PSurfVel;
        if (delr < -1.5) {
/*
 *          water depth is larger than 1.5 km
 */
            term = watervel * watervel * bp2 * bp2;
            if (term > 1.) term = 1.;
            *tcorw = -2. * delr * Sqrt(1. - term) / watervel;
        }
    }
    else if (ips == 2) {
/*
 *      pS* or sP*
 */
        term1 = PSurfVel * PSurfVel * bp2 * bp2;
        if (term1 > 1.) term1 = 1.;
        term2 = SSurfVel * SSurfVel * bp2 * bp2;
        if (term2 > 1.) term2 = 1.;
        tcorc = delr * (Sqrt(1. - term1) / PSurfVel +
                        Sqrt(1. - term2) / SSurfVel);
    }
    else if (ips == 3) {
/*
 *      sS*
 */
        term = SSurfVel * SSurfVel * bp2 * bp2;
        if (term > 1.) term = 1.;
        tcorc = 2. * delr * Sqrt(1. - term) / SSurfVel;
    }
    else
        tcorc = 0.;
    return tcorc;
}

/*
 *  Title:
 *     GetElevationCorrection
 *  Synopsis:
 *     Calculates elevation correction for a station.
 *  Input Arguments:
 *     pp - pointer to phase structure.
 *  Return:
 *     Travel time correction for station elevation.
 *  Called by:
 *     TravelTimeCorrections
 *  Calls:
 *     lastlag
 */
static double GetElevationCorrection(PHAREC *pp)
{
    double elev_corr = 0., surfvel = 0.;
    int lastlag = 0;
/*
 *  unknown station elevation
 */
    if (pp->StaElev == NULLVAL)
        return 0.;
/*
 *  find last lag of phase (P or S-type)
 */
    lastlag = GetLastLag(pp->phase);
    if (lastlag == 1)                 /* last lag is P */
        surfvel = PSurfVel;
    else if (lastlag == 2)            /* last lag is S */
        surfvel = SSurfVel;
    else                             /* invalid/unknown */
        return 0.;
/*
 *  elevation correction
 */
    elev_corr  = surfvel * (pp->dtdd / DEG2KM);
    elev_corr *= elev_corr;
    if (elev_corr > 1.)
        elev_corr = 1./ elev_corr;
    elev_corr  = Sqrt(1. - elev_corr);
    elev_corr *= pp->StaElev / (1000. * surfvel);
    return elev_corr;
}


/*
 *  Title:
 *     GetLastLag
 *  Synopsis:
 *     Finds last lag of phase (P or S-type)
 *  Input Arguments:
 *     phase - phase name
 *  Called by:
 *    GetElevationCorrection
 *  Return:
 *     1 if P-type, 2 if S-type, 0 otherwise
 */
static int GetLastLag(char phase[]) {
    int lastlag = 0;
    int i, n;
    n = (int)strlen(phase);
    if (n == 0)
        return lastlag;
    for (i = n - 1; i > -1; i--) {
        if (isupper(phase[i])) {
            if (phase[i] == 'P') {
                lastlag = 1;
                break;
            }
            if (phase[i] == 'S') {
                lastlag = 2;
                break;
            }
        }
    }
    return lastlag;
}

/*  EOF  */

