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
extern int numAgencies;
extern char agencies[MAXBUF][AGLEN];
extern double MaxHypocenterDepth;

/*
 * Functions:
 *    DepthResolution
 *    DepthPhaseCheck
 *    DepthPhaseStack
 */

/*
 * Local functions
 *    PhaseTTh
 *    Stacker
 *    StationTrace
 */
static int PhaseTTh(double delta, double esaz, SOLREC *sp,
                      TT_TABLE *tt_tablep, TT_TABLE *Pfirst,
                      short int **topo, int ips, int ispwP, double *pt);
static void Stacker(int n, double moveout, double deltim,
                    double *pt, double *tz, double *depths,
                    double *pp, int *trace, int *stack);
static void StationTrace(int n, double moveout, double deltim, double *pp,
                      double *h, int *trace);
/*
 *  Title:
 *     DepthResolution
 *  Synopsis:
 *     Decides if there is sufficient depth resolution for a free depth solution
 *     We have depth resolution if
 *        has depth resolution from depth phases (
 *        OR
 *        nlocal >= MinLocalStations
 *        OR
 *        nsdef >= MinSPpairs
 *        OR
 *        (ncoredef >= MinCorePhases AND nagent >= MindDepthPhaseAgencies))
 *     For any of these cases a defining first-arriving P in the reading
 *        is required.
 *  Input Arguments:
 *     sp        - pointer to current solution
 *     rdindx    - array of reading structures
 *     p[]       - array of phase structures
 *     isverbose - verbose this time?
 *  Return:
 *     1 if we have depth resolution, 0 otherwise
 *  Called by:
 *     Locator
 */
int DepthResolution(SOLREC *sp, READING *rdindx, PHAREC p[], int isverbose)
{
    extern double MaxLocalDistDeg;              /* max local distance (degs) */
    extern int MinLocalStations;      /* min stations within MaxLocalDistDeg */
    extern double MaxSPDistDeg;                   /* max S-P distance (degs) */
    extern int MinSPpairs;                  /* min number of S-P phase pairs */
    extern int MinCorePhases;   /* min number of core reflections ([PS]c[PS] */
    extern int MindDepthPhaseAgencies;/* min agencies reporting depth phases */
    int ageindex[MAXBUF];
    int i, k, m, np, nsdef = 0, ncoredef = 0, nlocal = 0;
    int hasDepthResolution = 0, nagent = 0;
    for (i = 0; i < numAgencies; i++) ageindex[i] = 0;
/*
 *  loop over readings
 */
    for (i = 0; i < sp->nreading; i++) {
        m = rdindx[i].start;
        np = rdindx[i].start + rdindx[i].npha;
/*
 *      local stations
 */
        if (!p[m].duplicate && p[m].timedef &&
            p[m].firstP && p[m].delta <= MaxLocalDistDeg)
            nlocal++;
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
 *          number of defining core reflections
 */
            if (!p[k].duplicate &&
                (streq(p[k].phase, "PcP") || streq(p[k].phase, "ScS")))
                ncoredef++;
/*
 *          number of defining S-P pairs within MaxSPDistDeg
 */
            if (p[m].firstP && p[k].firstS && p[m].delta <= MaxSPDistDeg &&
                (p[m].duplicate * p[k].duplicate) == 0)
                nsdef++;
        }
    }
/*
 *  agencies reporting core reflections
 */
    if (ncoredef) {
        for (i = 0; i < sp->numPhase; i++) {
            if (p[i].duplicate ||
                !(streq(p[i].phase, "PcP") || streq(p[i].phase, "ScS")))
                continue;
            for (k = 0; k < numAgencies; k++)
                if (streq(p[i].agency, agencies[k]))
                    ageindex[k]++;
        }
        if (isverbose && verbose > 1)
            fprintf(logfp, "    agencies contributing PcP/ScS\n");
        for (nagent = 0, i = 0; i < numAgencies; i++) {
            if (ageindex[i]) {
                nagent++;
                if (isverbose && verbose > 1)
                    fprintf(logfp, "      %3d %-17s %4d PcP/ScS\n",
                            nagent, agencies[i], ageindex[i]);
            }
        }
    }
