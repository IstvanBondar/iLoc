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
extern double Moho;                                            /* Moho depth */
extern double Conrad;                                        /* Conrad depth */
// extern int LocalTTfromRSTT;            /* get local velocity model from RSTT */
extern int numLocalPhaseTT;                        /* number of local phases */
extern char LocalPhaseTT[MAXLOCALTTPHA][PHALEN];         /* local phase list */

/*
 * Local functions
 */
static int ReadLocalVelocityModel(char *fname, VMODEL *LocalVelocityModelp);
static void FreeLocalVelocityModel(VMODEL *LocalVelocityModelp);
static TT_TABLE *AllocateLocalTTtable(int ndepths, int ndists);
static int GetVelocityProfileFromRSTT(double lat, double lon,
        VMODEL *LocalVelocityModelp);
static int GenerateLocalTT(double depth, double delta,
        VMODEL *LocalVelocityModelp, char **phcd, double *ttc,
        double *dtdd, double *dtdh);
static double DirectPhase(int n, double *v, double *vsq, double *thk, int iq,
        double dq, double delta, double depth, double *tdir);
static int RefractedPhase(int n, double *v, double *vsq, double *thk, int iq,
        double dq, double delta, double *tref);
static void CriticalDistanceTTIntercept(int n, double *v, double *vsq,
        double *thk, int iq, double *tid, double *did);

/*
 * file scope globals
 */
#define MAXLAY 21
#define NDEP 33
#define NDIS 20


/*
 *  Title:
 *     GenerateLocalTTtables
 *  Desc:
 *     Generate travel-time tables from local velocity model
 *  Input Arguments:
 *     filename - pathname for local velocity model
 *     lat, lon - reference point for local velocity model
 *  Return:
 *     TTtables - pointer to TT_TABLE structure or NULL on error
 *  Calls:
 *     ReadLocalVelocityModel, AllocateLocalTTtable, GenerateLocalTT,
 *     GetLocalPhaseIndex, FreeLocalVelocityModel
 */
