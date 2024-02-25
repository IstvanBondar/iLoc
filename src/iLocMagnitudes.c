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
extern int verbose;   /* verbose level */
extern FILE *logfp;
extern FILE *errfp;
extern int errorcode;
extern int numAgencies;
extern char agencies[MAXBUF][AGLEN];
extern char PhaseWithoutResidual[MAXNUMPHA][PHALEN];      /* from model file */
extern int PhaseWithoutResidualNum;       /* number of 'non-residual' phases */
extern double MagMaxTimeResidual;  /* max allowable time residual for stamag */
/*
 * Functions:
 *    NetworkMagnitudes
 *    ReadMagnitudeQ
 */

/*
 * Local functions
 *    GetNetmag
 *    GetStationmb
 *    GetStationMS
 *    GetStationML
 *    GetStationmB
 *    GetMagnitudeQ
 *    StationMagnitudeCompare
 *    SortReadingMagnitudes
 */
static int GetNetmag(SOLREC *sp, READING *rdindx, PHAREC p[], STAMAG *stamag,
                  STAMAG *rdmag, MSZH *mszh, int isQ, MAGQ *magQ,
                  int mtypeid, int k);
static double GetStationmb(int start, int n, PHAREC p[], int mtypeid,
                          double depth, int isQ, MAGQ *magQ);
static double GetStationmB(int start, int n, PHAREC p[], int mtypeid,
                          double depth, int isQ, MAGQ *magQ);
static double GetStationMS(int start, int n, PHAREC p[], int mtypeid, MSZH *mszh);
static double GetStationML(int start, int n, PHAREC p[], int mtypeid);
static double GetMagnitudeQ(double delta, double depth, int isQ, MAGQ *magQ);
static int StationMagnitudeCompare(const void *smag1, const void *smag2);
static int SortReadingMagnitudes(int nass, STAMAG *rdmag);

/*
 *  Title:
 *     NetworkMagnitudes
 *  Synopsis:
 *     Calculates network Ms and mb.
 *  Input Arguments:
 *     sp     - pointer to current solution
 *     rdindx - array of reading structures
 *     p[]    - array of phase structures
 *     stamag - array of station magnitude structures
 *     rdmag  - array of reading magnitude structures
 *     mszh   - array of MS vertical/horizontal magnitude structures
 *     ismbQ  - use mb magnitude attenuation table? (0/1)
 *     mbQ    - pointer to mb Q(d,h) table
 *  Return:
 *     0/1 on success/error.
 *  Called by:
 *     Locator
 *  Calls:
 *     GetNetmag
 */
int NetworkMagnitudes(SOLREC *sp, READING *rdindx, PHAREC p[], STAMAG **stamag,
                STAMAG **rdmag, MSZH *mszh, int ismbQ, MAGQ *mbQ)
{
    extern double MSMaxDepth;                /* From config file */
    extern int CalculateML;
    extern int CalculatemB;
    int k, mtypeid;
    sp->nnetmag = sp->nstamag = 0;
    for (k = 0; k < MAXMAG; k++) {
        sp->mag[k].magnitude = NULLVAL;
        sp->mag[k].uncertainty = 0.;
        sp->mag[k].numDefiningMagnitude = 0;
        sp->mag[k].numAssociatedMagnitude = 0;
        sp->mag[k].numMagnitudeAgency = 0;
        mtypeid = k + 1;
        sp->mag[k].mtypeid = mtypeid;
        if (k == 0)
            strcpy(sp->mag[k].magtype, "mb");
        else if (k == 1) {
            if (sp->depth > MSMaxDepth)
                continue;
            strcpy(sp->mag[k].magtype, "MS");
        }
        else if (k == 2) {
            if (!CalculateML)
                continue;
            strcpy(sp->mag[k].magtype, "ML");
        }
        else if (k == 3) {
            if (!CalculatemB)
                continue;
            strcpy(sp->mag[k].magtype, "mB");
        }
        GetNetmag(sp, rdindx, p, stamag[k], rdmag[k], mszh, ismbQ, mbQ,
                  mtypeid, k);
        if (sp->mag[k].magnitude != NULLVAL) {
            if (verbose)
                fprintf(logfp, "    %s=%.2f stderr=%.2f nsta=%d\n",
                        sp->mag[k].magtype, sp->mag[k].magnitude,
                        sp->mag[k].uncertainty, sp->mag[k].numDefiningMagnitude);
        }
        else
            fprintf(logfp, "    No data for %s\n", sp->mag[k].magtype);
    }
    return 0;
}

/*
 *  Title:
 *     GetNetmag
 *  Synopsis:
 *     Calculates network magnitude.
 *     At least three station magnitudes are required for a network magnitude.
 *     The network magnitude (mb or Ms) is defined as the median of the
 *         station magnitudes.
 *     The network magnitude uncertainty is defined as the SMAD of the
 *         alpha-trimmed station magnitudes.
 *     The station magnitude is defined as the median of reading magnitudes
 *         for a station.
 *     The reading magnitude is defined as the magnitude computed from the
 *         maximal log(A/T) in a reading.
 *  Input Arguments:
 *     sp     - pointer to current solution
 *     rdindx - array of reading structures
 *     p[]    - array of phase structures
 *     stamag - array of station magnitude structures
 *     rdmag  - array of reading magnitude structures
 *     mszh   - array of MS vertical/horizontal magnitude structures
 *     isQ    - use magnitude attenuation table? (0/1)
 *     magQ   - pointer to MAGQ Q(d,h) table
 *     mtypeid - magnitude type id (1..4)
 *     k      - magnitude index
 *  Return:
 *     0/1 on success/error.
 *  Called by:
 *     NetworkMagnitudes
 *  Calls:
 *     GetStationmb, GetStationMS, StationMagnitudeCompare, Free
 */