/*
 *  we have depth resolution if
 *      (has_depdpres || nlocal >= MinLocalStations || nsdef >= MinSPpairs ||
 *       (ncoredef >= MinCorePhases && nagent >= MindDepthPhaseAgencies)
 */
    if (nlocal >= MinLocalStations || nsdef >= MinSPpairs ||
        (ncoredef >= MinCorePhases && nagent >= MindDepthPhaseAgencies))
        hasDepthResolution = 1;
    if (isverbose) {
        fprintf(logfp, "    Depth resolution: %d\n", hasDepthResolution);
        fprintf(logfp, "        %d stations within %.2f degrees\n",
                nlocal, MaxLocalDistDeg);
        fprintf(logfp, "        %d defining S-P pairs within %.2f degrees\n",
                nsdef, MaxSPDistDeg);
        fprintf(logfp, "        %d agencies reported %d defining PcP/ScS phases\n",
                nagent, ncoredef);
    }
    return hasDepthResolution;
}


/*
 *  Title:
 *     DepthPhaseCheck
 *  Synopsis:
 *     Decides whether depth phases provide depth resolution.
 *     flags first-arriving defining P for a reading
 *     makes note of P - depth phase pairs
 *     makes orphan depth phases non-defining
 *  Input Arguments:
 *     sp        - pointer to current solution
 *     rdindx    - array of reading structures
 *     p[]       - array of phase structures
 *     isverbose - verbose this time?
 *  Return:
 *     1 if we have depth-phase depth resolution, 0 otherwise
 *  Called by:
 *     Locator, LocateEvent, GetResiduals
 */
int DepthPhaseCheck(SOLREC *sp, READING *rdindx, PHAREC p[], int isverbose)
{
    extern int MinDepthPhases;  /* min # of depth phases for depth-phase depth */
    extern int MindDepthPhaseAgencies;      /* min # of agencies reporting depth phases */
    int i, j, k, m, np, ndepassoc = 0, nagent = 0, has_depdpres = 0;
    int ageindex[MAXBUF];
    for (i = 0; i < numAgencies; i++) ageindex[i] = 0;
    for (i = 0; i < sp->numPhase; i++) {
        p[i].firstP = p[i].hasdepthphase = 0;
        p[i].pPindex = p[i].pwPindex = 0;
        p[i].pSindex = p[i].sPindex = p[i].sSindex = 0;
    }
/*
 *  loop over readings
 */
    for (i = 0; i < sp->nreading; i++) {
        m = rdindx[i].start;
        np = rdindx[i].start + rdindx[i].npha;
        if (p[m].timedef && p[m].phase[0] == 'P')
            p[m].firstP = 1;
/*
 *      only one phase in the reading
 */
        if (rdindx[i].npha == 1) {
/*
 *          do not allow depth phases without a first arriving P
 */
            if (!p[m].firstP && islower(p[m].phase[0]))
                p[m].timedef = 0;
        }
/*
 *      multiple phases in the reading
 */
        for (k = m + 1; k < np; k++) {
            if (!p[k].timedef || isupper(p[k].phase[0]))
                continue;
/*
 *          do not allow depth phases without a first arriving defining P
 */
            if (!p[m].firstP && islower(p[k].phase[0])) {
                p[k].timedef = 0;
                continue;
            }
/*
 *          pP*
 */
            if (strncmp(p[k].phase, "pP", 2) == 0) {
                p[m].hasdepthphase++;
                p[m].pPindex = k;
                if (!p[k].duplicate) ndepassoc++;
            }
/*
 *          pwP*
 */
            else if (strncmp(p[k].phase, "pw", 2) == 0) {
                p[m].hasdepthphase++;
                p[m].pwPindex = k;
                if (!p[k].duplicate) ndepassoc++;
            }
/*
 *          pS*
 */
            else if (strncmp(p[k].phase, "pS", 2) == 0) {
                p[m].hasdepthphase++;
                p[m].pSindex = k;
                if (!p[k].duplicate) ndepassoc++;
            }
/*
 *          sP*
 */
            else if (strncmp(p[k].phase, "sP", 2) == 0) {
                p[m].hasdepthphase++;
                p[m].sPindex = k;
                if (!p[k].duplicate) ndepassoc++;
            }
/*
 *          sS*
 */
            else if (strncmp(p[k].phase, "sS", 2) == 0) {
                p[m].hasdepthphase++;
                p[m].sSindex = k;
                if (!p[k].duplicate) ndepassoc++;
            }
            else
                continue;
        }
    }
    if (isverbose && verbose > 1)
        fprintf(logfp, "    %d associated depth phases\n", ndepassoc);
/*
 *  agencies reporting depth phases
 */
    if (ndepassoc) {
        for (i = 0; i < sp->numPhase; i++) {
            if (!p[i].firstP || !p[i].hasdepthphase)
                continue;
            if (p[i].pPindex) {
                k = p[i].pPindex;
                if (!p[k].timedef || p[k].duplicate) continue;
            }
            if (p[i].pwPindex) {
                k = p[i].pwPindex;
                if (!p[k].timedef || p[k].duplicate) continue;
            }
            if (p[i].pSindex) {
                k = p[i].pSindex;
                if (!p[k].timedef || p[k].duplicate) continue;
            }
            if (p[i].sPindex) {
                k = p[i].sPindex;
                if (!p[k].timedef || p[k].duplicate) continue;
            }
            if (p[i].sSindex) {
                k = p[i].sSindex;
                if (!p[k].timedef || p[k].duplicate) continue;
            }
            for (j = 0; j < numAgencies; j++)
                if (streq(p[i].agency, agencies[j]))
                    ageindex[j]++;
        }
        if (isverbose && verbose > 1)
            fprintf(logfp, "    agencies contributing depth phases\n");
        for (nagent = 0, i = 0; i < numAgencies; i++) {
            if (ageindex[i]) {
                nagent++;
                if (isverbose && verbose > 1)
                    fprintf(logfp, "      %3d %-17s %4d depth phases\n",
                            nagent, agencies[i], ageindex[i]);
            }
        }
    }
/*
 *  check for depth resolution by depth phases
 */
    has_depdpres = (ndepassoc < MinDepthPhases ||
                    nagent < MindDepthPhaseAgencies) ? 0 : 1;
    if (isverbose) {
        fprintf(logfp, "    Depth-phase depth resolution: %d\n", has_depdpres);
        fprintf(logfp, "        %d agencies reported %d defining depth phases\n",
                nagent, ndepassoc);
    }
    return has_depdpres;
}

