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
 *    GetEllipticityCorrection
 *    ReadEllipticityCorrectionTable
 *    FreeEllipticityCorrectionTable
 */

/*
 * Local functions:
 *    ECPhaseIndex
 */
static int ECPhaseIndex(char *phase, double delta);

/*
 *
 *  Title:
 *      GetEllipticityCorrection
 *  Synopsis:
 *      Calculates ellipticity correction for a phase.
 *      Dziewonski, A.M. and F. Gilbert, 1976,
 *         The effect of small, aspherical perturbations on travel times
 *         and a re-examination of the correction for ellipticity,
 *         Geophys., J. R. Astr. Soc., 44, 7-17.
 *      Kennett, B.L.N. and O. Gudmundsson, 1996,
 *         Ellipticity corrections for seismic phases,
 *         Geophys. J. Int., 127, 40-48.
 *      The routine uses the Dziewonski and Gilbert (1976) representation.
 *      The code is based on Brian Kennett's ellip.f
 *      The ellipticity corrections are found by linear interpolation
 *          in terms of values calculated for the ak135 model for a wide
 *          range of phases to match the output of the libtau software.
 *  Input Arguments:
 *      ec     - ellipticity correction coefs
 *      phase  - phase
 *      ecolat - epicenter's co-latitude [rad]
 *      edist  - epicentral distance [deg]
 *      edepth - source depth [km]
 *      esaz   - event-to-station azimuth [deg]
 *  Return:
 *      tcor - ellipticity correction
 *  Called by:
 *     correct_ttime
 *  Calls:
 *      ECPhaseIndex, BilinearInterpolation
 *  Notes:
 *      The available phases and the tabulated distance ranges are:
 *      (Kennett and Gudmundsson, 1996)
 *        #  Phase  ndist delta_min delta_max
 *
 *        0  Pup      3       0.0      10.0     (Pg, Pb)
 *        1  P       20       5.0     100.0     (Pn, PgPg, PbPb)
 *        2  Pdiff   11     100.0     150.0     (pPdiff, sPdiff)
 *        3  PKPab    7     145.0     175.0
 *        4  PKPbc    3     145.0     155.0     (PKPdiff)
 *        5  PKPdf   14     115.0     180.0
 *        6  PKiKP   32       0.0     155.0
 *        7  pP      17      20.0     100.0     (pwP, pPg, pPb, pPn, pPdiff)
 *        8  pPKPab   7     145.0     175.0
 *        9  pPKPbc   3     145.0     155.0     (pPKPdiff)
 *       10  pPKPdf  14     115.0     180.0
 *       11  pPKiKP  32       0.0     155.0
 *       12  sP      20       5.0     100.0     (sPg, sPb, sPn, sPdiff)
 *       13  sPKPab   7     145.0     175.0
 *       14  sPKPbc   3     145.0     155.0     (sPKPdiff)
 *       15  sPKPdf  14     115.0     180.0
 *       16  sPKiKP  32       0.0     155.0
 *       17  PcP     19       0.0      90.0
 *       18  ScP     13       0.0      60.0
 *       19  SKPab    3     130.0     140.0
 *       20  SKPbc    5     130.0     150.0
 *       21  SKPdf   15     110.0     180.0
 *       22  SKiKP   30       0.0     145.0
 *       23  PKKPab   5     235.0     255.0
 *       24  PKKPbc  11     235.0     285.0
 *       25  PKKPdf  31     210.0     360.0
 *       26  SKKPab   2     215.0     220.0
 *       27  SKKPbc  14     215.0     280.0
 *       28  SKKPdf  32     205.0     360.0
 *       29  PP      31      40.0     190.0     (PnPn)
 *       30  P'P'    26     235.0     360.0
 *       31  Sup      3       0.0      10.0     (Sg, Lg, Sb)
 *       32  S       19       5.0      95.0     (Sn, SgSg, SbSb)
 *       33  Sdiff   11     100.0     150.0     (pSdiff, sSdiff)
 *       34  SKSac   16      65.0     140.0
 *       35  SKSdf   16     105.0     180.0
 *       36  pS       9      60.0     100.0     (pSg, pSb, pSn, pSdiff)
 *       37  pSKSac  15      70.0     140.0
 *       38  pSKSdf  15     110.0     180.0
 *       39  sS      17      20.0     100.0     (sSg, sSb, sSn, sSdiff)
 *       40  sSKSac  16      65.0     140.0
 *       41  sSKSdf  15     110.0     180.0
 *       42  ScS     19       0.0      90.0
 *       43  PcS     13       0.0      60.0
 *       44  PKSab    3     130.0     140.0
 *       45  PKSbc    4     130.0     145.0
 *       46  PKSdf   15     110.0     180.0
 *       47  PKKSab   2     215.0     220.0
 *       48  PKKSbc  14     215.0     280.0
 *       49  PKKSdf  32     205.0     360.0
 *       50  SKKSac  43      65.0     275.0
 *       51  SKKSdf  33     200.0     360.0
 *       52  SS      31      40.0     190.0     (SnSn)
 *       53  S'S'    47     130.0     360.0
 *       54  SP      17      55.0     135.0     (SPn, SPb, SPg, SnP)
 *       55  PS      10      90.0     135.0     (PSn)
 *       56  PnS      6      65.0      90.0
 *
 *      The phase code is checked against the tabulated phases to find
 *          the entry point to the table corresponding to the class of arrivals.
 *      The presence of a value in the tables does not imply a physical
 *          arrival at all distance, depth combinations.  Where necessary,
 *          extrapolation has been used to ensure satisfactory results.
 *
 */
