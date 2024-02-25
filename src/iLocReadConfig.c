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
 *    ReadConfigFile
 *    ReadInstructionFile
 */

/*
 *  Title:
 *     ReadConfigFile
 *  Synopsis:
 *     Sets external variables to values read from configuration file
 *  Input Arguments:
 *     auxdir   - pathname for the auxiliary data files directory
 *     homedir  - pathname for the user's home directory
 *  Return:
 *     0/1 on success/error
 *  Notes:
 *     Each line can have one configuration variable and its value, eg:
 *        MaxIterations = 16 (note that spaces around '=')
 *     Diagnostics from this function get written to stderr as log and error
 *        files are not yet open (their names are read in here).
 *  Called by:
 *     main
 *  Calls:
 *     Free
 */
int ReadConfigFile(char *auxdir, char *homedir)
{
/*
 *  I/O
 */
    extern char logfile[FILENAMELEN];                        /* log filename */
    extern char errfile[FILENAMELEN];                      /* error filename */
    extern char StationFile[FILENAMELEN];   /* stafile when not read from db */
    extern int UpdateDB;                         /* write result to database */
    extern char NextidDB[VALLEN];           /* get new ids from this account */
    extern int repid;     /* 'reporter' field for new hypocentres and assocs */
    extern char InputDB[VALLEN];   /* read data from this DB account, if any */
    extern char OutputDB[VALLEN];        /* write results to this DB account */
    extern char DBuser[VALLEN];                             /* database user */
    extern char DBpasswd[VALLEN];                         /* database passwd */
    extern char DBname[VALLEN];                             /* database host */
    extern char OutAgency[VALLEN];  /* author for new hypocentres and assocs */
    extern char InAgency[VALLEN];                 /* author for input assocs */
    extern char KMLBulletinFile[FILENAMELEN];/* output KML bulletin filename */
    extern double KMLLatitude;            /* KML bulletin viewpoint latitude */
    extern double KMLLongitude;          /* KML bulletin viewpoint longitude */
    extern double KMLRange;              /* KML bulletin viewpoint elevation */
    extern double KMLHeading;              /* KML bulletin viewpoint azimuth */
    extern double KMLTilt;                    /* KML bulletin viewpoint tilt */
    extern int ZipKML;                    /* zip KMLEventFile and remove kml */
/*
 *  TT
 */
    extern char TTimeTable[VALLEN];                /* travel time table name */
    extern char LocalVmodelFile[FILENAMELEN];   /* pathname for local vmodel */
    extern double DefaultDepth;     /* used if seed hypocentre depth is NULL */
    extern int UseLocalTT;                       /* use local TT predictions */
    extern double MaxLocalTTDelta;       /* use local TT up to this distance */
//    extern int LocalTTfromRSTT;                  /* local TT from RSTT model */
    extern int UpdateLocalTT;                     /* static/dynamic local TT */
/*
 *  ETOPO
 */
    extern char EtopoFile[FILENAMELEN];           /* filename for ETOPO file */
    extern int EtopoNlon;            /* number of longitude samples in ETOPO */
    extern int EtopoNlat;             /* number of latitude samples in ETOPO */
    extern double EtopoRes;                             /* cellsize in ETOPO */
/*
 *  iteration control
 */
    extern int MinIterations;                    /* min number of iterations */
    extern int MaxIterations;                    /* max number of iterations */
    extern int MinNdefPhases;                        /* min number of phases */
    extern double SigmaThreshold;         /* to exclude phases from solution */
    extern int DoCorrelatedErrors;          /* account for correlated errors */
    extern int AllowDamping;             /* allow damping in LSQR iterations */
    extern double ConfidenceLevel;     /* confidence level for uncertainties */
/*
 *  depth-phase depth solution requirements
 */
    extern int MinDepthPhases;/* min # of depth phases for depth-phase depth */
    extern int MindDepthPhaseAgencies;/* min agencies reporting depth phases */
/*
 *  depth resolution
 */
    extern double MaxLocalDistDeg;              /* max local distance (degs) */
    extern int MinLocalStations;      /* min stations within MaxLocalDistDeg */
    extern double MaxSPDistDeg;                   /* max S-P distance (degs) */
    extern int MinSPpairs;                  /* min number of S-P phase pairs */
    extern int MinCorePhases;   /* min number of core reflections ([PS]c[PS] */
/*
 *  maximum allowable depth error for free depth solutions
 */
    extern double MaxShallowDepthError;  /* max error for crustal free-depth */
    extern double MaxDeepDepthError;        /* max error for deep free-depth */
/*
 *  magnitudes
 */
    extern char mbQtable[VALLEN];              /* magnitude correction table */
    extern int CalculateML;                                 /* calculate ML? */
    extern int CalculatemB;                                 /* calculate mB? */
    extern double BBmBMinDistDeg;            /* min delta for calculating mB */
    extern double BBmBMaxDistDeg;            /* max delta for calculating mB */
    extern double mbMinDistDeg;              /* min delta for calculating mb */
    extern double mbMaxDistDeg;              /* max delta for calculating mb */
    extern double MSMinDistDeg;              /* min delta for calculating Ms */
    extern double MSMaxDistDeg;              /* max delta for calculating Ms */
    extern double mbMinPeriod;              /* min period for calculating mb */
    extern double mbMaxPeriod;              /* max period for calculating mb */
    extern double MSMinPeriod;              /* min period for calculating Ms */
    extern double MSMaxPeriod;              /* max period for calculating Ms */
    extern double MSMaxDepth;                /* max depth for calculating Ms */
    extern double MSPeriodRange;   /* MSH period tolerance around MSZ period */
    extern double MagnitudeRangeLimit;   /* warn about outliers beyond range */
    extern double MLMaxDistkm;               /* max delta for calculating ML */
    extern int MinNetmagSta;             /* min number of stamags for netmag */
    extern double MagMaxTimeResidual;    /* max allowable timeres for stamag */
/*
 *  NA grid search parameters
 */
    extern int DoGridSearch;     /* perform grid search for initial location */
    extern double NAsearchRadius;   /* search radius around preferred origin */
    extern double NAsearchDepth;/* search radius (km) around preferred depth */
    extern double NAsearchOT;       /* search radius (s) around preferred OT */
    extern double NAlpNorm;            /* p-value for norm to compute misfit */
    extern int NAiterMax;                        /* max number of iterations */
    extern int NAinitialSample;                    /* size of initial sample */
    extern int NAnextSample;                   /* size of subsequent samples */
    extern int NAcells;                   /* number of cells to be resampled */
    extern long iseed;                                 /* random number seed */
/*
 *  agencies whose hypocenters not to be used in setting initial hypocentre
 */
    extern char DoNotUseAgencies[MAXBUF][AGLEN];
    extern int DoNotUseAgenciesNum;
/*
 *  RSTT parameters
 */
    extern char RSTTmodel[FILENAMELEN];      /* pathname for RSTT model file */
    extern int UseRSTTPnSn;                   /* use RSTT Pn/Sn predictions? */
    extern int UseRSTTPgLg;                   /* use RSTT Pg/Lg predictions? */
    extern int UseRSTT;                             /* use RSTT predictions? */

    FILE *fp;
    char filename[FILENAMELEN];
    char par[PARLEN], value[VALLEN], *s = NULL, *line = NULL;
    ssize_t nb = (ssize_t)0;
    size_t nbytes = 0;
    int i, j;
/*
 *  Default parameter values
 */
    strcpy(logfile, "stdout");
    strcpy(errfile, "stderr");
    strcpy(StationFile, "");
    strcpy(OutAgency, "ILOC");
    strcpy(InAgency, "ISC");
    strcpy(TTimeTable, "ak135");
    strcpy(EtopoFile, "etopo5_bed_g_i2.bin");
    strcpy(mbQtable, "GR");
    strcpy(KMLBulletinFile, "");
    EtopoNlon = 4321;
    EtopoNlat = 2161;
    EtopoRes = 0.0833333;
    DefaultDepth = 0.;
    MinNdefPhases = 4;
    ConfidenceLevel = 90.;
    UpdateDB = 0;
    KMLLatitude = 47.49833;
    KMLLongitude = 19.04045;
    KMLRange = 1000000.;
    KMLHeading = 0.;
    KMLTilt = 0.;
    ZipKML = 1;
    MinIterations = 4;
    MaxIterations = 20;
    SigmaThreshold = 4.;
    DoCorrelatedErrors = 1;
    AllowDamping = 1;
    MinDepthPhases = 5;
    MindDepthPhaseAgencies = 2;
    MaxLocalDistDeg = 0.2;
    MinLocalStations = 1;
    MaxSPDistDeg = 3.;
    MinSPpairs = 5;
    MinCorePhases = 5;
    MaxShallowDepthError = 30;
    MaxDeepDepthError = 60;
    DoGridSearch = 1;
    NAsearchRadius = 5.;
    NAsearchDepth = 300.;
    NAsearchOT = 30.;
    NAlpNorm = 1.;
    NAiterMax = 10;
    NAinitialSample = 1000;
    NAnextSample = 100;
    NAcells = 20;
    iseed = 5590L;
    CalculatemB = 0;
    CalculateML = 0;
    mbMinDistDeg = 21.;
    mbMaxDistDeg = 100.;
    MSMinDistDeg = 20.;
    MSMaxDistDeg = 160.;
    mbMinPeriod  = 0.3;
    mbMaxPeriod  = 3.;
    MSMinPeriod  = 10.;
    MSMaxPeriod  = 60.;
    MSMaxDepth = 60.;
    MLMaxDistkm = 1000.;
    MSPeriodRange = 5.;
    MagMaxTimeResidual = 10.;
    MagnitudeRangeLimit = 2.2;
    MinNetmagSta = 3;
    DoNotUseAgenciesNum = 0;
    strcpy(RSTTmodel, "");
    UseRSTTPgLg = 1;
    UseRSTTPnSn = 1;
//    LocalTTfromRSTT = 0;
    MaxLocalTTDelta = 3.;
    strcpy(LocalVmodelFile, "");
    strcpy(DBuser, "sysop");
    strcpy(DBpasswd, "sysop");
    strcpy(DBname, "");
/*
 *  Open configuration file
 */
    sprintf(filename, "%s/iLocpars/Config.txt", auxdir);
    if ((fp = fopen(filename, "r")) == NULL) {
        fprintf(stderr, "ReadConfigFile: ABORT: cannot open %s\n", filename);
        return 1;
    }
/*
 *  Read configuration parameters.
 */
    while ((nb = getline(&line, &nbytes, fp)) > 0) {
        if ((j = nb - 2) >= 0) {
            if (line[j] == '\r') line[j] = '\n';   /* replace CR with LF */
        }
/*
 *      skip blank lines or comments
 */
        if (sscanf(line, "%s = %s", par, value) < 2) continue;
        if (strncmp(  par, "#", 1) == 0) continue;
        if (strncmp(value, "#", 1) == 0) continue;
/*
 *      parse parameters
 */
        if (streq(par, "OutAgency"))             strcpy(OutAgency, value);
        else if (streq(par, "InAgency"))         strcpy(InAgency, value);
        else if (streq(par, "repid"))            repid = atoi(value);
        else if (streq(par, "UpdateDB"))         UpdateDB = atoi(value);
        else if (streq(par, "DBuser"))           strcpy(DBuser, value);
        else if (streq(par, "DBpasswd"))         strcpy(DBpasswd, value);
        else if (streq(par, "DBname"))           strcpy(DBname, value);
        else if (streq(par, "InputDB")) {
            strcpy(InputDB, value);
            strcat(InputDB, ".");
        }
        else if (streq(par, "OutputDB")) {
            strcpy(OutputDB, value);
            strcat(OutputDB, ".");
        }
        else if (streq(par, "NextidDB")) {
            strcpy(NextidDB, value);
            strcat(NextidDB, ".");
        }
        else if (streq(par, "KMLBulletinFile")) {
            if (strncmp(value, "~/", 2) == 0)
                sprintf(KMLBulletinFile, "%s/%s", homedir, value);
            else
                strcpy(KMLBulletinFile, value);
        }
        else if (streq(par, "KMLLatitude"))      KMLLatitude = atof(value);
        else if (streq(par, "KMLLongitude"))     KMLLongitude = atof(value);
        else if (streq(par, "KMLRange"))         KMLRange = atof(value);
        else if (streq(par, "KMLHeading"))       KMLHeading = atof(value);
        else if (streq(par, "KMLTilt"))          KMLTilt = atof(value);
        else if (streq(par, "ZipKML"))           ZipKML = atoi(value);
/*
 *      station file directory defaults to auxdir/Stations unless
 *      full pathname is given
 */
        else if (streq(par, "StationFile")) {
            if (strncmp(value, "~/", 2) == 0)
                sprintf(StationFile, "%s/%s", homedir, value);
            else if (strstr(value, "/") || streq(auxdir, "."))
                strcpy(StationFile, value);
            else
                sprintf(StationFile, "%s/Stations/%s", auxdir, value);
        }
        else if (streq(par, "logfile"))          strcpy(logfile, value);
        else if (streq(par, "errfile"))          strcpy(errfile, value);
        else if (streq(par, "ConfidenceLevel"))  ConfidenceLevel = atof(value);
        else if (streq(par, "DoCorrelatedErrors")) DoCorrelatedErrors = atoi(value);
        else if (streq(par, "AllowDamping"))     AllowDamping = atoi(value);
/*
 *      limits
 */
        else if (streq(par, "MinNdefPhases"))    MinNdefPhases = atoi(value);
        else if (streq(par, "SigmaThreshold"))   SigmaThreshold = atof(value);
        else if (streq(par, "MinIterations"))    MinIterations = atoi(value);
        else if (streq(par, "MaxIterations"))    MaxIterations = atoi(value);
/*
 *      TT
 */
        else if (streq(par, "TTimeTable"))       strcpy(TTimeTable, value);
        else if (streq(par, "LocalVmodelFile")) {
            if (strncmp(value, "~/", 2) == 0)
                sprintf(LocalVmodelFile, "%s/%s", homedir, value);
            else if (strstr(value, "/") || streq(auxdir, "."))
                strcpy(LocalVmodelFile, value);
            else
                sprintf(LocalVmodelFile, "%s/localmodels/%s", auxdir, value);
        }
//        else if (streq(par, "LocalTTfromRSTT"))  LocalTTfromRSTT = atoi(value);
        else if (streq(par, "MaxLocalTTDelta"))  MaxLocalTTDelta = atof(value);
/*
 *      ETOPO
 */
        else if (streq(par, "EtopoFile"))        strcpy(EtopoFile, value);
        else if (streq(par, "EtopoNlon"))        EtopoNlon = atoi(value);
        else if (streq(par, "EtopoNlat"))        EtopoNlat = atoi(value);
        else if (streq(par, "EtopoRes"))         EtopoRes = atof(value);
/*
 *      depth control
 */
        else if (streq(par, "DefaultDepth"))     DefaultDepth = atof(value);
        else if (streq(par, "MinDepthPhases"))   MinDepthPhases = atoi(value);
        else if (streq(par, "MindDepthPhaseAgencies")) MindDepthPhaseAgencies = atoi(value);
        else if (streq(par, "MaxLocalDistDeg"))  MaxLocalDistDeg = atof(value);
        else if (streq(par, "MinLocalStations")) MinLocalStations = atoi(value);
        else if (streq(par, "MaxSPDistDeg"))     MaxSPDistDeg = atof(value);
        else if (streq(par, "MinSPpairs"))       MinSPpairs = atoi(value);
        else if (streq(par, "MinCorePhases"))    MinCorePhases = atoi(value);
        else if (streq(par, "MaxShallowDepthError")) MaxShallowDepthError = atof(value);
        else if (streq(par, "MaxDeepDepthError")) MaxDeepDepthError = atof(value);
/*
 *      magnitude determination
 */
        else if (streq(par, "mbQtable"))         strcpy(mbQtable, value);
        else if (streq(par, "BBmBMinDistDeg"))   BBmBMinDistDeg = atof(value);
        else if (streq(par, "BBmBMaxDistDeg"))   BBmBMaxDistDeg = atof(value);
        else if (streq(par, "mbMinDistDeg"))     mbMinDistDeg = atof(value);
        else if (streq(par, "mbMaxDistDeg"))     mbMaxDistDeg = atof(value);
        else if (streq(par, "MSMinDistDeg"))     MSMinDistDeg = atof(value);
        else if (streq(par, "MSMaxDistDeg"))     MSMaxDistDeg = atof(value);
        else if (streq(par, "mbMinPeriod"))      mbMinPeriod = atof(value);
        else if (streq(par, "mbMaxPeriod"))      mbMaxPeriod = atof(value);
        else if (streq(par, "MSMinPeriod"))      MSMinPeriod = atof(value);
        else if (streq(par, "MSMaxPeriod"))      MSMaxPeriod = atof(value);
        else if (streq(par, "MSMaxDepth"))       MSMaxDepth = atof(value);
        else if (streq(par, "MSPeriodRange"))    MSPeriodRange = atof(value);
        else if (streq(par, "MagnitudeRangeLimit")) MagnitudeRangeLimit = atof(value);
        else if (streq(par, "MLMaxDistkm"))      MLMaxDistkm = atof(value);
        else if (streq(par, "MinNetmagSta"))     MinNetmagSta = atoi(value);
        else if (streq(par, "CalculatemB"))      CalculatemB = atoi(value);
        else if (streq(par, "CalculateML"))      CalculateML = atoi(value);
        else if (streq(par, "MagMaxTimeResidual")) MagMaxTimeResidual = atof(value);
/*
 *      NA search parameters
 */
        else if (streq(par, "DoGridSearch"))     DoGridSearch = atoi(value);
        else if (streq(par, "NAsearchRadius"))   NAsearchRadius = atof(value);
        else if (streq(par, "NAsearchDepth"))    NAsearchDepth = atof(value);
        else if (streq(par, "NAsearchOT"))       NAsearchOT = atof(value);
        else if (streq(par, "NAlpNorm"))         NAlpNorm = atof(value);
        else if (streq(par, "NAiterMax"))        NAiterMax = atoi(value);
        else if (streq(par, "NAinitialSample"))  NAinitialSample = atoi(value);
        else if (streq(par, "NAnextSample"))     NAnextSample = atoi(value);
        else if (streq(par, "NAcells"))          NAcells = atoi(value);
        else if (streq(par, "iseed"))            iseed = atol(value);
/*
 *      agencies whose hypocenters not to be used in setting the initial hypo
 */
        else if (streq(par, "NohypoAgencies")) {
            i = 0;
            s = strtok(value, ", ");
            strcpy(DoNotUseAgencies[i++], s);
            while ((s = strtok(NULL, ", ")) != NULL)
                strcpy(DoNotUseAgencies[i++], s);
            DoNotUseAgenciesNum = i;
        }
/*
 *      RSTT parameters
 *      RSTT model file directory defaults to auxdir/RSTTmodel unless
 *      full pathname is given
 */
        else if (streq(par, "RSTTmodel")) {
            if (strncmp(value, "~/", 2) == 0)
                sprintf(RSTTmodel, "%s/%s", homedir, value);
            else if (strstr(value, "/") || streq(auxdir, "."))
                strcpy(RSTTmodel, value);
            else
                sprintf(RSTTmodel, "%s/RSTTmodels/%s", auxdir, value);
        }
        else if (streq(par, "UseRSTTPnSn"))      UseRSTTPnSn = atoi(value);
        else if (streq(par, "UseRSTTPgLg"))      UseRSTTPgLg = atoi(value);
/*
 *      skip unrecognized parameters
 */
        else
            continue;
    }
    fclose(fp);
    Free(line);
/*
 *  use RSTT?
 */
    UseRSTT = 0;
    if (UseRSTTPnSn || UseRSTTPgLg)
        UseRSTT = 1;
/*
 *  use local TT?
 */
//    if (LocalTTfromRSTT) {
//        UseLocalTT = 1;
//        UpdateLocalTT = 1;
//    }
    if (strlen(LocalVmodelFile) > 0) {
//        LocalTTfromRSTT = 0;
        UpdateLocalTT = 0;
        UseLocalTT = 1;
    }
    else {
        UpdateLocalTT = 0;
        UseLocalTT = 0;
    }
    return 0;
}