/*
 *  Title:
 *     DepthPhaseStack
 *  Synopsis:
 *     calculates depth-phase depth using the Murphy and Barker (BSSA, 2006)
 *        depth phase stacking method
 *     Murphy J.R. and B.W. Barker, 2006,
 *        Improved focal-depth determination through automated identification
 *        of the seismic depth phases pP and sP,
 *        Bull. Seism. Soc. Am., 96, 1213-1229.
 *
 *     builds station traces with 1 km steps in depth
 *     a station trace maps moveout times to depth for a given delta
 *     a boxcar function is placed in the station trace at the depth
 *         corresponding to the observed moveout
 *     the station traces then stacked across the network
 *     the depth-phase depth is identified as the median of the stack
 *  Input Arguments:
 *     sp        - pointer to current solution.
 *     p[]       - array of phase structures.
 *     tt_tables - pointer to travel-time tables
 *     topo      - ETOPO bathymetry/elevation matrix
 *  Returns:
 *     number of consistent depth phases
 *  Called by:
 *     Locator
 *  Calls:
 *     GetPhaseIndex, PhaseTTh, Stacker, Free
 */
int DepthPhaseStack(SOLREC *sp, PHAREC p[], TT_TABLE *tt_tables,
                      short int **topo)
{
    int i, j, k, m, n, ndep = 0, ndel = 0, ndp = 0, ns = 0;
    int prev_rdid = 0, med = 0, d = 0, dlo = 0, dhi = 0;
    int iP = 0, ipP = 0, ipwP = 0, ipS = 0, isP = 0, isS = 0;
    int nsamp = (int)MaxHypocenterDepth + 1;
    double delta = 0., depth = 0., esaz = 0.;
    double moveout = 0., deltim = 0., smad = 0.;
    double *depths;
    double *tz = (double *)NULL;
    double *pt = (double *)NULL;
    double *pp = (double *)NULL;
    int *stack = (int *)NULL;
    int *trace = (int *)NULL;
    TT_TABLE *Pfirst = &tt_tables[0];
    depths = tt_tables[0].depths;
    sp->depdp = NULLVAL;
    sp->depdp_error = NULLVAL;
    sp->ndp = 0;
/*
 *  number of depth and delta samples in firstP TT table
 */
    ndep = tt_tables[0].ndep;
    ndel = tt_tables[0].ndel;
    tz = (double *)calloc(ndep, sizeof(double));
    pt = (double *)calloc(ndep, sizeof(double));
    trace = (int *)calloc(nsamp, sizeof(int));
    stack = (int *)calloc(nsamp, sizeof(int));
    if ((pp = (double *)calloc(ndep, sizeof(double))) == NULL) {
        Free(stack);
        Free(trace);
        Free(pt);
        Free(tz);
        fprintf(logfp, "DepthPhaseStack: cannot allocate memory\n");
        fprintf(errfp, "DepthPhaseStack: cannot allocate memory\n");
        errorcode = 1;
        return 0;
    }
/*
 *  Loop through readings with first arriving P with depth phases
 */
    prev_rdid = -1;
    for (i = 0; i < sp->numPhase; i++) {
/*
 *      skip if same reading
 */
        if (p[i].rdid == prev_rdid) continue;
/*
 *      skip if not first P or has no depth phases or non-defining phase
 */
        if (!p[i].timedef) continue;
        if (!p[i].firstP) continue;
        if (!p[i].hasdepthphase) continue;
        delta = p[i].delta;
        esaz = p[i].esaz;
/*
 *      check for out of range delta
 */
        if (delta < tt_tables[0].deltas[0] ||
            delta > tt_tables[0].deltas[ndel - 1])
            continue;
/*
 *      get phase indexes
 */
        ipP = ipwP = ipS = isP = isS = 0;
        if ((iP = GetPhaseIndex(p[i].phase)) < 0)
            continue;
        if (p[i].pPindex) {
            k = p[i].pPindex;
            if (!p[k].timedef || p[k].duplicate) continue;
            ipP = max(0, GetPhaseIndex(p[k].phase));
            if (ipP) {
                n = tt_tables[ipP].ndel;
                if (delta < tt_tables[ipP].deltas[0] ||
                    delta > tt_tables[ipP].deltas[n - 1])
                    ipP = 0;
            }
        }
        if (p[i].pwPindex) {
            k = p[i].pwPindex;
            if (!p[k].timedef || p[k].duplicate) continue;
            ipwP = max(0, GetPhaseIndex(p[k].phase));
            if (ipwP) {
                n = tt_tables[ipwP].ndel;
                if (delta < tt_tables[ipwP].deltas[0] ||
                    delta > tt_tables[ipwP].deltas[n - 1])
                    ipwP = 0;
            }
        }
        if (p[i].pSindex) {
            k = p[i].pSindex;
            if (!p[k].timedef || p[k].duplicate) continue;
            ipS = max(0, GetPhaseIndex(p[k].phase));
            if (ipS) {
                n = tt_tables[ipS].ndel;
                if (delta < tt_tables[ipS].deltas[0] ||
                    delta > tt_tables[ipS].deltas[n - 1])
                    ipS = 0;
            }
        }
        if (p[i].sPindex) {
            k = p[i].sPindex;
            if (!p[k].timedef || p[k].duplicate) continue;
            isP = max(0, GetPhaseIndex(p[k].phase));
            if (isP) {
                n = tt_tables[isP].ndel;
                if (delta < tt_tables[isP].deltas[0] ||
                    delta > tt_tables[isP].deltas[n - 1])
                    isP = 0;
            }
        }
        if (p[i].sSindex) {
            k = p[i].sSindex;
            if (!p[k].timedef || p[k].duplicate) continue;
            isS = max(0, GetPhaseIndex(p[k].phase));
            if (isS) {
                n = tt_tables[isS].ndel;
                if (delta < tt_tables[isS].deltas[0] ||
                    delta > tt_tables[isS].deltas[n - 1])
                    isS = 0;
            }
        }
/*
 *      no valid depth phases
 */
        if (!(ipP || ipwP || ipS || isP || isS))
            continue;
/*
 *      build first_P TT(h) for this delta
 */
        for (j = 0; j < ndep; j++) {
            tz[j] = -999.;
            pt[j] = -999.;
        }
        j = PhaseTTh(delta, esaz, sp, &tt_tables[iP], Pfirst, topo,
                       0, 0, tz);
/*
 *      pP* - first P
 */
        if (ipP) {
            k = p[i].pPindex;
            if (verbose > 1)
                fprintf(logfp, "        %s, %-s - %s, delta = %.2f statrace\n",
                        p[i].prista, p[k].phase, p[i].phase, delta);
            n = PhaseTTh(delta, esaz, sp, &tt_tables[ipP], Pfirst, topo,
                           1, 0, pt);
            m = min(n, j);
            deltim = p[k].deltim;
            moveout = p[k].ttime - p[i].ttime;
            Stacker(m, moveout, deltim, pt, tz, depths, pp, trace, stack);
            ndp++;
        }
/*
 *      pwP - first P
 */
        if (ipwP) {
            k = p[i].pwPindex;
            if (verbose > 1)
                fprintf(logfp, "        %s, %-s - %s, delta = %.2f statrace\n",
                        p[i].prista, p[k].phase, p[i].phase, delta);
            n = PhaseTTh(delta, esaz, sp, &tt_tables[ipwP], Pfirst, topo,
                           1, 1, pt);
            m = min(n, j);
            deltim = p[k].deltim;
            moveout = p[k].ttime - p[i].ttime;
            Stacker(m, moveout, deltim, pt, tz, depths, pp, trace, stack);
            ndp++;
        }
/*
 *      pS* - first P
 */
        if (ipS) {
            k = p[i].pSindex;
            if (verbose > 1)
                fprintf(logfp, "        %s, %-s - %s, delta = %.2f statrace\n",
                        p[i].prista, p[k].phase, p[i].phase, delta);
            n = PhaseTTh(delta, esaz, sp, &tt_tables[ipS], Pfirst, topo,
                           2, 0, pt);
            m = min(n, j);
            deltim = p[k].deltim;
            moveout = p[k].ttime - p[i].ttime;
            Stacker(m, moveout, deltim, pt, tz, depths, pp, trace, stack);
            ndp++;
        }
/*
 *      sP* - first P
 */
        if (isP) {
            k = p[i].sPindex;
            if (verbose > 1)
                fprintf(logfp, "        %s, %-s - %s, delta = %.2f statrace\n",
                        p[i].prista, p[k].phase, p[i].phase, delta);
            n = PhaseTTh(delta, esaz, sp, &tt_tables[isP], Pfirst, topo,
                           2, 0, pt);
            m = min(n, j);
            deltim = p[k].deltim;
            moveout = p[k].ttime - p[i].ttime;
            Stacker(m, moveout, deltim, pt, tz, depths, pp, trace, stack);
            ndp++;
        }
/*
 *      sS* - first P
 */
        if (isS) {
            k = p[i].sSindex;
            if (verbose > 1)
                fprintf(logfp, "        %s, %-s - %s, delta = %.2f statrace\n",
                        p[i].prista, p[k].phase, p[i].phase, delta);
            n = PhaseTTh(delta, esaz, sp, &tt_tables[isS], Pfirst, topo,
                           3, 0, pt);
            m = min(n, j);
            deltim = p[k].deltim;
            moveout = p[k].ttime - p[i].ttime;
            Stacker(m, moveout, deltim, pt, tz, depths, pp, trace, stack);
            ndp++;
        }
    }
/*
 *  check for number of depth phases
 */
    if (ndp < 3) {
        if (verbose)
            fprintf(logfp, "    insufficient number of depth phases (%d)!\n",
                    ndp);
    }
/*
 *  find the maximum of the stack
 *      d is the index (i.e. the depth) of the maximum, m is the maximum value
 */
    else {
        if (verbose)
            fprintf(logfp, "    network depth stack trace\n  ");
        d = 0;
        for (m = 0, j = 0; j < nsamp; j++) {
            if (verbose) fprintf(logfp, "%d ", stack[j]);
            if (stack[j] > m) {
                d = j;
                m = stack[j];
            }
        }
        if (verbose) fprintf(logfp, "\n");
/*
 *      check number of constructively adding depth phases
 */
        ndp = m;
        if (ndp < 3) {
            if (verbose) {
                fprintf(logfp, "    insufficient number of consistent ");
                fprintf(logfp, "depth phases (%d)!\n", ndp);
            }
        }
        else {
/*
 *          stack width at the maximum of the stack
 */
            ns = 0;
            for (j = d; j > -1; j--) {
                ns += stack[j];
                if (stack[j] < 1) break;
            }
            dlo = max(j + 1, 0);
            for (j = d; j < nsamp; j++) {
                ns += stack[j];
                if (stack[j] < 1) break;
            }
            dhi = min(j, nsamp);
            if (verbose > 1)
                fprintf(logfp, "        stack width (%d-%d)\n", dlo, dhi);
/*
 *          get median from the cumulative of the stack
 */
            trace[0] = stack[dlo];
            k = 1;
            for (j = dlo + 1; j < dhi; j++) {
                trace[k] = trace[k - 1] + stack[j];
                if ((double)trace[k - 1] / (double)ns < 0.5 &&
                    (double)trace[k] / (double)ns >= 0.5)
                    m = k;
                k++;
            }
/*
 *          depth-phase depth
 */
            med = (double)(m + dlo) +
                  ((double)ns / 2. - (double)trace[m - 1]) /
                   (double)stack[m + dlo];
            depth = med;
/*
 *          get smad from the the stack
 */
            for (j = 0; j < nsamp; j++) trace[j] = 0;
            n = 0;
            for (j = dlo; j < dhi; j++) {
                k = abs(j - med);
                trace[k] += stack[j];
                if (k > n) n = k;
            }
            stack[0] = trace[0];
            k = 1;
            for (j = 1; j < n; j++) {
                stack[k] = stack[k - 1] + trace[j];
                if ((double)stack[k - 1] / (double)ns < 0.5 &&
                    (double)stack[k] / (double)ns >= 0.5)
                    m = k;
                k++;
            }
/*
 *          smad
 */
            smad = 1.4826 * (double)(m) +
                   ((double)ns / 2. - (double)stack[m - 1]) /
                   (double)trace[m];
            if (verbose > 1)
                fprintf(logfp, "        depth=%.1f, smad=%.2f\n", depth, smad);
/*
 *          set depth and depth error
 */
            sp->depdp = depth;
            sp->depdp_error = smad;
            sp->ndp = ndp;
        }
    }
    Free(pp);
    Free(stack);
    Free(trace);
    Free(pt);
    Free(tz);
    return ndp;
}