static int GetNetmag(SOLREC *sp, READING *rdindx, PHAREC p[], STAMAG *stamag,
        STAMAG *rdmag, MSZH *mszh, int isQ, MAGQ *magQ, int mtypeid, int k)
{
    extern double MagnitudeRangeLimit;                  /* From config file */
    extern int MinNetmagSta;            /* min number of stamags for netmag */
    char prev_prista[STALEN];
    double *adev = (double *)NULL;
    double reading_mag = 0., median = 0., smad = 0.;
    double min_mag = 0., max_mag = 0.;
    int nass = 0, nsta = 0, nagent = 0;
    int ageindex[MAXBUF];
    int i, j, m, np;
/*
 *  loop over reading
 */
    for (i = 0; i < sp->nreading; i++) {
        m = rdindx[i].start;
        np = rdindx[i].start + rdindx[i].npha;
/*
 *      get reading magnitude
 */
        if (mtypeid == 1)
            reading_mag = GetStationmb(m, np, p, mtypeid, sp->depth, isQ, magQ);
        else if (mtypeid == 2)
            reading_mag = GetStationMS(m, np, p, mtypeid, &mszh[nass]);
        else if (mtypeid == 3)
            reading_mag = GetStationML(m, np, p, mtypeid);
        else if (mtypeid == 4)
            reading_mag = GetStationmB(m, np, p, mtypeid, sp->depth, isQ, magQ);
        else
            continue;
/*
 *      populate rdmag record
 */
        if (reading_mag > -999) {
            rdmag[nass].rdid = p[m].rdid;
            rdmag[nass].magdef = 0;
            rdmag[nass].mtypeid = mtypeid;
            strcpy(rdmag[nass].prista, p[m].prista);
            strcpy(rdmag[nass].agency, p[m].agency);
            strcpy(rdmag[nass].deploy, p[m].deploy);
            strcpy(rdmag[nass].sta, p[m].sta);
            strcpy(rdmag[nass].lcn, p[m].lcn);
            strcpy(rdmag[nass].magtype, sp->mag[k].magtype);
            rdmag[nass].magnitude = reading_mag;
            sp->mag[k].numAssociatedMagnitude++;
            nass++;
        }
    }
/*
 *  no amplitudes were reported
 */
    if (!nass)
        return 0;
/*
 *  agencies
 */
    for (i = 0; i < numAgencies; i++) ageindex[i] = 0;
    for (i = 0; i < nass; i++) {
        for (j = 0; j < numAgencies; j++)
            if (streq(rdmag[i].agency, agencies[j]))
                ageindex[j]++;
    }
/*
 *  sort reading magnitudes by station and magnitude
 */
    SortReadingMagnitudes(nass, rdmag);
/*
 *  define stamag as median of reading magnitudes for the same station
 */
    strcpy(prev_prista, rdmag[0].sta);
    j = 0;
    nsta = 0;
    for (i = 0; i < nass; i++) {
        if (strcmp(rdmag[i].sta, prev_prista) && j) {
            m = j / 2;
            if (j % 2) {
                median = rdmag[i-m-1].magnitude;
                rdmag[i-m-1].magdef = 1;
            }
            else {
                median = 0.5 * (rdmag[i-m-1].magnitude + rdmag[i-m].magnitude);
                rdmag[i-m-1].magdef = 1;
                rdmag[i-m].magdef = 1;
            }
            j = 0;
/*
 *          populate stamag record for previous sta
 */
            strcpy(stamag[nsta].deploy, rdmag[i-m-1].deploy);
            strcpy(stamag[nsta].sta, prev_prista);
            strcpy(stamag[nsta].lcn, rdmag[i-m-1].lcn);
            strcpy(stamag[nsta].agency, rdmag[i-m-1].agency);
            stamag[nsta].magnitude = median;
            stamag[nsta].magdef = 0;
            stamag[nsta].mtypeid = mtypeid;
            strcpy(stamag[nsta].magtype, sp->mag[k].magtype);
            sp->mag[k].numDefiningMagnitude++;
            nsta++;
        }
        strcpy(prev_prista, rdmag[i].sta);
        j++;
    }
/*
 *  last sta
 */
    if (j) {
        m = j / 2;
        if (j % 2) {
            median = rdmag[i-m-1].magnitude;
            rdmag[i-1-m].magdef = 1;
        }
        else {
            median = 0.5 * (rdmag[i-m-1].magnitude + rdmag[i-m].magnitude);
            rdmag[i-m-1].magdef = 1;
            rdmag[i-m].magdef = 1;
        }
        strcpy(stamag[nsta].deploy, rdmag[i-m-1].deploy);
        strcpy(stamag[nsta].sta, prev_prista);
        strcpy(stamag[nsta].lcn, rdmag[i-m-1].lcn);
        strcpy(stamag[nsta].agency, rdmag[i-m-1].agency);
        stamag[nsta].magnitude = median;
        stamag[nsta].magdef = 0;
        stamag[nsta].mtypeid = mtypeid;
        strcpy(stamag[nsta].magtype, sp->mag[k].magtype);
        sp->mag[k].numDefiningMagnitude++;
        nsta++;
    }
    sp->nstamag += sp->mag[k].numDefiningMagnitude;
/*
 *  insufficient number of station magnitudes?
 */
    if (nsta < MinNetmagSta) {
        if (verbose > 2)
            fprintf(logfp, "    GetNetmag: %s: insufficient number of stations %d\n",
                    sp->mag[k].magtype, nsta);
        return 0;
    }
/*
 *  network magnitude is calculated as an alpha-trimmed median; alpha = 20%
 */
    qsort(stamag, nsta, sizeof(STAMAG), StationMagnitudeCompare);
    i = nsta / 2;
    if (nsta % 2) median = stamag[i].magnitude;
    else          median = 0.5 * (stamag[i - 1].magnitude +
                                  stamag[i].magnitude);
/*
 *  alpha trim range
 */
    m = floor(0.2 * (double)nsta);
    min_mag = stamag[m].magnitude;
    max_mag = stamag[nsta - m - 1].magnitude;
/*
 *  allocate memory for adev
 */
    if ((adev = (double *)calloc(nsta, sizeof(double))) == NULL) {
        fprintf(logfp, "GetNetmag: cannot allocate memory\n");
        fprintf(errfp, "GetNetmag: cannot allocate memory\n");
        errorcode = 1;
        return 0;
    }
/*
 *  netmag uncertainty: smad of alpha-trimmed station magnitudes
 */
    for (j = 0, i = m; i < nsta - m; j++, i++) {
        adev[j] = fabs(stamag[i].magnitude - median);
        stamag[i].magdef = 1;
    }
    qsort(adev, j, sizeof(double), CompareDouble);
    m = j / 2;
    if (j % 2) smad = 1.4826 * adev[m];
    else       smad = 1.4826 * 0.5 * (adev[m - 1] + adev[m]);
    Free(adev);
/*
 *  report station magnitudes
 */
    if (verbose) {
       fprintf(logfp, "    %d %s station magnitudes\n", nsta, sp->mag[k].magtype);
       for (i = 0; i < nsta; i++)
           fprintf(logfp, "      %-6s stamag=%.3f magdef=%d\n",
                   stamag[i].sta, stamag[i].magnitude, stamag[i].magdef);
    }
/*
 *  number of reporting agencies
 */
    if (verbose)
        fprintf(logfp, "    agencies contributing to %s\n", sp->mag[k].magtype);
    for (nagent = 0, i = 0; i < numAgencies; i++) {
        if (ageindex[i]) {
            nagent++;
            if (verbose)
                fprintf(logfp, "      %3d %-17s %4d readings\n",
                        nagent, agencies[i], ageindex[i]);
        }
    }
    if (nagent == 0) nagent++;
/*
 *  network magnitude
 */
    sp->mag[k].magnitude = median;
    sp->mag[k].uncertainty = smad;
    sp->mag[k].numMagnitudeAgency = nagent;
    sp->nnetmag++;
/*
 *  Test for big differences in station magnitudes.
 */
    if ((max_mag - min_mag) > MagnitudeRangeLimit)
        fprintf(logfp, "WARNING: %s RANGE %.1f - %.1f\n",
                sp->mag[k].magtype, min_mag, max_mag);
    return 0;
}

