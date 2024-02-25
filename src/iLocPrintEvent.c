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
 *    PrintSolution
 *    PrintHypocenter
 *    PrintPhases
 *    PrintDefiningPhases
 */

/*
 *  Title:
 *      PrintSolution
 *  Synopsis:
 *      Prints current solution to the logfile
 *  Input Arguments:
 *      sp  - pointer to current solution.
 *      grn - geographic region number. gregname is printed if grn > 0.
 *  Calls:
 *      Gregion, EpochToHuman
 *  Called by:
 *      Locator, LocateEvent, NASearch
 */
void PrintSolution(SOLREC *sp, int grn)
{
    char timestr[25], gregname[255];
    if (grn) {
        Gregion(grn, gregname);
        fprintf(logfp, "%s\n", gregname);
    }
    EpochToHuman(timestr, sp->time);
    fprintf(logfp, "    OT = %s ", timestr);
    fprintf(logfp, "Lat = %.3f ", sp->lat);
    fprintf(logfp, "Lon = %.3f ", sp->lon);
    fprintf(logfp, "Depth = %.1f\n", sp->depth);
}

/*
 *  Title:
 *      PrintHypocenter
 *  Synopsis:
 *      Prints hypocentres for an event to the logfile
 *  Input Arguments:
 *      ep  - pointer to event record
 *      h[] - array of hypocentre records.
 *  Calls:
 *      EpochToHuman
 *  Called by:
 *      Locator
 */
void PrintHypocenter(EVREC *ep, HYPREC h[])
{
    char timestr[25];
    int i;
    fprintf(logfp, "EventID=%s numHypo=%d numPhase=%d\n",
            ep->EventID, ep->numHypo, ep->numPhase);
    fprintf(logfp, "hypid     agency   time                     lat");
    fprintf(logfp, "     lon     depth  nsta sdef ndef nass gap  mindist ");
    fprintf(logfp, "stime  sdep  smaj\n");
    for (i = 0; i < ep->numHypo; i++) {
        EpochToHuman(timestr, h[i].time);
        fprintf(logfp, "%9d ", h[i].hypid);
        fprintf(logfp, "%-8s %s ", h[i].agency, timestr);
        fprintf(logfp, "%7.3f %8.3f ", h[i].lat, h[i].lon);
        if (h[i].depth != NULLVAL)   fprintf(logfp, "%5.1f  ", h[i].depth);
        else                         fprintf(logfp, "%5s  ", "");
        if (h[i].nsta != NULLVAL)    fprintf(logfp, "%4d ", h[i].nsta);
        else                         fprintf(logfp, "%4s ", "");
        if (h[i].ndefsta != NULLVAL) fprintf(logfp, "%4d ", h[i].ndefsta);
        else                         fprintf(logfp, "%4s ", "");
        if (h[i].ndef != NULLVAL)    fprintf(logfp, "%4d ", h[i].ndef);
        else                         fprintf(logfp, "%4s ", "");
        if (h[i].nass != NULLVAL)    fprintf(logfp, "%4d ", h[i].nass);
        else                         fprintf(logfp, "%4s ", "");
        if (h[i].azimgap != NULLVAL) fprintf(logfp, "%5.1f ", h[i].azimgap);
        else                         fprintf(logfp, "%5s ", "");
        if (h[i].mindist != NULLVAL) fprintf(logfp, "%6.2f ", h[i].mindist);
        else                         fprintf(logfp, "%6s ", "");
        if (h[i].stime != NULLVAL)   fprintf(logfp, "%5.2f ", h[i].stime);
        else                         fprintf(logfp, "%5s ", "");
        if (h[i].sdepth != NULLVAL)  fprintf(logfp, "%5.2f ", h[i].sdepth);
        else                         fprintf(logfp, "%5s ", "");
        if (h[i].smajax != NULLVAL)  fprintf(logfp, "%5.1f ", h[i].smajax);
        else                         fprintf(logfp, "%5s ", "");
        fprintf(logfp, "\n");
    }
}

/*
 *  Title:
 *      PrintPhases
 *  Synopsis:
 *      Prints a table with all the phases for one event.
 *  Input Arguments:
 *      numPhase - number of associated phases
 *      p[] - array of phase structures
 *  Calls:
 *      EpochToHuman
 *  Called by:
 *      Locator, LocateEvent, Synthetic, ReadISF,
 *      GetISCPhases, GetSC3Phases, GetDeltaAzimuth
 */
