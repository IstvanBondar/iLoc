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
extern PHASEMAP PhaseMap[MAXPHACODES];   /* reported phase to IASPEI mapping */
extern int numPhaseMap;                      /* number of phases in PhaseMap */
extern char AllowablePhases[MAXTTPHA][PHALEN];           /* allowable phases */
extern int numAllowablePhases;                 /* number of allowable phases */
extern char firstPphase[MAXTTPHA][PHALEN];        /* first-arriving P phases */
extern int numFirstPphase;              /* number of first-arriving P phases */
extern char firstSphase[MAXTTPHA][PHALEN];        /* first-arriving S phases */
extern int numFirstSphase;              /* number of first-arriving S phases */
extern char firstPoptional[MAXTTPHA][PHALEN];     /* optional first P phases */
extern int numFirstPoptional;           /* number of optional first P phases */
extern char firstSoptional[MAXTTPHA][PHALEN];     /* optional first S phases */
extern int numFirstSoptional;           /* number of optional first S phases */
extern PHASEWEIGHT PhaseWeight[MAXNUMPHA];          /* prior time measerrors */
extern int numPhaseWeight;                /* number of phases in PhaseWeight */
extern char PhaseWithoutResidual[MAXNUMPHA][PHALEN];   /* no-residual phases */
extern int PhaseWithoutResidualNum;          /* number of no-residual phases */
extern double SigmaThreshold;             /* to exclude phases from solution */
extern int DoCorrelatedErrors;              /* account for correlated errors */
extern int UseRSTT;                                  /* use RSTT predictions */
extern int DoNotRenamePhase;                     /* do not reidentify phases */
extern double mbMinPeriod;                  /* min period for calculating mb */
extern double mbMaxPeriod;                  /* max period for calculating mb */
extern double MSMinPeriod;                  /* min period for calculating Ms */
extern double MSMaxPeriod;                  /* max period for calculating Ms */
extern int MinNdefPhases;                            /* min number of phases */

/*
 * Functions:
 *    IdentifyPhases
 *    ReIdentifyPhases
 *    IdentifyPFAKE
 *    RemovePFAKE
 *    DuplicatePhases
 *    ResidualsForReportedPhases
 */

/*
 * Local functions:
 *    PhaseIdentification
 *    isFirstP
 *    isFirstS
 *    GetPriorMeasurementError
 *    SameStation
 *    SameArrivalTime
 */
static void PhaseIdentification(SOLREC *sp, READING *rdindx, PHAREC p[],
        EC_COEF *ec, TT_TABLE *tt_tables, TT_TABLE *localtt_tables,
        short int **topo);
static int isFirstP(char *phase, char *mappedphase);
static int isFirstS(char *phase, char *mappedphase);
static void GetPriorMeasurementError(PHAREC *pp);
static void SameStation(int samesta[], int n, SOLREC *sp, PHAREC p[],
        EC_COEF *ec, TT_TABLE *tt_tables, TT_TABLE *localtt_tables,
        short int **topo);
static void SameArrivalTime(int sametime[], int n, SOLREC *sp, PHAREC p[],
        EC_COEF *ec, TT_TABLE *tt_tables, TT_TABLE *localtt_tables,
        short int **topo);

/*
 *  Title:
 *     IdentifyPhases
 *  Synopsis:
 *     Identifies phases with respect to the initial hypocentre.
 *     Maps reported phase ids to IASPEI standard phase names.
 *     Uses phase dependent information from <vmodel>_model.txt file.
 *        PhaseMap - list of possible reported phases with their
 *                    corresponding IASPEI phase id.
 *     Sets unrecognized reported phase names to null.
 *     Assumes that unidentified reported first-arriving phase is P.
 *     Identifies phases within a reading and marks first-arriving P and S.
 *     Sets time-defining flags and a priori measurement errors.
 *  Input Arguments:
 *     sp        - pointer to current solution
 *     rdindx    - array of reading structures
 *     p         - array of phase structures
 *     ec        - pointer to ellipticity correction coefficient structure
 *     tt_tables - pointer to travel-time tables
 *     localtt_tables - pointer to local travel-time tables, if any
 *     topo      - ETOPO bathymetry/elevation matrix
 *  Output Arguments:
 *     p         - array of phase structures
 *  Returns:
 *     initial number of time defining phases
 *  Called by:
 *     Locator, ResidualsForFixedHypocenter
 *  Calls:
 *     PhaseIdentification, GetPriorMeasurementError
 */