/*
 *
 * StationMagnitudeCompare: compares two stamag records based on the magnitude
 *
 */
static int StationMagnitudeCompare(const void *smag1, const void *smag2)
{
    if (((STAMAG *)smag1)->magnitude < ((STAMAG *)smag2)->magnitude)
        return -1;
    if (((STAMAG *)smag1)->magnitude > ((STAMAG *)smag2)->magnitude)
        return 1;
    return 0;
}

/*
 *
 * SortReadingMagnitudes: sorts rdmag records by station and magnitude
 *
 */
static int SortReadingMagnitudes(int nass, STAMAG *rdmag)
{
    int i, j;
    STAMAG temp;
    for (i = 1; i < nass; i++) {
        for (j = i - 1; j > -1; j--) {
            if (rdmag[j].magnitude > rdmag[j+1].magnitude) {
                swap(rdmag[j], rdmag[j+1]);
            }
        }
    }
    for (i = 1; i < nass; i++) {
        for (j = i - 1; j > -1; j--) {
            if (strcmp(rdmag[j].sta, rdmag[j+1].sta) > 0) {
                swap(rdmag[j], rdmag[j+1]);
            }
        }
    }
    return 0;
}

/*
 *  Title:
 *     GetStationmb
 *  Synopsis:
 *     Calculates mb for a single reading.
 *     Finds the reported amplitude, period pair for which A/T is maximal.
 *         Amplitude mb = log(A/T) + Q(d,h)
 *         Reading   mb = log(max(A/T)) + Q(d,h)
 *     If no amplitude, period pairs are reported, use the reported logat values
 *         Amplitude mb = logat + Q(d,h)
 *         Reading   mb = max(logat) + Q(d,h)
 *  Input Arguments:
 *     start - starting index in a reading
 *     n     - number of phases in the reading
 *     p[]   - array of phase structures
 *     mtypeid - magnitude type id
 *     depth - source depth
 *     isQ   - use magnitude attenuation table? (0/1/2)
 *               0 - do not use
 *               1 - Gutenberg-Richter
 *               2 - Veith-Clawson or Murphy-Barker
 *     magQ  - pointer to MAGQ Q(d,h) table
 *  Return:
 *     reading_mb - mb for this reading
 *  Called by:
 *     GetNetmag
 *  Calls:
 *     GetMagnitudeQ
 */
