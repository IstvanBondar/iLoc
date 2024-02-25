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
extern int MinNetmagSta;                 /* min number of stamags for netmag */

/*
 * Functions:
 *    WriteISF
 */

/*
 * Local functions
 */
static void WriteISFOriginBlock(FILE *fp, char *hday, char *htim, char timfix,
        double stime, double sdobs, double lat, double lon, char epifix,
        double smaj, double smin, int strike, double depth, char depfix,
        double sdepth, int ndef, int nsta, int gap, double mindist,
        double maxdist, char antype, char loctype, char *etype, char *author,
        char *origid, char *rep, double depdp, double dpderr, int sgap);
static void WriteISFPhaseBlock(FILE *fp, char *sta, double dist, double esaz,
        char *phase, char *htim, double timeres, double azim, double azimres,
        double slow, double slowres, char timedef, char azimdef, char slowdef,
        double snr, double amp, double per, char sp_fm, char detchar,
        char *magtype, double mag, char *arrid, char *agency, char *deploy,
        char *lcn, char *auth, char *rep, char *pch, char *ach, char lp_fm,
        double stalat, double stalon, double staelev, double stadepth);

/*
 * Title:
 *    WriteISF
 * Synopsis:
 *    Writes solution to ISF bulletin format file.
 * Input Arguments:
 *    ofp     - file pointer to write to.
 *    ep      - pointer to event info
 *    sp      - pointer to current solution.
 *    p[]     - array of phases.
 *    h[]     - array of hypocentres.
 *    stamag  - array of station magnitude structures
 *    rdmag   - array of reading magnitude structures
 *    grn     - geographic region number
 *    magbloc - reported magnitudes
 *  Called by:
 *     main
 * Calls:
 *    Gregion, EpochToHumanISF, WriteISFOriginBlock, WriteISFPhaseBlock
 */