int IdentifyPhases(SOLREC *sp, READING *rdindx, PHAREC p[], EC_COEF *ec,
        TT_TABLE *tt_tables, TT_TABLE *localtt_tables, short int **topo,
        int *is2nderiv)
{
    int i, j, n = 0, np;
    sp->ntimedef = sp->nazimdef = sp->nslowdef = 0;
/*
 *  map phases to IASPEI phase names
 */
    for (i = 0; i < sp->numPhase; i++) {
/*
 *      initializations
 */
        strcpy(p[i].prevphase, "");
        p[i].dupsigma = 0.;
        p[i].timedef = p[i].prevtimedef = 0;
        p[i].azimdef = p[i].prevazimdef = 0;
        p[i].slowdef = p[i].prevslowdef = 0;
        p[i].firstP = p[i].firstS = 0;
        p[i].duplicate = 0;
        strcpy(p[i].vmod, "null");
/*
 *      continue if phase name is fixed by analysts
 */
        if (p[i].phase_fixed)
            continue;
/*
 *      initialize phase name
 */
        strcpy(p[i].phase, "");
/*
 *      set unknown phases with amplitudes to AMB or AMS depending on period
 */
        if (streq(p[i].ReportedPhase, "") && p[i].numamps) {
//            if (p[i].a[0].per > mbMinPeriod && p[i].a[0].per < mbMaxPeriod)
                strcpy(p[i].phase, "AMB");
            if (p[i].a[0].per > MSMinPeriod && p[i].a[0].per < MSMaxPeriod)
                strcpy(p[i].phase, "AMS");
        }
/*
 *      assume that unknown initial phases are P
 */
        if (streq(p[i].ReportedPhase, "") && p[i].init)
            strcpy(p[i].phase, "P");
/*
 *      map reported phase names to IASPEI standard
 */
        for (j = 0; j < numPhaseMap; j++) {
            if (streq(p[i].ReportedPhase, PhaseMap[j].ReportedPhase)) {
                strcpy(p[i].phase, PhaseMap[j].phase);
                break;
            }
        }
    }
/*
 *  identify first arriving P and S in a reading
 */
    for (i = 0; i < sp->nreading; i++) {
        np = rdindx[i].start + rdindx[i].npha;
        for (j = rdindx[i].start; j < np; j++) {
            if (p[j].phase[0] == 'P' ||
                (islower(p[j].phase[0]) &&
                 (p[j].phase[1] == 'P' || p[j].phase[1] == 'w'))) {
                p[j].firstP = 1;
                break;
            }
        }
        for (j = rdindx[i].start; j < np; j++) {
            if (p[j].phase[0] == 'S' || streq(p[j].phase, "Lg") ||
                (islower(p[j].phase[0]) && p[j].phase[1] == 'S')) {
                p[j].firstS = 1;
                break;
            }
        }
    }
/*
 *  identify phases within a reading
 */
    for (i = 0; i < sp->nreading; i++) {
        PhaseIdentification(sp, (rdindx + i), p, ec, tt_tables, localtt_tables,
                            topo);
    }
/*
 *  clear current GreatCircle object and the pool of CrustalProfile objects
 */
    if (UseRSTT)
         slbm_shell_clear();
/*
 *  set timedef flags and get prior measurement errors
 */
    for (i = 0; i < sp->numPhase; i++) {
        if (streq(p[i].phase, "")) {
/*
 *          unidentified phases are made non-defining
 */
            p[i].timedef = 0;
            p[i].slowdef = 0;
            p[i].azimdef = 0;
        }
        else {
/*
 *          set defining flags and get prior measurement errors
 */
            GetPriorMeasurementError(&p[i]);
            if (fabs(p[i].d2tdd) < DEPSILON)
                p[i].slowdef = 0;
        }
        if (verbose > 2) {
            fprintf(logfp, "    %-6s %-8s: timedef=%d azimdef=%d slowdef=%d ",
                    p[i].sta, p[i].phase, p[i].timedef, p[i].azimdef, p[i].slowdef);
            fprintf(logfp, "deltim=%.3f delaz=%.1f delslo=%.1f\n",
                    p[i].deltim, p[i].delaz, p[i].delslo);
        }
        strcpy(p[i].prevphase, p[i].phase);
        if (p[i].timedef) sp->ntimedef++;
        if (p[i].azimdef) sp->nazimdef++;
        if (p[i].slowdef) sp->nslowdef++;
    }
    fprintf(logfp, "nTimedef=%d nAzimdef=%d nSlowdef=%d\n",
            sp->ntimedef, sp->nazimdef, sp->nslowdef);
    if (sp->ntimedef > MinNdefPhases) {
        *is2nderiv = 0;
        if (sp->nslowdef)
            fprintf(logfp, "There is enough time defining phases to ignore slowness observations\n");
        for (i = 0; i < sp->numPhase; i++)
            p[i].slowdef = 0;
        sp->nslowdef = 0;
    }
    n = sp->ntimedef + sp->nazimdef + sp->nslowdef;
    fprintf(logfp, "Total number of defining observations=%d\n", n);
    return n;
}

/*
 *  Title:
 *     ReIdentifyPhases
 *  Synopsis:
 *     Reidentifies phases after NA search is completed and/or
 *         if depth crosses Moho/Conrad discontinuity between iterations.
 *     At this point phase names are already mapped to IASPEI standards.
 *     Identifies phases within a reading and marks first-arriving P and S.
 *     Sets time-defining flags and a priori measurement errors.
 *  Input Arguments:
 *     sp        - pointer to current solution
 *     rdindx   - array of reading structures
 *     p         - array of phase structures
 *     ec        - pointer to ellipticity correction coefficient structure
 *     tt_tables - pointer to travel-time tables
 *     localtt_tables - pointer to local travel-time tables, if any
 *     topo      - ETOPO bathymetry/elevation matrix
 *  Output Arguments:
 *     p         - array of phase structures
 *  Return:
 *     1 on phasename changes, 0 otherwise
 *  Called by:
 *     Locator, LocateEvent
 *  Calls:
 *     PhaseIdentification, GetPriorMeasurementError
 */
int ReIdentifyPhases(SOLREC *sp, READING *rdindx, PHAREC p[], EC_COEF *ec,
        TT_TABLE *tt_tables, TT_TABLE *localtt_tables, short int **topo,
        int is2nderiv)
{
    int i, j, np, isphasechange = 0;
    for (i = 0; i < sp->numPhase; i++)
        p[i].firstP = p[i].firstS = p[i].duplicate = 0;
/*
 *  identify first arriving P and S in a reading
 */
    for (i = 0; i < sp->nreading; i++) {
        np = rdindx[i].start + rdindx[i].npha;
        for (j = rdindx[i].start; j < np; j++) {
            if (p[j].phase[0] == 'P' ||
                (islower(p[j].phase[0]) &&
                 (p[j].phase[1] == 'P' || p[j].phase[1] == 'w'))) {
                p[j].firstP = 1;
                break;
            }
        }
        for (j = rdindx[i].start; j < np; j++) {
            if (p[j].phase[0] == 'S' || streq(p[j].phase, "Lg") ||
                (islower(p[j].phase[0]) && p[j].phase[1] == 'S')) {
                p[j].firstS = 1;
                break;
            }
        }
    }
/*
 *  identify phases within a reading
 */
    for (i = 0; i < sp->nreading; i++) {
        PhaseIdentification(sp, &rdindx[i], p, ec, tt_tables, localtt_tables,
                            topo);
    }
/*
 *  clear current GreatCircle object and the pool of CrustalProfile objects
 */
    if (UseRSTT)
        slbm_shell_clear();
/*
 *  set timedef flags and get prior measurement errors
 */
    for (i = 0; i < sp->numPhase; i++) {
        if (streq(p[i].phase, "")) {
/*
 *          unidentified phases are made non-defining
 */
            p[i].timedef = 0;
            p[i].slowdef = 0;
            p[i].azimdef = 0;
        }
        else {
/*
 *          set defining flags and get prior measurement errors
 */
            GetPriorMeasurementError(&p[i]);
            if (!is2nderiv || fabs(p[i].d2tdd) < DEPSILON)
                p[i].slowdef = 0;
        }
        if (strcmp(p[i].phase, p[i].prevphase)) {
            isphasechange = 1;
            if (verbose > 2) {
                fprintf(logfp, "    %-6s %-8s: timedef=%d azimdef=%d ",
                        p[i].sta, p[i].phase, p[i].timedef, p[i].azimdef);
                fprintf(logfp, "deltim=%.3f delaz=%.1f\n",
                        p[i].deltim, p[i].delaz);
            }
            strcpy(p[i].prevphase, p[i].phase);
        }
    }
    return isphasechange;
}