static double GetStationmb(int start, int n, PHAREC p[], int mtypeid,
        double depth, int isQ, MAGQ *magQ)
{
    extern double mbMinDistDeg;                          /* From config file */
    extern double mbMaxDistDeg;                          /* From config file */
    extern double mbMinPeriod;                           /* From config file */
    extern double mbMaxPeriod;                           /* From config file */
    extern char MBPhase[MAXNUMPHA][PHALEN];              /* From model file  */
    extern int numMBPhase;                               /* From model file  */
    double logat = 0., amp = 0., apert = 0., delta = 0.;
    double reading_mb = -999., q = 0., maxat = 0.;
    int i, j, ind_amp = 0, ind_pha = 0, isIAmb = 0;
/*
 *
 *  calculate reading mb from max(A/T) in a reading
 *
 */
    ind_amp = ind_pha = -1;
    maxat = -999.;
    delta = p[start].delta;
    for (i = start; i < n; i++) {
/*
 *      Only need look at phases with amplitudes
 */
        if (p[i].numamps == 0)
            continue;
/*
 *      Ignore readings recorded outside the mb distance range
 */
        if (delta < mbMinDistDeg || delta > mbMaxDistDeg)
            continue;
/*
 *      Only consider phases contributing to mb
 */
        if (streq(p[i].phase, "AMB"))
            continue;
        for (j = 0; j < numMBPhase; j++)
            if (streq(p[i].phase, MBPhase[j]))
                break;
        if (j == numMBPhase)
            continue;
/*
 *      Only consider phases with acceptable time residuals (if any)
 */
        for (j = 0; j < PhaseWithoutResidualNum; j++)
            if (streq(p[i].phase, PhaseWithoutResidual[j]))
                break;
        if (j == PhaseWithoutResidualNum && fabs(p[i].timeres) > MagMaxTimeResidual)
            continue;
/*
 *      Loop over amplitudes
 */
        for (j = 0; j < p[i].numamps; j++) {
/*
 *          ignore amplitudes measured on horizontal components
 */
            if (p[i].a[j].comp == 'N' || p[i].a[j].comp == 'E')
                continue;
/*
 *          ignore amplitudes outside the period range
 */
            if (p[i].a[j].per < mbMinPeriod ||
                p[i].a[j].per > mbMaxPeriod)
                continue;
/*
 *          ignore null amplitudes
 */
            if (fabs(p[i].a[j].amp) < DEPSILON || p[i].a[j].amp == NULLVAL)
                continue;
/*
 *          ignore null periods
 */
            if (fabs(p[i].a[j].per) < DEPSILON || p[i].a[j].per == NULLVAL)
                continue;
/*
 *          ignore non-mb amplitude mesaurements (SeisComp3)
 */
            if (p[i].a[j].magtype[0] != '\0') {
                if (p[i].a[j].magtype[1] != 'b')
                    continue;
            }
/*
 *          for VC corrections amplitudes are measured as peak-to-peak
 */
            amp = (isQ == 2) ? 2. * p[i].a[j].amp : p[i].a[j].amp;
            apert = amp / p[i].a[j].per;
/*
 *          keep track of maximum amplitude
 */
            if (apert > maxat) {
                maxat = apert;
                ind_amp = j;
                ind_pha = i;
            }
/*
 *          amplitude magnitude
 *          mb = log(A/T) + Q(d,h)
 */
            logat = log10(apert);
            q = GetMagnitudeQ(delta, depth, isQ, magQ);
            p[i].a[j].magnitude = logat + q;
            p[i].a[j].ampdef = 0;
            if (p[i].a[j].magtype[0] == '\0')
                strcpy(p[i].a[j].magtype, "mb");
            p[i].a[j].mtypeid = mtypeid;
            if (verbose > 2) {
                fprintf(logfp, "          i=%2d j=%2d amp=%.2f ",
                        i, j, p[i].a[j].amp);
                fprintf(logfp, "per=%.2f logat%.2f magnitude: %.2f\n",
                        p[i].a[j].per, logat, p[i].a[j].magnitude);
            }
/*
 *          force IAmb to be the chosen one
 */
            if (streq(p[i].phase, "IAmb")) {
                ind_amp = j;
                ind_pha = i;
                isIAmb = 1;
                break;
            }
        }
        if (isIAmb)
            break;
    }
/*
 *  take the magnitude belonging to the maximal A/T as the
 *  reading magnitude
 *  mb = log(max(A/T)) + Q(d,h)
 */
    if (ind_pha > -1) {
        p[ind_pha].a[ind_amp].ampdef = 1;
        reading_mb = p[ind_pha].a[ind_amp].magnitude;
        if (verbose > 1)
            fprintf(logfp, "        rdid=%d %-6s depth=%.2f delta=%.2f mb=%.2f\n",
                    p[ind_pha].rdid, p[ind_pha].sta, depth, delta, reading_mb);
        return reading_mb;
    }
/*
 *
 *  if no amp/per pairs were reported, calculate reading mb from
 *      the maximal reported logat value
 *
 */
    ind_amp = ind_pha = -1;
    maxat = -999;
    for (i = start; i < n; i++) {
/*
 *      Only need look at phases with amplitudes.
 */
        if (p[i].numamps == 0)
            continue;
/*
 *      Only consider phases contributing to mb
 */
        if (streq(p[i].phase, "AMB"))
            continue;
        for (j = 0; j < numMBPhase; j++)
            if (streq(p[i].phase, MBPhase[j]))
                break;
        if (j == numMBPhase)
            continue;
/*
 *      Ignore readings recorded outside the mb distance range
 */
        if (delta < mbMinDistDeg || delta > mbMaxDistDeg)
            continue;
/*
 *      Loop over amplitudes
 */
        for (j = 0; j < p[i].numamps; j++) {
/*
 *          ignore non-mb amplitude mesaurements (SeisComp3)
 */
            if (p[i].a[j].magtype[0] != '\0') {
                if (p[i].a[j].magtype[1] != 'b')
                    continue;
            }
            if (p[i].a[j].logat != NULLVAL ) {
                logat = p[i].a[j].logat;
/*
 *              keep track of maximum logat
 */
                if (logat > maxat) {
                    maxat = logat;
                    ind_amp = j;
                    ind_pha = i;
                }
/*
 *              amplitude magnitude
 *              mb = logat + Q(d,h)
 */
                q = GetMagnitudeQ(delta, depth, isQ, magQ);
                p[i].a[j].magnitude = logat + q;
                p[i].a[j].ampdef = 0;
                if (p[i].a[j].magtype[0] == '\0')
                    strcpy(p[i].a[j].magtype, "mb");
                p[i].a[j].mtypeid = mtypeid;
                if (verbose > 2) {
                    fprintf(logfp, "          i=%2d j=%2d amp=%.2f ",
                        i, j, p[i].a[j].amp);
                    fprintf(logfp, "per=%.2f logat%.2f magnitude: %.2f\n",
                        p[i].a[j].per, logat, p[i].a[j].magnitude);
                }
            }
        }
    }
/*
 *  take the magnitude belonging to the maximal logat as the
 *  reading mb
 *  mb = max(logat) + Q(d,h)
 */
    if (ind_pha > -1) {
        p[ind_pha].a[ind_amp].ampdef = 1;
        reading_mb = p[ind_pha].a[ind_amp].magnitude;
        if (verbose > 1)
            fprintf(logfp, "        rdid=%d %-6s depth=%.2f delta=%.2f mb=%.2f\n",
                    p[ind_pha].rdid, p[ind_pha].sta, depth, delta, reading_mb);
        return reading_mb;
    }
    return -999.;
}

/*
 *  Title:
 *     GetStationmB
 *  Synopsis:
 *     Calculates mb for a single reading.
 *     Finds the reported amplitude, period pair for which A/T is maximal.
 *         Amplitude mb = log(A/2PI) + Q(d,h)
 *         Reading   mb = log(max(A/2PI)) + Q(d,h)
 *     If no amplitude, period pairs are reported, use the reported logat values
 *         Amplitude mb = logat + Q(d,h)
 *         Reading   mb = max(logat) + Q(d,h)
 *  Input Arguments:
 *     start - starting index in a reading
 *     n     - number of phases in the reading
 *     p[]   - array of phase structures
 *     mtypeid - magnitude type id
 *     depth - source depth
 *     isQ   - use magnitude attenuation table? (0/1/2)
 *               0 - do not use
 *               1 - Gutenberg-Richter
 *               2 - Veith-Clawson or Murphy-Barker
 *     magQ  - pointer to MAGQ Q(d,h) table
 *  Return:
 *     reading_mB - mB for this reading
 *  Called by:
 *     GetNetmag
 *  Calls:
 *     GetMagnitudeQ
 */
