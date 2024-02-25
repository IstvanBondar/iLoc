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

/*
 * Functions:
 *    Uncertainties
 */

/*
 * Local functions
 */
static double Ftest(int m, int n, double conf);

/*
 *  Title:
 *     Uncertainties
 *  Synopsis:
 *     Calculates nass, nreading, ndefsta, sdobs.
 *        nass     = the number of associated phases
 *        ndefsta  = the number of time defining stations
 *        nreading = the number of readings
 *        sdobs    = sqrt(sum of squares of UNweighted residuals / (N - M))
 *     Calculates formal uncertainty estimates scaled to confidence level.
 *        Jordan T.H. and K.A. Sverdrup,
 *        Teleseismic location techniques and their application
 *        to earthquake clusters in the South-Central Pacific,
 *        Bull. Seism. Soc. Am., 71, 1105-1130, 1981.
 *  Input Arguments:
 *     sp  - pointer to current solution
 *     p[] - array of phase structures
 *  Return:
 *     0/1 for success/error
 *  Called by:
 *     LocateEvent
 *  Calls:
 *     Ftest
 */
int Uncertainties(SOLREC *sp, PHAREC p[])
{
    extern double ConfidenceLevel;                       /* from config file */
    double fs = 0., scale_fact = 0., conf = ConfidenceLevel / 100.;
    double ndf = 0., b = 0., d = 0., eigen1 = 0., eigen2 = 0.;
    double ssq = 0., sigmahat = 0., sxx = 0., sxy = 0., syy = 0., strike = 0.;
    char PrevSta[STALEN];
    int used = 0, totndf = 0, k = 0, i;
    strcpy(PrevSta, "");
/*
 *  Count readings/phases/defining stations
 *  Rely on phases being ordered by delta/prista/rdid/day
 */
    sp->nass = sp->ndefsta = sp->ndef = 0;
    sp->ntimedef = sp->nazimdef = sp->nslowdef = 0;
    used = 0;
    for (i = 0; i < sp->numPhase; i++) {
        used = 0;
        if (p[i].timedef) {
            sp->ndef++;
            sp->ntimedef++;
            used = 1;
        }
        if (p[i].azimdef) {
            sp->ndef++;
            sp->nazimdef++;
            used = 1;
        }
        if (p[i].slowdef) {
            sp->ndef++;
            sp->nslowdef++;
            used = 1;
        }
        sp->nass++;
/*
 *      new defining station
 */
        if (used && (strcmp(p[i].prista, PrevSta)))
            sp->ndefsta++;
        strcpy(PrevSta, p[i].prista);
    }
/*
 *  sdobs = sqrt(sum of squares of UNweighted residuals / (N - M))
 */
    totndf = max(sp->ndef - sp->numUnknowns, 1);
    sp->sdobs = sp->urms * Sqrt((double)sp->ndef / (double)(totndf));
    if (verbose > 2) {
        fprintf(logfp, "    Uncertainties: nreading=%d nass=%d ndefsta=%d ",
                sp->nreading, sp->nass, sp->ndefsta);
        fprintf(logfp, "ndef=%d urms=%f sdobs=%f\n",
                sp->ndef, sp->urms, sp->sdobs);
    }
/*
 *  No uncertainties for fixed hypocenter
 */
    if (sp->hypofix)
        return 0;
/*
 *  sigmahat and total number of degrees of freedom
 *      Jordan T.H. and K.A. Sverdrup,
 *      Teleseismic location techniques and their application
 *      to earthquake clusters in the South-Central Pacific,
 *      Bull. Seism. Soc. Am., 71, 1105-1130, 1981.
 *
 *      K is taken as infinity -> coverage error ellipse
 */
    k = 99999;
    totndf = sp->prank - sp->numUnknowns + k;
    ssq = sp->wrms * sp->wrms * (double)sp->ndef + (double)k;
    ndf = (double)totndf;
    if (fabs(ndf) < DEPSILON)
        ndf = 0.001;
    if (fabs(totndf - ssq) < 0.00001)
        ndf = ssq;
    sigmahat = Sqrt(ssq / ndf);
/*
 *  Calculate standard errors, scaled to ConfidenceLevel% confidence level
 */
    fs = Ftest(1, totndf, conf);
    scale_fact = Sqrt(fs) * sigmahat;
    for (i = 0; i < 4; i++)
        if (sp->covar[i][i] != NULLVAL)
            sp->error[i] = scale_fact * Sqrt(sp->covar[i][i]);
/*
 *  Convert lat, lon errors to degrees
 */
    if (sp->error[1] != NULLVAL)  sp->error[1] /= DEG2KM;
    if (sp->error[2] != NULLVAL)  sp->error[2] /= DEG2KM;
/*
 *  No error ellipse for fixed location solution
 */
    if (sp->epifix)
        return 0;
/*
 *  Calculate error ellipse
 */
    sxx = sp->covar[1][1];
    sxy = sp->covar[1][2];
    syy = sp->covar[2][2];
/*
 *  eigenvalues of the 2x2 covariance matrix
 */
    b = sxx + syy;
    d = Sqrt(b * b - 4. * (sxx * syy - sxy * sxy));
    eigen1 = fabs((b + d) / 2.);
    eigen2 = fabs((b - d) / 2.);
    strike = 0.5 * atan2(2. * sxy , (syy - sxx));
    if (strike < 0.)    strike += TWOPI;
    if (strike > TWOPI) strike -= TWOPI;
    if (strike > PI)    strike -= PI;
    sp->strike = RAD_TO_DEG * strike;
/*
 *  scale to ConfidenceLevel% confidence level
 */
    fs = Ftest(2, totndf, conf);
    scale_fact = Sqrt(2. * fs) * sigmahat;
    sp->smajax = Sqrt(eigen1) * scale_fact;
    sp->sminax = Sqrt(eigen2) * scale_fact;
    return 0;
}