/*
 *  Title:
 *     PhaseIdentification
 *  Synopsis:
 *     Identifies phases in a reading according to their time residuals.
 *     At this point phase names are already mapped to IASPEI standards.
 *     Uses phase dependent information from <vmodel>_model.txt file.
 *        PhaseWithoutResidual - list of phases that don't get residuals, i.e.
 *                         never used in the location (e.g. amplitude phases)
 *        AllowablePhases - list of phases to which reported phases can be
 *                          renamed
 *        AllowableFirstP - list of phases to which reported first-arriving
 *                          P phases can be renamed
 *        OptionalFirstP - additional list of phases to which reported
 *                         first-arriving P phases can be renamed
 *        AllowableFirstS - list of phases to which reported first-arriving
 *                          S phases can be renamed
 *        OptionalFirstS - additional list of phases to which reported
 *                         first-arriving S phases can be renamed
 *     Skips phases that do not get residuals.
 *        The list of no-residual phases contains the list of phases for which
 *        residuals will not be calculated, such as amplitude phases or IASPEI
 *        phases for which no travel-time tables exist. These phases are not
 *        used in the location.
 *     Considers only P or S type phases.
 *        phase type is determined by the first leg of the phase id;
 *        for depth phases phase type is determined by the second letter.
 *     Does not reidentify phases fixed by analyst (phase_fixed flag).
 *     For exact duplicates (|dT| < 0.01) keeps the first non-null phaseid.
 *     Checks if the reported phase is in the list of allowable phases.
 *        The list of allowable phases were introduced to prevent the locator
 *        to rename phases to unlikely 'exotic' phases, just because a
 *        travel-time prediction fits better the observed travel-time. For
 *        instance, we do not want to reidentify a reported Sn as SgSg, SPn or
 *        horribile dictu, sSn. Recall that phases may suffer from picking
 *        errors or the observed travel-times may reflect true 3D structures
 *        not modeled by the velocity model. Introducing the list of
 *        allowable phases helps to maintain the sanity of the bulletin and
 *        mitigates the risk of misidentifying phases. However, if a reported
 *        phase is not in the list of allowable phases, it is temporarily
 *        added to the list accepting the fact that station operators may
 *        confidently pick later phases. In other words, exotic phase names
 *        can appear in the final bulletin only if they were reported as such.
 *     Loops through the (possibly amended) list of allowable phases and
 *        calculates the time residual.
 *        Does not allow renaming a P-type phase to S-type, and vice versa.
 *        Does not allow allow S(*) phases to be renamed to depth phases (s*).
 *        Does not allow for repeating phaseids in a reading.
 *        Further restrictions apply to first-arriving P and S phases.
 *            First-arriving P and S phases can be identified as those in the
 *            list of allowable first-arriving P and S phases. Occasionally a
 *            station operator may not report the true first-arriving phase
 *            due to high noise conditions. To account for this situation the
 *            list of optional first-arriving P and S phases is also checked.
 *        Keeps track of the phaseid with the smallest residual.
 *     Sets the phase id to the phase in the allowable phase list with the
 *        smallest residual.
 *     If no eligible phase is found, leaves the phase unidentified.
 *  Input Arguments:
 *     sp        - pointer to current solution
 *     rdindx    - reading record
 *     pp        - phase record
 *     ec        - pointer to ellipticity correction coefficient structure
 *     tt_tables - pointer to travel-time tables
 *     localtt_tables - pointer to local travel-time tables, if any
 *     topo      - ETOPO bathymetry/elevation matrix
 *  Output Arguments:
 *     p         - array of phase structures
 *  Called by:
 *     IdentifyPhases, ReIdentifyPhases
 *  Calls:
 *     GetTravelTimePrediction, isFirstP, isFirstS
 */