TT_TABLE *GenerateLocalTTtables(char *filename, double lat, double lon)
{
    char *phcd[MAXLOCALTTPHA], phcd_buf[MAXLOCALTTPHA * PHALEN];
    double ttc[MAXLOCALTTPHA], dtdd[MAXLOCALTTPHA], dtdh[MAXLOCALTTPHA];
    TT_TABLE *TTtables = (TT_TABLE *)NULL;
    VMODEL LocalVelocityModel;
    double hmax, hd, delta, depth, h[2 * MAXLAY];
    int npha, n, i, j, k, ind, ndists, ndepths, icon, imoh;
    static double dists[NDIS] = {
          0.0,  0.025, 0.05, 0.1, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5,
          1.75, 2.0,   2.5,  3.0, 3.5,  4.0, 4.5,  5.0, 5.5,  6.0
    };
    static double depths[NDEP] = {
          0.0,   1.0,   2.5,   5.0,   7.5,  10.0,  12.5,  15.0,  17.5,  20.0,
         22.5,  25.0,  27.5,  30.0,  32.5,  35.0,  40.0,  45.0,  50.0,  75.0,
        100.0, 150.0, 200.0, 250.0, 300.0, 350.0, 400.0, 450.0, 500.0, 550.0,
        600.0, 650.0, 700.0
    };
    for (i = 0; i < MAXLOCALTTPHA; i++) phcd[i] = phcd_buf + i * PHALEN;
//    if (LocalTTfromRSTT) {
/*
 *      get local velocity model from RSTT velocity profile at lat,lon
 */
//        if (GetVelocityProfileFromRSTT(lat, lon, &LocalVelocityModel))
//            return (TT_TABLE *)NULL;
//        fprintf(logfp, "Local velocity model from RSTT at (%7.2f , %6.2f)\n",lat, lon);
//    }
//    else {
/*
 *      read local velocity model
 */
        if (ReadLocalVelocityModel(filename, &LocalVelocityModel))
            return (TT_TABLE *)NULL;
        fprintf(logfp, "Local velocity model from %s\n", filename);
//    }
    n = LocalVelocityModel.n;
    icon = LocalVelocityModel.iconr;
    imoh = LocalVelocityModel.imoho;
    hmax = LocalVelocityModel.h[n-1] - 1.;
/*
 *  print local velocity model
 */
    fprintf(logfp, "    LAYER   DEPTH   VP    VS\n");
    for (i = 0; i < n; i++) {
        if (i == icon)
            fprintf(logfp, "%9d %7.3f %5.3f %5.3f CONRAD\n",
                    i, LocalVelocityModel.h[i],
                    LocalVelocityModel.vp[i], LocalVelocityModel.vs[i]);
        else if (i == imoh)
            fprintf(logfp, "%9d %7.3f %5.3f %5.3f MOHO\n",
                    i, LocalVelocityModel.h[i],
                    LocalVelocityModel.vp[i], LocalVelocityModel.vs[i]);
        else
            fprintf(logfp, "%9d %7.3f %5.3f %5.3f\n",
                    i, LocalVelocityModel.h[i],
                    LocalVelocityModel.vp[i], LocalVelocityModel.vs[i]);
    }
/*
 *  generate depth samples
 */
    k = 0;
    if (icon) {
        hd = LocalVelocityModel.h[icon] - 0.01;
        for (i = 0; depths[i] < hd; i++)
            h[k++] = depths[i];
        h[k++] = hd;
        if (LocalVelocityModel.h[icon] <= depths[i]) {
            h[k++] = LocalVelocityModel.h[icon];
            h[k++] = LocalVelocityModel.h[icon] + 0.01;
            i++;
        }
        if (imoh) {
            hd = LocalVelocityModel.h[imoh] - 0.01;
            for (; depths[i] < hd; i++)
                h[k++] = depths[i];
            h[k++] = hd;
            if (LocalVelocityModel.h[imoh] <= depths[i]) {
                h[k++] = LocalVelocityModel.h[imoh];
                h[k++] = LocalVelocityModel.h[imoh] + 0.01;
                i++;
            }
            for (; depths[i] < hmax; i++)
                h[k++] = depths[i];
        }
        else {
            for (; depths[i] < hmax; i++)
                h[k++] = depths[i];
        }
    }
    else if (imoh) {
        hd = LocalVelocityModel.h[imoh] - 0.01;
        for (i = 0; depths[i] < hd; i++)
            h[k++] = depths[i];
        h[k++] = hd;
        if (LocalVelocityModel.h[imoh] <= depths[i]) {
            h[k++] = LocalVelocityModel.h[imoh];
            h[k++] = LocalVelocityModel.h[imoh] + 0.01;
        }
        for (; depths[i] < hmax; i++)
            h[k++] = depths[i];
    }
    else {
        for (i = 0; depths[i] < hmax; i++)
            h[k++] = depths[i];
    }
/*
 *  memory allocations
 */
    ndepths = k;
    ndists = NDIS;
    if ((TTtables = AllocateLocalTTtable(ndepths, ndists)) == NULL) {
        FreeLocalVelocityModel(&LocalVelocityModel);
        return (TT_TABLE *)NULL;
    }
/*
 *  set delta and depth samples
 */
    for (ind = 0; ind < numLocalPhaseTT; ind++) {
        for (i = 0; i < ndists; i++)
            TTtables[ind].deltas[i] = dists[i];
        for (j = 0; j < ndepths; j++)
            TTtables[ind].depths[j] = h[j];
    }
/*
 *  generate TT tables
 */
    for (i = 0; i < ndists; i++) {
        delta = TTtables[0].deltas[i];
        for (j = 0; j < ndepths; j++) {
            depth = TTtables[0].depths[j];
            for (k = 0; k < numLocalPhaseTT; k++) {
                TTtables[k].tt[i][j] = NULLVAL;
                TTtables[k].dtdd[i][j] = -999.;
                TTtables[k].dtdh[i][j] = -999.;
                if (TTtables[k].isbounce)
                    TTtables[k].bpdel[i][j] = -999.;
            }
            npha = GenerateLocalTT(depth, delta, &LocalVelocityModel,
                                   phcd, ttc, dtdd, dtdh);
            if (npha) {
                for (k = 0; k < npha; k++) {
                    ind = GetLocalPhaseIndex(phcd[k]);
                    TTtables[ind].tt[i][j] = ttc[k];
                    TTtables[ind].dtdd[i][j] = dtdd[k];
                    TTtables[ind].dtdh[i][j] = dtdh[k];
                    if (ttc[k] < 0.) continue;
/*
 *                  first arriving P
 */
                    if (LocalPhaseTT[ind][0] == 'P' &&
                        ttc[k] < TTtables[0].tt[i][j]) {
                        TTtables[0].tt[i][j] = ttc[k];
                        TTtables[0].dtdd[i][j] = dtdd[k];
                        TTtables[0].dtdh[i][j] = dtdh[k];
                    }
/*
 *                  first arriving S
 */
                    if (LocalPhaseTT[ind][0] == 'S' &&
                        ttc[k] < TTtables[1].tt[i][j]) {
                        TTtables[1].tt[i][j] = ttc[k];
                        TTtables[1].dtdd[i][j] = dtdd[k];
                        TTtables[1].dtdh[i][j] = dtdh[k];
                    }
                }
            }
            for (k = 0; k < numLocalPhaseTT; k++) {
                if (TTtables[k].tt[i][j] == NULLVAL)
                    TTtables[k].tt[i][j] = -999.;
            }
        }
    }
    FreeLocalVelocityModel(&LocalVelocityModel);
    return TTtables;
}