double GetEllipticityCorrection(EC_COEF *ec, char *phase, double ecolat,
                      double delta, double depth, double esaz)
{
    int k = -1;
    double azim = 0., tau0 = 0., tau1 = 0., tau2 = 0.;
    double s3 = sqrt(3.) / 2.;
    double tcor = 0.;
    double sc0 = 0., sc1 = 0., sc2 = 0.;
    azim = esaz * DEG_TO_RAD;
/*
 *  get corresponding index in ec
 */
    k = ECPhaseIndex(phase, delta);
/*
 *  no phase was found, return zero correction
 */
    if (k < 0) return 0.;
/*
 *  bilinear interpolation of tau coefficients of Dziewonski and Gilbert (1976)
 */
    tau0 = BilinearInterpolation(delta, depth, ec[k].numDistanceSamples,
                    ec[k].numDepthSamples, ec[k].delta, ec[k].depth, ec[k].t0);
    tau1 = BilinearInterpolation(delta, depth, ec[k].numDistanceSamples,
                    ec[k].numDepthSamples, ec[k].delta, ec[k].depth, ec[k].t1);
    tau2 = BilinearInterpolation(delta, depth, ec[k].numDistanceSamples,
                    ec[k].numDepthSamples, ec[k].delta, ec[k].depth, ec[k].t2);
/*
 *  ellipticity correction: eqs. (22) and (26) of Dziewonski and Gilbert (1976)
 */
    sc0 = 0.25 * (1.0 + 3.0 * cos(2.0 * ecolat));
    sc1 = s3 * sin(2.0 * ecolat);
    sc2 = s3 * sin(ecolat) * sin(ecolat);
    tcor = sc0 * tau0 + sc1 * cos(azim) * tau1 + sc2 * cos(2. * azim) * tau2;
    return tcor;
}