void WriteISF(FILE *ofp, EVREC *ep, SOLREC *sp, HYPREC h[], PHAREC p[],
        STAMAG **stamag, STAMAG **rdmag, int grn, char *magbloc)
{
    extern char OutAgency[VALLEN];  /* author for new hypocentres and assocs */
    extern char MBPhase[MAXNUMPHA][PHALEN];            /* phases for mb calc */
    extern int numMBPhase;                            /* number of mb phases */
    extern char MSPhase[MAXNUMPHA][PHALEN];            /* phases for Ms calc */
    extern int numMSPhase;                            /* number of Ms phases */
    extern char MLPhase[MAXNUMPHA][PHALEN];            /* phases for ML calc */
    extern int numMLPhase;                            /* number of ML phases */
    extern double MLMaxDistkm;               /* max delta for calculating ML */
    STAMAG *smag = (STAMAG *)NULL;
    char antype, loctype, epifix, depfix, timfix;
    char hday[25], htim[25], gregname[255], phase[PHALEN];
    char timedef, azimdef, slowdef, id[15], prevsta[STALEN], magtype[6];
    double amp = 0., per = 0., mag = 0., snr = 0., x = NULLVAL;
    int i, j, k, rdid;
    antype = loctype = ' ';
    strcpy(gregname, "");
    if (grn) Gregion(grn, gregname);
    fprintf(ofp, "\nEvent %s %-s\n\n", ep->EventID, gregname);
/*
 *  Write origin header
 */
    fprintf(ofp, "   Date       Time        Err   RMS Latitude Longitude  ");
    fprintf(ofp, "Smaj  Smin  Az Depth   Err Ndef Nsta Gap  mdist  Mdist ");
    fprintf(ofp, "Qual   Author      OrigID    Rep   DPdep   Err Sgp\n");
/*
 *  Write old hypocentres
 */
    for (i = 0; i < ep->numHypo; i++) {
/*
 *      skip old preferred origin if it was OutAgency
 */
        if (streq(h[i].agency, OutAgency)) continue;
        EpochToHumanISF(hday, htim, h[i].time);
        htim[11] = '\0';
        depfix = epifix = timfix = ' ';
        if (h[i].depfix) depfix = 'f';
        if (h[i].epifix) epifix = 'f';
        if (h[i].timfix) timfix = 'f';
        WriteISFOriginBlock(ofp, hday, htim, timfix, h[i].stime, h[i].sdobs,
                     h[i].lat, h[i].lon, epifix,
                     h[i].smajax, h[i].sminax, h[i].strike,
                     h[i].depth, depfix, h[i].sdepth, h[i].ndef, h[i].ndefsta,
                     h[i].azimgap, h[i].mindist, h[i].maxdist, antype,
                     loctype, h[i].etype, h[i].agency, h[i].origid, h[i].rep,
                     h[i].depdp, h[i].dpderr, h[i].sgap);
    }
/*
 *  Write new solution.
 */
    EpochToHumanISF(hday, htim, sp->time);
    htim[11] = '\0';
    depfix = epifix = timfix = ' ';
    if (sp->depfix) depfix = 'f';
    if (ep->FixDepthToDepdp) depfix = 'd';
    if (sp->epifix) epifix = 'f';
    if (sp->timfix) timfix = 'f';
    sprintf(id, "%d", sp->hypid);
    WriteISFOriginBlock(ofp, hday, htim, timfix, sp->error[0], sp->sdobs,
                 sp->lat, sp->lon, epifix,
                 sp->smajax, sp->sminax, sp->strike,
                 sp->depth, depfix, sp->error[3], sp->ndef, sp->ndefsta,
                 sp->azimgap, sp->mindist, sp->maxdist, antype,
                 loctype, ep->etype, OutAgency, id, OutAgency,
                 sp->depdp, sp->depdp_error, sp->sgap);
    fprintf(ofp, " (#PRIME)\n\n");
/*
 *  Write magnitude block
 */
    i = strlen(magbloc);
    if (i || sp->nnetmag)
        fprintf(ofp, "Magnitude  Err Nsta Author      OrigID\n");
    if (i) fprintf(ofp, "%s", magbloc);
    for (i = 0; i < MAXMAG; i++) {
        if (sp->mag[i].numDefiningMagnitude < MinNetmagSta) continue;
        fprintf(ofp, "%-5s %4.1f %3.1f %4d %-9s %d\n",
                sp->mag[i].magtype, sp->mag[i].magnitude,
                sp->mag[i].uncertainty, sp->mag[i].numDefiningMagnitude,
                OutAgency, sp->hypid);
    }
/*
 *  Write phase block
 */
    fprintf(ofp, "\nSta     Dist  EvAz Phase        Time      TRes  ");
    fprintf(ofp, "Azim AzRes   Slow   SRes Def   SNR       Amp   Per ");
    fprintf(ofp, "Qual Magnitude    ArrID    Agy   Deploy   Ln Auth  ");
    fprintf(ofp, "Rep   PCh ACh L   Lat       Lon     Elev    Depth\n");
    rdid = -1;
    strcpy(prevsta, "");
    for (i = 0; i < ep->numPhase; i++) {
/*
 *      comment lines for reading magnitudes if verbose is on
 */
        if (verbose) {
            if (p[i].rdid != rdid && i) {
                for (k = 0; k < MAXMAG; k++) {
                    smag = rdmag[k];
                    for (j = 0; j < sp->mag[k].numAssociatedMagnitude; j++) {
                        if (rdid != smag[j].rdid)
                            continue;
                        fprintf(ofp, " (Reading %s: %4.1f magdef=%d\n",
                                smag[j].magtype, smag[j].magnitude, smag[j].magdef);
                        break;
                   }
                }
            }
            rdid = p[i].rdid;
/*
 *          comment lines for station magnitudes
 */
            if (strcmp(p[i].prista, prevsta) && i) {
                for (k = 0; k < MAXMAG; k++) {
                    smag = stamag[k];
                    for (j = 0; j < sp->mag[k].numDefiningMagnitude; j++) {
                        if (strcmp(smag[j].sta, prevsta))
                            continue;
                        fprintf(ofp, " (Station %s: %4.1f magdef=%d\n",
                                smag[j].magtype, smag[j].magnitude, smag[j].magdef);
                        break;
                    }
                }
            }
            strcpy(prevsta, p[i].prista);
        }
/*
 *      phase lines
 */
        amp = per = mag = snr = NULLVAL;
        EpochToHumanISF(hday, htim, p[i].time);
        if (p[i].timedef) timedef = 'T';
        else              timedef = '_';
        if (p[i].azimdef) azimdef = 'A';
        else              azimdef = '_';
        if (p[i].slowdef) slowdef = 'S';
        else              slowdef = '_';
        sprintf(id, "???");
        strcpy(magtype, "     ");
        if (p[i].numamps > 0) {
            if (strcmp(p[i].a[0].magtype, "Mwp")) {
                strcpy(magtype, p[i].a[0].magtype);
                amp = p[i].a[0].amp;
                per = p[i].a[0].per;
                mag = p[i].a[0].magnitude;
                snr = p[i].a[0].snr;
                if (streq(magtype, "") && p[i].a[0].mtypeid) {
                    if      (p[i].a[0].mtypeid == 1) strcpy(magtype, "mb");
                    else if (p[i].a[0].mtypeid == 2) strcpy(magtype, "MS");
                    else if (p[i].a[0].mtypeid == 3) strcpy(magtype, "ML");
                    else                             strcpy(magtype, "M");
                }
                else if (streq(magtype, "")) {
                    strcpy(magtype, "     ");
                }
                else {
                    if (streq(magtype, "mB") || streq(magtype, "MLv"))
                        per = NULLVAL;
                }
                sprintf(id, "%s", p[i].a[0].ach);
            }
        }
        WriteISFPhaseBlock(ofp, p[i].sta, p[i].delta, p[i].esaz, p[i].phase,
                           htim, p[i].timeres, p[i].azim, p[i].azimres,
                           p[i].slow, p[i].slowres, timedef, azimdef, slowdef,
                           snr, amp, per, p[i].sp_fm, p[i].detchar, magtype,
                           mag, p[i].arrid, "FDSN", p[i].deploy, p[i].lcn,
                           p[i].auth, p[i].rep, p[i].pch, id, p[i].lp_fm,
                           p[i].StaLat, p[i].StaLon, p[i].StaElev, p[i].StaDepth);
/*
 *      fake phase lines for extra amplitudes, periods
 */
        if (p[i].numamps > 1) {
            strcpy(phase, p[i].phase);
            if (DEG2KM * p[i].delta < MLMaxDistkm) {
                for (j = 0; j < numMLPhase; j++) {
                    if (streq(p[i].phase, MLPhase[j])) {
                        strcpy(phase, "AML");
                        break;
                    }
                }
            }
            else {
                for (j = 0; j < numMBPhase; j++) {
                    if (streq(p[i].phase, MBPhase[j])) {
                        strcpy(phase, "AMB");
                        break;
                    }
                }
            }
            for (j = 0; j < numMSPhase; j++) {
                if (streq(p[i].phase, MSPhase[j])) {
                    strcpy(phase, "AMS");
                    break;
                }
            }
            strcpy(htim, "            ");
            for (j = 1; j < p[i].numamps; j++) {
                strcpy(magtype, p[i].a[j].magtype);
                if (streq(magtype, "Mwp"))
                    continue;
                per = p[i].a[j].per;
                mag = p[i].a[j].magnitude;
                if (streq(magtype, "") && p[i].a[j].mtypeid) {
                    if      (p[i].a[j].mtypeid == 1) strcpy(magtype, "mb");
                    else if (p[i].a[j].mtypeid == 2) strcpy(magtype, "MS");
                    else if (p[i].a[j].mtypeid == 3) strcpy(magtype, "ML");
                    else                             strcpy(magtype, "M");
                }
                else if (streq(magtype, "")) {
                    strcpy(magtype, "     ");
                }
                else {
                    if (streq(magtype, "mB") || streq(magtype, "MLv"))
                        per = NULLVAL;
                }
                WriteISFPhaseBlock(ofp, p[i].sta, p[i].delta, p[i].esaz, phase,
                            htim, x, x, x, x, x, '_', '_', '_', p[i].a[j].snr,
                            p[i].a[j].amp, per, 'x', 'x', magtype, mag,
                            p[i].arrid, "FDSN", p[i].deploy, p[i].lcn,
                            p[i].auth, p[i].rep, "???", p[i].a[j].ach, '_',
                            p[i].StaLat, p[i].StaLon, p[i].StaElev, p[i].StaDepth);
            }
        }
    }
/*
 *  last reading
 */
    for (k = 0; k < MAXMAG; k++) {
        smag = rdmag[k];
        for (j = 0; j < sp->mag[k].numAssociatedMagnitude; j++) {
            if (rdid != smag[j].rdid)
                continue;
            fprintf(ofp, " (Reading %s: %4.1f magdef=%d\n",
                    smag[j].magtype, smag[j].magnitude, smag[j].magdef);
            break;
       }
    }
/*
 *  last station
 */
    for (k = 0; k < MAXMAG; k++) {
        smag = stamag[k];
        for (j = 0; j < sp->mag[k].numDefiningMagnitude; j++) {
            if (strcmp(smag[j].sta, prevsta))
                continue;
            fprintf(ofp, " (Station %s: %4.1f magdef=%d\n",
                    smag[j].magtype, smag[j].magnitude, smag[j].magdef);
            break;
        }
    }
    fprintf(ofp, "\nSTOP\n");
}