/*
 *  Title:
 *     AllocateLocalTTtable
 *  Desc:
 *     Allocate meory for local travel-time tables
 *  Input Arguments:
 *     ndepths - number of depth samples
 *     ndists - number of delta samples
 *  Return:
 *     TTtables - pointer to TT_TABLE structure or NULL on error
 */
static TT_TABLE *AllocateLocalTTtable(int ndepths, int ndists)
{
    TT_TABLE *TTtables = (TT_TABLE *)NULL;
    int ind = 0, isbounce = 0;
/*
 *  memory allocation
 */
    TTtables = (TT_TABLE *)calloc(numLocalPhaseTT, sizeof(TT_TABLE));
    if (TTtables == NULL) {
        fprintf(errfp, "AllocateLocalTTtable: cannot allocate memory\n");
        fprintf(errfp, "AllocateLocalTTtable: cannot allocate memory\n");
        errorcode = 1;
        return (TT_TABLE *) NULL;
    }
/*
 *  loop for local phases
 *      numLocalPhaseTT and LocalPhaseTT are specified in iloc.h, iloc_main.c
 */
    for (ind = 0; ind < numLocalPhaseTT; ind++) {
/*
 *      initialize TTtables for this phase
 */
        isbounce = 0;
        if (LocalPhaseTT[ind][0] == 'p' || LocalPhaseTT[ind][0] == 's' ||
            LocalPhaseTT[ind][0] == LocalPhaseTT[ind][1] ||
            !strncmp(&LocalPhaseTT[ind][0], &LocalPhaseTT[ind][2], 2))
            isbounce = 1;
        strcpy(TTtables[ind].phase, LocalPhaseTT[ind]);
        TTtables[ind].ndel = ndists;
        TTtables[ind].ndep = ndepths;
        TTtables[ind].isbounce = isbounce;
        TTtables[ind].deltas = (double *)NULL;
        TTtables[ind].depths = (double *)NULL;
        TTtables[ind].tt = (double **)NULL;
        TTtables[ind].bpdel = (double **)NULL;
        TTtables[ind].dtdd = (double **)NULL;
        TTtables[ind].dtdh = (double **)NULL;
/*
 *      memory allocations
 */
        TTtables[ind].deltas = (double *)calloc(ndists, sizeof(double));
        TTtables[ind].depths = (double *)calloc(ndepths, sizeof(double));
        if (isbounce)
            TTtables[ind].bpdel = AllocateFloatMatrix(ndists, ndepths);
        TTtables[ind].tt = AllocateFloatMatrix(ndists, ndepths);
        TTtables[ind].dtdd = AllocateFloatMatrix(ndists, ndepths);
        if ((TTtables[ind].dtdh = AllocateFloatMatrix(ndists, ndepths)) == NULL) {
            FreeLocalTTtables(TTtables);
            fprintf(logfp, "AllocateLocalTTtable: cannot allocate memory\n");
            fprintf(errfp, "AllocateLocalTTtable: cannot allocate memory\n");
            errorcode = 1;
            return (TT_TABLE *) NULL;
        }
    }
    return TTtables;
}

