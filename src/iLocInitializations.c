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
extern char InputDB[VALLEN];       /* read data from this DB account, if any */
extern char OutputDB[VALLEN];    /* write results to this DB account, if any */

/*
 * Functions:
 *    InitializeEvent
 *    InitialHypocenter
 *    InitialSolution
 */

/*
 *  Title:
 *     InitializeEvent
 *  Synopsis:
 *     Deals with DepthAgency/EpicenterAgency/OTAgency instructions.
 *     If event identified as anthropogenic then fix depth at surface,
 *        unless being fixed explicitly to some depth.
 *  Input Arguments:
 *     ep  - pointer to event info
 *     h[] - array of hypocentre structures
 *     p[] - array of phase structures
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     Locator
 */
int InitializeEvent(EVREC *ep, HYPREC h[])
{
    extern double DefaultDepth;                          /* From config file */
    HYPREC temp;
    int manmade = 0;
    int i;
/*
 *  check for anthropogenic events
 */
    if (verbose) fprintf(logfp, "etype=%s\n", ep->etype);
    if (streq(ep->etype, "  "))
        strcpy(ep->etype, "se");
    if (ep->etype[1] == 'n' ||
        ep->etype[1] == 'x' ||
        ep->etype[1] == 'm' ||
        ep->etype[1] == 'q' ||
        ep->etype[1] == 'r' ||
        ep->etype[1] == 'h' ||
        ep->etype[1] == 's' ||
        ep->etype[1] == 'i')
        manmade = 1;
/*
 *  If event identified as anthropogenic then fix depth at surface,
 *  unless being fixed explicitly to some depth.
 */
    if (manmade) {
        if (!ep->FixedDepth) {
            ep->StartDepth = 0;
            ep->FixedDepth   = 1;
            ep->FixDepthToZero = 1;              /* To stop options 2 and 3. */
            fprintf(logfp, "Fix depth to zero because etype=%s\n", ep->etype);
        }
        else {
            ep->FixDepthToZero = 1;              /* To stop options 2 and 3. */
        }
    }
/*
 *  DepthAgency instruction - set depth to that given by chosen agency.
 */
    if (ep->DepthAgency[0] && ep->StartDepth == NULLVAL) {
        for (i = 0; i < ep->numHypo; i++) {
            if (streq(h[i].agency, ep->DepthAgency)) {
                ep->StartDepth = h[i].depth;
                break;
            }
        }
        if (ep->StartDepth == NULLVAL) {
            fprintf(errfp, "ABORT: %d invalid depth agency %s!\n",
                    ep->evid, ep->DepthAgency);
            fprintf(logfp, "ABORT: %d invalid depth agency %s!\n",
                    ep->evid, ep->DepthAgency);
            return 1;
        }
    }
/*
 *  EpicenterAgency instruction - set lat,lon to that given by agency,
 */
    if (ep->EpicenterAgency[0] && ep->StartLat == NULLVAL) {
        for (i = 0; i < ep->numHypo; i++) {
            if (streq(h[i].agency, ep->EpicenterAgency)) {
                ep->StartLat = h[i].lat;
                ep->StartLon = h[i].lon;
                break;
            }
        }
        if (ep->StartLat == NULLVAL) {
            fprintf(errfp, "ABORT: %d invalid location agency %s!\n",
                    ep->evid, ep->EpicenterAgency);
            fprintf(logfp, "ABORT: %d invalid location agency %s!\n",
                    ep->evid, ep->EpicenterAgency);
            return 1;
        }
    }
/*
 *  OTAgency instruction - set time to that given by agency.
 */
    if (ep->OTAgency[0] && ep->StartOT == NULLVAL) {
        for (i = 0; i < ep->numHypo; i++) {
            if (streq(h[i].agency, ep->OTAgency)) {
                ep->StartOT = h[i].time;
                break;
            }
        }
        if (ep->StartOT == NULLVAL) {
            fprintf(errfp, "ABORT: %d invalid time agency %s!\n",
                    ep->evid, ep->OTAgency);
            fprintf(logfp, "ABORT: %d invalid time agency %s!\n",
                    ep->evid, ep->OTAgency);
            return 1;
        }
    }
/*
 *  HypocenterAgency instruction - set hypocenter to that given by agency.
 */
    if (ep->HypocenterAgency[0]) {
        for (i = 0; i < ep->numHypo; i++) {
            if (streq(h[i].agency, ep->HypocenterAgency)) {
                ep->StartOT    = h[i].time;
                ep->StartLat   = h[i].lat;
                ep->StartLon   = h[i].lon;
                ep->StartDepth = h[i].depth;
                swap(h[i], h[0]);
                break;
            }
        }
        if (ep->StartOT == NULLVAL) {
            fprintf(errfp, "ABORT: %d invalid hypo agency %s!\n",
                    ep->evid, ep->HypocenterAgency);
            fprintf(logfp, "ABORT: %d invalid hypo agency %s!\n",
                    ep->evid, ep->HypocenterAgency);
            return 1;
        }
        ep->FixedHypocenter = 1;
    }
/*
 *  Set FixedHypocenter if everything is fixed separately
 *      known issue: prime_hypid is undefined!
 */
    if (ep->FixedEpicenter && ep->FixedDepth && ep->FixedOT)
        ep->FixedHypocenter = 1;
    if (ep->StartDepth != NULLVAL && ep->StartDepth < 0.)
        ep->StartDepth = DefaultDepth;
    return 0;
}