static void PhaseIdentification(SOLREC *sp, READING *rdindx, PHAREC p[],
        EC_COEF *ec, TT_TABLE *tt_tables, TT_TABLE *localtt_tables,
        short int **topo)
{
    double resid, bigres = 60., min_resid, dtdd = NULLVAL;
    double rstterr = 0., pickerr = 0.;
    double ttime = NULLVAL, pPttime = NULLVAL, d2tdd = 0., d2tdh = 0.;
    char candidate_phase[PHALEN], mappedphase[PHALEN], phase[PHALEN];
    int isS = 0, isP = 0, isI = 0, isH = 0, iss = 0, isp = 0;
    int ptype = 0, stype = 0, ttype = 0;
    int j, k, m, n, ii, isseen = 0, npha = 0;
/*
 *  loop over phases in this reading
 */
    n = rdindx->start + rdindx->npha;
    for (m = 0, k = rdindx->start; k < n; m++, k++) {
        p[k].timeres = p[k].azimres = p[k].slowres = NULLVAL;
        p[k].d2tdd = p[k].d2tdh = 0.;
        p[k].rsttPickErr = p[k].rsttTotalErr = 0.;
/*
 *      skip phases that don't get residuals (amplitudes etc)
 */
        for (j = 0; j < PhaseWithoutResidualNum; j++) {
            if (streq(p[k].phase, PhaseWithoutResidual[j]))
                break;
        }
        if (j != PhaseWithoutResidualNum)
            continue;
        min_resid = bigres + 10.;
        resid = NULLVAL;
/*
 *      phase type is determined by the first leg of the phase id
 */
        isS = isP = isI = iss = isp = ptype = stype = 0;
        if (p[k].phase[0] == 'P') isP = 1;
        if (p[k].phase[0] == 'S' || streq(p[k].phase, "Lg")) isS = 1;
/*
 *      for depth phases phase type is determined by the second letter
 */
        if (islower(p[k].phase[0])) {
            if (p[k].phase[1] == 'P' || p[k].phase[1] == 'w') isp = 1;
            if (p[k].phase[1] == 'S') iss = 1;
        }
/*
 *      Infrasound phases
 */
        if (streq(p[k].phase, "I")) {
            isI = 1;
            p[k].phase_fixed = 1;
        }
/*
 *      Hydroacoustic phases
 */
        if (p[k].phase[0] == 'H' || p[k].phase[0] == 'O') {
            isH = 1;
            p[k].phase_fixed = 1;
        }
/*
 *      consider only P or S-type or infrasound/hydro phases
 */
        if (!(isP || isp || isS || iss || isI || isH)) continue;
        if (isP || isp) ptype = 1;
        if (isS || iss) stype = 1;
/*
 *      do not reidentify fixed phase names
 */
        if (p[k].phase_fixed || DoNotRenamePhase) {
/*
 *          get travel time for phase
 */
            if (GetTravelTimePrediction(sp, &p[k], ec, tt_tables,
                    localtt_tables, topo, 1, -1, 1)) {
                p[k].ttime = NULLVAL;
                p[k].timeres = NULLVAL;
                p[k].slowres = NULLVAL;
                if (verbose) {
                    fprintf(logfp, "    can't get TT for fixed phase %s! ",
                            p[k].phase);
                    fprintf(logfp, "(depth=%.2f delta=%.2f sta=%s)\n",
                            sp->depth, p[k].delta, p[k].sta);
                }
            }
            else {
                p[k].timeres = p[k].time - sp->time - p[k].ttime;
                if (p[k].azim != NULLVAL) p[k].azimres = p[k].azim - p[k].seaz;
                if (p[k].slow != NULLVAL) p[k].slowres = p[k].slow - p[k].dtdd;
                if (verbose > 4)
                    fprintf(logfp, "              %-8s %9.4f %9.4f %9.4f %9.4f %9.4f %9.4f\n",
                            p[k].phase, p[k].ttime, p[k].timeres,
                            p[k].azim, p[k].azimres, p[k].slow, p[k].slowres);
            }
        }
/*
 *      phase is not fixed by analysts
 */
        else {
/*
 *          deal with duplicates: keep the first non-null phaseid
 *              phases ordered by time within a reading
 */
            if (m) {
                if (fabs(p[k].time - p[k-1].time) < 0.05) {
/*
 *                  if previous phaseid is null, rename it
 */
                    if (streq(p[k-1].phase, "")) {
                        strcpy(p[k-1].phase, p[k].phase);
                        p[k-1].ttime = p[k].ttime;
                        p[k-1].timeres = p[k].timeres;
                        p[k-1].dtdd = p[k].dtdd;
                        p[k-1].d2tdd = p[k].d2tdd;
                        p[k-1].d2tdh = p[k].d2tdh;
                        p[k-1].rsttTotalErr = p[k].rsttTotalErr;
                        p[k-1].rsttPickErr = p[k].rsttPickErr;
                        p[k].duplicate = 1;
                        continue;
                    }
/*
 *                  otherwise use previous phase
 */
                    else {
                        for (j = 0; j < PhaseWithoutResidualNum; j++) {
                            if (streq(p[k-1].phase, PhaseWithoutResidual[j]))
                                break;
                        }
/*
 *                      if it is a no-residual phase, leave it alone
 */
                        if (j != PhaseWithoutResidualNum)
                            continue;
                        strcpy(p[k].phase, p[k-1].phase);
                        p[k].ttime = p[k-1].ttime;
                        p[k].timeres = p[k-1].timeres;
                        p[k].dtdd = p[k-1].dtdd;
                        p[k].d2tdd = p[k-1].d2tdd;
                        p[k].d2tdh = p[k-1].d2tdh;
                        p[k].rsttTotalErr = p[k-1].rsttTotalErr;
                        p[k].rsttPickErr = p[k-1].rsttPickErr;
                        p[k].duplicate = 1;
                        continue;
                    }
                }
            }
            strcpy(mappedphase, p[k].phase);
/*
 *
 *          see if mapped phase is in the allowable phase list
 *
 */
            npha = numAllowablePhases;
            for (j = 0; j < numAllowablePhases; j++) {
                if (streq(mappedphase, AllowablePhases[j]))
                    break;
            }
/*
 *          not in the list; temporarily add it to the list
 */
            if (j == numAllowablePhases) {
                strcpy(AllowablePhases[npha++], mappedphase);
/*
 *              deal with PP et al (PP is in the list of allowable phases)
 */
                if (streq(mappedphase, "PnPn")) {
                    strcpy(AllowablePhases[npha++], "PbPb");
                    strcpy(AllowablePhases[npha++], "PgPg");
                }
                else if (streq(mappedphase, "PbPb")) {
                    strcpy(AllowablePhases[npha++], "PnPn");
                    strcpy(AllowablePhases[npha++], "PgPg");
                }
                else if (streq(mappedphase, "PgPg")) {
                    strcpy(AllowablePhases[npha++], "PnPn");
                    strcpy(AllowablePhases[npha++], "PbPb");
                }
/*
 *              deal with SS et al (SS is in the list of allowable phases)
 */
                if (streq(mappedphase, "SnSn")) {
                    strcpy(AllowablePhases[npha++], "SbSb");
                    strcpy(AllowablePhases[npha++], "SgSg");
                }
                else if (streq(mappedphase, "SbSb")) {
                    strcpy(AllowablePhases[npha++], "SnSn");
                    strcpy(AllowablePhases[npha++], "SgSg");
                }
                else if (streq(mappedphase, "SgSg")) {
                    strcpy(AllowablePhases[npha++], "SnSn");
                    strcpy(AllowablePhases[npha++], "SbSb");
                }
/*
 *              deal with PS et al
 */
                if (streq(mappedphase, "PS")) {
                    strcpy(AllowablePhases[npha++], "PnS");
                    strcpy(AllowablePhases[npha++], "PgS");
                }
                else if (streq(mappedphase, "PnS")) {
                    strcpy(AllowablePhases[npha++], "PS");
                    strcpy(AllowablePhases[npha++], "PgS");
                }
                else if (streq(mappedphase, "PgS")) {
                    strcpy(AllowablePhases[npha++], "PS");
                    strcpy(AllowablePhases[npha++], "PnS");
                }
/*
 *              deal with SP et al
 */
                if (streq(mappedphase, "SP")) {
                    strcpy(AllowablePhases[npha++], "SPn");
                    strcpy(AllowablePhases[npha++], "SPg");
                }
                else if (streq(mappedphase, "SPn")) {
                    strcpy(AllowablePhases[npha++], "SP");
                    strcpy(AllowablePhases[npha++], "SPg");
                }
                else if (streq(mappedphase, "SPg")) {
                    strcpy(AllowablePhases[npha++], "SP");
                    strcpy(AllowablePhases[npha++], "SPn");
                }
            }
            if (streq(mappedphase, "PP")) {
                strcpy(AllowablePhases[npha++], "PnPn");
                strcpy(AllowablePhases[npha++], "PbPb");
                strcpy(AllowablePhases[npha++], "PgPg");
            }
            if (streq(mappedphase, "SS")) {
                strcpy(AllowablePhases[npha++], "SnSn");
                strcpy(AllowablePhases[npha++], "SbSb");
                strcpy(AllowablePhases[npha++], "SgSg");
            }
/*
 *
 *          loop through allowable phases and calculate residual
 *
 */
            for (j = 0; j < npha; j++) {
                strcpy(phase, AllowablePhases[j]);
/*
 *              only do matching phase types
 */
                if (islower(phase[0])) {
                    if (phase[1] == 'P' || phase[1] == 'w') ttype = 'P';
                    if (phase[1] == 'S') ttype = 'S';
                }
                else
                    ttype = toupper(phase[0]);
                if ((ttype == 'P' && stype) ||
                    ((ttype == 'S' || ttype == 'L') && ptype))
                    continue;
/*
 *              do not allow repeating phase names in a reading
 */
                isseen = 0;
                for (ii = rdindx->start; ii < k; ii++)
                    if (streq(p[ii].phase, phase)) isseen = 1;
                if (isseen) continue;
/*
 *              first-arriving P in a reading
 */
                if (p[k].firstP) {
                    if (!isFirstP(phase, mappedphase))
                        continue;
                }
/*
 *              first-arriving S in a reading
 */
                if (p[k].firstS) {
                    if (!isFirstS(phase, mappedphase))
                        continue;
                }
/*
 *              do not allow S(*) phases to be renamed to depth phases (s*)
 */
                if (isS && islower(phase[0]) && phase[1] == 'S') continue;
/*
 *              get travel time for candidate phase
 */
                strcpy(p[k].phase, phase);
                if (GetTravelTimePrediction(sp, &p[k], ec, tt_tables,
                                            localtt_tables, topo, 1, -1, 1))
                    continue;
/*
 *              keep record of pP ttime
 */
                if (streq(phase, "pP")) pPttime = p[k].ttime;
/*
 *              do not allow pwP if there was no water column correction
 */
                if (streq(phase, "pwP") &&
                    fabs(pPttime - p[k].ttime) < DEPSILON)
                    continue;
/*
 *              time residual
 */
                resid = p[k].time - sp->time - p[k].ttime;
                if (verbose > 4)
                    fprintf(logfp, "              %-8s %9.4f %9.4f\n",
                            p[k].phase, p[k].ttime, resid);
/*
 *              find phase with smallest residual
 */
                if (fabs(resid) < fabs(min_resid)) {
                    strcpy(candidate_phase, p[k].phase);
                    min_resid = resid;
                    ttime = p[k].ttime;
                    dtdd = p[k].dtdd;
                    d2tdd = p[k].d2tdd;
                    d2tdh = p[k].d2tdh;
                    rstterr = p[k].rsttTotalErr;
                    pickerr = p[k].rsttPickErr;
                }
            }
/*
 *          if no eligible phase found, set phase code to "".
 */
            if (fabs(min_resid) > bigres) {
                strcpy(p[k].phase, "");
                if (verbose > 3) {
                    fprintf(logfp, "            %9d %9d %-6s ",
                            p[k].rdid, p[k].phid, p[k].sta);
                    fprintf(logfp, "cannot identify phase!\n");
                }
            }

/*
 *          otherwise set to best fitting phase
 */
            else {
                if (verbose > 3) {
                    fprintf(logfp, "            %9d %9d %-6s %-8s -> ",
                            p[k].rdid, p[k].phid, p[k].sta, mappedphase);
                    fprintf(logfp, "%-8s %9.4f\n",
                            candidate_phase, min_resid);
                }
                strcpy(p[k].phase, candidate_phase);
                p[k].timeres = min_resid;
                p[k].ttime = ttime;
                p[k].dtdd = dtdd;
                p[k].d2tdd = d2tdd;
                p[k].d2tdh = d2tdh;
                p[k].rsttTotalErr = rstterr;
                p[k].rsttPickErr = pickerr;
                if (p[k].azim != NULLVAL) p[k].azimres = p[k].azim - p[k].seaz;
                if (p[k].slow != NULLVAL) p[k].slowres = p[k].slow - p[k].dtdd;
            }
        }
    }
}

