/*
 * Copyright (c) 2018-2026, Istvan Bondar,
 * Written by Istvan Bondar, Seismic Location Services
 * istvan.bondar@slsiloc.eu
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
 *    GetNdefSP150
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
 *     hypid    - hypocentre id
 *     numPhase - number of associated phases
 *     p[]      - array of phase structures
 *  Output Arguments:
 *     hq       - pointer to hypocentre quality structure
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
    double gap = 0., sgap = 0., du = 0., cpq = 0., score = 0.;
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
    du = GetdUGapSgap(nsta, esaz, &gap, &sgap, &cpq);
    hq->LocalNetwork.du = du;
    hq->LocalNetwork.cpq = cpq;
    hq->LocalNetwork.gap = gap;
    hq->LocalNetwork.sgap = sgap;
    hq->LocalNetwork.mindist = mind;
    hq->LocalNetwork.maxdist = maxd;
    hq->numStaWithin10km = numStaWithin10km;
    hq->GT5candidatedU = (du > 0.35 || numStaWithin10km < 1 || sgap > 160.) ? 0 : 1;
    d = (du < 0.0001) ? 0.0001 : du;
    hq->GT5candidateCPQ = (cpq < 0.4 || sgap > 210. ||
                           (numStaWithin10km < 1 && hq->nspdef150 < 5)) ? 0 : 1;
    score += 2. * (1. / d + nsta / 7.5 + (360. - sgap) / 60.);
    if (verbose > 1) {
        fprintf(logfp, "    local network:         nsta=%3d ndef=%3d",
                nsta, ndef);
        fprintf(logfp, " gap=%5.1f sgap=%5.1f dU=%5.3f CPQ=%5.3f\n",
                gap, sgap, du, cpq);
        fprintf(logfp, " gap=%5.1f sgap=%5.1f dU=%5.3f", gap, sgap, du);
        fprintf(logfp, " numStaWithin10km=%d GT5candDU=%d  GT5candCPQ=%d",
                numStaWithin10km, hq->GT5candidatedU, hq->GT5candidateCPQ);
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
    du = GetdUGapSgap(nsta, esaz, &gap, &sgap, &cpq);
    hq->FullNetwork.du = du;
    hq->FullNetwork.cpq = cpq;
    hq->FullNetwork.gap = gap;
    hq->FullNetwork.sgap = sgap;
    hq->FullNetwork.mindist = mind;
    hq->FullNetwork.maxdist = maxd;
    d = (du < 0.0001) ? 0.0001 : du;
    score += (1. / d + nsta / 7.5 + (360. - sgap) / 60.);
    if (verbose > 1) {
        fprintf(logfp, "    entire network:        nsta=%3d ndef=%3d",
                nsta, ndef);
        fprintf(logfp, " gap=%5.1f sgap=%5.1f dU=%5.3f CPQ=%5.3f\n",
                gap, sgap, du, cpq);
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
 *           4 * sum(abs(esaz[i] - (360 * i / nsta + b))
 *     dU = ---------------------------------------------
 *                          360 * nsta
 *
 *     b = avg(esaz) - avg(360i/N)  where esaz is sorted
 *
 *
 *           abs(sum(x[i] * y[i+1]) - sum(x[i+1] * y[i]))
 *     cpq = --------------------------------------------
 *                             2 * pi
 *
 *
 *  Input Arguments:
 *     nsta - number of defining stations
 *     esaz - array of event-to-station azimuths
 *  Output Arguments:
 *     gap  - largest azimuthal gap
 *     sgap - largest secondary azimuthal gap
 *     cpq  - cyclic polygon quotient
 *  Return:
 *     dU   - network quality metric
 *  Called by:
 *     LocationQuality
 */
double GetdUGapSgap(int nsta, double *esaz, double *gap, double *sgap, double *cpq)
{
    int i;
    double du = 1., bb = 0., uesaz = 0., v = 0., w = 0., s1 = 0., s2 = 0.;
    *gap = 360.;
    *sgap = 360.;
    if (nsta < 2) return du;
/*
 *  sort esaz
 */
    qsort(esaz, nsta, sizeof(double), CompareDouble);
/*
 *  du: mean absolute deviation from best fitting uniform network
 *  Bondar and McLaughlin, 2009
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
 *  Cyclic Polygon Quotient (Gallacher et al, 2025)
 *  Calculate the area of the polygon with the surveyor's formula
 *  Bart Braden, The College Mathematics Journal, 1986
 */
    s1 = 0.;
    s2 = 0.;
    for (i = 0; i < nsta; i++) {
        v = esaz[i] * DEG_TO_RAD;
        w = esaz[i+1] * DEG_TO_RAD;
        s1 += sin(v) * cos(w);
        s2 += sin(w) * cos(v);
    }
    *cpq = fabs(s1 -s2) / (2. * PI);
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

/*
 *  Title:
 *     GetNdefSP150
 *  Synopsis:
 *     Calculates number of defining S-P pairs within 150 km.
 *
 *  Input Arguments:
 *     sp        - pointer to current solution
 *     rdindx    - array of reading structures
 *     p[]       - array of phase structures
 *  Return:
 *     nspdef150 - number of defining S-P pairs within 150 km
 *  Called by:
 *     Locator
 */
int GetNdefSP150(SOLREC *sp, READING *rdindx, PHAREC p[], int isverbose)
{
    int i, k, m, np, numSPdef150 = 0;
    double delta = 150. * RAD_TO_DEG / EARTH_RADIUS;
/*
 *  loop over readings
 */
    for (i = 0; i < sp->nreading; i++) {
        m = rdindx[i].start;
        np = rdindx[i].start + rdindx[i].npha;
/*
 *      multiple phases in the reading
 */
        for (k = m + 1; k < np; k++) {
/*
 *          both the first-arriving and the later phase must be defining
 */
            if (!p[m].timedef || !p[k].timedef)
                continue;
/*
 *          number of defining S-P pairs within 150km for CPQ GT5 test
 */
            if (p[m].firstP && p[k].firstS && p[m].delta <= delta &&
                (p[m].duplicate * p[k].duplicate) == 0)
                numSPdef150++;
        }
    }
    if (isverbose)
        fprintf(logfp, "        %d defining S-P pairs within 150 km\n", numSPdef150);
    return numSPdef150;
}


/*  EOF  */
