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
 *    LocationQuality
 *    GetdUGapSgap
 */

/*
 *  Title:
 *     LocationQuality
 *  Synopsis:
 *     Calculates network geometry based location quality metrics
 *     gap, sgap and dU for local, near-regional, teleseismic
 *     distance ranges and the entire network.
 *         Local network:  0 - 150 km
 *         Near regional:  3 - 10 degrees
 *         Teleseismic:   28 - 180 degrees
 *         Entire network: 0 - 180 degrees
 *     Only defining stations are considered.
 *     dU is defined in:
 *        Bondár, I. and K. McLaughlin, 2009,
 *        A new ground truth data set for seismic studies,
 *        Seism. Res. Let., 80, 465-472.
 *     sgap is defined in:
 *        Bondár, I., S.C. Myers, E.R. Engdahl and E.A. Bergman, 2004,
 *        Epicenter accuracy based on seismic network criteria,
 *        Geophys. J. Int., 156, 483-496, doi: 10.1111/j.1365-246X.2004.02070.x.
 *  Input Arguments:
 *     hypid   - hypocentre id
 *     numPhase - number of associated phases
 *     p[]     - array of phase structures
 *  Output Arguments:
 *     hq      - pointer to hypocentre quality structure
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     Locator
 *  Calls:
 *     GetdUGapSgap
 */
int LocationQuality(int numPhase, PHAREC p[], HYPQUAL *hq)
{
    double *esaz = (double *)NULL;
    double gap = 0., sgap = 0., du = 0., score = 0.;
    double delta = 0., d10 = 0., mind = 0., maxd = 0., d = 0.;
    char prevsta[STALEN];
    int i, ndef = 0, nsta = 0, numStaWithin10km = 0;
    if ((esaz = (double *)calloc(numPhase + 2, sizeof(double))) == NULL) {
        fprintf(logfp, "LocationQuality: cannot allocate memory\n");
        fprintf(errfp, "LocationQuality: cannot allocate memory\n");
        errorcode = 1;
        return 1;
    }
/*
 *  local network (0-150 km)
 */
    delta = 150. * RAD_TO_DEG / EARTH_RADIUS;
    d10 = 10. * RAD_TO_DEG / EARTH_RADIUS;
    strcpy(prevsta, "");
    mind = 180.;
    maxd = 0.;
    for (ndef = nsta = 0, i = 0; i < numPhase; i++) {
        if (!p[i].timedef && !p[i].azimdef && !p[i].slowdef) continue;
        if (p[i].delta > delta) continue;
        if (p[i].timedef) ndef++;
        if (p[i].azimdef) ndef++;
        if (p[i].slowdef) ndef++;
        if (streq(p[i].prista, prevsta)) continue;
        esaz[nsta++] = p[i].esaz;
        strcpy(prevsta, p[i].prista);
        if (p[i].delta > maxd) maxd = p[i].delta;
        if (p[i].delta < mind) mind = p[i].delta;
        if (p[i].delta <= d10) numStaWithin10km++;
    }
    hq->LocalNetwork.ndefsta = nsta;
    hq->LocalNetwork.ndef = ndef;
    du = GetdUGapSgap(nsta, esaz, &gap, &sgap);
    hq->LocalNetwork.du = du;
    hq->LocalNetwork.gap = gap;
    hq->LocalNetwork.sgap = sgap;
    hq->LocalNetwork.mindist = mind;
    hq->LocalNetwork.maxdist = maxd;
    hq->numStaWithin10km = numStaWithin10km;
    hq->GT5candidate = (du > 0.35 || numStaWithin10km < 1 || sgap > 160.) ? 0 : 1;
    d = (du < 0.0001) ? 0.0001 : du;
    score += 2. * (1. / d + nsta / 7.5 + (360. - sgap) / 60.);
    if (verbose > 1) {
        fprintf(logfp, "    local network:         nsta=%3d ndef=%3d",
                nsta, ndef);
        fprintf(logfp, " gap=%5.1f sgap=%5.1f dU=%5.3f", gap, sgap, du);
        fprintf(logfp, " numStaWithin10km=%d GT5cand=%d\n",
                numStaWithin10km, hq->GT5candidate);
    }
/*
 *  near-regional network (3-10 degrees)
 */
    strcpy(prevsta, "");
    mind = 180.;
    maxd = 0.;
    for (ndef = nsta = 0, i = 0; i < numPhase; i++) {
        if (!p[i].timedef && !p[i].azimdef && !p[i].slowdef) continue;
        if (p[i].delta < 3. || p[i].delta > 10.) continue;
        if (p[i].timedef) ndef++;
        if (p[i].azimdef) ndef++;
        if (p[i].slowdef) ndef++;
        if (streq(p[i].prista, prevsta)) continue;
        esaz[nsta++] = p[i].esaz;
        strcpy(prevsta, p[i].prista);
        if (p[i].delta > maxd) maxd = p[i].delta;
        if (p[i].delta < mind) mind = p[i].delta;
    }
    hq->NearRegionalNetwork.ndefsta = nsta;
    hq->NearRegionalNetwork.ndef = ndef;
    du = GetdUGapSgap(nsta, esaz, &gap, &sgap);
    hq->NearRegionalNetwork.du = du;
    hq->NearRegionalNetwork.gap = gap;
    hq->NearRegionalNetwork.sgap = sgap;
    hq->NearRegionalNetwork.mindist = mind;
    hq->NearRegionalNetwork.maxdist = maxd;
    d = (du < 0.0001) ? 0.0001 : du;
    score += 1.2 * (1. / d + nsta / 7.5 + (360. - sgap) / 60.);
    if (verbose > 1) {
        fprintf(logfp, "    near-regional network: nsta=%3d ndef=%3d",
                nsta, ndef);
        fprintf(logfp, " gap=%5.1f sgap=%5.1f dU=%5.3f\n", gap, sgap, du);
    }
/*
 *  teleseismic network (28-180 degrees)
 */
    strcpy(prevsta, "");
    mind = 180.;
    maxd = 0.;
    for (ndef = nsta = 0, i = 0; i < numPhase; i++) {
        if (!p[i].timedef && !p[i].azimdef && !p[i].slowdef) continue;
        if (p[i].delta < 28.) continue;
        if (p[i].timedef) ndef++;
        if (p[i].azimdef) ndef++;
        if (p[i].slowdef) ndef++;
        if (streq(p[i].prista, prevsta)) continue;
        esaz[nsta++] = p[i].esaz;
        strcpy(prevsta, p[i].prista);
        if (p[i].delta > maxd) maxd = p[i].delta;
        if (p[i].delta < mind) mind = p[i].delta;
    }
    hq->TeleseismicNetwork.ndefsta = nsta;
    hq->TeleseismicNetwork.ndef = ndef;
    du = GetdUGapSgap(nsta, esaz, &gap, &sgap);
    hq->TeleseismicNetwork.du = du;
    hq->TeleseismicNetwork.gap = gap;
    hq->TeleseismicNetwork.sgap = sgap;
    hq->TeleseismicNetwork.mindist = mind;
    hq->TeleseismicNetwork.maxdist = maxd;
    d = (du < 0.0001) ? 0.0001 : du;
    score += 1.5 * (1. / d + nsta / 7.5 + (360. - sgap) / 60.);
    if (verbose > 1) {
        fprintf(logfp, "    teleseismic network:   nsta=%3d ndef=%3d",
                nsta, ndef);
        fprintf(logfp, " gap=%5.1f sgap=%5.1f dU=%5.3f\n", gap, sgap, du);
    }
/*
 *  entire network
 */
    strcpy(prevsta, "");
    mind = 180.;
    maxd = 0.;
    for (ndef = nsta = 0, i = 0; i < numPhase; i++) {
        if (!p[i].timedef && !p[i].azimdef && !p[i].slowdef) continue;
        if (p[i].timedef) ndef++;
        if (p[i].azimdef) ndef++;
        if (p[i].slowdef) ndef++;
        if (streq(p[i].prista, prevsta)) continue;
        esaz[nsta++] = p[i].esaz;
        strcpy(prevsta, p[i].prista);
        if (p[i].delta > maxd) maxd = p[i].delta;
        if (p[i].delta < mind) mind = p[i].delta;
    }
    hq->FullNetwork.ndefsta = nsta;
    hq->FullNetwork.ndef = ndef;
    du = GetdUGapSgap(nsta, esaz, &gap, &sgap);
    hq->FullNetwork.du = du;
    hq->FullNetwork.gap = gap;
    hq->FullNetwork.sgap = sgap;
    hq->FullNetwork.mindist = mind;
    hq->FullNetwork.maxdist = maxd;
    d = (du < 0.0001) ? 0.0001 : du;
    score += (1. / d + nsta / 7.5 + (360. - sgap) / 60.);
    if (verbose > 1) {
        fprintf(logfp, "    entire network:        nsta=%3d ndef=%3d",
                nsta, ndef);
        fprintf(logfp, " gap=%5.1f sgap=%5.1f dU=%5.3f\n", gap, sgap, du);
    }
    if (score < 0.1) score = 4;
    hq->score = score;
    Free(esaz);
    return 0;
}