/*
 *  Title:
 *     GenerateLocalTT
 *  Desc:
 *     Calculate travel-times from local velocity model for (depth, delta)
 *  Input Arguments:
 *     depth - source depth in km
 *     delta - receiver distance in degrees
 *     LocalVelocityModelp - local velocity model
 *  Output Arguments:
 *     phcd - phasenames with valid TT prediction
 *     ttc  - TT predictions
 *     dtdd - horizontal slowness predictions
 *     dtdh - vertical slowness predictions
 *  Return:
 *     npha - number of phases with valid TT prediction
 *  Calls:
 *     DirectPhase, RefractedPhase
 */
static int GenerateLocalTT(double depth, double delta,
        VMODEL *LocalVelocityModelp, char **phcd, double *ttc,
        double *dtdd, double *dtdh)
{
    char phase[PHALEN], wtype[PHALEN], lag[PHALEN];
    double vsq[MAXLAY], tref[MAXLAY], thk[MAXLAY], z[MAXLAY], v[MAXLAY];
    double *vq, b, tdir, u, dq, dkm;
    int iq, i, ks, n, npha, icon, imoh, noref;
    n = LocalVelocityModelp->n;
    icon = LocalVelocityModelp->iconr;
    imoh = LocalVelocityModelp->imoho;
    dkm = delta * DEG2KM;
/*
 *  cannot generate TT for depths below the bottom of the velocity model
 */
    if (depth > LocalVelocityModelp->h[n-1])
        return 0;
/*
 *  initializations
 */
    for (i = 0; i < numLocalPhaseTT; i++)
        ttc[i] = dtdd[i] = dtdh[i] = -999.;
/*
 *  phase loop
 */
    npha = 0;
    for (ks = 0; ks < 2; ks++) {
        if (ks) {
            vq = LocalVelocityModelp->vs;
            strcpy(wtype, "S");
        }
        else {
            vq = LocalVelocityModelp->vp;
            strcpy(wtype, "P");
        }
/*
 *      Earth flattening
 *      get event layer (iq), get depth of event from top of event layer (dq)
 *      make sure that source depth doesn't fall on layer boundary
 */
        for (i = 0; i < n; i++) {
            b = EARTH_RADIUS / (EARTH_RADIUS - LocalVelocityModelp->h[i]);
            z[i] = EARTH_RADIUS * log(b);
            v[i] = b * vq[i];
            vsq[i] = v[i] * v[i];
            if (fabs(depth - z[i]) < 0.0001)
                depth -= 0.001;
        }
        if (depth < 0.)
            depth = 0.001;
        iq = n;
        for (i = 0; i < n - 1; i++) {
            thk[i] = z[i+1] - z[i];
            if (depth >= z[i] && depth < z[i+1]) {
                iq = i;
                dq = depth - z[i];
            }
        }
        thk[i] = 10.;

/*
 *      direct wave Pg/Pb/Pn, Sg/Sb/Sn
 *
 *      takeoff angle = u
 *      horizontal slowness = sin(u) / v[iq]
 *      vertical slowness   = cos(u) / v[iq]
 */
        u = DirectPhase(n, v, vsq, thk, iq, dq, dkm, depth, &tdir);
        if (iq < imoh) {
            if (iq < icon || icon < 0) strcpy(lag, "g");
            else           strcpy(lag, "b");
        }
        else {
            strcpy(lag, "n");
        }
        if (depth > 400.) strcpy(lag, "");
        strcpy(phase, wtype);
        strcat(phase, lag);
        strcpy(phcd[npha], phase);
        ttc[npha] = tdir;
        dtdh[npha] = -cos(u) / v[iq];
        dtdd[npha] = sin(u) * DEG2KM / v[iq];
        npha++;
        if (streq(phase, "Sg")) {
/*
 *          duplicate Sg as Lg
 */
            strcpy(phcd[npha], "Lg");
            ttc[npha] = tdir;
            dtdh[npha] = -cos(u) / v[iq];
            dtdd[npha] = sin(u) * DEG2KM / v[iq];
            npha++;
        }
/*
 *      refracted waves
 *
 *      takeoff angle =  u  = asin(v[iq] / v[imoh])
 *      horizontal slowness = sin(u) / v[iq] = 1 / v[imoh]
 *      vertical slowness   = cos(u) / v[iq] = sqrt(1 - sin2(u)) / v[iq]
 *                          = sqrt(vsq[imoh] - vsq[iq]) / (v[imoh] * v[iq])
 */
        if (iq > imoh)
           continue;
        noref = RefractedPhase(n, v, vsq, thk, iq, dq, dkm, tref);
        if (noref)
            continue;
        if (iq < icon) {
/*
 *          Pb/Sb
 */
            if (tref[icon] < 999999.) {
                strcpy(lag, "b");
                strcpy(phase, wtype);
                strcat(phase, lag);
                strcpy(phcd[npha], phase);
                ttc[npha] = tref[icon];
                dtdh[npha] = -sqrt(vsq[icon] - vsq[iq]) / (v[icon] * v[iq]);
                dtdd[npha] = DEG2KM / v[icon];
                npha++;
            }
        }
/*
 *      Pn/Sn
 */
        if (tref[imoh] < 999999.) {
            strcpy(lag, "n");
            strcpy(phase, wtype);
            strcat(phase, lag);
            strcpy(phcd[npha], phase);
            ttc[npha] = tref[imoh];
            dtdh[npha] = -sqrt(vsq[imoh] - vsq[iq]) / (v[imoh] * v[iq]);
            dtdd[npha] = DEG2KM / v[imoh];
            npha++;
        }
    }
    return npha;
}

