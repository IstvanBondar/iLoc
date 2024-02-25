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
 *    ReadAuxDataFiles
 *
 */
/*
 * Local functions
 */
static int ReadGlobal1DModelPhaseList(char *filename);
static int ReadIASPEIPhaseMap(char *filename);

/*
 *  Title:
 *     ReadAuxDataFiles
 *  Synopsis:
 *     read data files from auxiliary data directory
 *        $ILOCROOT/auxdata/iLocpars/ReadGlobal1DModelPhaseList.txt
 *        $ILOCROOT/auxdata/iLocpars/IASPEIPhaseMap.txt
 *        $ILOCROOT/auxdata/<TTimeTable>/[*].tab
 *        $ILOCROOT/auxdata/<TTimeTable>/ELCOR.dat
 *        $ILOCROOT/auxdata/topo/etopo5_bed_g_i2.bin
 *        $ILOCROOT/auxdata/FlinnEngdahl/FE.dat
 *        $ILOCROOT/auxdata/FlinnEngdahl/DefaultDepth0.5.grid
 *        $ILOCROOT/auxdata/FlinnEngdahl/GRNDefaultDepth.<TTimeTable>.dat
 *        $ILOCROOT/auxdata/variogram/variogram.model
 *        $ILOCROOT/auxdata/magnitude/<mbQtable>.dat
 *  Input Arguments:
 *     auxdir  - pathname for the auxiliary data files directory
 *  Output Arguments:
 *     ismbQ        - use magnitude attenuation table? (0/1)
 *     mbQp         - pointer to MAGQ structure
 *     fep          - pointer to FE structure
 *     gres         - grid spacing in default depth grid
 *     ngrid        - number of grid points in default depth grid
 *     DepthGrid    - pointer to default depth grid (lat, lon, depth)
 *     numECPhases - number of phases with ellipticity correction
 *     ec           - ellipticity correction coefs
 *     TTtables     - pointer to travel-time tables
 *     variogramp   - pointer to variogram structure
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     main
 *  Calls:
 *     ReadGlobal1DModelPhaseList, ReadIASPEIPhaseMap, ReadEtopo1,
 *     ReadFlinnEngdahl, ReadDefaultDepthGregion, ReadDefaultDepthGrid,
 *     ReadEllipticityCorrectionTable, ReadTTtables,
 *     ReadMagnitudeQ, ReadVariogram, FreeFlinnEngdahl,
 *     FreeEllipticityCorrectionTable, FreeTTtables, Free, FreeFloatMatrix,
 *     FreeShortMatrix
 */