/*
 *  Title:
 *      ECPhaseIndex
 *  Synopsis:
 *      Returns index to EC_COEF struct based on phase and delta
 *  Input Arguments:
 *      phase - phase
 *      delta - delta
 *  Called by:
 *      GetEllipticityCorrection
 *
 *      Pup,    P,      Pdiff,  PKPab,  PKPbc,  PKPdf,  PKiKP,  pP,
 *      pPKPab, pPKPbc, pPKPdf, pPKiKP, sP,     sPKPab, sPKPbc, sPKPdf,
 *      sPKiKP, PcP,    ScP,    SKPab,  SKPbc,  SKPdf,  SKiKP,  PKKPab,
 *      PKKPbc, PKKPdf, SKKPab, SKKPbc, SKKPdf, PP,     P'P',   Sup,
 *      S,      Sdiff,  SKSac,  SKSdf,  pS,     pSKSac, pSKSdf, sS,
 *      sSKSac, sSKSdf, ScS,    PcS,    PKSab,  PKSbc,  PKSdf,  PKKSab,
 *      PKKSbc, PKKSdf, SKKSac, SKKSdf, SS,     S'S',   SP,     PS,
 *      PnS
 */
static int ECPhaseIndex(char *phase, double delta)
{
    if (streq(phase, "Pup")  ||
        streq(phase, "Pg")   ||
        streq(phase, "Pb")   ||
        streq(phase, "p"))      return 0;
    if (streq(phase, "P")    ||
        streq(phase, "Pn")   ||
        streq(phase, "PgPg") ||
        streq(phase, "PbPb"))   return 1;
    if (streq(phase, "Pdiff") ||
        streq(phase, "Pdif"))   return 2;
    if (streq(phase, "PKPab"))  return 3;
    if (streq(phase, "PKPbc"))  return 4;
    if (streq(phase, "PKPdf"))  return 5;
    if (streq(phase, "PKiKP"))  return 6;
    if (streq(phase, "pP")  ||
        streq(phase, "pPg") ||
        streq(phase, "pPb") ||
        streq(phase, "pPn") ||
        streq(phase, "pwP"))    return 7;
    if (streq(phase, "pPKPab")) return 8;
    if (streq(phase, "pPKPbc")) return 9;
    if (streq(phase, "pPKPdf")) return 10;
    if (streq(phase, "pPKiKP")) return 11;
    if (streq(phase, "sP")  ||
        streq(phase, "sPg") ||
        streq(phase, "sPb") ||
        streq(phase, "sPn"))    return 12;
    if (streq(phase, "sPKPab")) return 13;
    if (streq(phase, "sPKPbc")) return 14;
    if (streq(phase, "sPKPdf")) return 15;
    if (streq(phase, "sPKiKP")) return 16;
    if (streq(phase, "PcP"))    return 17;
    if (streq(phase, "ScP"))    return 18;
    if (streq(phase, "SKPab"))  return 19;
    if (streq(phase, "SKPbc"))  return 20;
    if (streq(phase, "SKPdf"))  return 21;
    if (streq(phase, "SKiKP"))  return 22;
    if (streq(phase, "PKKPab")) return 23;
    if (streq(phase, "PKKPbc")) return 24;
    if (streq(phase, "PKKPdf")) return 25;
    if (streq(phase, "SKKPab")) return 26;
    if (streq(phase, "SKKPbc")) return 27;
    if (streq(phase, "SKKPdf")) return 28;
    if (streq(phase, "PP") ||
        streq(phase, "PnPn"))   return 29;
    if (streq(phase, "P'P'ab") ||
        streq(phase, "P'P'bc") ||
        streq(phase, "P'P'df")) return 30;
    if (streq(phase, "Sup")  ||
        streq(phase, "Sg")   ||
        streq(phase, "Lg")   ||
        streq(phase, "Sb")   ||
        streq(phase, "s"))      return 31;
    if (streq(phase, "S")    ||
        streq(phase, "Sn")   ||
        streq(phase, "SgSg") ||
        streq(phase, "SbSb"))   return 32;
    if (streq(phase, "Sdiff") ||
        streq(phase, "Sdif"))   return 33;
    if (streq(phase, "SKSac"))  return 34;
    if (streq(phase, "SKSdf"))  return 35;
    if (streq(phase, "pS")  ||
        streq(phase, "pSg") ||
        streq(phase, "pSb") ||
        streq(phase, "pSn"))    return 36;
    if (streq(phase, "pSKSac")) return 37;
    if (streq(phase, "pSKSdf")) return 38;
    if (streq(phase, "sS")  ||
        streq(phase, "sSg") ||
        streq(phase, "sSb") ||
        streq(phase, "sSn"))    return 39;
    if (streq(phase, "sSKSac")) return 40;
    if (streq(phase, "sSKSdf")) return 41;
    if (streq(phase, "ScS"))    return 42;
    if (streq(phase, "PcS"))    return 43;
    if (streq(phase, "PKSab"))  return 44;
    if (streq(phase, "PKSbc"))  return 45;
    if (streq(phase, "PKSdf"))  return 46;
    if (streq(phase, "PKKSab")) return 47;
    if (streq(phase, "PKKSbc")) return 48;
    if (streq(phase, "PKKSdf")) return 49;
    if (streq(phase, "SKKSac")) return 50;
    if (streq(phase, "SKKSdf")) return 51;
    if (streq(phase, "SS") ||
        streq(phase, "SnSn"))   return 52;
    if (streq(phase, "S'S'ac") ||
        streq(phase, "S'S'df")) return 53;
    if (streq(phase, "SP")  ||
        streq(phase, "SPg") ||
        streq(phase, "SPb") ||
        streq(phase, "SPn") ||
        streq(phase, "SnP"))    return 54;
    if (streq(phase, "PS")  ||
        streq(phase, "PSg") ||
        streq(phase, "PSb") ||
        streq(phase, "PSn"))    return 55;
    if (streq(phase, "PnS"))    return 56;

    if (delta <= 100) {
        if (streq(phase, "pPdiff") || streq(phase, "pPdif")) return 7;
        if (streq(phase, "sPdiff") || streq(phase, "sPdif")) return 12;
        if (streq(phase, "pSdiff") || streq(phase, "pSdif")) return 36;
        if (streq(phase, "sSdiff") || streq(phase, "sSdif")) return 39;
    }
    if (delta > 100) {
        if (streq(phase, "pPdiff") || streq(phase, "sPdiff") ||
            streq(phase, "pPdif")  || streq(phase, "sPdif")) return 2;
        if (streq(phase, "pSdiff") || streq(phase, "sSdiff") ||
            streq(phase, "pSdif")  || streq(phase, "sSdif")) return 33;
    }
    if (delta <= 165) {
        if (streq(phase, "PKPdiff")  || streq(phase, "PKPdif"))  return 4;
        if (streq(phase, "pPKPdiff") || streq(phase, "pPKPdif")) return 9;
        if (streq(phase, "sPKPdiff") || streq(phase, "sPKPdif")) return 14;
    }
    return -1;
}