/*
 *   Title:
 *      ReadInstructionFile
 *   Synopsis:
 *      Parses a line of instructions for evid and options
 *      Parses an instruction line for an evid and its instructions.
 *      Instructions can either be given in the command line or
 *      read from an instruction file. An instruction line is expected
 *      for every event in the form:
 *          isc_evid [par=value [par=value ...]]
 *      where isc_evid is the event identifier, and the par=value pairs
 *      (no space around the equal sign!) denote the optional instruction
 *      name and value pairs.
 *   Input Arguments:
 *      instruction - string containing a single line of instructions
 *      ep          - pointer to event info
 *   Return:
 *      0/1 on success/error
 *   Notes:
 *      One or more options may be given in any order in parameter=value
 *          format separated by white space.
 *      Possible options:
 *          Verbose              - verbose level
 *          StartDepth           - an agency code or a number
 *          StartLat,StartLon    - numbers (must have both)
 *          StartOT              - an agency code or a number
 *          FixDepth             - an agency code or a number or nothing
 *          FixEpicenter         - an agency code or nothing
 *          FixOriginTime        - an agency code or a number or nothing
 *          FixHypocenter        - an agency code or a number or nothing
 *          FixDepthToDepdp      - presence sets flag to 1
 *          FixDepthToDefault    - presence sets flag to 1
 *          InputDB              - read data from this DB account
 *          OutputDB             - write results to this DB account
 *          ISFInputFile         - read ISF input from here
 *          ISFOutputFile        - write ISF output to here
 *          KMLEventFile         - KML event output filename for Google Earth
 *          WriteNAResultsToFile - write NA results to file
 *      The options below can override external parameters in the config file:
 *          MindDepthPhaseAgencies - min agencies reporting depth/core phases
 *          MinDepthPhases       - min # of depth phases for depth-phase depth
 *          MinCorePhases        - min number of core reflections ([PS]c[PS]
 *          MaxLocalDistDeg      - max local distance (degs)
 *          MinLocalStations     - min number of stations within MaxLocalDistDeg
 *          MaxSPDistDeg         - max S-P distance (degs)
 *          MinSPpairs           - min number of S-P phase pairs
 *          UpdateDB             - write results to database?
 *          DoCorrelatedErrors   - account for correlated error structure?
 *          DoGridSearch         - perform initial grid search?
 *          iseed                - random number initial seed
 *          NAsearchRadius       - search radius [deg] around initial epicentre
 *          NAsearchOT           - search radius [s] around initial origin time
 *          NAsearchDepth        - search radius [km] around initial depth
 *          NAinitialSample      - size of initial sample
 *          NAnextSample         - size of subsequent samples
 *          NAcells              - number of cells to be resampled
 *          NAiterMax            - max number of iterations
 *          StationFile          - file of station coordinates
 *          MSPeriodRange        - MSH period tolerance around MSZ period
 *          RSTTmodel            - pathname for RSTT model file
 *          UseRSTTPnSn          - use RSTT Pn/Sn predictions?
 *          UseRSTTPgLg          - use RSTT Pg/Lg predictions?
 *          KMLBulletinFile      - KML bulletin output filename for Google Earth
 *  Called by:
 *     main
 *  Calls:
 *     ReadHumanTime
 */