/*
 *  Title:
 *     GetdUGapSgap
 *  Synopsis:
 *     Calculates gap, sgap and dU.
 *
 *           4 * sum|esaz[i] - (360 *i / nsta + b)|
 *     dU = ---------------------------------------
 *                      360 * nsta
 *
 *     b = avg(esaz) - avg(360i/N)  where esaz is sorted
 *
 *  Input Arguments:
 *     nsta - number of defining stations
 *     esaz - array of event-to-station azimuths
 *  Output Arguments:
 *     gap  - largest azimuthal gap
 *     sgap - largest secondary azimuthal gap
 *  Return:
 *     dU   - network quality metric
 *  Called by:
 *     LocationQuality
 */
double GetdUGapSgap(int nsta, double *esaz, double *gap, double *sgap)
{
    int i;
    double du = 1., bb = 0., uesaz = 0., w = 0., s1 = 0., s2 = 0.;
    *gap = 360.;
    *sgap = 360.;
    if (nsta < 2) return du;
/*
 *  sort esaz
 */
    qsort(esaz, nsta, sizeof(double), CompareDouble);
/*
 *  du: mean absolute deviation from best fitting uniform network
 */
    for (i = 0; i < nsta; i++) {
        uesaz = 360. * (double)i / (double)nsta;
        s1 += esaz[i];
        s2 += uesaz;
    }
    bb = (s1 - s2) / (double)nsta;
    for (w = 0., i = 0; i < nsta; i++) {
        uesaz = 360. * (double)i / (double)nsta;
        w += fabs(esaz[i] - uesaz - bb);
    }
    du = 4. * w / (360. * (double)nsta);
/*
 *  gap
 */
    esaz[nsta] = esaz[0] + 360.;
    for (w = 0., i = 0; i < nsta; i++)
        w = max(w, esaz[i+1] - esaz[i]);
    if (w > 360.) w = 360.;
    *gap = w;
/*
 *  sgap
 */
    esaz[nsta+1] = esaz[1] + 360.;
    for (w = 0., i = 0; i < nsta; i++)
        w = max(w, esaz[i+2] - esaz[i]);
    if (w > 360.) w = 360.;
    *sgap = w;
    return du;
}


/*  EOF  */