/*
 *  Title:
 *     PhaseTTh
 *  Synopsis:
 *     Calculates TT(h) vector for a given delta and phase.
 *  Input Arguments:
 *     delta     - distance [deg]
 *     esaz      - event-to-station azimuth
 *     sp        - pointer to current solution.
 *     tt_tablep - pointer to travel-time table for a phase
 *     Pfirst    - pointer to travel-time table of first-arriving P
 *     topo      - ETOPO bathymetry/elevation matrix
 *     ips       - depth phase index for bounce corrections
 *     ispwP     - is the phase pwP?
 *  Output Arguments:
 *     pt        - TT(h) vector
 *  Returns:
 *     number of depth samples
 *  Called by:
 *     DepthPhaseStack
 *  Calls:
 *     FloatBracket, SplineCoeffs, SplineInterpolation, PointAtDeltaAzimuth,
 *     GetEtopoCorrection
 */
static int PhaseTTh(double delta, double esaz, SOLREC *sp, TT_TABLE *tt_tablep,
            TT_TABLE *Pfirst, short int **topo, int ips, int ispwP, double *pt)
{
    int i, j, k, m, exactdelta = 0;
    int ilo = 0, ihi = 0, jlo = 0, jhi = 0, idel = 0, jdel = 0;
    double dydx = 0., d2ydx = 0.;
    double d2y[DELTA_SAMPLES], tmp[DELTA_SAMPLES];
    double x[DELTA_SAMPLES], t[DELTA_SAMPLES];
    double b[DELTA_SAMPLES], p[DELTA_SAMPLES];
    double tcor = 0., tcorw = 0., rayp = 0., tt = 0.;
    double bpaz = 0., bpdel = 0., bplat = 0., bplon = 0.;
    int ndep = tt_tablep->ndep;
    int ndel = tt_tablep->ndel;
    int jndel = Pfirst->ndel;
/*
 *  delta range
 */
    FloatBracket(delta, ndel, tt_tablep->deltas, &ilo, &ihi);
    if (fabs(delta - tt_tablep->deltas[ilo]) < DEPSILON) {
        idel = ilo;
        exactdelta = 1;
    }
    else if (fabs(delta - tt_tablep->deltas[ihi]) < DEPSILON) {
        idel = ihi;
        exactdelta = 1;
    }
    else {
        idel = ilo;
        ilo = idel - DELTA_SAMPLES / 2 + 1;
        ihi = idel + DELTA_SAMPLES / 2 + 1;
        if (ilo < 0) {
            ilo = 0;
            ihi = ilo + DELTA_SAMPLES;
        }
        if (ihi > ndel) {
            ihi = ndel;
            ilo = ihi - DELTA_SAMPLES;
        }
    }
    FloatBracket(delta, jndel, Pfirst->deltas, &jlo, &jhi);
    if (fabs(delta - Pfirst->deltas[jlo]) < DEPSILON) {
        jdel = jlo;
    }
    else if (fabs(delta - Pfirst->deltas[jhi]) < DEPSILON) {
        jdel = jhi;
    }
    else {
        jdel = jlo;
        jlo = jdel - DELTA_SAMPLES / 2 + 1;
        jhi = jdel + DELTA_SAMPLES / 2 + 1;
        if (jlo < 0) {
            jlo = 0;
            jhi = jlo + DELTA_SAMPLES;
        }
        if (jhi > jndel) {
            jhi = jndel;
            jlo = jhi - DELTA_SAMPLES;
        }
    }
/*
 *  build phase TT(h) for this delta
 */
    for (j = 0; j < ndep; j++) {
/*
 *      no need for SplineCoeffs interpolation if exact delta
 */
        if (exactdelta) {
            if (!ips) {
                if (tt_tablep->tt[idel][j] < 0.) {
                    pt[j] = Pfirst->tt[jdel][j];
                }
                else {
                    pt[j] = tt_tablep->tt[idel][j];
                }
            }
            else {
                pt[j] = tt_tablep->tt[idel][j];
            }
/*
 *          bounce point correction
 */
            if (ips) {
                bpdel = tt_tablep->bpdel[idel][j];
                rayp = tt_tablep->dtdd[idel][j];
                bpaz = esaz;
                if (rayp < 0.)   bpaz += 180.;
                if (bpaz > 360.) bpaz -= 360.;
                PointAtDeltaAzimuth(sp->lat, sp->lon, bpdel, bpaz, &bplat, &bplon);
                tcor = GetEtopoCorrection(ips, rayp, bplat, bplon, topo, &tcorw);
                pt[j] += tcor;
                if (ispwP) pt[j] += tcorw;
            }
        }
/*
 *      SplineCoeffs interpolation in delta
 */
        else {
            for (m = 0, k = jlo, i = ilo; i < ihi; k++, i++) {
                if (!ips) {
                    if (tt_tablep->tt[i][j] < 0.) {
                        tt = Pfirst->tt[k][j];
                    }
                    else {
                        tt = tt_tablep->tt[i][j];
                    }
                }
                else {
                    tt = tt_tablep->tt[i][j];
                }
                if (tt < 0.) continue;
                x[m] = tt_tablep->deltas[i];
                t[m] = tt;
                if (ips) {
                    b[m] = tt_tablep->bpdel[i][j];
                    p[m] = tt_tablep->dtdd[i][j];
                }
                m++;
            }
            if (m < MIN_SAMPLES)
                pt[j] = -999.;
            else {
                SplineCoeffs(m, x, t, d2y, tmp);
                pt[j] = SplineInterpolation(delta, m, x, t, d2y, 0, &dydx, &d2ydx);
/*
 *              bounce point correction
 */
                if (ips) {
                    SplineCoeffs(m, x, b, d2y, tmp);
                    bpdel = SplineInterpolation(delta, m, x, b, d2y, 0,
                                                &dydx, &d2ydx);
                    SplineCoeffs(m, x, p, d2y, tmp);
                    rayp = SplineInterpolation(delta, m, x, p, d2y, 0,
                                               &dydx, &d2ydx);
                    bpaz = esaz;
                    if (rayp < 0.)   bpaz += 180.;
                    if (bpaz > 360.) bpaz -= 360.;
                    PointAtDeltaAzimuth(sp->lat, sp->lon, bpdel, bpaz,
                                        &bplat, &bplon);
                    tcor = GetEtopoCorrection(ips, rayp, bplat, bplon, topo,
                                              &tcorw);
                    pt[j] += tcor;
                    if (ispwP) pt[j] += tcorw;
                }
            }
        }
    }
    return ndep;
}