/*
 *  Title:
 *     isFirstP
 *  Synopsis:
 *     Finds if a phase is in the list of allowable first-arriving P phases.
 *     Uses phase dependent information from <vmodel>_model.txt file.
 *        AllowableFirstP - list of phases to which reported first-arriving
 *                         P phases can be renamed
 *        OptionalFirstP - additional list of phases to which reported
 *                         first-arriving P phases can be renamed
 *  Input Arguments:
 *     phase       - phase
 *     mappedphase - reported phase
 *  Returns 1 if found, 0 otherwise
 *  Called by:
 *     PhaseIdentification
 */
static int isFirstP(char *phase, char *mappedphase)
{
    int j;
/*
 *  see if phase is in the list of allowable first-arriving P phases
 */
    for (j = 0; j < numFirstPphase; j++) {
        if (streq(phase, firstPphase[j]))
            return 1;
    }
/*
 *  not in the list of allowable first-arriving P phases;
 *  see if it is in the optional list
 */
    if (j == numFirstPphase && streq(mappedphase, phase)) {
        for (j = 0; j < numFirstPoptional; j++) {
            if (streq(phase, firstPoptional[j]))
                return 1;
        }
    }
    return 0;
}

/*
 *  Title:
 *     isFirstS
 *  Synopsis:
 *     Finds if a phase is in the list of allowable first-arriving S phases.
 *     Uses phase dependent information from <vmodel>_model.txt file.
 *        AllowableFirstS - list of phases to which reported first-arriving
 *                         S phases can be renamed
 *        OptionalFirstS - additional list of phases to which reported
 *                         first-arriving S phases can be renamed
 *  Input Arguments:
 *     phase       - phase
 *     mappedphase - reported phase
 *  Returns 1 if found, 0 otherwise
 *  Called by:
 *     PhaseIdentification
 */
static int isFirstS(char *phase, char *mappedphase)
{
    int j;
/*
 *  see if phase is in the list of allowable first-arriving S phases
 */
    for (j = 0; j < numFirstSphase; j++) {
        if (streq(phase, firstSphase[j]))
            return 1;
    }
/*
 *  not in the list of allowable first-arriving S phases;
 *  see if it is in the optional list
 */
    if (j == numFirstSphase && streq(mappedphase, phase)) {
        for (j = 0; j < numFirstSoptional; j++) {
            if (streq(phase, firstSoptional[j]))
                return 1;
        }
    }
    return 0;
}