int ReadAuxDataFiles(char *auxdir, int *ismbQ, MAGQ *mbQp, FE *fep,
        double **GrnDepth, double *gres, int *ngrid, double ***DepthGrid,
        short int ***topo, int *numECPhases, EC_COEF *ec[],
        TT_TABLE *TTtables[], VARIOGRAM *variogramp)
{
    extern char EtopoFile[FILENAMELEN];           /* filename for ETOPO file */
    extern char TTimeTable[VALLEN];                /* travel time table name */
    extern char mbQtable[VALLEN];              /* magnitude correction table */
    char filename[FILENAMELEN];
    char dirname[FILENAMELEN];
    double *gd = (double *)NULL;
/*
 *  Read PhaseConfig.txt file from auxdir/iLocpars directory
 */
    if (streq(TTimeTable, "iasp91")) {
        sprintf(dirname, "%s/iasp91", auxdir);
        fprintf(logfp, "model=iasp91\n");
    }
    else if (streq(TTimeTable, "ak135")) {
        sprintf(dirname, "%s/ak135", auxdir);
        fprintf(logfp, "model=ak135\n");
    }
    else {
        fprintf(errfp, "Bad TTimeTable %s in config.txt\n", TTimeTable);
        return 1;
    }
/*
 *  Read global 1D model phaselist file from auxdir/iLocpars directory
 */
    sprintf(filename, "%s/iLocpars/Global1DModelPhaseList.txt", auxdir);
    fprintf(logfp, "    Global1DModelPhaseList: %s\n", filename);
    if (ReadGlobal1DModelPhaseList(filename))
        return 1;
/*
 *  Read IASPEIPhaseMap.txt file from auxdir/iLocpars directory
 */
    sprintf(filename, "%s/iLocpars/IASPEIPhaseMap.txt", auxdir);
    fprintf(logfp, "    IASPEIPhaseMap: %s\n", filename);
    if (ReadIASPEIPhaseMap(filename))
        return 1;
/*
 *  Read Flinn-Engdahl region numbers and default depth files
 *  from auxdir/FlinnEngdahl directory
 */
    sprintf(filename, "%s/FlinnEngdahl/FE.dat", auxdir);
    fprintf(logfp, "    ReadFlinnEngdahl: %s\n", filename);
    if (ReadFlinnEngdahl(filename, fep))
        return 1;
    sprintf(filename, "%s/FlinnEngdahl/GRNDefaultDepth.%s.dat",
            auxdir, TTimeTable);
    fprintf(logfp, "    ReadDefaultDepthGregion: %s\n", filename);
    if ((gd = ReadDefaultDepthGregion(filename)) == NULL) {
        fprintf(errfp, "Cannot read default GRN depths!\n");
        FreeFlinnEngdahl(fep);
        return 1;
    }
    sprintf(filename, "%s/FlinnEngdahl/DefaultDepth0.5.grid", auxdir);
    fprintf(logfp, "    ReadDefaultDepthGrid: %s\n", filename);
    if ((*DepthGrid = ReadDefaultDepthGrid(filename, gres, ngrid)) == NULL) {
        fprintf(errfp, "Cannot read default depth grid!\n");
        FreeFlinnEngdahl(fep); Free(gd);
        return 1;
    }
/*
 *  Read travel-time tables
 */
    if (streq(TTimeTable, "iasp91")) {
/*
 *      read iasp91 ellipticity correction file
 */
        sprintf(filename, "%s/ELCOR.dat", dirname);
        fprintf(logfp, "    ReadEllipticityCorrectionTable: %s\n", filename);
        if ((*ec = ReadEllipticityCorrectionTable(numECPhases, filename)) == NULL) {
            fprintf(errfp, "Cannot read iasp91 ellipticity correction file %s\n",
                    filename);
            FreeFlinnEngdahl(fep); Free(gd);
            FreeFloatMatrix(*DepthGrid);
            return 1;
        }
/*
 *      read iasp91 TT tables
 */
        fprintf(logfp, "    ReadTTtables: %s\n", dirname);
        if ((*TTtables = ReadTTtables(dirname)) == NULL) {
            fprintf(errfp, "Cannot read iasp91 TT tables!\n");
            FreeFlinnEngdahl(fep); Free(gd);
            FreeFloatMatrix(*DepthGrid);
            FreeEllipticityCorrectionTable(*ec, *numECPhases);
            return 1;
        }
    }
    else {
/*
 *      read ak135 ellipticity correction file
 */
        sprintf(filename, "%s/ELCOR.dat", dirname);
        fprintf(logfp, "    ReadEllipticityCorrectionTable: %s\n", filename);
        if ((*ec = ReadEllipticityCorrectionTable(numECPhases, filename)) == NULL) {
            fprintf(errfp, "Cannot read ak135 ellipticity correction file %s\n",
                    filename);
            FreeFlinnEngdahl(fep); Free(gd);
            FreeFloatMatrix(*DepthGrid);
            return 1;
        }
/*
 *      read ak135 TT tables
 */
        fprintf(logfp, "    ReadTTtables: %s\n", dirname);
        if ((*TTtables = ReadTTtables(dirname)) == NULL) {
            fprintf(errfp, "Cannot read ak135 TT tables!\n");
            FreeEllipticityCorrectionTable(*ec, *numECPhases);
            FreeFlinnEngdahl(fep); Free(gd);
            FreeFloatMatrix(*DepthGrid);
            return 1;
        }
    }
/*
 *  read ETOPO1 file for bounce point corrections from auxdir/topo directory
 */
    sprintf(filename, "%s/topo/%s", auxdir, EtopoFile);
    fprintf(logfp, "    ReadEtopo1: %s\n", filename);
    if ((*topo = ReadEtopo1(filename)) == NULL) {
        fprintf(errfp, "Cannot read ETOPO file %s\n", filename);
        if (*TTtables != NULL) FreeTTtables(*TTtables);
        if (*ec != NULL) FreeEllipticityCorrectionTable(*ec, *numECPhases);
        FreeFlinnEngdahl(fep); Free(gd);
        FreeFloatMatrix(*DepthGrid);
        return 1;
    }
/*
 *  Read magnitude attenuation Q(d, h) table from auxdir/magnitude directory
 */
    if (strcmp(mbQtable, "none")) {
        sprintf(filename, "%s/magnitude/%smbQ.dat", auxdir, mbQtable);
        fprintf(logfp, "    ReadMagnitudeQ: %s\n", filename);
        if (ReadMagnitudeQ(filename, mbQp)) {
            fprintf(errfp, "Cannot read magnitude Q file %s\n", filename);
            if (*TTtables != NULL) FreeTTtables(*TTtables);
            if (*ec != NULL) FreeEllipticityCorrectionTable(*ec, *numECPhases);
            FreeFlinnEngdahl(fep); Free(gd);
            FreeFloatMatrix(*DepthGrid);
            FreeShortMatrix(*topo);
            return 1;
        }
        if (strstr(mbQtable, "GR")) *ismbQ = 1;
        if (strstr(mbQtable, "VC")) *ismbQ = 2;
        if (strstr(mbQtable, "MB")) *ismbQ = 2;
    }
/*
 *  Read generic variogram model from auxdir/variogram directory
 */
    sprintf(filename, "%s/variogram/variogram.model", auxdir);
    fprintf(logfp, "    ReadVariogram: %s\n", filename);
    if (ReadVariogram(filename, variogramp)) {
        fprintf(errfp, "Cannot read variogram file %s\n", filename);
        if (*TTtables != NULL) FreeTTtables(*TTtables);
        if (*ec != NULL) FreeEllipticityCorrectionTable(*ec, *numECPhases);
        FreeFlinnEngdahl(fep); Free(gd);
        FreeFloatMatrix(*DepthGrid);
        FreeShortMatrix(*topo);
        if (*ismbQ) {
            Free(mbQp->deltas);
            Free(mbQp->depths);
            FreeFloatMatrix(mbQp->q);
        }
        return 1;
    }
    *GrnDepth = gd;
    return 0;
}