void PrintPhases(int numPhase, PHAREC p[])
{
    int i, j;
    char timestr[25], s[255];
    fprintf(logfp, "RDID      ARRID      STA                 DELTA   ESAZ ");
    fprintf(logfp, "REPORTED IASPEI   TIME                     TIMERES   ");
    fprintf(logfp, "AZIM  AZRES   SLOW SLORES TAS ");
    fprintf(logfp, "PCH MODEL MTYPE        AMP  PER ACH M\n");
    for (i = 0; i < numPhase; i++) {
        sprintf(s, "%-9d", p[i].rdid);
        sprintf(s, "%s %-10s", s, p[i].arrid);
        sprintf(s, "%s %-18s", s, p[i].fdsn);
        sprintf(s, "%s %6.2f", s, p[i].delta);
        sprintf(s, "%s %6.2f", s, p[i].esaz);
        sprintf(s, "%s %-8s", s, p[i].ReportedPhase);
        sprintf(s, "%s %-8s", s, p[i].phase);
        if (p[i].time != NULLVAL) {
            EpochToHuman(timestr, p[i].time);
            sprintf(s, "%s %s", s, timestr);
            if (p[i].timeres != NULLVAL) sprintf(s, "%s %8.2f", s, p[i].timeres);
            else                         sprintf(s, "%s %8s", s, "");
        }
        else sprintf(s, "%s %32s", s, "");
        if (p[i].azim != NULLVAL) {
            sprintf(s, "%s %6.1f", s, p[i].azim);
            if (p[i].azimres != NULLVAL) sprintf(s, "%s %6.1f", s, p[i].azimres);
            else                         sprintf(s, "%s %6s", s, "");
        }
        else sprintf(s, "%s %13s", s, "");
        if (p[i].slow != NULLVAL) {
            sprintf(s, "%s %6.1f", s, p[i].slow);
            if (p[i].slowres != NULLVAL) sprintf(s, "%s %6.1f", s, p[i].slowres);
            else                         sprintf(s, "%s %6s", s, "");
        }
        else sprintf(s, "%s %13s", s, "");
        if (p[i].timedef)            sprintf(s, "%s T", s);
        else                         sprintf(s, "%s _", s);
        if (p[i].azimdef)            sprintf(s, "%sA", s);
        else                         sprintf(s, "%s_", s);
        if (p[i].slowdef)            sprintf(s, "%sS", s);
        else                         sprintf(s, "%s_", s);
        sprintf(s, "%s %3s", s, p[i].pch);
        sprintf(s, "%s %5s", s, p[i].vmod);
        if (p[i].numamps > 0) {
/*
 *          amplitude measurements reported with the pick
 */
            for (j = 0; j < p[i].numamps; j++) {
                if (tolower(p[i].a[j].magtype[1]) == 'w') {
                    sprintf(s, "%s %27s", s, "");
                    fprintf(logfp, "%s\n", s);
                    continue;
                }
                sprintf(s, "%s %-6s", s, p[i].a[j].magtype);
                if (p[i].a[j].amp != NULLVAL)
                    sprintf(s, "%s %9.1f", s, p[i].a[j].amp);
                else
                    sprintf(s, "%s %9s", s, "");
                if (p[i].a[j].per != NULLVAL) {
                    if ((p[i].a[j].magtype[1] == 'b' ||
                        p[i].a[j].magtype[1] == 'S'))
                        sprintf(s, "%s %4.1f", s, p[i].a[j].per);
                    else
                        sprintf(s, "%s %4s", s, "");
                }
                else
                    sprintf(s, "%s %4s", s, "");
                sprintf(s, "%s %3s", s, p[i].a[j].ach);
                sprintf(s, "%s %d", s, p[i].a[j].ampdef);
                fprintf(logfp, "%s\n", s);
                sprintf(s, "%-9d %136s", p[i].rdid, "");
            }
        }
        else
            fprintf(logfp, "%s\n", s);
    }
}

/*
 *  Title:
 *      PrintDefiningPhases
 *  Synopsis:
 *      Prints a table with all the time-defining phases for one event
 *  Input Arguments:
 *      numPhase - number of associated phases
 *      p[] - array of phase structures
 *  Calls:
 *      EpochToHuman
 *  Called by:
 *      LocateEvent, NASearch
 */
void PrintDefiningPhases(int numPhase, PHAREC p[])
{
    int i;
    char timestr[25];
    fprintf(logfp, "    RDID      ARRID      STA                PHASE     ");
    fprintf(logfp, "DELTA   ESAZ TIME                     TIMERES   ");
    fprintf(logfp, "AZIM  AZRES   SLOW SLORES TAS MODEL\n");
    for (i = 0; i < numPhase; i++) {
        if (!p[i].timedef && !p[i].azimdef && !p[i].slowdef) continue;
        fprintf(logfp, "    %-9d ", p[i].rdid);
        fprintf(logfp, "%-10s ", p[i].arrid);
        fprintf(logfp,"%-18s ", p[i].fdsn);
        fprintf(logfp,"%-8s ", p[i].phase);
        fprintf(logfp,"%6.2f ", p[i].delta);
        fprintf(logfp,"%6.2f ", p[i].esaz);
        if (p[i].timedef) {
            EpochToHuman(timestr, p[i].time);
            fprintf(logfp, "%s ", timestr);
            if (p[i].timeres != NULLVAL) fprintf(logfp, "%8.2f ", p[i].timeres);
            else                         fprintf(logfp, "%8s ", "");
        }
        else fprintf(logfp, "%32s ", "");
        if (p[i].azimdef) {
            fprintf(logfp, "%6.1f ", p[i].azim);
            if (p[i].azimres != NULLVAL) fprintf(logfp, "%6.1f ", p[i].azimres);
            else                         fprintf(logfp, "%6s ", "");
        }
        else fprintf(logfp, "%13s ", "");
        if (p[i].slowdef) {
            fprintf(logfp, "%6.1f ", p[i].slow);
            if (p[i].slowres != NULLVAL) fprintf(logfp, "%6.1f ", p[i].slowres);
            else                         fprintf(logfp, "%6s ", "");
        }
        else fprintf(logfp, "%13s ", "");
        if (p[i].timedef)            fprintf(logfp, "T");
        else                         fprintf(logfp, "_");
        if (p[i].azimdef)            fprintf(logfp, "A");
        else                         fprintf(logfp, "_");
        if (p[i].slowdef)            fprintf(logfp, "S ");
        else                         fprintf(logfp, "_ ");
        fprintf(logfp, "%5s\n", p[i].vmod);
    }
}

/*  EOF  */