static double GetStationmB(int start, int n, PHAREC p[], int mtypeid,
        double depth, int isQ, MAGQ *magQ)
{
    extern double BBmBMinDistDeg;              /* From config file */
    extern double BBmBMaxDistDeg;              /* From config file */
    extern char MBPhase[MAXNUMPHA][PHALEN];    /* From model file  */
    extern int numMBPhase;                    /* From model file  */
    double logat = 0., amp = 0., apert = 0., delta = 0.;
    double reading_mB = -999., q = 0., maxat = 0.;
    int i, j, ind_amp = 0, ind_pha = 0, isAMB = 0;
/*
 *
 *  calculate reading mB from max(A/T) in a reading
 *
 */
    ind_amp = ind_pha = -1;
    maxat = -999.;
    delta = p[start].delta;
    for (i = start; i < n; i++) {
/*
 *      Only need look at phases with amplitudes
 */
        if (p[i].numamps == 0)
            continue;
/*
 *      Ignore readings recorded outside the mb distance range
 */
        if (delta < BBmBMinDistDeg || delta > BBmBMaxDistDeg)
            continue;
/*
 *      Only consider phases contributing to mB
 */
        if (streq(p[i].phase, "IAmb"))
            continue;
        for (j = 0; j < numMBPhase; j++)
            if (streq(p[i].phase, MBPhase[j]))
                break;
        if (j == numMBPhase)
            continue;
/*
 *      Only consider phases with acceptable time residuals (if any)
 */
        for (j = 0; j < PhaseWithoutResidualNum; j++)
            if (streq(p[i].phase, PhaseWithoutResidual[j]))
                break;
        if (j == PhaseWithoutResidualNum && fabs(p[i].timeres) > MagMaxTimeResidual)
            continue;
/*
 *      Loop over amplitudes
 */
        for (j = 0; j < p[i].numamps; j++) {
/*
 *          ignore amplitudes measured on horizontal components
 */
            if (p[i].a[j].comp == 'N' || p[i].a[j].comp == 'E')
                continue;
/*
 *          ignore null amplitudes
 */
            if (fabs(p[i].a[j].amp) < DEPSILON || p[i].a[j].amp == NULLVAL)
                continue;
/*
 *          ignore non-mB amplitude mesaurements (SeisComp3)
 */
            if (p[i].a[j].magtype[0] != '\0') {
                if (p[i].a[j].magtype[1] != 'B')
                    continue;
            }
/*
 *          for VC corrections amplitudes are measured as peak-to-peak
 */
            amp = (isQ == 2) ? 2. * p[i].a[j].amp : p[i].a[j].amp;
/*
 *          SeisComp3 fix: amplitudes are in nanometer
 *
 *          for CAD it is commented out!
 */
            if (streq(p[i].phase, "AMB") || strchr(p[i].rep, '@'))
                amp /= 1000.;
            apert = amp / TWOPI;
/*
 *          keep track of maximum amplitude
 */
            if (apert > maxat) {
                maxat = apert;
                ind_amp = j;
                ind_pha = i;
            }
/*
 *          amplitude magnitude
 *          mb = log(A/T) + Q(d,h)
 */
            logat = log10(apert);
            q = GetMagnitudeQ(delta, depth, isQ, magQ);
            p[i].a[j].magnitude = logat + q;
            p[i].a[j].ampdef = 0;
            if (p[i].a[j].magtype[0] == '\0')
                strcpy(p[i].a[j].magtype, "mB");
            p[i].a[j].mtypeid = mtypeid;
            if (verbose > 2) {
                fprintf(logfp, "          i=%2d j=%2d amp=%.2f ",
                        i, j, p[i].a[j].amp);
                fprintf(logfp, "per=%.2f logat%.2f magnitude: %.2f\n",
                        p[i].a[j].per, logat, p[i].a[j].magnitude);
            }
/*
 *          force AMB to be the chosen one
 */
            if (streq(p[i].phase, "AMB")) {
                ind_amp = j;
                ind_pha = i;
                isAMB = 1;
                break;
            }
        }
        if (isAMB)
            break;
    }
/*
 *  take the magnitude belonging to the maximal A/T as the reading magnitude
 *  mb = log(max(A/T)) + Q(d,h)
 */
    if (ind_pha > -1) {
        p[ind_pha].a[ind_amp].ampdef = 1;
        reading_mB = p[ind_pha].a[ind_amp].magnitude;
        if (verbose > 1)
            fprintf(logfp, "        rdid=%d %-6s depth=%.2f delta=%.2f mB=%.2f\n",
                    p[ind_pha].rdid, p[ind_pha].sta, depth, delta, reading_mB);
        return reading_mB;
    }
    return -999.;
}

/*
 *  Title:
 *     GetMagnitudeQ
 *  Synopsis:
 *     Calculates magnitude attenuation Q(d,h) value.
 *     Gutenberg, B. and C.F. Richter, 1956,
 *         Magnitude and energy of earthquakes,
 *         Ann. Geof., 9, 1-5.
 *     Veith, K.F. and G.E. Clawson, 1972,
 *         Magnitude from short-period P-wave data,
 *         Bull. Seism. Soc. Am., 62, 2, 435-452.
 *     Murphy, J.R. and B.W. Barker, 2003,
 *         Revised B(d,h) correction factors for use
 *         in estimation of mb magnitudes,
 *         Bull. Seism. Soc. Am., 93, 1746-1764.
 *     For the Gutenberg-Richter tables amplitudes are measured in micrometers
 *         Q(d,h) = QGR(d,h) - 3
 *     For the Veith-Clawson and Murphy and Barker tables amplitudes are
 *     measured in nanometers
 *         Q(d,h) = QVC(d,h)
 *  Input Arguments:
 *     delta - epicentral distance
 *     depth - source depth
 *     isQ   - use magnitude attenuation table? (0/1/2)
 *             0 - do not use
 *             1 - Gutenberg-Richter
 *             2 - Veith-Clawson
 *     magQ  - pointer to MAGQ Q(d,h) table
 *  Return:
 *     q     - Q(d,h)
 *  Called by:
 *     GetStationmb
 *  Calls:
 *     BilinearInterpolation
 */
static double GetMagnitudeQ(double delta, double depth, int isQ, MAGQ *magQ)
{
    double q = 0.;
/*
 *  check for validity
 */
    if (!isQ ||
        depth < magQ->mindepth || depth > magQ->maxdepth ||
        delta < magQ->mindist  || delta > magQ->maxdist)
        return q;
/*
 *  bilinear interpolation on Q(d,h)
 */
    else
        q = BilinearInterpolation(delta, depth, magQ->numDistanceSamples,
                        magQ->numDepthSamples, magQ->deltas, magQ->depths, magQ->q);
/*
 *  Gutenberg-Richter Q(d,h) is valid for amplitudes in micrometer
 */
    if (isQ == 1) q -= 3;
    return q;
}

/*
 *  Title:
 *     GetStationMS
 *  Synopsis:
 *     Calculates MS for a single reading.
 *     Vanek, J., A. Zatopek, V. Karnik, N.V. Kondorskaya, Y.V. Riznichenko,
 *         Y.F. Savarensky, S.L. Solovev and N.V. Shebalin, 1962,
 *         Standardization of magnitude scales,
 *         Bull. (Izvest.) Acad. Sci. USSR Geophys. Ser., 2, 108-111.
 *     Amplitude MS = log(A/T) + 1.66 * log(d) + 0.3 (Prague formula)
 *     First, find max(A/T) for Z component and calculate vertical MS
 *         MS_Z = log(max(A_z/T_z)) + 1.66 * log(d) + 0.3
 *     Second, find max(A/T) for E and N components within +/- MSPeriodRange
 *         of Z period and calculate horizontal MS
 *         max(A/T)_h = sqrt(max(A_e/T_e)^2 + max(A_n/T_n)^2)
 *         max(A/T)_h = sqrt(2 * max(A_e/T_e)^2)      if N does not exist
 *         max(A/T)_h = sqrt(2 * max(A_n/T_n)^2)      if E does not exist
 *         MS_H = log(max(A/T)_h) + 1.66 * log(d) + 0.3
 *     Reading MS
 *         MS = (MS_H + MS_Z) / 2  if MS_Z and MS_H exist
 *         MS = MS_H               if MS_Z does not exist
 *         MS = MS_Z               if MS_H does not exist
 *  Input Arguments:
 *     start - starting index in a reading
 *     n     - number of phases in the reading
 *     p[]   - array of phase structures
 *     mtypeid - magnitude type id
 *     mszh  - pointer to MS vertical/horizontal magnitude structure
 *  Return:
 *     reading_ms - MS for this reading
 *  Called by:
 *     GetNetmag
 */