#define	MAX_TAB	34
#define	MAX_P   3
/*
 *  Title:
 *     Ftest
 *  Synopsis:
 *     Critical value of F distribution with M, N degrees of freedom
 *     The critical values for the F distribution are listed in
 *         D. Zwillinger and S. Kokoska, 2000,
 *         CRC Standard Probability and Statistics Tables and Formulae,
 *         Chapman and Hall/CRC.
 *  Input Arguments:
 *     m    - number of model parameters (1, 2 or 3)
 *     n    - number of degrees of freedom (observations - model params)
 *     conf - ConfidenceLevel level (90, 95 or 98)
 *  Return:
 *     Critical value Prob[F >= F(M,N)] = 1 - conf
 *  Called by:
 *     Uncertainties
 */
static double Ftest(int m, int n, double conf)
{
    int i, ip = 0, j = 0;
    double an = 0., an1 = 0., an2 = 0.;
    double x1 = 0., x2 = 0., y = 0., y1 = 0., y2 = 0., x = 0.;
/*
 *  number of degree of freedoms
 */
    static int ns[] = {  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                        13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                        23, 24, 25, 26, 27, 28, 29, 30, 40, 60,
                       120, 99999 };
/*
 *  confidence levels
 */
    static double ps[] = { 0.90, 0.95, 0.99 };
/*
 *  F[M,N]  critical values table
 */
    static double xs[][MAX_P][MAX_TAB] = {
/*
 *       90th percentile
 */
    {
        { 39.86, 8.53, 5.54, 4.54, 4.06, 3.78, 3.59, 3.46, 3.36,
          3.29, 3.23, 3.18, 3.14, 3.10, 3.07, 3.05, 3.03, 3.01,
          2.99, 2.97, 2.96, 2.95, 2.94, 2.93, 2.92, 2.91, 2.90,
          2.89, 2.89, 2.88, 2.84, 2.79, 2.75, 2.71 },
        { 49.50, 9.00, 5.46, 4.32, 3.78, 3.46, 3.26, 3.11, 3.01,
          2.92, 2.86, 2.81, 2.76, 2.73, 2.70, 2.67, 2.64, 2.62,
          2.61, 2.59, 2.57, 2.56, 2.55, 2.54, 2.53, 2.52, 2.51,
          2.50, 2.50, 2.49, 2.44, 2.39, 2.35, 2.30 },
        { 53.59, 9.16, 5.39, 4.19, 3.62, 3.29, 3.07, 2.92, 2.81,
          2.73, 2.66, 2.61, 2.56, 2.52, 2.49, 2.46, 2.44, 2.42,
          2.40, 2.38, 2.36, 2.35, 2.34, 2.33, 2.32, 2.31, 2.30,
          2.29, 2.28, 2.28, 2.23, 2.18, 2.13, 2.08 }
       },
/*
 *      95th percentile
 */
    {
        { 161.4, 18.51,10.13, 7.71, 6.61, 5.99, 5.59, 5.32, 5.12,
          4.96, 4.84, 4.75, 4.67, 4.60, 4.54, 4.49, 4.45, 4.41,
          4.38, 4.35, 4.32, 4.30, 4.28, 4.26, 4.24, 4.23, 4.21,
          4.20, 4.18, 4.17, 4.08, 4.00, 3.92, 3.84 },
        { 199.5, 19.00, 9.55, 6.94, 5.79, 5.14, 4.74, 4.46, 4.26,
          4.10, 3.98, 3.89, 3.81, 3.74, 3.68, 3.63, 3.59, 3.55,
          3.52, 3.49, 3.47, 3.44, 3.42, 3.40, 3.39, 3.37, 3.35,
          3.34, 3.33, 3.32, 3.23, 3.15, 3.07, 3.00 },
        { 215.7, 19.16, 9.28, 6.59, 5.41, 4.76, 4.35, 4.07, 3.86,
          3.71, 3.59, 3.49, 3.41, 3.34, 3.29, 3.24, 3.20, 3.16,
          3.13, 3.10, 3.07, 3.05, 3.03, 3.01, 2.99, 2.98, 2.96,
          2.95, 2.93, 2.92, 2.84, 2.76, 2.68, 2.60 }
       },
/*
 *     99th percentile
 */
    {
        { 4052.0, 98.50,34.12,21.20,16.26,13.75,12.25,11.26,10.56,
         10.04, 9.65, 9.33, 9.07, 8.86, 8.68, 8.53, 8.40, 8.29,
          8.18, 8.10, 8.02, 7.95, 7.88, 7.82, 7.77, 7.72, 7.68,
          7.64, 7.60, 7.56, 7.31, 7.08, 6.85, 6.63 },
        { 4999.5, 99.00,30.82,18.00,13.27,10.92, 9.55, 8.65, 8.02,
          7.56, 7.21, 6.93, 6.70, 6.51, 6.36, 6.23, 6.11, 6.01,
          5.93, 5.85, 5.78, 5.72, 5.66, 5.61, 5.57, 5.53, 5.49,
          5.45, 5.42, 5.39, 5.18, 4.98, 4.79, 4.61 },
        { 5403.0, 99.17,29.46,16.69,12.06, 9.78, 8.45, 7.59, 6.99,
          6.55, 6.22, 5.95, 5.74, 5.56, 5.42, 5.29, 5.18, 5.09,
          5.01, 4.94, 4.87, 4.82, 4.76, 4.72, 4.68, 4.64, 4.60,
          4.57, 4.54, 4.51, 4.31, 4.13, 3.95, 3.78 }
       }
    };
    if (m < 1 || m > MAX_P) return x;
    if (n < 1) {
        x = 1000.0;
        return x;
    }
/*
 *  find indexes
 */
    for (i = 0; i < 3; i++)
        if (fabs(conf - ps[i]) < 0.001)
            ip = i;
    for (i = MAX_TAB - 1; i >= 0; i--){
        if (n >= ns[i]) {
            j = i;
            break;
        }
    }
/*
 *  exact match
 */
    if (n == ns[j] || j == MAX_TAB - 1) {
        x = *(xs[ip][m-1] + j);
    }
/*
 *  interpolate
 */
    else {
        an1 = ns[j];
        an2 = ns[j+1];
        an  = n;
        y1  = an1 / (1.0 + an1);
        y2  = an2 / (1.0 + an2);
        y   = an / (1.0 + an);
        x1  = *(xs[ip][m-1] + j);
        x2  = *(xs[ip][m-1] + j+1);
        x   = x1 + (x2 - x1) * ((y - y1) / (y2 - y1));
   }
   return x;
}