/*
 *  Title:
 *     ReadGlobal1DModelPhaseList
 *  Synopsis:
 *     Read velocity model parameters and list of phases with TT tables
 *  Input Arguments:
 *     filename - pathname for the Global1DModelPhaseList file
 *  Return:
 *     0/1 for success/error
 *  Notes:
 *     Each line can either have a parameter and its value, eg: Moho = 33
 *        or be part of a list ended by a blank line
 *  Called by:
 *     ReadAuxDataFiles
 */
static int ReadGlobal1DModelPhaseList(char *filename)
{
    extern double Moho;                                     /* depth of Moho */
    extern double Conrad;                                 /* depth of Conrad */
    extern double MaxHypocenterDepth;                /* max hypocenter depth */
    extern double PSurfVel;         /* Pg velocity for elevation corrections */
    extern double SSurfVel;         /* Sg velocity for elevation corrections */
    extern int numPhaseTT;                               /* number of phases */
    extern char PhaseTT[MAXTTPHA][PHALEN];              /* global phase list */
    FILE *fp;
    char par[PARLEN], value[VALLEN], phase[VALLEN];
    char line[LINLEN];
    int i;
/*
 *  Open model file or return an error.
 */
    if ((fp = fopen(filename, "r")) == NULL) {
        fprintf(errfp, "ABORT: cannot open %s\n", filename);
        fprintf(logfp, "ABORT: cannot open %s\n", filename);
        return 1;
    }
    while (fgets(line, LINLEN, fp)) {
/*
 *      skip blank lines or comments
 */
        if (sscanf(line, "%s", par) < 1) continue;
        if (strncmp(par, "#", 1) == 0)   continue;
/*
 *      parameter = value pairs
 */
        if (sscanf(line, "%s = %s", par, value) == 2) {
            if (streq(par, "Moho"))
                Moho = atof(value);
            else if (streq(par, "Conrad"))
                Conrad = atof(value);
            else if (streq(par, "MaxHypocenterDepth"))
                MaxHypocenterDepth = atof(value);
            else if (streq(par, "PSurfVel"))
                PSurfVel = atof(value);
            else if (streq(par, "SSurfVel"))
                SSurfVel = atof(value);
            else continue;
            if (verbose > 1)
                fprintf(logfp, "    ReadGlobal1DModelPhaseList: %s = %s\n", par, value);
        }
/*
 *      Phase map - end list with a blank line.
 */
        else if (streq(par, "PhaseTT")) {
            i = 0;
            while (fgets(line, LINLEN, fp)) {
                if (sscanf(line, "%s", phase) < 1)
                    break;
                if (strlen(phase) > PHALEN) {
                    fprintf(errfp, "ABORT: phase too long %s\n", line);
                    fprintf(logfp, "ABORT: phase too long %s\n", line);
                    return 1;
                }
                strcpy(PhaseTT[i], phase);
                if (++i > MAXTTPHA) {
                    fprintf(errfp, "ABORT: too many phases in ReadGlobal1DModelPhaseList\n");
                    fprintf(logfp, "ABORT: too many phases in ReadGlobal1DModelPhaseList\n");
                    return 1;
                }
            }
            numPhaseTT = i;
            if (verbose > 1)
                fprintf(logfp, "    read %d codes in PhaseTT\n", i);
        }
    }
    return 0;
}