/*
 *  Title:
 *     NumTimeDef
 *  Synopsis:
 *     Returns the number of time defining phases.
 *     If zero, there are only azimuth defining (infra) phases. In this case
 *     the origin time cannot be resolved and therefore it has to be fixed.
 *  Input Arguments:
 *     numPha - number of associated phases
 *     p      - array of phase structures
 *  Return:
 *     ntimedef - number of time defining phases
 *  Called by:
 *     PhaseIdentification
 */
int NumTimeDef(int numPha, PHAREC p[])
{
    int i, ntimedef = 0;
/*
 *  count the number of time defining phases
 */
    for (i = 0; i < numPha; i++)
        ntimedef += p[i].timedef;
     return ntimedef;
}

/*
 *  Title:
 *     GetPriorMeasurementError
 *  Synopsis:
 *     Sets timedef flag and a priori estimate of measurement error for a phase.
 *     Uses phase dependent information from <vmodel>_model.txt file.
 *        PhaseWeight - list of a priori measurement errors within specified
 *                delta ranges for IASPEI phases
 *     Sets the defining flags to true if a valid entry is found in the
 *        PhaseWeight table.
 *     Makes the phase non-defining if its residual is larger than a threshold,
 *        or it was explicitly set to non-defining by an analyst.
 *     Only phases with timedef = 1 are used in the location.
 *     For infrasound phases set the timedef flag to zero and use the azimuth
 *        instead.
 *  Input Arguments:
 *     pp    - pointer to phase structure
 *  Output Arguments:
 *     pp    - pointer to phase structure
 *  Called by:
 *     IdentifyPhases, ReIdentifyPhases
 */
static void GetPriorMeasurementError(PHAREC *pp)
{
    int j;
    double threshold;
/*
 *  set timedef and azimdef flags and get measurement errors
 */
    pp->timedef = pp->azimdef = pp->slowdef = 0;
    pp->deltim = pp->delaz = pp->delslo = 0;
    for (j = 0; j < numPhaseWeight; j++) {
        if (streq(pp->phase, PhaseWeight[j].phase)) {
            if (pp->delta >= PhaseWeight[j].delta1 &&
                pp->delta <  PhaseWeight[j].delta2) {
                if (verbose > 3)
                    fprintf(logfp, "GetPriorMeasurementError: %9d %9d %-6s %-8s\n",
                            pp->rdid, pp->phid, pp->sta, pp->phase);
                if (pp->time != NULLVAL) {
/*
 *                  a priori time measurement error
 */
                    pp->deltim = PhaseWeight[j].deltim;
                    if (pp->rsttPickErr > 0)
                        pp->deltim = pp->rsttPickErr;
                    if (pp->dupsigma > 0.)  pp->deltim += pp->dupsigma;
                    pp->timedef = 1;
/*
 *                  make phase non-defining if its residual is large
 */
                    threshold = SigmaThreshold * pp->deltim;
                    if (fabs(pp->timeres) > threshold)
                        pp->timedef = 0;
/*
 *                  make phase non-defining if editors made it non-defining
 */
                    if (pp->force_undef)
                        pp->timedef = 0;
                    if (verbose > 3)
                        fprintf(logfp, "    timeres=%7.2f timedef=%d, deltim=%f, pp->rsttPickErr=%f\n",
                                pp->timeres, pp->timedef, pp->deltim, pp->rsttPickErr);
                }
                if (pp->azim != NULLVAL) {
/*
 *                  a priori azimuth measurement error
 */
                    pp->delaz = PhaseWeight[j].delaz;
                    pp->azimdef = 1;
/*
 *                  make phase non-defining if its residual is large
 */
                    threshold = SigmaThreshold * pp->delaz;
                    if (fabs(pp->azimres) > threshold)
                        pp->azimdef = 0;
                    if (verbose > 3)
                        fprintf(logfp, "    azimres=%7.2f azimdef=%d, delaz=%f\n",
                                pp->azimres, pp->azimdef, pp->delaz);
                }
                if (pp->slow != NULLVAL) {
/*
 *                  a priori slowness measurement error
 */
                    pp->delslo = PhaseWeight[j].delslo;
                    pp->slowdef = 1;
/*
 *                  make phase non-defining if its residual is large
 */
                    threshold = SigmaThreshold * pp->delslo;
                    if (fabs(pp->slowres) > threshold)
                        pp->slowdef = 0;
                    if (verbose > 3)
                        fprintf(logfp, "    slowres=%7.2f slowdef=%d, delslo=%f\n",
                                pp->slowres, pp->slowdef, pp->delslo);
                }
                break;
            }
        }
    }
}

/*
 *  Title:
 *     DuplicatePhases
 *  Synopsis:
 *     Identifies and fixes duplicate arrivals.
 *     Once the phases are identified in each reading, it checks for duplicates
 *        reported by various agencies at the same site.
 *     To account for alternative station codes it uses the primary station
 *        code.
 *     Considers time-defining phases only.
 *     Arrival picks are considered duplicate if they are reported at the
 *        same site for the same event and if they arrival time is within
 *        0.1 seconds. For duplicates the arrival time is taken as the mean
 *        of the arrival time, and the phase id is forced to be the same.
 *     If accounting for correlated errors is turned off, duplicates are
 *        explicitly down-weighted. If correlated errors are accounted for,
 *        downweighting is not necessary as duplicates are simply projected
 *        to the null space.
 *     Collects indices of time-defining phases at a site and calls SameStation.
 *  Input Arguments:
 *     sp        - pointer to current solution
 *     p         - array of phase structures
 *     ec        - pointer to ellipticity correction coefficient structure
 *     tt_tables - pointer to travel-time tables
 *     localtt_tables - pointer to local travel-time tables, if any
 *     topo      - ETOPO bathymetry/elevation matrix
 *  Output Arguments:
 *     p         - array of phase structures
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     Locator
 *  Calls:
 *     SameStation, PrintPhases
 */
int DuplicatePhases(SOLREC *sp, PHAREC p[], EC_COEF *ec,
        TT_TABLE *tt_tables, TT_TABLE *localtt_tables, short int **topo)
{
    int samesta[MAXPHAINREADING];
    int i, j, k;
/*
 *  create an array samesta of defining phase indexes for each site (prista)
 */
    for (i = 0; i < sp->numPhase; i += j) {
        j = 0;                             /* Number of phases for this sta */
/*
 *      deal only with time-defining phases
 */
        if (!p[i].timedef) {
            i++;
            continue;
        }
        for (k = i; k < sp->numPhase; k++) {
/*
 *          recall that phases are ordered by delta, prista, rdid, time
 */
            if (strcmp(p[k].prista, p[i].prista))
                break;
            if (!p[k].timedef) {
                i++;
                continue;
            }
            samesta[j++] = k;
/*
 *          check not going past end of samesta array
 */
            if (j > MAXPHAINREADING) {
                fprintf(errfp, "DuplicatePhases: %s (%s): too many phases\n",
                        p[i].sta, p[i].prista);
                fprintf(logfp, "DuplicatePhases: %s (%s): too many phases\n",
                        p[i].sta, p[i].prista);
                return 1;
            }
        }
/*
 *      look for duplicates
 */
        if (j > 1)
            SameStation(samesta, j, sp, p, ec, tt_tables, localtt_tables, topo);
    }
    return 0;
}