/*
 *  Title:
 *     InitialHypocenter
 *  Synopsis:
 *     Sets starting hypocentre to median of all hypocentre parameters
 *        if preferred origin is not IASPEI
 *  Input Arguments:
 *     ep  - pointer to event info
 *     h[] - array of hypocentre structures
 *  Output Arguments:
 *     starthyp - pointer to starting hypocentre
 *  Return:
 *     median of reported depths
 *  Called by:
 *     Locator
 */
int InitialHypocenter(EVREC *ep, HYPREC h[], HYPREC *starthyp)
{
    extern double DefaultDepth;
    extern char DoNotUseAgencies[MAXBUF][AGLEN];
    extern int DoNotUseAgenciesNum;
    double *x = (double *)NULL;
    double medtim = 0., medlat = 0., medlon = 0., meddep = 0., z = 0., y;
    int i, j, n = 0, m;
    starthyp->time = h[0].time;
    starthyp->lat  = h[0].lat;
    starthyp->lon  = h[0].lon;
    if (h[0].depth == NULLVAL || h[0].depth < 0.)
        starthyp->depth = DefaultDepth;
    else
        starthyp->depth = h[0].depth;
    z = starthyp->depth;
/*
 *  single preferred origin
 */
    if (ep->numHypo == 1) return 0;
/*
 *  flag hypocenters that may not be used in setting initial hypocentre
 */
    for (i = 0; i < ep->numHypo; i++) {
        h[i].ignorehypo = 0;
        for (j = 0; j < DoNotUseAgenciesNum; j++) {
            if (streq(h[i].agency, DoNotUseAgencies[j])) {
                h[i].ignorehypo = 1;
                break;
            }
        }
        if (!h[i].ignorehypo) n++;
    }
    m = n / 2;
    if ((x = (double *)calloc(n, sizeof(double))) == NULL) {
        fprintf(logfp, "InitialHypocenter: cannot allocate memory!\n");
        fprintf(errfp, "InitialHypocenter: cannot allocate memory!\n");
        errorcode = 1;
        return 1;
    }
/*
 *  median origin time
 */
    for (j = 0, i = 0; i < ep->numHypo; i++)
        if (!h[i].ignorehypo) x[j++] = h[i].time;
    if (n == 2)
        medtim = (x[0] + x[1]) / 2.;
    else {
        qsort(x, n, sizeof(double), CompareDouble);
        medtim = x[m];
    }
/*
 *  median latitude
 */
    for (j = 0, i = 0; i < ep->numHypo; i++) {
        if (!h[i].ignorehypo) x[j++] = h[i].lat;
    }
    if (n == 2)
        medlat = (x[0] + x[1]) / 2.;
    else {
        qsort(x, n, sizeof(double), CompareDouble);
        medlat = x[m];
    }
/*
 *  median longitude
 */
    for (j = 0, i = 0; i < ep->numHypo; i++) {
        if (!h[i].ignorehypo) x[j++] = h[i].lon;
    }
    if (n == 2) {
        y = fabs(x[0]) + fabs(x[1]);
        if (y > 180. && x[0] * x[1] < 0.) medlon = y / 2.;
        else                              medlon = (x[0] + x[1]) / 2.;
    }
    else {
        qsort(x, n, sizeof(double), CompareDouble);
        medlon = x[m];
    }
/*
 *  median depth
 */
    for (j = 0, i = 0; i < ep->numHypo; i++) {
        if (!h[i].ignorehypo && h[i].depth != NULLVAL && h[i].depth >= 0.)
            x[j++] = h[i].depth;
    }
    if (j == 0)      meddep = DefaultDepth;
    else if (j == 1) meddep = x[0];
    else if (j == 2) meddep = (x[0] + x[1]) / 2.;
    else {
        qsort(x, j, sizeof(double), CompareDouble);
        j /= 2;
        meddep = x[j];
    }
    Free(x);
    starthyp->time = medtim;
    starthyp->lat = medlat;
    starthyp->lon = medlon;
    starthyp->depth = meddep;
/*
 *  trusted preferred origins: set initial epicentre/hypocentre to that
 */
    if (streq(h[0].agency, "IASPEI")) {
        if (!ep->FixedOT)
            starthyp->time = h[0].time;
        if (!ep->FixedEpicenter) {
            starthyp->lat = h[0].lat;
            starthyp->lon = h[0].lon;
        }
        if (!ep->FixedDepth)
            starthyp->depth = z;
    }
    return 0;
}