static double GetStationMS(int start, int n, PHAREC p[], int mtypeid,
                          MSZH *mszh)
{
    extern double MSMinDistDeg;                          /* From config file */
    extern double MSMaxDistDeg;                          /* From config file */
    extern double MSMinPeriod;                           /* From config file */
    extern double MSMaxPeriod;                           /* From config file */
    extern char MSPhase[MAXNUMPHA][PHALEN];               /* From model file */
    extern int numMSPhase;                                /* From model file */
    extern double MSPeriodRange;   /* MSH period tolerance around MSZ period */

    double reading_ms = -999.;
    double apert = 0., apertn = 0., maxatz = 0., maxate = 0., maxatn = 0.;
    double ms_h = 0., ms_z = 0., logat = 0., delta = 0.;
    double minper = MSMinPeriod, maxper = MSMaxPeriod;
    int ind_phaz = 0, ind_phae = 0, ind_phan = 0;
    int ind_ampz = 0, ind_ampe = 0, ind_ampn = 0;
    int i, j;
/*
 *  Calculate reading MS from max(A/T)
 */
    ind_phaz = ind_phae = ind_phan = -1;
    ind_ampz = ind_ampe = ind_ampn = -1;
    maxatz = maxate = maxatn = -999.;
    ms_z = ms_h = NULLVAL;
    delta = p[start].delta;
/*
 *  First, find max(A/T) for Z component
 */
    for (i = start; i < n; i++) {
/*
 *      Only need look at phases with amplitudes
 */
        if (p[i].numamps == 0)
            continue;
/*
 *      Only consider phases contributing to MS
 */
        for (j = 0; j < numMSPhase; j++)
            if (streq(p[i].phase, MSPhase[j]))
                break;
        if (j == numMSPhase)
            continue;
/*
 *      ignore observations outside the MS distance range
 */
        if (delta < MSMinDistDeg || delta > MSMaxDistDeg)
            continue;
/*
 *      Loop over amplitudes
 */
        for (j = 0; j < p[i].numamps; j++) {
/*
 *          ignore amplitudes outside the period range
 */
            if (p[i].a[j].per < MSMinPeriod ||
                p[i].a[j].per > MSMaxPeriod)
                continue;
/*
 *          ignore null amplitudes
 */
            if (fabs(p[i].a[j].amp) < DEPSILON || p[i].a[j].amp == NULLVAL)
                continue;
/*
 *          ignore null periods
 */
            if (fabs(p[i].a[j].per) < DEPSILON || p[i].a[j].per == NULLVAL)
                continue;
/*
 *          ignore non-MS amplitude measurements (SeisComp3)
 */
            if (p[i].a[j].magtype[0] != '\0') {
                if (tolower(p[i].a[j].magtype[1]) != 's')
                    continue;
                if (streq(p[i].a[j].magtype, "MS(BB)"))
                    continue;
            }
/*
 *          skip amplitudes measured on horizontal components
 */
            if (p[i].a[j].comp == 'N' || p[i].a[j].comp == 'E')
                continue;
/*
 *          keep track of maximum(A/T)
 */
            apert = p[i].a[j].amp / p[i].a[j].per;
            if (apert > maxatz) {
                maxatz = apert;
                ind_ampz = j;
                ind_phaz = i;
            }
/*
 *          amplitude magnitude using Prague formula
 */
//            if (p[i].phase[0] == 'I')
//                p[i].a[j].amp *= 1000.;
            logat = log10(p[i].a[j].amp / p[i].a[j].per);
            p[i].a[j].magnitude = logat + 1.66 * log10(delta) + 0.3;
            p[i].a[j].ampdef = 0;
            if (p[i].a[j].magtype[0] == '\0')
                strcpy(p[i].a[j].magtype, "MS");
            p[i].a[j].mtypeid = mtypeid;
            if (verbose > 2) {
                fprintf(logfp, "          i=%2d j=%2d comp=%c amp=%.2f ",
                        i, j, p[i].a[j].comp, p[i].a[j].amp);
                fprintf(logfp, "per=%.2f logat%.2f magnitude: %.2f\n",
                        p[i].a[j].per, logat, p[i].a[j].magnitude);
            }
        }
    }
/*
 *  MS_Z
 */
    if (ind_phaz > -1) {
        minper = p[ind_phaz].a[ind_ampz].per - MSPeriodRange;
        maxper = p[ind_phaz].a[ind_ampz].per + MSPeriodRange;
        p[ind_phaz].a[ind_ampz].ampdef = 1;
        ms_z = p[ind_phaz].a[ind_ampz].magnitude;
        if (verbose > 1)
            fprintf(logfp, "        rdid=%d %-6s delta=%.2f MS_z=%.2f\n",
                    p[ind_phaz].rdid, p[ind_phaz].sta, delta, ms_z);
    }
/*
 *
 *  Second, find max(A/T) for E and N components
 *
 */
    for (i = start; i < n; i++) {
/*
 *      Only need look at phases with amplitudes
 */
        if (p[i].numamps == 0)
            continue;
/*
 *      Only consider phases contributing to MS
 */
        for (j = 0; j < numMSPhase; j++)
            if (streq(p[i].phase, MSPhase[j]))
                break;
        if (j == numMSPhase)
            continue;
/*
 *      ignore observations outside the MS distance range
 */
        if (delta < MSMinDistDeg || delta > MSMaxDistDeg)
            continue;
/*
 *      Loop over amplitudes
 */
        for (j = 0; j < p[i].numamps; j++) {
/*
 *          ignore amplitudes outside the period range
 */
            if (p[i].a[j].per < minper || p[i].a[j].per > maxper)
                continue;
/*
 *          ignore null amplitudes
 */
            if (fabs(p[i].a[j].amp) < DEPSILON || p[i].a[j].amp == NULLVAL)
                continue;
/*
 *          ignore non-MS amplitude measurements (SeisComp3)
 */
            if (p[i].a[j].magtype[0] != '\0') {
                if (tolower(p[i].a[j].magtype[1]) != 's')
                    continue;
                if (streq(p[i].a[j].magtype, "MS(BB)"))
                    continue;
            }
/*
 *          ignore non-horizontal components
 */
            if (p[i].a[j].comp != 'N' && p[i].a[j].comp != 'E')
                continue;
/*
 *          keep track of maximum(A/T)
 */
            if (p[i].a[j].comp == 'E') {
                apert = p[i].a[j].amp / p[i].a[j].per;
                if (apert > maxate) {
                    maxate = apert;
                    ind_ampe = j;
                    ind_phae = i;
                }
            }
            if (p[i].a[j].comp == 'N') {
                apertn = p[i].a[j].amp / p[i].a[j].per;
                if (apertn > maxatn) {
                    maxatn = apertn;
                    ind_ampn = j;
                    ind_phan = i;
                }
            }
/*
 *          amplitude magnitude using Prague formula
 */
//            if (p[i].phase[0] == 'I')
//                p[i].a[j].amp *= 1000.;
            logat = log10(p[i].a[j].amp / p[i].a[j].per);
            p[i].a[j].magnitude = logat + 1.66 * log10(delta) + 0.3;
            p[i].a[j].ampdef = 0;
            if (p[i].a[j].magtype[0] == '\0')
                strcpy(p[i].a[j].magtype, "MS");
            p[i].a[j].mtypeid = mtypeid;
            if (verbose > 2) {
                fprintf(logfp, "          i=%2d j=%2d comp=%c amp=%.2f ",
                        i, j, p[i].a[j].comp, p[i].a[j].amp);
                fprintf(logfp, "per=%.2f logat%.2f magnitude: %.2f\n",
                        p[i].a[j].per, logat, p[i].a[j].magnitude);
            }
        }
    }
/*
 *  MS_H
 */
    if (ind_phan > -1 && ind_phae > -1) {
/*
 *      ms_h from E and N components
 */
        p[ind_phan].a[ind_ampn].ampdef = 1;
        p[ind_phae].a[ind_ampe].ampdef = 1;
        apert = sqrt(maxatn * maxatn + maxate * maxate);
        ms_h = log10(apert) + 1.66 * log10(delta) + 0.3;
        if (verbose)
            fprintf(logfp, "        rdid=%d %-6s delta=%.2f MS_h=%.2f\n",
                    p[ind_phan].rdid, p[ind_phan].sta, delta, ms_h);
    }
    else if (ind_phae > -1) {
/*
 *      ms_h from E component
 */
        p[ind_phae].a[ind_ampe].ampdef = 1;
        apert = sqrt(2. * maxate * maxate);
        ms_h = log10(apert) + 1.66 * log10(delta) + 0.3;
        if (verbose)
            fprintf(logfp, "        rdid=%d %-6s delta=%.2f MS_h=%.2f\n",
                    p[ind_phae].rdid, p[ind_phae].sta, delta, ms_h);
    }
    else if (ind_phan > -1) {
/*
 *      ms_h from N component
 */
        p[ind_phan].a[ind_ampn].ampdef = 1;
        apert = sqrt(2. * maxatn * maxatn);
        ms_h = log10(apert) + 1.66 * log10(delta) + 0.3;
        if (verbose)
            fprintf(logfp, "        rdid=%d %-6s delta=%.2f MS_h=%.2f\n",
                    p[ind_phan].rdid, p[ind_phan].sta, delta, ms_h);
    }
/*
 *
 *  reading MS from the MS horizontal and vertical components
 *
 */
    if (ms_h != NULLVAL) {
        mszh->rdid = p[start].rdid;
        mszh->msh = ms_h;
        mszh->mshdef = 1;
        if (ms_z != NULLVAL) {
            reading_ms = (ms_h + ms_z) / 2.;
            mszh->msz = ms_z;
            mszh->mszdef = 1;
        }
        else {
            reading_ms = ms_h;
            mszh->msz = ms_z;
            mszh->mszdef = 0;
        }
    }
    else if (ms_z != NULLVAL) {
        reading_ms = ms_z;
        mszh->rdid = p[start].rdid;
        mszh->msz = ms_z;
        mszh->mszdef = 1;
        mszh->msh = ms_h;
        mszh->mshdef = 0;
    }
    return reading_ms;
}