/*
 *  Writes an origin line
 */
static void WriteISFOriginBlock(FILE *fp, char *hday, char *htim, char timfix,
        double stime, double sdobs, double lat, double lon, char epifix,
        double smaj, double smin, int strike, double depth, char depfix,
        double sdepth, int ndef, int nsta, int gap, double mindist,
        double maxdist, char antype, char loctype, char *etype, char *author,
        char *origid, char *reporter, double depdp, double dpderr, int sgap)
{
/*
 *  Chars 1-24: origin time and fixed time flag
 */
    fprintf(fp, "%s %s%c ", hday, htim, timfix);
/*
 *  Chars 25-30: origin time error
 */
    if (stime == NULLVAL)   fprintf(fp, "%5s ", "");
    else if (stime > 99.99) fprintf(fp, "99.99 ");
    else                    fprintf(fp, "%5.2f ", stime);
/*
 *  Chars 31-36: sdobs
 */
    if (sdobs == NULLVAL)   fprintf(fp, "%5s ", "");
    else if (sdobs > 99.99) fprintf(fp, "99.99 ");
    else                    fprintf(fp, "%5.2f ", sdobs);
/*
 *  Chars 37-55: latitude, longitude and fixed epicentre flag
 */
    fprintf(fp, "%8.4f %9.4f%c", lat, lon, epifix);
/*
 *  Chars 56-71: optional semi-major, semi-minor axes, strike
 */
    if (smaj == NULLVAL)    fprintf(fp, "%5s ", "");
    else if (smaj > 999.9)  fprintf(fp, "999.9 ");
    else                    fprintf(fp, "%5.1f ", smaj);
    if (smin == NULLVAL)    fprintf(fp, "%5s ", "");
    else if (smin > 999.9)  fprintf(fp, "999.9 ");
    else                    fprintf(fp, "%5.1f ", smin);
    if (strike == NULLVAL)  fprintf(fp, "%3s ", "");
    else                    fprintf(fp, "%3d ", strike);
/*
 *  Chars 72-78: optional depth and fixed depth flag
 */
    if (depth == NULLVAL)   fprintf(fp, "%5s%c ", "", depfix);
    else                    fprintf(fp, "%5.1f%c ", depth, depfix);
/*
 *  Chars 79-83: optional depth error
 */
    if (sdepth == NULLVAL)  fprintf(fp, "%4s ", "");
    else if (sdepth > 99.9) fprintf(fp, "99.9 ");
    else                    fprintf(fp, "%4.1f ", sdepth);
/*
 *  Chars 84-111: optional ndef, nsta, gap, mindist, maxdist
 */
    if (ndef == NULLVAL)    fprintf(fp, "%4s ", "");
    else                    fprintf(fp, "%4d ", ndef);
    if (nsta == NULLVAL)    fprintf(fp, "%4s ", "");
    else                    fprintf(fp, "%4d ", nsta);
    if (gap == NULLVAL)     fprintf(fp, "%3s ", "");
    else                    fprintf(fp, "%3d ", gap);
    if (mindist == NULLVAL) fprintf(fp, "%6s ", "");
    else                    fprintf(fp, "%6.2f ", mindist);
    if (maxdist == NULLVAL) fprintf(fp, "%6s ", "");
    else                    fprintf(fp, "%6.2f ", maxdist);
/*
 *  Char 112-115: analysis type, location method
 */
    fprintf(fp, "%c %c ", antype, loctype);
/*
 *  Chars 116-118: event type
 */
    fprintf(fp, "%-2s ", etype);
/*
 *  Chars 119-128: author
 */
    fprintf(fp, "%-9s ", author);
/*
 *  Chars 129-139: origid
 */
    fprintf(fp, "%-11s ", origid);
/*
 *  Chars 141-145: reporter
 */
    fprintf(fp, "%-5s ", reporter);
/*
 *  Chars 147-161: depth-phase depth, depdp error, sgap
 */
    if (depdp == NULLVAL)  fprintf(fp, "%5s ", "");
    else                   fprintf(fp, "%5.1f ", depdp);
    if (dpderr == NULLVAL) fprintf(fp, "%5s ", "");
    else                   fprintf(fp, "%5.1f ", dpderr);
    if (sgap == NULLVAL)   fprintf(fp, "%3s ", "");
    else                   fprintf(fp, "%3d ", sgap);
    fprintf(fp, "\n");
}