/*
 *  Title:
 *     ReadIASPEIPhaseMap
 *  Synopsis:
 *     Sets external variables to values read from model file
 *  Input Arguments:
 *     filename - pathname for the model file
 *  Return:
 *     0/1 for success/error
 *  Notes:
 *     Each line can either have a parameter and its value, eg: Moho = 33
 *        or be part of a list ended by a blank line eg PhaseMap
 *  Called by:
 *     ReadAuxDataFiles
 */
static int ReadIASPEIPhaseMap(char *filename)
{
    extern PHASEMAP PhaseMap[MAXPHACODES];           /* IASPEI phase mapping */
    extern int numPhaseMap;                  /* number of phases in PhaseMap */
    extern PHASEWEIGHT PhaseWeight[MAXNUMPHA];      /* prior time measerrors */
    extern int numPhaseWeight;            /* number of phases in PhaseWeight */
    extern char AllowablePhases[MAXTTPHA][PHALEN];       /* allowable phases */
    extern int numAllowablePhases;             /* number of allowable phases */
    extern char firstPphase[MAXTTPHA][PHALEN];      /* first-arriving P list */
    extern int numFirstPphase;          /* number of first-arriving P phases */
    extern char firstSphase[MAXTTPHA][PHALEN];      /* first-arriving S list */
    extern int numFirstSphase;          /* number of first-arriving S phases */
    extern char firstPoptional[MAXTTPHA][PHALEN];   /* optional first P list */
    extern int numFirstPoptional;       /* number of optional first P phases */
    extern char firstSoptional[MAXTTPHA][PHALEN];   /* optional first S list */
    extern int numFirstSoptional;       /* number of optional first S phases */
    extern char PhaseWithoutResidual[MAXNUMPHA][PHALEN];/* no-residual phases*/
    extern int PhaseWithoutResidualNum;      /* number of no-residual phases */
    extern char MBPhase[MAXNUMPHA][PHALEN];            /* phases for mb calc */
    extern int numMBPhase;                            /* number of mb phases */
    extern char MSPhase[MAXNUMPHA][PHALEN];            /* phases for Ms calc */
    extern int numMSPhase;                            /* number of Ms phases */
    extern char MLPhase[MAXNUMPHA][PHALEN];            /* phases for ML calc */
    extern int numMLPhase;                            /* number of ML phases */

    FILE *fp;
    char line[LINLEN];
    char par[PARLEN];
    char value[VALLEN];
    char ReportedPhase[VALLEN], phase[VALLEN];
    int numval, i;
    double x;
/*
 *  Open IASPEIPhaseMap file or return an error.
 */
    if ((fp = fopen(filename, "r")) == NULL) {
        fprintf(errfp, "ABORT: cannot open %s\n", filename);
        fprintf(logfp, "ABORT: cannot open %s\n", filename);
        return 1;
    }
/*
 *  Read IASPEIPhaseMap file
 */
    while (fgets(line, LINLEN, fp)) {
/*
 *      skip blank lines or comments
 */
        if (sscanf(line, "%s", par) < 1) continue;
        if (strncmp(par, "#", 1) == 0)    continue;
/*
 *      Phase map - end list with a blank line.
 */
        if (streq(par, "PhaseMap")) {
            i = 0;
            while (fgets(line, LINLEN, fp)) {
                if (sscanf(line, "%s", par) < 1)
                    break;
                if (sscanf(line, "%s%s", ReportedPhase, phase) < 2) {
                    strcpy(PhaseMap[i].ReportedPhase, ReportedPhase);
                    strcpy(PhaseMap[i].phase, "");
                }
                else {
                    if (strlen(ReportedPhase) > PHALEN ||
                        strlen(phase) > PHALEN) {
                        fprintf(errfp, "ABORT: phase too long %s\n", line);
                        fprintf(logfp, "ABORT: phase too long %s\n", line);
                        return 1;
                    }
                    strcpy(PhaseMap[i].ReportedPhase, ReportedPhase);
                    strcpy(PhaseMap[i].phase, phase);
                }
                if (++i > MAXPHACODES) {
                    fprintf(errfp, "ABORT: too many phases\n");
                    fprintf(logfp, "ABORT: too many phases\n");
                    return 1;
                }
            }
            numPhaseMap = i;
            if (verbose > 1)
                fprintf(logfp, "    read %d codes into PhaseMap\n", i);
        }
/*
 *      List of allowable phase codes
 */
        else if (streq(par, "AllowablePhases")) {
            i = 0;
            while (fgets(line, LINLEN, fp)) {
                if (sscanf(line, "%s", phase) < 1)
                    break;
                if (strlen(phase) > PHALEN) {
                    fprintf(errfp, "ABORT: phase too long %s\n", line);
                    fprintf(logfp, "ABORT: phase too long %s\n", line);
                    return 1;
                }
                strcpy(AllowablePhases[i], phase);
                if (++i > MAXTTPHA) {
                    fprintf(errfp, "ABORT: too many PhaseWithoutResidual\n");
                    fprintf(logfp, "ABORT: too many PhaseWithoutResidual\n");
                    return 1;
                }
            }
            numAllowablePhases = i;
            if (verbose > 1)
                fprintf(logfp, "    read %d codes in AllowablePhases\n", i);
        }
/*
 *      List of allowable first-arriving P phase codes
 */
        else if (streq(par, "AllowableFirstP")) {
            i = 0;
            while (fgets(line, LINLEN, fp)) {
                if (sscanf(line, "%s", phase) < 1)
                    break;
                if (strlen(phase) > PHALEN) {
                    fprintf(errfp, "ABORT: phase too long %s\n", line);
                    fprintf(logfp, "ABORT: phase too long %s\n", line);
                    return 1;
                }
                strcpy(firstPphase[i], phase);
                if (++i > MAXTTPHA) {
                    fprintf(errfp, "ABORT: too many PhaseWithoutResidual\n");
                    fprintf(logfp, "ABORT: too many PhaseWithoutResidual\n");
                    return 1;
                }
            }
            numFirstPphase = i;
            if (verbose > 1)
                fprintf(logfp, "    read %d codes in AllowableFirstP\n", i);
        }
/*
 *      List of optional first-arriving P phase codes
 */
        else if (streq(par, "OptionalFirstP")) {
            i = 0;
            while (fgets(line, LINLEN, fp)) {
                if (sscanf(line, "%s", phase) < 1)
                    break;
                if (strlen(phase) > PHALEN) {
                    fprintf(errfp, "ABORT: phase too long %s\n", line);
                    fprintf(logfp, "ABORT: phase too long %s\n", line);
                    return 1;
                }
                strcpy(firstPoptional[i], phase);
                if (++i > MAXTTPHA) {
                    fprintf(errfp, "ABORT: too many PhaseWithoutResidual\n");
                    fprintf(logfp, "ABORT: too many PhaseWithoutResidual\n");
                    return 1;
                }
            }
            numFirstPoptional = i;
            if (verbose > 1)
                fprintf(logfp, "    read %d codes in OptionalFirstP\n", i);
        }
/*
 *      List of allowable first-arriving S phase codes
 */
        else if (streq(par, "AllowableFirstS")) {
            i = 0;
            while (fgets(line, LINLEN, fp)) {
                if (sscanf(line, "%s", phase) < 1)
                    break;
                if (strlen(phase) > PHALEN) {
                    fprintf(errfp, "ABORT: phase too long %s\n", line);
                    fprintf(logfp, "ABORT: phase too long %s\n", line);
                    return 1;
                }
                strcpy(firstSphase[i], phase);
                if (++i > MAXTTPHA) {
                    fprintf(errfp, "ABORT: too many PhaseWithoutResidual\n");
                    fprintf(logfp, "ABORT: too many PhaseWithoutResidual\n");
                    return 1;
                }
            }
            numFirstSphase = i;
            if (verbose > 1)
                fprintf(logfp, "    read %d codes in AllowableFirstS\n", i);
        }
/*
 *      List of optional first-arriving P phase codes
 */
        else if (streq(par, "OptionalFirstS")) {
            i = 0;
            while (fgets(line, LINLEN, fp)) {
                if (sscanf(line, "%s", phase) < 1)
                    break;
                if (strlen(phase) > PHALEN) {
                    fprintf(errfp, "ABORT: phase too long %s\n", line);
                    fprintf(logfp, "ABORT: phase too long %s\n", line);
                    return 1;
                }
                strcpy(firstSoptional[i], phase);
                if (++i > MAXTTPHA) {
                    fprintf(errfp, "ABORT: too many PhaseWithoutResidual\n");
                    fprintf(logfp, "ABORT: too many PhaseWithoutResidual\n");
                    return 1;
                }
            }
            numFirstSoptional = i;
            if (verbose > 1)
                fprintf(logfp, "    read %d codes in OptionalFirstS\n", i);
        }
/*
 *      List of phase codes that shouldn't have residuals calculated
 */
        else if (streq(par, "PhaseWithoutResidual")) {
            i = 0;
            while (fgets(line, LINLEN, fp)) {
                if (sscanf(line, "%s", phase) < 1)
                    break;
                if (strlen(phase) > PHALEN) {
                    fprintf(errfp, "ABORT: phase too long %s\n", line);
                    fprintf(logfp, "ABORT: phase too long %s\n", line);
                    return 1;
                }
                strcpy(PhaseWithoutResidual[i], phase);
                if (++i > MAXNUMPHA) {
                    fprintf(errfp, "ABORT: too many PhaseWithoutResidual\n");
                    fprintf(logfp, "ABORT: too many PhaseWithoutResidual\n");
                    return 1;
                }
            }
            PhaseWithoutResidualNum = i;
            if (verbose > 1)
                fprintf(logfp, "    read %d codes in PhaseWithoutResidual\n", i);
        }
/*
 *      distance-phase dependent a priori measurement errors
 *      time, azimuth and slowness
 */
        else if (streq(par, "PhaseWeight")) {
            i = 0;
            while (fgets(line, LINLEN, fp)) {
                if (sscanf(line, "%s", par) < 1)
                    break;
                numval = sscanf(line, "%s%lf%lf%lf%lf%lf",
                    phase, &PhaseWeight[i].delta1, &PhaseWeight[i].delta2,
                    &PhaseWeight[i].deltim, &PhaseWeight[i].delaz,
                    &PhaseWeight[i].delslo);
                if (numval < 6) {
                    fprintf(errfp, "ABORT: incomplete entry %s\n", line);
                    fprintf(logfp, "ABORT: incomplete entry %s\n", line);
                    return 1;
                }
                if (strlen(phase) > PHALEN) {
                    fprintf(errfp, "ABORT: phase too long %s\n", line);
                    fprintf(logfp, "ABORT: phase too long %s\n", line);
                    return 1;
                }
                strcpy(PhaseWeight[i].phase, phase);
                if (++i > MAXNUMPHA) {
                    fprintf(errfp, "ABORT: too many phases\n");
                    fprintf(logfp, "ABORT: too many phases\n");
                    return 1;
                }
            }
            numPhaseWeight = i;
            if (verbose > 1)
                fprintf(logfp, "    read %d lines into PhaseWeight\n",i);
        }
/*
 *      List of phase codes that contribute to mb calculation.
 */
        else if (streq(par, "MBPhase")) {
            i = 0;
            while (fgets(line, LINLEN, fp)) {
                if (sscanf(line, "%s", phase) < 1)
                    break;
                if (strlen(phase) > PHALEN) {
                    fprintf(errfp, "ABORT: phase too long %s\n", line);
                    fprintf(logfp, "ABORT: phase too long %s\n", line);
                    return 1;
                }
                strcpy(MBPhase[i], phase);
                if (++i > MAXNUMPHA) {
                    fprintf(errfp, "ABORT: too many MBPhase\n");
                    fprintf(logfp, "ABORT: too many MBPhase\n");
                    return 1;
                }
            }
            numMBPhase = i;
            if (verbose > 1)
                fprintf(logfp, "    read %d codes in MBPhase\n", i);
        }
/*
 *      List of phase codes that contribute to Ms calculation.
 */
        else if (streq(par, "MSPhase")) {
            i = 0;
            while (fgets(line, LINLEN, fp)) {
                if (sscanf(line, "%s", phase) < 1)
                    break;
                if (strlen(phase) > PHALEN) {
                    fprintf(errfp, "ABORT: phase too long %s\n", line);
                    fprintf(logfp, "ABORT: phase too long %s\n", line);
                    return 1;
                }
                strcpy(MSPhase[i], phase);
                if (++i > MAXNUMPHA) {
                    fprintf(errfp, "ABORT: too many MSPhase\n");
                    fprintf(logfp, "ABORT: too many MSPhase\n");
                    return 1;
                }
            }
            numMSPhase = i;
            if (verbose > 1)
                fprintf(logfp, "    read %d codes in MSPhase\n", i);
        }
/*
 *      List of phase codes that contribute to ML calculation.
 */
        else if (streq(par, "MLPhase")) {
            i = 0;
            while (fgets(line, LINLEN, fp)) {
                if (sscanf(line, "%s", phase) < 1)
                    break;
                if (strlen(phase) > PHALEN) {
                    fprintf(errfp, "ABORT: phase too long %s\n", line);
                    fprintf(logfp, "ABORT: phase too long %s\n", line);
                    return 1;
                }
                strcpy(MLPhase[i], phase);
                if (++i > MAXNUMPHA) {
                    fprintf(errfp, "ABORT: too many MLPhase\n");
                    fprintf(logfp, "ABORT: too many MLPhase\n");
                    return 1;
                }
            }
            numMLPhase = i;
            if (verbose > 1)
                fprintf(logfp, "    read %d codes in MLPhase\n", i);
        }
        else
            continue;
    }
    fclose(fp);
    return 0;
}