int ReadInstructionFile(char *instruction, EVREC *ep, int isf, char *auxdir,
        char *homedir)
{
    extern int DoCorrelatedErrors;          /* account for correlated errors */
    extern int DoGridSearch;           /* perform NA search for initial hypo */
    extern int ZipKML;                    /* zip KMLEventFile and remove kml */
    extern double NAsearchRadius;   /* search radius around preferred origin */
    extern double NAsearchDepth;/* search radius (km) around preferred depth */
    extern double NAsearchOT;       /* search radius (s) around preferred OT */
    extern int NAiterMax;                        /* max number of iterations */
    extern int NAinitialSample;                    /* size of initial sample */
    extern int NAnextSample;                   /* size of subsequent samples */
    extern int NAcells;                   /* number of cells to be resampled */
    extern int WriteNAResultsToFile;
    extern int MinDepthPhases;/* min # of depth phases for depth-phase depth */
    extern int MindDepthPhaseAgencies;/* min agencies reporting depth phases */
    extern double MaxLocalDistDeg;              /* max local distance (degs) */
    extern int MinLocalStations;      /* min stations within MaxLocalDistDeg */
    extern double MaxSPDistDeg;                   /* max S-P distance (degs) */
    extern int MinSPpairs;                  /* min number of S-P phase pairs */
    extern int MinCorePhases;   /* min number of core reflections ([PS]c[PS] */
    extern long iseed;
    extern double MSPeriodRange;   /* MSH period tolerance around MSZ period */
    extern int MinNetmagSta;             /* min number of stamags for netmag */
    extern double MagMaxTimeResidual;    /* max allowable timeres for stamag */
    extern int UpdateDB;                         /* write result to database */
    extern char InputDB[VALLEN];   /* read data from this DB account, if any */
    extern char OutputDB[VALLEN];        /* write results to this DB account */
    extern char NextidDB[VALLEN];           /* get new ids from this account */
    extern char StationFile[FILENAMELEN];   /* stafile when not read from db */
    extern char ISFInputFile[FILENAMELEN];             /* input ISF filename */
    extern char ISFOutputFile[FILENAMELEN];           /* output ISF filename */
    extern char KMLBulletinFile[FILENAMELEN];/* output KML bulletin filename */
    extern char KMLEventFile[FILENAMELEN];      /* output KML event filename */
    extern char RSTTmodel[FILENAMELEN];      /* pathname for RSTT model file */
    extern int UseRSTTPnSn;                   /* use RSTT Pn/Sn predictions? */
    extern int UseRSTTPgLg;                   /* use RSTT Pg/Lg predictions? */
    extern int UseRSTT;                             /* use RSTT predictions? */
//    extern int LocalTTfromRSTT;                  /* local TT from RSTT model */
    extern char LocalVmodelFile[FILENAMELEN];   /* pathname for local vmodel */
    extern double DefaultDepth;     /* used if seed hypocentre depth is NULL */
    extern int UseLocalTT;                       /* use local TT predictions */
    extern double MaxLocalTTDelta;       /* use local TT up to this distance */
    extern int UpdateLocalTT;                     /* static/dynamic local TT */
    extern int DoNotRenamePhase;                 /* do not reidentify phases */
    extern int CalculateML;                                 /* calculate ML? */
    extern int CalculatemB;                                 /* calculate mB? */
    extern int MagnitudesOnly;                  /* calculate magnitudes only */
    char opt[MAXOPT][VALLEN];
    char val[MAXOPT][VALLEN];
    char *remnant;
    int i, j, k, eid;
    int value_option, numopt;
/*
 *  Set default values for options.
 */
    eid = ep->evid;
    ep->evid = NULLVAL;
    ep->DepthAgency[0] = '\0';
    ep->EpicenterAgency[0] = '\0';
    ep->OTAgency[0] = '\0';
    ep->HypocenterAgency[0] = '\0';
    ep->StartDepth = NULLVAL;
    ep->StartLat = NULLVAL;
    ep->StartLon = NULLVAL;
    ep->StartOT = NULLVAL;
    ep->FixDepthToZero = 0;
    ep->FixedOT = 0;
    ep->FixedDepth = 0;
    ep->FixDepthToUser = 0;
    ep->FixedEpicenter = 0;
    ep->FixedHypocenter = 0;
    ep->FixDepthToDefault = 0;
    ep->FixDepthToDepdp = 0;
    ep->FixDepthToMedian = 0;
/*
 *  Remove leading white space
 */
    i = 0;
    while (instruction[i] == ' ' || instruction[i] == '\t') { i++; }
/*
 *  Read in unknown number of options.
 */
    k = 0;
    while (instruction[i] != '\n') {
/*
 *      Option name
 */
        j = 0;
        value_option = 1;
        while (instruction[i] != '\n') {
            if (instruction[i] == '=') {
                i++;
                break;
            }
            if (instruction[i] == ' ') {
                value_option = 0;
                i++;
                break;
            }
            opt[k][j++] = instruction[i++];
        }
        opt[k][j] = '\0';
/*
 *      Option value
 */
        j = 0;
        if (value_option)
            while (instruction[i] != '\n') {
                if (instruction[i] == ' ' ) {
                    i++;
                    break;
                }
                val[k][j++] = instruction[i++];
            }
        val[k][j] = '\0';
        k++;
    }
    numopt = k;
/*
 *  Parse options and update event structure.
 */
    for (i = 0; i < numopt; i++) {
/*
 *      A parameter without a value is interpreted as EventID.
 */
        if (val[i][0] == '\0') {
            if (ep->evid < NULLVAL || isf) {
                fprintf(errfp, "ABORT: parameter with no value %s\n", val[i]);
                fprintf(errfp, "ABORT: parameter with no value %s\n", val[i]);
                return 1;
            }
            strcpy(ep->EventID, opt[i]);
            if (isInteger(opt[i]))
                ep->evid = eid + 1;
            else
                ep->evid = atoi(opt[i]);
        }
/*
 *      set initial depth to (number or agency)
 */
        else if (streq(opt[i], "StartDepth")) {
            if (isdigit(val[i][0])) {
                ep->StartDepth = strtod(val[i], &remnant);
                if (remnant[0]) {
                    fprintf(errfp, "ABORT: bad depth %s\n", val[i]);
                    fprintf(logfp, "ABORT: bad depth %s\n", val[i]);
                    return 1;
                }
            }
            else
                strcpy(ep->DepthAgency, val[i]);
        }
/*
 *      fix depth to (number or agency)
 */
        else if (streq(opt[i], "FixDepth")) {
            if (isdigit(val[i][0])) {
                ep->StartDepth = strtod(val[i], &remnant);
                if (remnant[0]) {
                    fprintf(errfp, "ABORT: bad FixDepth %s\n",
                            val[i]);
                    fprintf(logfp, "ABORT: bad FixDepth %s\n",
                            val[i]);
                    return 1;
                }
                ep->FixDepthToUser = 1;
            }
            else
                strcpy(ep->DepthAgency, val[i]);
            ep->FixedDepth = 1;
        }
/*
 *      set initial latitude to (number or agency)
 */
        else if (streq(opt[i], "StartLat")) {
            if (isdigit(val[i][0]) || val[i][0] == '-') {
                ep->StartLat = strtod(val[i], &remnant);
                if (remnant[0]) {
                    fprintf(errfp, "ABORT: bad lat %s\n", val[i]);
                    fprintf(logfp, "ABORT: bad lat %s\n", val[i]);
                    return 1;
                }
            }
            else
                strcpy(ep->EpicenterAgency, val[i]);
        }
/*
 *      set initial longitude to (number or agency or none)
 */
        else if (streq(opt[i], "StartLon")) {
            if (isdigit(val[i][0]) || val[i][0] == '-') {
                ep->StartLon = strtod(val[i], &remnant);
                if (remnant[0]) {
                    fprintf(errfp, "ABORT: bad lon %s\n", val[i]);
                    fprintf(logfp, "ABORT: bad lon %s\n", val[i]);
                    return 1;
                }
            }
            else
                strcpy(ep->EpicenterAgency, val[i]);
        }
/*
 *      fix epicenter to agency
 */
        else if (streq(opt[i], "FixEpicenter")) {
            strcpy(ep->EpicenterAgency, val[i]);
            ep->FixedEpicenter = 1;
        }
/*
 *      set initial origin time to (yyyy-mm-dd_hh:mi:ss.sss or agency)
 */
        else if (streq(opt[i], "StartOT")) {
            if (isdigit(val[i][0])) {
                if ((ep->StartOT = ReadHumanTime(val[i])) == 0) {
                    fprintf(errfp, "ABORT: bad time %s\n", val[i]);
                    fprintf(logfp, "ABORT: bad time %s\n", val[i]);
                    return 1;
                }
            }
            else
                strcpy(ep->OTAgency, val[i]);
        }
/*
 *      fix origin time to (yyyy-mm-dd_hh:mi:ss.sss or agency)
 */
        else if (streq(opt[i], "FixOriginTime")) {
            if (isdigit(val[i][0])) {
                if ((ep->StartOT = ReadHumanTime(val[i])) == 0) {
                    fprintf(errfp, "ABORT: bad time %s\n", val[i]);
                    fprintf(logfp, "ABORT: bad time %s\n", val[i]);
                    return 1;
                }
            }
            else
                strcpy(ep->OTAgency, val[i]);
            ep->FixedOT = 1;
        }
/*
 *      fix hypocenter to agency
 */
        else if (streq(opt[i], "FixHypocenter"))
            strcpy(ep->HypocenterAgency, val[i]);
/*
 *      fix depth to default region-dependent depth
 */
        else if (streq(opt[i], "FixDepthToDefault")) {
            ep->FixDepthToDefault = atoi(val[i]);
            if (ep->FixDepthToDefault)
                ep->FixedDepth = 1;
        }
/*
 *      fix depth to median of reported depths
 */
        else if (streq(opt[i], "FixDepthToMedian")) {
            ep->FixDepthToMedian = atoi(val[i]);
            if (ep->FixDepthToMedian)
                ep->FixedDepth = 1;
        }
/*
 *      depth resolution
 */
        else if (streq(opt[i], "MinDepthPhases"))
            MinDepthPhases = atoi(val[i]);
        else if (streq(opt[i], "MindDepthPhaseAgencies"))
            MindDepthPhaseAgencies = atoi(val[i]);
        else if (streq(opt[i], "MaxLocalDistDeg")) {
            MaxLocalDistDeg = strtod(val[i], &remnant);
            if (remnant[0]) {
                fprintf(errfp, "ABORT: bad MaxLocalDistDeg %s\n", val[i]);
                fprintf(logfp, "ABORT: bad MaxLocalDistDeg %s\n", val[i]);
                return 1;
            }
        }
        else if (streq(opt[i], "MaxSPDistDeg")) {
            MaxSPDistDeg = strtod(val[i], &remnant);
            if (remnant[0]) {
                fprintf(errfp, "ABORT: bad MaxSPDistDeg %s\n", val[i]);
                fprintf(logfp, "ABORT: bad MaxSPDistDeg %s\n", val[i]);
                return 1;
            }
        }
        else if (streq(opt[i], "MinLocalStations"))
            MinLocalStations = atoi(val[i]);
        else if (streq(opt[i], "MinSPpairs"))     MinSPpairs = atoi(val[i]);
        else if (streq(opt[i], "MinCorePhases"))  MinCorePhases = atoi(val[i]);
/*
 *      account for correlated error structure [0/1]?
 */
        else if (streq(opt[i], "DoCorrelatedErrors"))
            DoCorrelatedErrors = atoi(val[i]);
/*
 *      set verbose level
 */
        else if (streq(opt[i], "Verbose"))       verbose = atoi(val[i]);
        else if (streq(opt[i], "ZipKML"))        ZipKML = atoi(val[i]);
/*
 *      perform NA grid search [0/1]?
 */
        else if (streq(opt[i], "DoGridSearch"))  DoGridSearch = atoi(val[i]);
/*
 *      NA search radius [degrees] around initial epicentre
 */
        else if (streq(opt[i], "NAsearchRadius")) {
            NAsearchRadius = strtod(val[i], &remnant);
            if (remnant[0]) {
                fprintf(errfp, "ABORT: bad NAsearchRadius %s\n", val[i]);
                fprintf(logfp, "ABORT: bad NAsearchRadius %s\n", val[i]);
                return 1;
            }
        }
/*
 *      NA search radius for depth [km] around initial depth
 */
        else if (streq(opt[i], "NAsearchDepth")) {
            NAsearchDepth = strtod(val[i], &remnant);
            if (remnant[0]) {
                fprintf(errfp, "ABORT: bad NAsearchDepth %s\n", val[i]);
                fprintf(logfp, "ABORT: bad NAsearchDepth %s\n", val[i]);
                return 1;
            }
        }
/*
 *      NA search radius for origin time [s] around initial OT
 */
        else if (streq(opt[i], "NAsearchOT")) {
            NAsearchOT = strtod(val[i], &remnant);
            if (remnant[0]) {
                fprintf(errfp, "ABORT: bad NAsearchOT %s\n", val[i]);
                fprintf(logfp, "ABORT: bad NAsearchOT %s\n", val[i]);
                return 1;
            }
        }
/*
 *      number of initial samples in NA search
 */
        else if (streq(opt[i], "NAinitialSample"))
            NAinitialSample = atoi(val[i]);
/*
 *      number of subsequent samples in NA search
 */
        else if (streq(opt[i], "NAnextSample"))
            NAnextSample = atoi(val[i]);
/*
 *      number of cells to be resampled in NA search
 */
        else if (streq(opt[i], "NAcells"))      NAcells = atoi(val[i]);
/*
 *      max number of iterations in NA search
 */
        else if (streq(opt[i], "NAiterMax"))     NAiterMax = atoi(val[i]);
/*
 *      write NA search results to file [0/1]?
 */
        else if (streq(opt[i], "WriteNAResultsToFile"))
            WriteNAResultsToFile = atoi(val[i]);
/*
 *      initial random number seed for NA search
 */
        else if (streq(opt[i], "iseed")) {
            iseed = (long)strtod(val[i], &remnant);
            if (remnant[0]) {
                fprintf(errfp, "ReadInstructionFile: bad iseed %s\n", val[i]);
                fprintf(logfp, "ReadInstructionFile: bad iseed %s\n", val[i]);
                return 1;
            }
        }
/*
 *      MS_H period tolerance around MS_Z period
 */
        else if (streq(opt[i], "MSPeriodRange")) {
            MSPeriodRange = strtod(val[i], &remnant);
            if (remnant[0]) {
                fprintf(errfp, "ABORT: bad MSPeriodRange %s\n", val[i]);
                fprintf(logfp, "ABORT: bad MSPeriodRange %s\n", val[i]);
                return 1;
            }
        }
/*
 *      maximum allowable time residual (if any) magnitude phases
 */
        else if (streq(opt[i], "MagMaxTimeResidual")) {
            MagMaxTimeResidual = strtod(val[i], &remnant);
            if (remnant[0]) {
                fprintf(errfp, "ABORT: bad MagMaxTimeResidual %s\n", val[i]);
                fprintf(logfp, "ABORT: bad MagMaxTimeResidual %s\n", val[i]);
                return 1;
            }
        }
/*
 *      Minimum number of station magnitudes for network magnitude
 */
        else if (streq(opt[i], "MinNetmagSta"))  MinNetmagSta = atoi(val[i]);
/*
 *      calculate broadband body wave magnitude?
 */
        else if (streq(opt[i], "CalculatemB"))   CalculatemB = atoi(val[i]);
/*
 *      calculate local magnitude?
 */
        else if (streq(opt[i], "CalculateML"))   CalculateML = atoi(val[i]);
/*
 *      ISF input/output
 */
        else if (streq(opt[i], "StationFile")) {
            if (strncmp(val[i], "~/", 2) == 0)
                sprintf(StationFile, "%s/%s", homedir, val[i]);
            else if (strstr(val[i], "/") || streq(auxdir, "."))
                strcpy(StationFile, val[i]);
            else
                sprintf(StationFile, "%s/Stations/%s", auxdir, val[i]);
        }
        else if (streq(opt[i], "ISFInputFile")) {
            if (strncmp(val[i], "~/", 2) == 0)
                sprintf(ISFInputFile, "%s/%s", homedir, val[i]);
            else
                strcpy(ISFInputFile, val[i]);
        }
        else if (streq(opt[i], "ISFOutputFile")) {
            if (strncmp(val[i], "~/", 2) == 0)
                sprintf(ISFInputFile, "%s/%s", homedir, val[i]);
            else
                strcpy(ISFOutputFile, val[i]);
        }
/*
 *      KML output
 */
        else if (streq(opt[i], "KMLEventFile")) {
            if (strncmp(val[i], "~/", 2) == 0)
                sprintf(KMLEventFile, "%s/%s", homedir, val[i]);
            else
                strcpy(KMLEventFile, val[i]);
        }
        else if (streq(opt[i], "KMLBulletinFile")) {
            if (strncmp(val[i], "~/", 2) == 0)
                sprintf(KMLBulletinFile, "%s/%s", homedir, val[i]);
            else
                strcpy(KMLBulletinFile, val[i]);
        }
/*
 *      set input DB account
 */
        else if (streq(opt[i], "InputDB")) {
            strcpy(InputDB, val[i]);
            strcat(InputDB, ".");
        }
/*
 *      set output DB account
 */
        else if (streq(opt[i], "OutputDB")) {
            strcpy(OutputDB, val[i]);
            strcat(OutputDB, ".");
        }
/*
 *      set nextid DB account
 */
        else if (streq(opt[i], "NextidDB")) {
            strcpy(NextidDB, val[i]);
            strcat(NextidDB, ".");
        }
/*
 *      update DB [0/1]?
 */
        else if (streq(opt[i], "UpdateDB"))      UpdateDB = atoi(val[i]);
/*
 *      set RSTT model
 */
        else if (streq(opt[i], "RSTTmodel")) {
            if (strncmp(val[i], "~/", 2) == 0)
                sprintf(RSTTmodel, "%s/%s", homedir, val[i]);
            else if (strstr(val[i], "/") || streq(auxdir, "."))
                strcpy(RSTTmodel, val[i]);
            else
                sprintf(RSTTmodel, "%s/RSTTmodel/%s", auxdir, val[i]);
        }
/*
 *      use RSTT Pn/Sn predictions [0/1]?
 */
        else if (streq(opt[i], "UseRSTTPnSn"))   UseRSTTPnSn = atoi(val[i]);
/*
 *      use RSTT Pg/Lg predictions [0/1]?
 */
        else if (streq(opt[i], "UseRSTTPgLg"))   UseRSTTPgLg = atoi(val[i]);
/*
 *      local TT
 */
        else if (streq(opt[i], "LocalVmodelFile")) {
            if (strncmp(val[i], "~/", 2) == 0)
                sprintf(LocalVmodelFile, "%s/%s", homedir, val[i]);
            else if (strstr(val[i], "/") || streq(auxdir, "."))
                strcpy(LocalVmodelFile, val[i]);
            else
                sprintf(LocalVmodelFile, "%s/localmodels/%s", auxdir, val[i]);
        }
//        else if (streq(opt[i], "LocalTTfromRSTT")) LocalTTfromRSTT = atoi(val[i]);
        else if (streq(opt[i], "MaxLocalTTDelta"))
            MaxLocalTTDelta = atof(val[i]);
/*
 *      do not reidentify phases
 */
        else if (streq(opt[i], "DoNotRenamePhase"))
            DoNotRenamePhase = atoi(val[i]);
        else if (streq(opt[i], "MagnitudesOnly"))
            MagnitudesOnly = atoi(val[i]);
/*
 *      instruction not recognised
 */
        else {
            fprintf(errfp, "ReadInstructionFile: unknown option %s\n", opt[i]);
            fprintf(logfp, "ReadInstructionFile: unknown option %s\n", opt[i]);
        }
        if (verbose > 1)
            fprintf(logfp, "    ReadInstructionFile: %d %s %s\n",
                    i, opt[i], val[i]);
    }
/*
 *
 *  sanity checks
 *
 *
 *  must be an evid or a filename somewhere on line
 */
    if (ep->evid == NULLVAL && !isf) {
        fprintf(errfp, "ABORT: no evid is given!\n%s\n", instruction);
        fprintf(logfp, "ABORT: no evid is given!\n %s\n", instruction);
        return 1;
    }
/*
 *  check for valid ISF input if it is expected
 */
    if (isf) {
        if (streq(StationFile, "")) {
            fprintf(errfp, "ABORT: No ISF station file is given!\n");
            fprintf(logfp, "ABORT: No ISF station file is given!\n");
            return 1;
        }
        if (streq(ISFInputFile, "")) {
            fprintf(errfp, "ABORT: No ISF input file is given!\n");
            fprintf(logfp, "ABORT: No ISF input file is given!\n");
            return 1;
        }
    }
/*
 *  Checks to see that not fixing something two ways at once.
 */
    if (ep->StartLat != NULLVAL || ep->StartLon != NULLVAL) {
        if (ep->StartLat == NULLVAL || ep->StartLon == NULLVAL) {
            fprintf(errfp, "ABORT: must set both lat and lon\n");
            fprintf(logfp, "ABORT: must set both lat and lon\n");
            return 1;
        }
        if (ep->EpicenterAgency[0]) {
            fprintf(errfp, "ABORT: location set twice\n");
            fprintf(logfp, "ABORT: location set twice\n");
            return 1;
        }
    }
/*
 *  use RSTT?
 */
    UseRSTT = 0;
    if (UseRSTTPnSn || UseRSTTPgLg)
        UseRSTT = 1;
/*
 *  use local TT?
 */
//  if (LocalTTfromRSTT) {
//      UseLocalTT = 1;
//      UpdateLocalTT = 1;
//  }
    if (strlen(LocalVmodelFile) > 0) {
//        LocalTTfromRSTT = 0;
        UpdateLocalTT = 0;
        UseLocalTT = 1;
    }
    else {
        UpdateLocalTT = 0;
        UseLocalTT = 0;
    }
    if (verbose > 1)
        fprintf(logfp, "    ReadInstructionFile: evid: %d\n", ep->evid);
    return 0;
}