/*
 *  Title:
 *     DirectPhase
 *  Synopsis:
 *	   Computes DirectPhase wave travel time and takeoff angle.
 *     To find the takeoff angle of the ray the regula falsi method
 *     is applied. From u and x, tdir is found by summing the travel
 *     time in each layer.  finally, a slight correction to tdir is made,
 *     based on the misfit between the final del and delta.
 *  Input Arguments:
 *     n     - number of layers
 *     v     - velocities
 *     vsq   - velocity squares
 *     thk   - layer thicknesses
 *     iq    - event layer
 *     dq    - depth of event in event layer
 *     delta - epicentral distance in km
 *     depth - depth of event
 *  Output Arguments:
 *     tdir - DirectPhase wave travel time
 *  Return:
 *     u - takeoff angle
 *  Called by:
 *     GenerateLocalTT
 */
static double DirectPhase(int n, double *v, double *vsq, double *thk, int iq,
        double dq, double delta, double depth, double *tdir)
{
    int i, j, imax;
    double u, ua, ub, usq, uasq, ubsq, xa, xb, x, del, dela, delb;
    double t, dmax, vmax;
/*
 *  source is in the surface layer
 */
    if (iq == 0) {
        x = sqrt(depth * depth + delta * delta);
        *tdir = x / v[iq];
        u = 0.;
        if (x > 0.)
            u = PI - asin(delta / x);
        return u;
    }
/*
 *  Find the fastest layer above and including event layer
 */
    imax = iq;
    dmax = dq;
    vmax = v[iq];
    for (i = 0; i < iq; i++) {
        if (v[i] > vmax) {
            imax = i;
            dmax = thk[i];
            vmax = v[i];
        }
    }
/*
 *  Initial bounds on the sine of takeoff angle
 *               v[iq]          v[iq]           delta
 *      sin(i) = ----- sin(j) = ----- * -------------------------
 *               vmax           vmax    sqrt(depth**2 + delta**2)
 *
 *      x = dq * tan(i)
 */
    ua = v[iq] * delta / (vmax * sqrt(depth * depth + delta * delta));
    x = sqrt(dmax * dmax + delta * delta);
    ub = 0.999999;
    if (x > 0.)
        ub = v[iq] * delta / (vmax * x);
    uasq = ua * ua;
    ubsq = ub * ub;
    xa = dq * ua / sqrt(1. - uasq);
    if (imax == iq) {
        xb = delta;
        if (delta < 0.001) xb += 0.05;
    }
    else
        xb = dq * ub / sqrt(1. - ubsq);
    dela = xa;
    delb = xb;
    for (i = 0; i < iq; i++) {
        dela += thk[i] * ua / sqrt(vsq[iq] / vsq[i] - uasq);
        delb += thk[i] * ub / sqrt(vsq[iq] / vsq[i] - ubsq);
    }
/*
 *  Root finding by regula falsi method
 *     find u so that del = delta
 */
    for (j = 0; j < 25; j++) {
        if ((delb - dela) < 0.01) {
            x = 0.5 * (xa + xb);
            u = x / sqrt(dq *dq + x * x);
            usq = u * u;
            break;
        }
        x = xa + (delta - dela) * (xb - xa) / (delb - dela);
        u = x / sqrt(dq *dq + x * x);
        usq = u * u;
        del = x;
        for (i = 0; i < iq; i++)
            del += thk[i] * u / sqrt(vsq[iq] / vsq[i] - usq);
        if (fabs(del - delta) < 0.01)
            break;
        if ((del - delta) < 0.) {
            xa = x;
            dela = del;
        }
        else {
            xb = x;
            delb = del;
        }
    }
/*
 *  travel time and takeoff angle of DirectPhase ray
 */
    t = sqrt(dq * dq + x * x) / v[iq];
    for (i = 0; i < iq; i++)
        t += thk[i] * v[iq] / (vsq[i] * sqrt(vsq[iq] / vsq[i] - usq));
    t -= u * (del - delta) / v[iq];
    *tdir = t;
    u = PI - asin(u);
    return u;
}