/*
 *  Title:
 *     Stacker
 *  Synopsis:
 *     Calculates station traces h(TT) and stacks them.
 *  Input Arguments:
 *     n       - number of samples
 *     moveout - observed depth phase - first P time (moveout)
 *     deltim  - tolerance (width of the boxcar function)
 *     pt      - predicted depth phase times w.r.t. depth
 *     tz      - predicted first P times w.r.t. depth
 *     depths  - depth vector
 *  Output Arguments:
 *     pp     - predicted moveout times w.r.t. depth
 *     trace  - station trace h(TT) with a boxcar around moveout
 *     stack  - network stack
 *  Called by:
 *     DepthPhaseStack
 *  Calls:
 *     StationTrace
 */
static void Stacker(int n, double moveout, double deltim,
                    double *pt, double *tz, double *depths,
                    double *pp, int *trace, int *stack)
{
    int j, k = 0;
    int nsamp = (int)MaxHypocenterDepth + 1;
/*
 *  depth phase - first P travel times (moveout curve vs depth)
 */
    pp[k++] = 0.;
    for (j = 0; j < n; j++) {
        if (tz[j+1] < 0. || pt[j] < 0.)
            continue;
        else
            pp[k++] = pt[j] - tz[j+1];
    }
    if (k < 1) return;
/*
 *  station trace: depth vs moveout curve with a boxcar around observed moveout
 */
    StationTrace(k, moveout, deltim, pp, depths, trace);
    if (verbose > 2) fprintf(logfp, "        ");
/*
 *  stack station traces
 */
    for (j = 0; j < nsamp; j++) {
        stack[j] += trace[j];
        if (verbose > 2) fprintf(logfp, "%d ", trace[j]);
    }
    if (verbose > 2) fprintf(logfp, "\n");
}