/*
 *  Title:
 *     SameStation
 *  Synopsis:
 *     Collects indices of time-defining phases at a site arriving at the same
 *        time (within 0.1s tolerance) and calls SameArrivalTime.
 *     Downweights duplicates if accounting for correlated errors is turned off.
 *  Input Arguments:
 *     samesta   - array of defining phase indices for a single site
 *     n         - size of samesta array
 *     sp        - pointer to current solution
 *     p         - array of phase structures
 *     ec        - pointer to ellipticity correction coefficient structure
 *     tt_tables - pointer to travel-time tables
 *     localtt_tables - pointer to local travel-time tables, if any
 *     topo      - ETOPO bathymetry/elevation matrix
 *  Output Arguments:
 *     p         - array of phase structures
 *  Called by:
 *     DuplicatePhases
 *  Calls:
 *     SameArrivalTime
 */
static void SameStation(int samesta[], int n, SOLREC *sp, PHAREC p[],
        EC_COEF *ec, TT_TABLE *tt_tables, TT_TABLE *localtt_tables,
        short int **topo)
{
    int sametime[MAXPHAINREADING];
    int samepha[MAXPHAINREADING];
    int done[MAXPHAINREADING];
    int i, j, k;
    if (verbose > 3)
        fprintf(logfp, "        SameStation: %s %d phases\n",
                p[samesta[0]].sta, n);
    for (i = 0; i < n; i++) done[i] = 0;
/*
 *  get indices of phases with the same arrival times
 */
    for (i = 0; i < n; i++) {
        if (done[i])
            continue;
        j = 0;                           /* number of phases with this time */
        for (k = i; k < n; k++) {
            if (fabs(p[samesta[k]].time - p[samesta[i]].time) < SAMETIME_TOL) {
                sametime[j++] = samesta[k];
                done[k] = 1;
            }
        }
        if (j > 1)
/*
 *          deal with the duplicates
 */
            SameArrivalTime(sametime, j, sp, p, ec, tt_tables, localtt_tables,
                            topo);
    }
/*
 *  if correlated errors are to be accounted for, we are done here
 */
    if (DoCorrelatedErrors)
       return;

    for (i = 0; i < n; i++) done[i] = 0;
/*
 *  get duplicates with the same phase code
 */
    for (i = 0; i < n; i++) {
        if (done[i])
            continue;
        j = 0;                    /* number of phases with this phase code. */
        for (k = i; k < n; k++) {
            if (streq(p[samesta[k]].phase, p[samesta[i]].phase)) {
                samepha[j++] = samesta[k];
                done[k] = 1;
            }
        }
/*
 *      downweight duplicates
 */
        for (k = 0; k < j; k++) {
            p[samepha[k]].dupsigma = p[samepha[k]].deltim - 1. / (double)j;
            if (verbose > 2)
                fprintf(logfp, "        duplicates: %d %-6s %-8s\n",
                        samepha[k], p[samepha[k]].sta, p[samepha[k]].phase);
        }
    }
}

/*
 *  Title:
 *     SameArrivalTime
 *  Synopsis:
 *     Sets the arrival time of duplicates to the mean reported arrival time,
 *        and if there are more than one phase names, sets the phase id of all
 *        duplicates to the one with the smallest residual.
 *  Input Arguments:
 *     sametime  - array of phase indexes of duplicate arrivals at a site
 *     n         - size of sametime array
 *     sp        - pointer to current solution
 *     p         - array of phase structures
 *     ec        - pointer to ellipticity correction coefficient structure
 *     tt_tables - pointer to travel-time tables
 *     localtt_tables - pointer to local travel-time tables, if any
 *     topo      - ETOPO bathymetry/elevation matrix
 *  Output Arguments:
 *     p         - array of phase structures
 *  Called by:
 *     SameStation
 *  Calls:
 *     GetTravelTimePrediction
 */
static void SameArrivalTime(int sametime[], int n, SOLREC *sp, PHAREC p[],
        EC_COEF *ec, TT_TABLE *tt_tables, TT_TABLE *localtt_tables,
        short int **topo)
{
    int match, number_of_codes, min_resid_index, i, j;
    int phacode[MAXPHAINREADING];
    double meantime, resid,  min_resid;
    char temp_phase[PHALEN];
    if (verbose > 3)
        fprintf(logfp, "        SameArrivalTime: %s %f %d phases\n",
                p[sametime[0]].sta, p[sametime[0]].time, n);
/*
 *  get all the different phase codes given for this time
 */
    phacode[0] = sametime[0];
    number_of_codes = 1;
    meantime = p[sametime[0]].time;
    for (i = 1; i < n; i++) {
        meantime += p[sametime[i]].time;
        match = 0;
        for (j = 0; j < number_of_codes; j++) {
            if (streq(p[sametime[i]].phase, p[phacode[j]].phase)) {
                match = 1;
                break;
            }
        }
        if (!match)
            phacode[number_of_codes++] = sametime[i];
    }
    meantime /= (double)n;
    if (verbose > 3)
        fprintf (logfp, "        %d different phases\n", number_of_codes);
/*
 *  set arrival time to mean of reported arrival times
 */
    for (i = 0; i < n; i++) {
        p[sametime[i]].time = meantime;
        if (i) p[sametime[i]].duplicate = 1;
    }
/*
 *  if all the duplicates have the same phase code, we're done
 */
    if (number_of_codes == 1)
        return;
/*
 *  if not, loop over phase codes to find smallest residual
 */
    min_resid = (double)NULLVAL;
    min_resid_index = phacode[0];
    for (i = 0; i < number_of_codes; i++) {
/*
 *      if it is an initial phase in a reading, stick to it
 */
        if (p[phacode[i]].init) {
            min_resid_index = phacode[i];
            break;
        }
        if (GetTravelTimePrediction(sp, &p[phacode[i]], ec, tt_tables,
                                    localtt_tables, topo, 0, -1, 0))
            continue;
        resid = fabs(p[phacode[i]].time - sp->time - p[phacode[i]].ttime);
        if (resid < min_resid) {
            min_resid = resid;
            min_resid_index = phacode[i];
        }
        if (verbose > 3)
            fprintf(logfp, "            %6s %f %f\n",
                    p[phacode[i]].phase, p[phacode[i]].ttime, resid);
    }
    if (verbose > 3)
        fprintf(logfp, "            SameArrivalTime: set to %s\n",
                p[min_resid_index].phase);
/*
 *  set phase codes to the one with the smallest residual
 */
    strcpy(temp_phase, p[min_resid_index].phase);
    for (i = 0; i < n; i++)
        strcpy(p[sametime[i]].phase, temp_phase);
    return;
}