/*
 *  Title:
 *     RefractedPhase
 *  Synopsis:
 *	   Computes head wave travel times from the Conrad and Moho.
 *     There may not be a RefractedPhaseed ray, either because all layers
 *     below the event layer are low velocity layers or because for
 *     all layers below the event layer which are not low velocity
 *     layers the critical distance exceeds delta.
 *  Input Arguments:
 *     n     - number of layers
 *     v     - velocities
 *     vsq   - velocity squares
 *     thk   - layer thicknesses
 *     iq    - event layer
 *     dq    - depth of event in event layer
 *     delta - epicentral distance in km
 *  Output Arguments:
 *     tref - head wave travel times (999999. if not exists)
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     GenerateLocalTT
 *  Calls:
 *     CriticalDistanceTTIntercept
 */
static int RefractedPhase(int n, double *v, double *vsq, double *thk, int iq,
        double dq, double delta, double *tref)
{
    int noref = 0, m;
    double did[MAXLAY], tid[MAXLAY];
    double didq, tinq, tmin, sqt;
    CriticalDistanceTTIntercept(n, v, vsq, thk, iq, tid, did);
    tmin = 999999.;
    for (m = iq + 1; m < n; m++) {
        tref[m] = 999999.;
        if (tid[m] < 999999.) {
            sqt = sqrt(vsq[m] - vsq[iq]);
            tinq = tid[m] - dq * sqt / (v[m] * v[iq]);
            didq = did[m] - dq * v[iq] / sqt;
            tref[m] = tinq + delta / v[m];
            if (didq > delta)
                tref[m] = 999999.;
            if (tref[m] < tmin)
                tmin = tref[m];
        }
    }
    if (tmin > 999998.)
        noref = 1;
    return noref;
}

/*
 *  Title:
 *     CriticalDistanceTTIntercept
 *  Synopsis:
 *     Compute intercept times and critical distances for refracted rays
 *     Determines the travel time intercept and critical distance for
 *     a seismic ray in a layered earth model originating at the top of
 *     layer iq, refracted in layer m, and terminating at the surface.
 *  Input Arguments:
 *     n     - number of layers
 *     v     - velocities
 *     vsq   - velocity squares
 *     thk   - layer thicknesses
 *     iq    - event layer
 *  Output Arguments:
 *     tid - travel time intercepts (999999. if not exists)
 *     did - critical distances
 *  Called by:
 *     RefractedPhase
 */
static void CriticalDistanceTTIntercept(int n, double *v, double *vsq,
        double *thk, int iq, double *tid, double *did)
{
    int i, m;
    double tid1, tid2, did1, did2, sqt, tim, dim;
    for (m = iq + 1; m < n; m++) {
        tid[m] = 0.;
        did[m] = 0.;
        tid1 = tid2 = did1 = did2 = 0.;
        for (i = 0; i < m; i++) {
            if (vsq[m] <= vsq[i]) {
                tid[m] = 999999.;
                did[m] = 999999.;
            }
            else {
                sqt = sqrt(vsq[m] - vsq[i]);
                tim = thk[i] * sqt / (v[i] * v[m]);
                dim = thk[i] * v[i] / sqt;
                if (i < iq) {
/*
 *                  sum for layers above event layer
 */
                    tid1 += tim;
                    did1 += dim;
                }
                else {
/*
 *                  sum for layers below and including the event layer
 */
                    tid2 += tim;
                    did2 += dim;
                }
            }
        }
        if (tid[m] < 999999.) {
            tid[m] = tid1 + 2. * tid2;
            did[m] = did1 + 2. * did2;
        }
    }
}