/*
 *  Title:
 *     GetStationML
 *  Synopsis:
 *     Calculates ML for a single reading.
 *     Finds the reported amplitude, period pair for which A/T is maximal.
 *         Amplitude ml = log(A/T) + C * log(d) + D
 *         Reading   ml = log(max(A/T)) + C * log(d) + D
 *     If no amplitude, period pairs are reported, use the reported logat values
 *         Amplitude ml = logat + C * log(d) + D
 *         Reading   ml = max(logat) + C * log(d) + D
 *  Input Arguments:
 *     start - starting index in a reading
 *     n     - number of phases in the reading
 *     p[]   - array of phase structures
 *     mtypeid - magnitude type id
 *  Return:
 *     reading_ml - ML for this reading
 *  Called by:
 *     GetNetmag
 */
static double GetStationML(int start, int n, PHAREC p[], int mtypeid)
{
    extern double MLMaxDistkm;               /* max delta for calculating ML */
    extern char MLPhase[MAXNUMPHA][PHALEN];               /* From model file */
    extern int numMLPhase;                                /* From model file */
    double logat = 0., dist = 0.;
    double reading_ml = -999., q = 0., maxamp = 0.;
    int i, j, ind_amp = 0, ind_pha = 0;
/*
 *
 *  calculate reading ml from max(A) in a reading
 *
 */
    ind_amp = ind_pha = -1;
    maxamp = -999.;
/*
 *  Ignore readings recorded outside the ML distance range
 */
    dist = DEG2KM * p[start].delta;
    if (dist > MLMaxDistkm)
        return -999.;
/*
 *  general attenuation correction
 *
 *    q = ML_mag_b_coef * log10(dist) + ML_mag_c_coef * dist + ML_mag_d_coef;
 *
 *  SeisComp3 attenuation correction:
 *     linear between 0:-1.3, 60:-2.8, 400:-4.5, 1000:-5.85
 */
    if (dist <= 60)       q = dist * 0.025 + 1.3;
    else if (dist <= 400) q = dist * 0.005 + 2.5;
    else                  q = dist * 0.00225 + 3.6;
    for (i = start; i < n; i++) {
/*
 *      Only need look at phases with amplitudes
 */
        if (p[i].numamps == 0)
            continue;
/*
 *      Only consider phases contributing to ml
 */
        for (j = 0; j < numMLPhase; j++)
            if (streq(p[i].phase, MLPhase[j]))
                break;
        if (j == numMLPhase)
            continue;
/*
 *      Only consider phases with acceptable time residuals (if any)
 */
        for (j = 0; j < PhaseWithoutResidualNum; j++)
            if (streq(p[i].phase, PhaseWithoutResidual[j]))
                break;
        if (j == PhaseWithoutResidualNum && fabs(p[i].timeres) > MagMaxTimeResidual)
            continue;
/*
 *      Loop over amplitudes
 */
        for (j = 0; j < p[i].numamps; j++) {
/*
 *          ignore amplitudes measured on horizontal components
 */
            if (p[i].a[j].comp == 'N' || p[i].a[j].comp == 'E')
                continue;
/*
 *          ignore null amplitudes
 */
            if (fabs(p[i].a[j].amp) < DEPSILON || p[i].a[j].amp == NULLVAL)
                continue;
/*
 *          ignore non-ML amplitude mesaurements (SeisComp3)
 */
            if (p[i].a[j].magtype[0] != '\0') {
                if (toupper(p[i].a[j].magtype[1]) != 'L')
                    continue;
            }
/*
 *          keep track of maximum amplitude
 */
            if (p[i].a[j].amp > maxamp) {
                maxamp = p[i].a[j].amp;
                ind_amp = j;
                ind_pha = i;
            }
/*
 *          amplitude magnitude
 *          ML = log(A) + Alog(d) + Bd + C
 */
            logat = log10(p[i].a[j].amp / 1000.);
//            logat = log10(p[i].a[j].amp / 100.);
/*
 *          SeisComp3 fix: amplitudes are in nanometer
 */
            if (streq(p[i].phase, "IAML") || strchr(p[i].rep, '@')) {
                strcpy(p[i].a[j].magtype, "MLv");
                logat = log10(p[i].a[j].amp);
            }
            p[i].a[j].magnitude = logat + q;
            p[i].a[j].ampdef = 0;
            if (p[i].a[j].magtype[0] == '\0')
                strcpy(p[i].a[j].magtype, "ML");
            p[i].a[j].mtypeid = mtypeid;
            if (verbose > 2) {
                fprintf(logfp, "          i=%2d j=%2d amp=%.2f ",
                        i, j, p[i].a[j].amp);
                fprintf(logfp, "logamp=%.2f magnitude: %.2f\n",
                        logat, p[i].a[j].magnitude);
            }
        }
    }
/*
 *  take the magnitude belonging to the maximal A as the reading magnitude
 *  ML = log(max(A)) + A*log(d) + B*d + C
 */
    if (ind_pha > -1) {
        p[ind_pha].a[ind_amp].ampdef = 1;
        reading_ml = p[ind_pha].a[ind_amp].magnitude;
        if (verbose > 1)
            fprintf(logfp, "        rdid=%d %-6s dist=%.2f km ML=%.2f\n",
                    p[ind_pha].rdid, p[ind_pha].sta, dist, reading_ml);
        return reading_ml;
    }
    return -999.;
}