/*
 *  Title:
 *     IdentifyPFAKE
 *  Synopsis:
 *     PFAKE was used by the NEIC to associate a fake phase name to amplitude
 *     readings. PFAKE is not used in locations, but residuals are useful for
 *     the editors to decide whether the reading is correctly associated to
 *     an event.
 *     Uses phase dependent information from <vmodel>_model.txt file.
 *        AllowableFirstP - list of phases to which reported first-arriving
 *                         P phases can be renamed
 *  Input Arguments:
 *     sp        - pointer to current solution
 *     p         - array of phase structures
 *     ec        - pointer to ellipticity correction coefficient structure
 *     tt_tables - pointer to travel-time tables
 *     localtt_tables - pointer to local travel-time tables, if any
 *     topo      - ETOPO bathymetry/elevation matrix
 *  Output Arguments:
 *     p         - array of phase structures
 *  Called by:
 *     Locator, ResidualsForFixedHypocenter
 *  Calls:
 *     GetTravelTimePrediction
 */
void IdentifyPFAKE(SOLREC *sp, PHAREC p[], EC_COEF *ec, TT_TABLE *tt_tables,
        TT_TABLE *localtt_tables, short int **topo)
{
    int i, j;
    double resid, bigres = 100., min_resid, ttime = NULLVAL;
    char candidate_phase[PHALEN];
/*
 *  loop over all phases for this event.
 */
    for (i = 0; i < sp->numPhase; i++) {
/*
 *      only interested in PFAKE phases here
 */
        if (streq(p[i].ReportedPhase, "PFAKE")) {
/*
 *          get residual w.r.t. first-arriving P
 */
            min_resid = bigres + 1.;
            for (j = 0; j < numFirstPphase; j++) {
                strcpy(p[i].phase, firstPphase[j]);
                if (GetTravelTimePrediction(sp, &p[i], ec, tt_tables,
                                            localtt_tables, topo, 0, -1, 0))
                    continue;
/*
 *              residual
 */
                resid = p[i].time - sp->time - p[i].ttime;
/*
 *              find phase with smallest residual
 */
                if (fabs(resid) < fabs(min_resid)) {
                    strcpy(candidate_phase, p[i].phase);
                    min_resid = resid;
                    ttime = p[i].ttime;
                }
            }
            if (fabs(min_resid) > bigres) {
/*
 *              if no eligible phase found, set phase code to "".
 */
                strcpy(p[i].phase, "");
                p[i].ttime = NULLVAL;
                p[i].timeres = NULLVAL;
            }
            else {
/*
 *              otherwise set to best fitting phase
 */
                strcpy(p[i].phase, candidate_phase);
                p[i].timeres = min_resid;
                p[i].ttime = ttime;
            }
        }
    }
}

/*
 *  Title:
 *     RemovePFAKE
 *  Synopsis:
 *     Remove phase code for temporarily renamed PFAKE phases.
 *  Input Arguments:
 *     sp - pointer to current solution
 *     p  - array of phase structures
 *  Output Arguments:
 *     p         - array of phase structures
 *  Called by:
 *     Locator, ResidualsForFixedHypocenter
 */
void RemovePFAKE(SOLREC *sp, PHAREC p[])
{
    int i;
    for (i = 0; i < sp->numPhase; i++) {
        if (streq(p[i].ReportedPhase, "PFAKE"))
            strcpy(p[i].phase, "");
    }
}

/*
 *  Title:
 *     ResidualsForReportedPhases
 *  Synopsis:
 *     Tries to get a residual for a phase that was set to null due to a
 *     large residual. Uses the reported phase name this time.
 *     Uses phase dependent information from <vmodel>_model.txt file.
 *        PhaseMap - list of possible reported phases with their
 *                    corresponding IASPEI phase id.
 *     Only used once the locator is finished. It is intended to give a
 *     hint to the analyst about the nature of the outlier.
 *  Input Arguments:
 *     sp        - pointer to current solution
 *     pp        - pointer to a phase record
 *     ec        - pointer to ellipticity correction coefficient structure
 *     tt_tables - pointer to travel-time tables
 *     localtt_tables - pointer to local travel-time tables, if any
 *     topo      - ETOPO bathymetry/elevation matrix
 *  Output Arguments:
 *     pp        - pointer to a phase record
 *  Called by:
 *     GetTTResidual
 *  Calls:
 *     GetTravelTimePrediction
 */
void ResidualsForReportedPhases(SOLREC *sp, PHAREC *pp, EC_COEF *ec,
        TT_TABLE *tt_tables, TT_TABLE *localtt_tables, short int **topo)
{
    int j;
/*
 *  try reported phase id
 */
    if (pp->ReportedPhase[0]) {
/*
 *      map reported phase name to IASPEI standard
 */
        strcpy(pp->phase, pp->ReportedPhase);
        for (j = 0; j < numPhaseMap; j++) {
            if (streq(pp->ReportedPhase, PhaseMap[j].ReportedPhase)) {
                strcpy(pp->phase, PhaseMap[j].phase);
                break;
            }
        }
/*
 *      try to get a residual
 */
        if (GetTravelTimePrediction(sp, pp, ec, tt_tables, localtt_tables,
                                    topo, 0, -1, 0)) {
            strcpy(pp->phase, "");
            pp->ttime = pp->timeres = pp->slowres = NULLVAL;
        }
        else {
            pp->timeres = pp->time - sp->time - pp->ttime;
            if (pp->slow != NULLVAL)
                pp->slowres = pp->slow - pp->dtdd;
        }
        if (pp->azim != NULLVAL)
            pp->azimres = pp->azim - pp->seaz;
    }
    else {
        strcpy(pp->phase, "");
        pp->ttime = pp->timeres = NULLVAL;
    }
}

/*  EOF  */