/*
 *  Title:
 *     InitialSolution
 *  Synopsis:
 *     Initializes solution structure with initial hypocentre guess.
 *  Input Arguments:
 *     sp  - pointer to solution structure to be initialised.
 *     ep  - pointer to event info
 *     hp  - pointer to starting hypocentre
 *  Return:
 *     0/1 on success/error.
 *  Called by:
 *     Locator, ResidualsForFixedHypocenter
 */
int InitialSolution(SOLREC *sp, EVREC *ep, HYPREC *hp)
{
    extern double DefaultDepth;                          /* From config file */
    int i, j;
/*
 *  Mark as not converged yet.
 */
    sp->converged = 0;
    sp->diverging = 0;
/*
 *  Check if there are sufficient number of observations
 */
    sp->numPhase = ep->numPhase;
    sp->nreading = ep->numReading;
    if (sp->numPhase < sp->numUnknowns) {
        fprintf(errfp, "InitialSolution: insufficient number of observations!\n");
        fprintf(errfp, "          unknowns=%d numPhase=%d\n",
                sp->numUnknowns, sp->numPhase);
        fprintf(logfp, "InitialSolution: insufficient number of observations!\n");
        fprintf(logfp, "          unknowns=%d numPhase=%d\n",
                sp->numUnknowns, sp->numPhase);
        errorcode = 5;
        return 1;
    }
/*
 *  Set starting point for location to the median hypocentre
 *  or to that set by the user.
 */
    sp->lat   = (ep->StartLat   != NULLVAL) ? ep->StartLat   : hp->lat;
    sp->lon   = (ep->StartLon   != NULLVAL) ? ep->StartLon   : hp->lon;
    sp->depth = (ep->StartDepth != NULLVAL) ? ep->StartDepth : hp->depth;
    sp->time  = (ep->StartOT    != NULLVAL) ? ep->StartOT    : hp->time;
/*
 *  set epifix and timfix flags
 */
    sp->epifix = ep->FixedEpicenter;
    sp->timfix = ep->FixedOT;
    sp->depfix = ep->FixedDepth;
    sp->hypofix = ep->FixedHypocenter;
/*
 *  Initialize errors and sdobs
 */
    for (i = 0; i < 4; i++) {
        sp->error[i] = NULLVAL;
        for (j = 0; j < 4; j++) sp->covar[i][j] = NULLVAL;
    }
    sp->sdobs = NULLVAL;
    sp->mindist = sp->maxdist = NULLVAL;
    sp->smajax = sp->sminax = sp->strike = NULLVAL;
/*
 *  Initialize the rest
 */
    if (strcmp(InputDB, OutputDB) && ep->OutputDBPreferredOrid) {
        sp->hypid = ep->OutputDBPreferredOrid;
        strcpy(sp->OriginID, ep->OutputDBprefOriginID);
    }
    else {
        sp->hypid = ep->PreferredOrid;
        strcpy(sp->OriginID, ep->prefOriginID);
    }
    sp->ndef = 0;
    sp->nass = 0;
    sp->ndefsta = 0;
    sp->prank = 0;
    sp->depdp = NULLVAL;
    sp->depdp_error = NULLVAL;
    sp->ndp = 0;
    sp->urms = 0.;
    sp->wrms = 0.;
    sp->nnetmag = 0;
    sp->nstamag = 0;
    for (i = 0; i < MAXMAG; i++) {
        sp->mag[i].magnitude = 0.;
        sp->mag[i].uncertainty = 0.;
        sp->mag[i].numAssociatedMagnitude = 0;
        sp->mag[i].numDefiningMagnitude = 0;
        sp->mag[i].magid = 0;
        sp->mag[i].mtypeid = 0;
        strcpy(sp->mag[i].agency, "");
        strcpy(sp->mag[i].magtype, "");
    }
    return 0;
}