/*
 *  Title:
 *     GetVelocityProfileFromRSTT
 *  Desc:
 *     Get RSTT velocity model profile from RSTT model
 *     RSTT velocity profile:
 *        0 - water depth, Vp, Vs
 *        1 - sediment depth, Vp, Vs
 *        2 - sediment depth, Vp, Vs
 *        3 - sediment depth, Vp, Vs
 *        4 - upper crust depth, Vp, Vs
 *        5 - middle crust depth, Vp, Vs
 *        6 - middle crust depth, Vp, Vs
 *        7 - lower crust depth, Vp, Vs
 *        8 - Moho depth, Vp, Vs,
 *        Vp gradient, Vs gradient
 *  Input Arguments:
 *     lat - latitude
 *     lon - longitude
 *  Output Arguments:
 *     LocalVelocityModelp - pointer to VMODEL structure
 *  Return:
 *     0/1 on success/error
 */
static int GetVelocityProfileFromRSTT(double lat, double lon,
                                      VMODEL *LocalVelocityModelp)
{
    double vp[9], vs[9], h[9], t[10], dw, vpg = 0., vsg = 0., d, c[10];
    double rlat, rlon;
    int i, j, k, n, nodeids[10];
/*
 *  get interpolated node
 */
    rlat = DEG_TO_RAD * lat;
    rlon = DEG_TO_RAD * lon;
    if (slbm_shell_getInterpolatedPoint(&rlat, &rlon, nodeids, c, &n,
                                        h, vp, vs, &vpg, &vsg)) {
        fprintf(logfp, "error in getting model from RSTT!\n");
        fprintf(errfp, "error in getting model from RSTT!\n");
        errorcode = 0;
        return 1;
    }
    fprintf(logfp, "RSTT velocity profile at %.3f, %.3f from %d nodes\n",
            lat, lon, n);
    fprintf(logfp, "     i coeff node\n");
    for (i = 0; i < n; i++)
        fprintf(logfp, "    %2d %.3f %d\n", i, c[i], nodeids[i]);
    fprintf(logfp, "        i   DEPTH  VP    VS\n");
    for (i = 1; i < 9; i++)
        t[i-1] = h[i] - h[i-1];
    t[i-1] = 400. - h[i-1];
    for (i = 0; i < 9; i++)
        fprintf(logfp, "%9d %7.3f %5.3f %5.3f\n", i, h[i], vp[i], vs[i]);
    fprintf(logfp, "    Pgrad=%f Sgrad=%f\n", vpg, vsg);

/*
 *  get rid of water layer
 */
    dw = t[0];
    for (i = 0; i < 8; i++) {
        vp[i] = vp[i+1];
        vs[i] = vs[i+1];
        t[i] = t[i+1] + dw;
    }
/*
 *  get rid of zero-thickness layers
 */
    j = 8;
    for (i = 0; i < j; i++) {
        if (t[i] > 0.0001) continue;
        for (k = i; k < j - 1; k++) {
            vp[k] = vp[k+1];
            vs[k] = vs[k+1];
            t[k] = t[k+1];
        }
        j--;
    }
/*
 *  memory allocations
 */
    n = 2 * j;
    LocalVelocityModelp->n = n;
    LocalVelocityModelp->h = (double *)calloc(n, sizeof(double));
    LocalVelocityModelp->vp = (double *)calloc(n, sizeof(double));
    if ((LocalVelocityModelp->vs = (double *)calloc(n, sizeof(double))) == NULL) {
        fprintf(logfp, "GetVelocityProfileFromRSTT: cannot allocate memory!\n");
        fprintf(errfp, "GetVelocityProfileFromRSTT: cannot allocate memory!\n");
        errorcode = 1;
        Free(LocalVelocityModelp->h);
        Free(LocalVelocityModelp->vp);
        return 1;
    }
/*
 *  velocity model:
 *      h [km], Vp [km/s], Vs [km/s]
 */
    k = LocalVelocityModelp->iconr = LocalVelocityModelp->imoho = 0;
    d = 0.;
    for (i = 1; i < j; i++) {
        LocalVelocityModelp->h[k] = d;
        LocalVelocityModelp->vp[k] = vp[i-1];
        LocalVelocityModelp->vs[k] = vs[i-1];
        d += t[i-1];
        if (i == 1) d += dw;
        k++;
        LocalVelocityModelp->h[k] = d;
        LocalVelocityModelp->vp[k] = vp[i-1];
        LocalVelocityModelp->vs[k] = vs[i-1];
        k++;
        if (i == j - 2) {
            LocalVelocityModelp->iconr = k;
            Conrad = d;
        }
    }
    LocalVelocityModelp->h[k] = d;
    LocalVelocityModelp->vp[k] = vp[i-1];
    LocalVelocityModelp->vs[k] = vs[i-1];
    LocalVelocityModelp->imoho = k;
    Moho = d;
    d += t[i-1];
    k++;
    LocalVelocityModelp->h[k] = d;
    LocalVelocityModelp->vp[k] = vp[i] + vpg * t[i-1];
    LocalVelocityModelp->vs[k] = vs[i] + vsg * t[i-1];
    return 0;
}