/*
 *  Title:
 *     StationTrace
 *  Synopsis:
 *     Calculates station trace (h(TT)) vector for a given
 *         moveout time with deltim tolerance.
 *  Input Arguments:
 *     n       - number of samples
 *     moveout - observed moveout
 *     deltim  - tolerance
 *     pp      - predicted moveout w.r.t. depth
 *     h       - depth vector
 *  Output Arguments:
 *     trace - station trace Z(tt) with a boxcar around moveout
 *  Called by:
 *    DepthPhaseStack
 *  Calls:
 *     FloatBracket, SplineCoeffs, SplineInterpolation
 */
static void StationTrace(int n, double moveout, double deltim, double *pp,
                      double *h, int *trace)
{
    int i, j, ilo, ihi, idel, hlo, hhi;
    int nsamp = (int)MaxHypocenterDepth + 1;
    double dydx = 0., d2ydx = 0., tp = 0., hp = 0.;
    double d2y[DELTA_SAMPLES], tmp[DELTA_SAMPLES];
    double z[DELTA_SAMPLES], t[DELTA_SAMPLES];
    for (j = 0; j < nsamp; j++) trace[j] = 0;
/*
 *  h(TT) lower limit
 */
    tp = moveout - deltim;
    FloatBracket(tp, n, pp, &ilo, &ihi);
    if (fabs(tp - pp[ilo]) < DEPSILON)
        hlo = (int)floor(h[ilo]);
    else if (fabs(tp - pp[ihi]) < DEPSILON)
        hlo = (int)floor(h[ihi]);
    else {
        idel = ilo;
        ilo = idel - DELTA_SAMPLES / 2 + 1;
        ihi = idel + DELTA_SAMPLES / 2 + 1;
        if (ilo < 0) {
            ilo = 0;
            ihi = ilo + DELTA_SAMPLES;
        }
        if (ihi > n) {
            ihi = n;
            ilo = ihi - DELTA_SAMPLES;
        }
        for (j = 0, i = ilo; i < ihi; j++, i++) {
            z[j] = h[i];
            t[j] = pp[i];
        }
        SplineCoeffs(j, t, z, d2y, tmp);
        hp = SplineInterpolation(tp, j, t, z, d2y, 0, &dydx, &d2ydx);
        hlo = (int)floor(hp);
    }
    if (hlo < 0) hlo = 0;
/*
 *  h(TT) upper limit
 */
    tp = moveout + deltim;
    FloatBracket(tp, n, pp, &ilo, &ihi);
    if (fabs(tp - pp[ilo]) < DEPSILON)
        hhi = (int)ceil(h[ilo]);
    else if (fabs(tp - pp[ihi]) < DEPSILON)
        hhi = (int)ceil(h[ihi]);
    else {
        idel = ilo;
        ilo = idel - DELTA_SAMPLES / 2 + 1;
        ihi = idel + DELTA_SAMPLES / 2 + 1;
        if (ilo < 0) {
            ilo = 0;
            ihi = ilo + DELTA_SAMPLES;
        }
        if (ihi > n) {
            ihi = n;
            ilo = ihi - DELTA_SAMPLES;
        }
        for (j = 0, i = ilo; i < ihi; j++, i++) {
            z[j] = h[i];
            t[j] = pp[i];
        }
        SplineCoeffs(j, t, z, d2y, tmp);
        hp = SplineInterpolation(tp, j, t, z, d2y, 0, &dydx, &d2ydx);
        hhi = (int)ceil(hp);
    }
    if (hhi > nsamp) hhi = nsamp;
/*
 *  put boxcar at depth consistent with moveout observation
 */
    for (i = hlo; i < hhi; i++)
        trace[i] = 1;
}