/*
 * Writes a phase block data line.
 */
static void WriteISFPhaseBlock(FILE *fp, char *sta, double dist, double esaz,
        char *phase, char *htim, double timeres, double azim, double azimres,
        double slow, double slowres, char timedef, char azimdef, char slowdef,
        double snr, double amp, double per, char sp_fm, char detchar,
        char *magtype, double mag, char *arrid, char *agency, char *deploy,
        char *lcn, char *auth, char *rep, char *pch, char *ach, char lp_fm,
        double stalat, double stalon, double staelev, double stadepth)
{
    char c, d;
/*
 *  Chars 1-5: station code
 */
    fprintf(fp, "%-5s ", sta);
/*
 *  Chars 7-12: distance
 */
    if (dist == NULLVAL)  fprintf(fp, "%6s ", "");
    else                  fprintf(fp, "%6.2f ", dist);
/*
 *  Chars 14-18: event to sta azimuth
 */
    if (esaz == NULLVAL) fprintf(fp, "%5s ", "");
    else                 fprintf(fp, "%5.1f ", esaz);
/*
 *  Chars 20-27: phase code - can be null
 */
    fprintf(fp, "%-8s ", phase);
/*
 *  Chars 29-40: time. Time can be completely null.
 */
    fprintf(fp, "%s ", htim);
/*
 *  Chars 42-46: time residual
 */
    if (timeres == NULLVAL)  fprintf(fp, "%5s ", "");
    else {
        if (timeres > 999) {
            timeres = 999;
            fprintf(fp, "%5.0f ", timeres);
        }
        else if (timeres < -100) {
            timeres = -999;
            fprintf(fp, "%5.0f ", timeres);
        }
        else
            fprintf(fp, "%5.1f ", timeres);
    }
/*
 *  Chars 48-58: observed azimuth and azimuth residual
 */
    if (azim == NULLVAL)      fprintf(fp, "%5s ", "");
    else                      fprintf(fp, "%5.1f ", azim);
    if (azimres == NULLVAL)   fprintf(fp, "%5s ", "");
    else if (azimres < -99.9) fprintf(fp, "%5.0f ", azimres);
    else                      fprintf(fp, "%5.1f ", azimres);
/*
 *  Chars 60-72: slownessand slowness residual
 */
    if (slow == NULLVAL)      fprintf(fp, "%6s ", "");
    else                      fprintf(fp, "%6.2f ", slow);
    if (slowres == NULLVAL)   fprintf(fp, "%6s ", "");
    else if (slowres < -1000) fprintf(fp, "%6.0f ", slowres);
    else                      fprintf(fp, "%6.1f ", slowres);
/*
 *  Char 74-76: defining flags
 */
    fprintf(fp, "%c%c%c ", timedef, azimdef, slowdef);
/*
 *  Chars 78-82: signal-to noise ratio
 */
    if (snr == NULLVAL || snr < 1.1)  fprintf(fp, "%5s ", "");
    else {
       if (snr > 9999) snr = 9999;
       fprintf(fp, "%5.1f ", snr);
    }
/*
 *  Chars 84-92: amplitude
 */
    if (amp == NULLVAL) fprintf(fp, "%9s ", "");
    else {
        if (amp > 99999999) amp = 99999999;
        fprintf(fp, "%9.1f ", amp);
    }
/*
 *  Chars 94-98: period
 */
    if (per == NULLVAL) fprintf(fp, "%5s ", "");
    else                fprintf(fp, "%5.2f ", per);
/*
 *  Char 100-102: picktype, sp_fm, detchar
 */
    d = tolower(sp_fm);
    if (d == 'c' || d == 'u' || d == '+') c = 'c';
    else if (d == 'd' || d == '-')        c = 'd';
    else                                  c = '_';
    d = tolower(detchar);
    if (!(d == 'e' || d == 'i' || d == 'q')) d = '_';
    fprintf(fp, "_%c%c ", c, d);
/*
 *  Chars 104-109: magnitude type, magnitude indicator
 */
    fprintf(fp, "%-5s ", magtype);
/*
 *  Chars 110-113: magnitude
 */
    if (mag == NULLVAL) fprintf(fp, "%4s ", "");
    else                fprintf(fp, "%4.1f ", mag);
/*
 *  Chars 115-125: arrival ID
 */
    fprintf(fp, "%-11s ", arrid);
/*
 *  ISF2.1 extensions
 */
    fprintf(fp, "%-5s %-8s %-2s ", agency, deploy, lcn);
    if (strchr(rep, '@'))
        fprintf(fp, "%-5s %-5s ", auth, auth);
    else
        fprintf(fp, "%-5s %-5s ", auth, rep);
    fprintf(fp, "%3s %3s ", pch, ach);
    d = tolower(lp_fm);
    if (d == 'c' || d == 'u' || d == '+') c = 'c';
    else if (d == 'd' || d == '-')        c = 'd';
    else                                  c = '_';
    fprintf(fp, "%c ", c);
    fprintf(fp, "%8.4f %9.4f %7.1f %6.1f\n", stalat, stalon, staelev, stadepth);
}

/*  EOF  */