/*
 *  Title:
 *     ReadEllipticityCorrectionTable
 *  Synopsis:
 *     Reads ak135 ellipticity correction coefficients table
 *      Kennett, B.L.N. and O. Gudmundsson, 1996,
 *         Ellipticity corrections for seismic phases,
 *         Geophys. J. Int., 127, 40-48.
 *     The tau corrections are stored at 5 degree intervals in distance
 *         and at the depths of 0, 100, 200, 300, 500, 700 km.
 *  Input Arguments:
 *     filename - filename
 *  Output Arguments:
 *     numECPhases - number of phases with ellipticity correction
 *  Return:
 *     ec - pointer to ec_coef structure or NULL on error
 *  Calls:
 *     AllocateFloatMatrix, FreeEllipticityCorrectionTable
 *  Called by:
 *      ReadAuxDataFiles
 */
EC_COEF *ReadEllipticityCorrectionTable(int *numECPhases, char *filename)
{
    FILE *fp;
    EC_COEF *ec = (EC_COEF *)NULL;
    char buf[LINLEN], phase[PHALEN];
    int num_pha = 0, num_dist = 0, numDepthSamples = 6;
    int i, j, k;
    double mindist = 0., maxdist = 0., d = 0., t = 0.;
/*
 *  open ellipticity correction file and get number of phases
 */
    if ((fp = fopen(filename, "r")) == NULL) {
        fprintf(logfp, "ReadEllipticityCorrectionTable: cannot open %s\n", filename);
        fprintf(errfp, "ReadEllipticityCorrectionTable: cannot open %s\n", filename);
        errorcode = 2;
        return (EC_COEF *)NULL;
    }
    for (;;) {
        if (!fgets(buf, sizeof(buf), fp))             /* get next input line */
            break;                                            /* reached EOF */
        if (isalpha(buf[0])) num_pha++;                      /* count phases */
    }
    rewind(fp);
    *numECPhases = num_pha;
/*
 *  memory allocation
 */
    ec = (EC_COEF *)calloc(num_pha, sizeof(EC_COEF));
    if (ec == NULL) {
        fprintf(logfp, "ReadEllipticityCorrectionTable: cannot allocate memory\n");
        fprintf(errfp, "ReadEllipticityCorrectionTable: cannot allocate memory\n");
        fclose(fp);
        errorcode = 1;
        return (EC_COEF *)NULL;
    }
/*
 *  populate ec structure
 */
    for (i = 0; i < num_pha; i++) {
        fscanf(fp, "%s%d%lf%lf", phase, &num_dist, &mindist, &maxdist);
        strcpy(ec[i].phase, phase);
        ec[i].numDistanceSamples = num_dist;
        ec[i].mindist = mindist;
        ec[i].maxdist = maxdist;
        ec[i].numDepthSamples = numDepthSamples;
        ec[i].depth[0] = 0.;
        ec[i].depth[1] = 100.;
        ec[i].depth[2] = 200.;
        ec[i].depth[3] = 300.;
        ec[i].depth[4] = 500.;
        ec[i].depth[5] = 700.;

        ec[i].delta = (double *)calloc(num_dist, sizeof(double));
        ec[i].t0 = AllocateFloatMatrix(num_dist, numDepthSamples);
        ec[i].t1 = AllocateFloatMatrix(num_dist, numDepthSamples);
        if ((ec[i].t2 = AllocateFloatMatrix(num_dist, numDepthSamples)) == NULL) {
            FreeEllipticityCorrectionTable(ec, num_pha);
            return (EC_COEF *) NULL;
        }
        for (j = 0; j < num_dist; j++) {
            fscanf(fp, "%lf", &d);
            ec[i].delta[j] = d;
            for (k = 0; k < numDepthSamples; k++) {
                fscanf(fp, "%lf", &t);
                ec[i].t0[j][k] = t;
            }
            for (k = 0; k < numDepthSamples; k++) {
                fscanf(fp, "%lf", &t);
                ec[i].t1[j][k] = t;
            }
            for (k = 0; k < numDepthSamples; k++) {
                fscanf(fp, "%lf", &t);
                ec[i].t2[j][k] = t;
            }
        }
    }
    fclose(fp);
    return ec;
}

/*
 *  Title:
 *     FreeEllipticityCorrectionTable
 *  Synopsis:
 *     Frees memory allocated to ec_coef structure.
 *  Input Arguments:
 *      ec         - ellipticity correction coefs
 *      numECPhases - number of distinct phases
 *  Calls:
 *     FreeFloatMatrix, Free
 *  Called by:
 *      ReadAuxDataFiles, main
 */
void FreeEllipticityCorrectionTable(EC_COEF *ec, int numECPhases)
{
    int i;
    for (i = 0; i < numECPhases; i++) {
        FreeFloatMatrix(ec[i].t2);
        FreeFloatMatrix(ec[i].t1);
        FreeFloatMatrix(ec[i].t0);
        Free(ec[i].delta);
    }
    Free(ec);
}