/*
 *  Title:
 *     ReadLocalVelocityModel
 *  Desc:
 *     Read local velocity model
 *  Input Arguments:
 *     fname - pathname for local velocity model
 *  Output Arguments:
 *     LocalVelocityModelp - pointer to VMODEL structure
 *  Return:
 *     0/1 on success/error
 *  Calls:
 *     Free, SkipComments
 */
static int ReadLocalVelocityModel(char *fname, VMODEL *LocalVelocityModelp)
{
    FILE *fp;
    char buf[LINLEN], layname[10];
    int n = 0, i;
    double x, y, z;
/*
 *  open local velocity model file
 */
    if ((fp = fopen(fname, "r")) == NULL) {
        fprintf(errfp, "ReadLocalVelocityModel: cannot open %s\n", fname);
        fprintf(logfp, "ReadLocalVelocityModel: cannot open %s\n", fname);
        errorcode = 2;
        return 1;
    }
/*
 *  number of layers
 */
    fgets(buf, LINLEN, fp);
    SkipComments(buf, fp);
    if (verbose > 2) fprintf(logfp, "local vmodel %s\n", buf);
    sscanf(buf, "%d", &n);
    if (verbose > 2) fprintf(logfp, "local vmodel n=%d\n", n);
    LocalVelocityModelp->n = n;
/*
 *  memory allocations
 */
    LocalVelocityModelp->h = (double *)calloc(n, sizeof(double));
    LocalVelocityModelp->vp = (double *)calloc(n, sizeof(double));
    if ((LocalVelocityModelp->vs = (double *)calloc(n, sizeof(double))) == NULL) {
        fprintf(logfp, "ReadLocalVelocityModel: cannot allocate memory!\n");
        fprintf(errfp, "ReadLocalVelocityModel: cannot allocate memory!\n");
        errorcode = 1;
        Free(LocalVelocityModelp->h);
        Free(LocalVelocityModelp->vp);
        return 1;
    }
/*
 *  velocity model:
 *      h [km], Vp [km/s], Vs [km/s], discontinuity (x|CONR|MOHO)
 */
    LocalVelocityModelp->iconr = LocalVelocityModelp->imoho = -1;
    Moho = Conrad = -1.;
    for (i = 0; i < n; i++) {
        fscanf(fp, "%lf%lf%lf%s", &z, &x, &y, layname);
        LocalVelocityModelp->h[i] = z;
        LocalVelocityModelp->vp[i] = x;
        LocalVelocityModelp->vs[i] = y;
        if (streq(layname, "CONRAD")) {
            Conrad = z;
            LocalVelocityModelp->iconr = i;
        }
        if (streq(layname, "MOHO")) {
            Moho = z;
            LocalVelocityModelp->imoho = i;
        }
    }
    fclose(fp);
    return 0;
}

/*
 *  Title:
 *     FreeLocalVelocityModel
 *  Desc:
 *     frees memory allocated to VMODEL structure
 *  Input Arguments:
 *     LocalVelocityModelp - pointer to VMODEL structure
 *  Calls:
 *     Free
 */
static void FreeLocalVelocityModel(VMODEL *LocalVelocityModelp)
{
    Free(LocalVelocityModelp->h);
    Free(LocalVelocityModelp->vp);
    Free(LocalVelocityModelp->vs);
}