/*
 *  Title:
 *     ReadMagnitudeQ
 *  Synopsis:
 *     Reads magnitude attenuation Q(d,h) table from file.
 *  Input Arguments:
 *     fname - pathname for magnitude attenuation table
 *     magq  - pointer to MAGQ structure
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     read_data_files
 *  Calls:
 *     SkipComments, AllocateFloatMatrix, Free
 */
int ReadMagnitudeQ(char *filename, MAGQ *magq)
{
    FILE *fp;
    char buf[LINLEN];
    int ndis = 0, ndep = 0;
    int i, j;
    double x = 0.;
/*
 *  open magnitude attenuation file
 */
    if ((fp = fopen(filename, "r")) == NULL) {
        fprintf(logfp, "ReadMagnitudeQ: cannot open %s\n", filename);
        fprintf(errfp, "ReadMagnitudeQ: cannot open %s\n", filename);
        errorcode = 2;
        return 1;
    }
    while (fgets(buf, LINLEN, fp)) {
/*
 *      distance samples
 */
        if (strstr(buf, "DISTANCE")) {
            SkipComments(buf, fp);
            sscanf(buf, "%d", &ndis);
            if ((magq->deltas = (double *)calloc(ndis, sizeof(double))) == NULL) {
                fprintf(logfp, "ReadMagnitudeQ: cannot allocate memory\n");
                fprintf(errfp, "ReadMagnitudeQ: cannot allocate memory\n");
                errorcode = 1;
                return 1;
            }
            magq->numDistanceSamples = ndis;
            for (i = 0; i < ndis; i++) {
                fscanf(fp, "%lf", &x);
                magq->deltas[i] = x;
            }
            magq->mindist = magq->deltas[0];
            magq->maxdist = magq->deltas[ndis - 1];
        }
/*
 *      depth samples
 */
        if (strstr(buf, "DEPTH")) {
            SkipComments(buf, fp);
            sscanf(buf, "%d", &ndep);
            if ((magq->depths = (double *)calloc(ndep, sizeof(double))) == NULL) {
                fprintf(logfp, "ReadMagnitudeQ: cannot allocate memory\n");
                fprintf(errfp, "ReadMagnitudeQ: cannot allocate memory\n");
                Free(magq->deltas);
                errorcode = 1;
                return 1;
            }
            magq->numDepthSamples = ndep;
            for (i = 0; i < ndep; i++) {
                fscanf(fp, "%lf", &x);
                magq->depths[i] = x;
            }
            magq->mindepth = magq->depths[0];
            magq->maxdepth = magq->depths[ndep - 1];
        }
/*
 *      Q(d,h) table
 */
        if (strstr(buf, "MAGNITUDE")) {
            SkipComments(buf, fp);
            sscanf(buf, "%d %d", &i, &j);
            if ((magq->q = AllocateFloatMatrix(ndis, ndep)) == NULL) {
                fprintf(logfp, "ReadMagnitudeQ: cannot allocate memory\n");
                fprintf(errfp, "ReadMagnitudeQ: cannot allocate memory\n");
                Free(magq->deltas);
                Free(magq->depths);
                errorcode = 1;
                return 1;
            }
            for (i = 0; i < ndis; i++) {
                for (j = 0; j < ndep; j++) {
                    fscanf(fp, "%lf", &x);
                    magq->q[i][j] = x;
                }
            }
        }
    }
    fclose(fp);
    return 0;
}

/*  EOF  */
