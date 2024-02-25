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
/*
 * iloc 4.2
 *
 * Istvan Bondar
 * Research Centre for Astronomy and Earth Sciences,
 * Hungarian Academy of Sciences
 * ibondar2014@gmail.com
 *
 * Single-event location with RSTT travel-time predictions
 *     based on ISC location algorithm (Bondar and Storchak, 2011)
 *     supports ISF/ISF2 input/output files
 *     supports various database schemas
 *         SeisComp3 MYSQL database schema
 *         IDC NDC-in-a-Box PostgreSQL database schema
 *         IDC Oracle database schema
 *         ISC PostgreSQL database schema
 *     can create individual and/or bulletin Google Earth kml/kmz files
 *     supports RSTT (Myers et al., 2010) global upper mantle model
 *         RSTT regional/local travel-time predictions (Pg/Lg/Pn/Sn)
 *     supports user-provided local velocity models
 *         local phase travel-time predictions (Pg/Pb/Pn/Lg/Sg/Sb/Sn)
 *     supports ak135 global travel-time predictions
 *         ellipticity (Dziewonski and Gilbert, 1976, Kennett and Gudmundsson,
 *             1996) and elevation corrections
 *         bounce point corrections for depth phases as well as water column
 *             correction for pwP (Engdahl et al., 1998)
 *     supports infrasound and hydroacoustic azimuth measurements in location
 *     neighbourhood algorithm (NA) grid-search for a refined initial
 *         hypocentre guess (Sambridge, 1999; Sambridge and Kennett, 2001)
 *     iterative linearized least-squares inversion that accounts for
 *         correlated model errors (Bondar and McLaughlin, 2009)
 *     nearest-neighbour ordering of stations to block-diagonalize data
 *         covariance matrix (de Hoon et al., 2004)
 *     depth-phase stacking for robust depth estimation
 *         (Murphy and Barker, 2006)
 *     free-depth solution is attempted only if there is depth resolution
 *         (number of defining depth phases > MinDepthPhases and
 *          number of agencies reporting depth phases >= MindDepthPhaseAgencies)
 *          or number of local defining stations >= MinLocalStations
 *          or number of defining P and S pairs >= MinSPpairs
 *          or (number of defining PcP, ScS >= MinCorePhases and
 *              number of agencies reporting core reflections >= MindDepthPhaseAgencies)
 *     mb, mB, MS and ML determination from reported amplitudes
 *     supports Gutenberg-Richter, Veith-Clawson, and Murphy-Barker magnitude
 *         attenuation curves
 *
 * Reference
 *     Bondar, I. and D. Storchak, (2011).
 *     Improved location procedures at the International Seismological Centre,
 *     Geophys. J. Int., 186, 1220-1244,
 *     DOI: 10.1111/j.1365-246X.2011.05107.x
 *
 * Environment variables
 *     ILOCROOT - directory pathname for data files. If not exists, defaults
 *         to current working directory.
 *     Postgres specific env variables (when ISC database scheme is selected)
 *         PGHOSTADDR, PGPORT, PGDATABASE, PGUSER, PGPASSWORD
 *
 * Input files in the auxiliary data directory
 *     Configuration parameters
 *         $ILOCROOT/auxdata/iLocpars/Config.txt
 *         $ILOCROOT/auxdata/iLocpars/IASPEIPhaseMap.txt
 *         $ILOCROOT/auxdata/iLocpars/Global1DModelPhaseList.txt
*     Global 1D travel-time tables and ellipticity corrections
 *         $ILOCROOT/auxdata/<TTimeTable>/[*].tab
 *         $ILOCROOT/auxdata/<TTimeTable>/ELCOR.dat
 *     RSTT velocity model
 *         $ILOCROOT/auxdata/RSTTmodels/pdu202009Du.geotess
 *     Topography file for bounce point corrections
 *         $ILOCROOT/auxdata/topo/etopo5_bed_g_i2.bin
 *         ETOPO1 resampled to 5'x5' resolution
 *     Flinn-Engdahl regionalization (1995)
 *         $ILOCROOT/auxdata/FlinnEngdahl/FE.dat
 *     Default depth grid
 *         $ILOCROOT/auxdata/FlinnEngdahl/DefaultDepth0.5.grid
 *     Default depth for Flinn-Engdahl regions
 *         $ILOCROOT/auxdata/FlinnEngdahl/GRNDefaultDepth.<TTimeTable>.dat
 *         For locations where no default depth grid point exists.
 *     Generic variogram model for data covariance matrix
 *         $ILOCROOT/auxdata/variogram/variogram.model
 *     Magnitude attenuation Q(d,h) curves for mb calculation
 *         $ILOCROOT/auxdata/magnitude/GRmbQ.dat (Gutenberg-Richter, 1956) or
 *         $ILOCROOT/auxdata/magnitude/VCmbQ.dat (Veith-Clawson, 1972) or
 *         $ILOCROOT/auxdata/magnitude/MBmbQ.dat (Murphy-Barker, 2003)
 *
 * Configuration parameters
 *     Output files
 *         logfile = stdout - detailed log dependent on verbosity level
 *         errfile = stderr - for errors
 *     Database options
 *         UpdateDB = 0    - write results to database?
 *         NextidDB = isc  - get new ids from this account
 *         repid = 100     - ISC reporter field for new hypocentres and assocs
 *         InputDB         - input DB account
 *         OutputDB        - output DB account
 *         DBuser          - DB user account
 *         DBpasswd        - DB user password
 *         DBname          - DB host
 *     Preferred agency names
 *         OutAgency = ILOC - agency for new hypocentres and assocs
 *         InAgency = ISC   - agency for input phase associations
 *     ISF input/output
 *         StationFile      - station coordinates file
 *     Google Earth
 *         KMLBulletinFile         - Output bulletin in KML file
 *         KMLLatitude = 47.49833  - KML bulletin viewpoint latitude
 *         KMLLongitude = 19.04045 - KML bulletin viewpoint longitude
 *         KMLRange = 1000000      - KML bulletin viewpoint elevation
 *         KMLHeading = 0          - KML bulletin viewpoint azimuth
 *         KMLTilt = 0             - KML bulletin viewpoint tilt
 *         ZipKML                  - zip KML output file(s) and remove kml
 *     Travel time table [ak135|iasp91]
 *         TTimeTable = ak135 - travel time table name
 *     ETOPO parameters
 *         EtopoFile = etopo5_bed_g_i2.bin - ETOPO file name
 *         EtopoNlon = 4321                - ETOPO longitude samples
 *         EtopoNlat = 2161                - ETOPO latitude samples
 *         EtopoRes = 0.0833333            - ETOPO resolution
 *     Depth resolution
 *         MinDepthPhases = 3  - min number of depth phases for depth-phase depth
 *         MindDepthPhaseAgencies = 1 - min number of depth-phase agencies
 *         MaxLocalDistDeg = 0.2    - max local distance to be considered [deg]
 *         MinLocalStations = 1     - min number of local defining stations
 *         MaxSPDistDeg = 2.  - max distance for defining S-P phase pairs
 *                              to be considered [deg]
 *         MinSPpairs = 3      - min number of defining S-P phase pairs
 *         MinCorePhases = 3   - min number of defining core reflection phases
 *         MaxShallowDepthError = 30. - max depth error for crustal free-depth
 *         MaxDeepDepthError = 60.    - max depth error for deep free-depth
 *         DefaultDepth = 0           - used if seed hypocentre depth is NULL
 *     Iteration control
 *         MinIterations = 4      - min number of iterations
 *         MaxIterations = 20     - max number of iterations
 *         MinNdefPhases = 4      - min number of phases
 *         SigmaThreshold = 6.    - used to exclude phases from solution
 *         DoCorrelatedErrors = 1 - account for correlated errors?
 *         AllowDamping = 1       - allow damping in LSQR iterations?
 *         ConfidenceLevel = 90.  - confidence level for uncertainties
 *     Agencies whose hypocenters not to be used in setting the initial guess
 *         NohypoAgencies = UNK,NIED,HFS,HFS1,HFS2,NAO,LAO
 *                          # UNK   - unknown agency
 *                          # NIED  - truncates origin time to the minute
 *                          # HFS, NAO, LAO - single array locations
 *     Neighbourhood Algorithm parameters
 *         DoGridSearch = 1     - perform NA grid search?
 *         NAsearchRadius = 5.  - search radius [deg] around initial epicentre
 *         NAsearchDepth = 300. - search radius [km] around initial depth
 *         NAsearchOT = 30.     - search radius [s] around initial origin time
 *         NAlpNorm = 1.        - p-value for Lp-norm to compute misfit
 *         NAiterMax = 5        - max number of iterations
 *         NAinitialSample = 1500  - size of initial sample
 *         NAnextSample = 150   - size of subsequent samples
 *         NAcells = 25         - number of cells to be resampled
 *         iseed = 5590         - random number seed
 *     Magnitude calculations
 *         mbQtable = GR       - magnitude correction table [GR,VC,MB,none]
 *         mbMinDistDeg = 21.  - min delta for calculating mb
 *         mbMaxDistDeg = 100. - max delta for calculating mb
 *         MSMinDistDeg = 20.  - min delta for calculating Ms
 *         MSMaxDistDeg = 160. - max delta for calculating Ms
 *         mbMinPeriod  = 0.   - min period for calculating mb
 *         mbMaxPeriod  = 3.   - max period for calculating mb
 *         MSMinPeriod  = 10.  - min period for calculating Ms
 *         MSMaxPeriod  = 60.  - max period for calculating Ms
 *         MSMaxDepth   = 60.  - max depth for calculating Ms
 *         MSPeriodRange = 5.  - MSH period tolerance around MSZ period
 *         MagnitudeRangeLimit = 2.2  - allowable range around network mag
 *         CalculateML = 0     - calculate ML?
 *         CalculatemB = 0     - calculate broadband mB?
 *     RSTT-specific parameters
 *         RSTTmodel        - RSTT model file
 *         UseRSTTPnSn = 1  - use RSTT Pn/Sn predictions?
 *         UseRSTTPgLg = 1  - use RSTT Pg/Lg predictions?
 *     Local velocity model
 *         MaxLocalTTDelta = 3. - use local TT up to this distance
 *         LocalTTfromRSTT = 0  - get local TT from RSTT model at epicentre
 *         LocalVmodelFile =    - pathname for local velocity model (non-RSTT)
 *
 * Instructions
 *     iloc locates events with evids one per line in the instruction file
 *         Verbose            - verbosity level (0..5)
 *         StartDepth         - an agency code or a number
 *         StartLat, StartLon - an agency code or a number
 *         StartOT            - an agency code or a number
 *         FixDepth           - an agency code or a number
 *         FixEpicenter       - an agency code
 *         FixOriginTime      - an agency code or a number
 *         FixHypocenter      - an agency code
 *         FixDepthToDefault  - presence sets flag to 1
 *         ISFInputFile       - read ISF input from here
 *         ISFOutputFile      - write ISF output to here
 *         KMLEventFile       - KML event output filename for Google Earth
 *         WriteNAResultsToFile - write NA results to file
 *     The options below can override external parameters in the config file:
 *         MinDepthPhases     - min # of depth phases for depth-phase depth
 *         MindDepthPhaseAgencies - min # of agencies reporting depth phases
 *         MaxLocalDistDeg    - max local distance (degs)
 *         MinLocalStations   - min number of stations within MaxLocalDistDeg
 *         MaxSPDistDeg       - max S-P distance (degs)
 *         MinSPpairs         - min number of S-P phase pairs
 *         MinCorePhases      - min number of core reflections ([PS]c[PS]
 *         iseed              - random number initial seed
 *         UpdateDB           - update DB?
 *         InputDB            - input DB account
 *         OutputDB           - output DB account
 *         KMLBulletinFile    - KML bulletin output filename for Google Earth
 *         KMLLatitude        - KML bulletin viewpoint latitude
 *         KMLLongitude       - KML bulletin viewpoint longitude
 *         KMLRange           - KML bulletin viewpoint elevation
 *         KMLHeading         - KML bulletin viewpoint azimuth
 *         KMLTilt            - KML bulletin viewpoint tilt
 *         ZipKML             - zip KML files and remove kml
 *         DoGridSearch       - perform initial grid search?
 *         NAsearchRadius     - search radius [deg] around initial epicentre
 *         NAsearchDepth      - search radius [km] around initial depth
 *         NAsearchOT         - search radius [s] around initial origin time
 *         NAinitialSample    - size of initial sample
 *         NAnextSample       - size of subsequent samples
 *         NAcells            - number of cells to be resampled
 *         NAiterMax          - max number of iterations
 *         DoCorrelatedErrors - account for correlated error structure?
 *         StationFile        - full pathname of station coordinates file
 *         MSPeriodRange      - MSH period tolerance around MSZ period
 *         MagMaxTimeResidual - max allowable timeres for magnitude phases
 *         RSTTmodel          - full pathname of RSTT model file
 *         UseRSTTPnSn        - use RSTT Pn/Sn predictions?
 *         UseRSTTPgLg        - use RSTT Pg/Lg predictions?
 *         MaxLocalTTDelta    - use local TT up to this distance
 *         LocalTTfromRSTT    - get local TT from RSTT model at epicentre
 *         LocalVmodelFile    - pathname for local velocity model (non-RSTT)
 *         CalculateML        - calculate ML?
 *         CalculatemB        - calculate broadband mB?
 *
 * For each event try 2 options:
 *     0 - free depth (if there is depth resolution)
 *     1 - fix depth to region-dependent default depth (if option 0 fails)
 * Further options can be requested via instruction arguments:
 *     2 - fix depth to a reported hypocentre's depth
 *     3 - fix depth to median of reported depths
 *     4 - fix epicentre
 *     5 - fix depth and epicentre
 *     6 - fix hypocentre (in this case simply calculate the residuals)
 * If convergence is reached, no further options are tried.
 *
 * ISC terminology
 *    prime/preferred origin
 *       the preferred hypocentre among the various reported hypocentres
 *    reading
 *       the entire set of phases/amplitudes reported by an agency
 *       at a particular station for a particular event
 *       submitted in a single report
 *       (e.g. Pg, Pn, Sn at ARCES reported by the IDC for an event)
 *    phase
 *       arrival/amplitude data reported for a phase pick
 */

#include "iLoc.h"

/*
 *
 * Global variables for entries from the model file
 *
 */
double Moho;                                                /* depth of Moho */
double Conrad;                                            /* depth of Conrad */
double MaxHypocenterDepth;                           /* max hypocenter depth */
double PSurfVel;                    /* Pg velocity for elevation corrections */
double SSurfVel;                    /* Sg velocity for elevation corrections */
int numPhaseTT;                             /* number of ak135/iasp91 phases */
char PhaseTT[MAXTTPHA][PHALEN];           /* global phase list (ak135/iasp91 */
char PhaseWithoutResidual[MAXNUMPHA][PHALEN];/* no residuals for these phases*/
int PhaseWithoutResidualNum;                 /* number of no-residual phases */
char MBPhase[MAXNUMPHA][PHALEN];          /* phases used in mb determination */
int numMBPhase;                                       /* number of mb phases */
char MSPhase[MAXNUMPHA][PHALEN];          /* phases used in MS determination */
int numMSPhase;                                       /* number of Ms phases */
char MLPhase[MAXNUMPHA][PHALEN];          /* phases used in ML determination */
int numMLPhase;                                       /* number of ML phases */
PHASEMAP PhaseMap[MAXPHACODES];    /* reported phase to IASPEI phase mapping */
int numPhaseMap;                             /* number of phases in PhaseMap */
PHASEWEIGHT PhaseWeight[MAXNUMPHA];       /* prior time measerror for phases */
int numPhaseWeight;                       /* number of phases in PhaseWeight */
char AllowablePhases[MAXTTPHA][PHALEN];                  /* allowable phases */
int numAllowablePhases;                        /* number of allowable phases */
char firstPphase[MAXTTPHA][PHALEN];               /* first-arriving P phases */
int numFirstPphase;                     /* number of first-arriving P phases */
char firstSphase[MAXTTPHA][PHALEN];               /* first-arriving S phases */
int numFirstSphase;                     /* number of first-arriving S phases */
char firstPoptional[MAXTTPHA][PHALEN];            /* optional first P phases */
int numFirstPoptional;                  /* number of optional first P phases */
char firstSoptional[MAXTTPHA][PHALEN];            /* optional first S phases */
int numFirstSoptional;                  /* number of optional first S phases */

/*
 *
 * Global variables for entries in the config.txt file or for instructions
 *
 *
 * I/O
 */
char logfile[FILENAMELEN];                                   /* log filename */
char errfile[FILENAMELEN];                                 /* error filename */
char StationFile[FILENAMELEN];      /* station file when not reading from db */
char ISFInputFile[FILENAMELEN];                        /* input ISF filename */
char ISFOutputFile[FILENAMELEN];                      /* output ISF filename */
char KMLEventFile[FILENAMELEN];                 /* output KML event filename */
char KMLBulletinFile[FILENAMELEN];           /* output KML bulletin filename */
double KMLLatitude;                       /* KML bulletin viewpoint latitude */
double KMLLongitude;                     /* KML bulletin viewpoint longitude */
double KMLRange;                         /* KML bulletin viewpoint elevation */
double KMLHeading;                         /* KML bulletin viewpoint azimuth */
double KMLTilt;                               /* KML bulletin viewpoint tilt */
int ZipKML;                                  /* zip KML files and remove kml */
int UpdateDB;                                    /* write result to database */
char NextidDB[VALLEN];            /* get new unique ids from this DB account */
int repid;                     /* reporter id for new hypocentres and assocs */
char OutAgency[VALLEN];             /* author for new hypocentres and assocs */
char InAgency[VALLEN];                            /* author for input assocs */
char InputDB[VALLEN];              /* read data from this DB account, if any */
char OutputDB[VALLEN];           /* write results to this DB account, if any */
char DBuser[VALLEN];                                        /* database user */
char DBpasswd[VALLEN];                                  /* database password */
char DBname[VALLEN];                                        /* database host */
int verbose;                                              /* verbosity level */
/*
 * iteration control
 */
int MinIterations;                               /* min number of iterations */
int MaxIterations;                               /* max number of iterations */
int MinNdefPhases;                                   /* min number of phases */
double SigmaThreshold;                    /* to exclude phases from solution */
int DoCorrelatedErrors;                     /* account for correlated errors */
int AllowDamping;                        /* allow damping in LSQR iterations */
double ConfidenceLevel;                /* confidence level for uncertainties */
int DoNotRenamePhase;                            /* do not reidentify phases */
int MagnitudesOnly;                             /* calculate magnitudes only */
/*
 *  depth-phase depth solution requirements
 */
int MinDepthPhases;      /* min number of depth phases for depth-phase depth */
int MindDepthPhaseAgencies; /* min number of agencies reporting depth phases */
/*
 *  depth resolution
 */
double MaxLocalDistDeg;                         /* max local distance (degs) */
int MinLocalStations;       /* min number of stations within MaxLocalDistDeg */
double MaxSPDistDeg;                              /* max S-P distance (degs) */
int MinSPpairs;                             /* min number of S-P phase pairs */
int MinCorePhases;              /* min number of core reflections ([PS]c[PS] */
/*
 * maximum allowable depth error for free depth solutions
 */
double MaxShallowDepthError;
double MaxDeepDepthError;
/*
 * magnitudes
 */
char mbQtable[VALLEN];                         /* magnitude correction table */
double BBmBMinDistDeg;                       /* min delta for calculating mB */
double BBmBMaxDistDeg;                       /* max delta for calculating mB */
double mbMinDistDeg;                         /* min delta for calculating mb */
double mbMaxDistDeg;                         /* max delta for calculating mb */
double MSMinDistDeg;                         /* min delta for calculating Ms */
double MSMaxDistDeg;                         /* max delta for calculating Ms */
double mbMinPeriod;                         /* min period for calculating mb */
double mbMaxPeriod;                         /* max period for calculating mb */
double MSMinPeriod;                         /* min period for calculating Ms */
double MSMaxPeriod;                         /* max period for calculating Ms */
double MSMaxDepth;                           /* max depth for calculating Ms */
double MagnitudeRangeLimit;              /* warn about outliers beyond range */
double MSPeriodRange;              /* MSH period tolerance around MSZ period */
double MagMaxTimeResidual;     /* max allowable timeres for magnitude phases */
double MLMaxDistkm;                          /* max delta for calculating ML */
int MinNetmagSta;            /* min number of stamags for calculating netmag */
int CalculateML;                                            /* calculate ML? */
int CalculatemB ;                                 /* calculate broadband mB? */
/*
 * NA search parameters
 */
int DoGridSearch;                /* perform grid search for initial location */
int WriteNAResultsToFile;                           /* write results to file */
double NAsearchRadius;              /* search radius around preferred origin */
double NAsearchDepth;           /* search radius (km) around preferred depth */
double NAsearchOT;                  /* search radius (s) around preferred OT */
double NAlpNorm;                       /* p-value for norm to compute misfit */
int NAiterMax;                                   /* max number of iterations */
int NAinitialSample;                               /* size of initial sample */
int NAnextSample;                              /* size of subsequent samples */
int NAcells;                              /* number of cells to be resampled */
long iseed;                                            /* random number seed */
/*
 * agencies whose hypocenters not to be used in setting initial hypocentre
 */
char DoNotUseAgencies[MAXBUF][AGLEN];
int DoNotUseAgenciesNum;
/*
 * ETOPO parameters
 */
char EtopoFile[FILENAMELEN];                      /* filename for ETOPO file */
int EtopoNlon;                       /* number of longitude samples in ETOPO */
int EtopoNlat;                        /* number of latitude samples in ETOPO */
double EtopoRes;                                        /* cellsize in ETOPO */
/*
 * TT
 */
char TTimeTable[VALLEN];                    /* global travel time table name */
char LocalVmodelFile[FILENAMELEN];      /* pathname for local velocity model */
int UseLocalTT;                                  /* use local TT predictions */
double MaxLocalTTDelta;                  /* use local TT up to this distance */
double DefaultDepth;                /* used if seed hypocentre depth is NULL */
double PrevLat, PrevLon;                   /* epicentre of previous solution */
int UpdateLocalTT;                                /* static/dynamic local TT */
/*
 * RSTT parameters
 */
char RSTTmodel[FILENAMELEN];                                   /* RSTT model */
int UseRSTTPnSn;                               /* use RSTT Pn/Sn predictions */
int UseRSTTPgLg;                               /* use RSTT Pg/Lg predictions */
int UseRSTT;                                         /* use RSTT predictions */
// int LocalTTfromRSTT;                             /* local TT from RSTT model */
/*
 *
 * file and database pointers
 *
 */
FILE *logfp = (FILE *)NULL;                      /* file pointer to log file */
FILE *errfp = (FILE *)NULL;                    /* file pointer to error file */
#ifdef PGSQL
PGconn *conn = NULL;                       /* PostgreSQL database connection */
#endif
#ifdef MYSQLDB
MYSQL *mysql = NULL;                           /* MySQL database connection */
#endif
#ifdef ORASQL
dpiConn *conn = NULL;                          /* Oracle database connection */
dpiContext *gContext = NULL;
#endif
struct timeval t0;
/*
 * station list from ISF file
 */
STAREC *StationList;
int numSta;                             /* number of stations in StationList */
/*
 * agencies contributing phase data
 */
char agencies[MAXBUF][AGLEN];
int numAgencies;
/*
 * list of local phases
 */
int numLocalPhaseTT = MAXLOCALTTPHA;
char LocalPhaseTT[MAXLOCALTTPHA][PHALEN] = {
    "firstP", "firstS",
    "Pg",   "Pb",   "Pn",   "P",
    "Sg",   "Sb",   "Sn",   "S",  "Lg"
};

/*
 * Error codes
 */
int errorcode;
char *errorcodes[] = {
    "unknown error, please consult log file",
    "memory allocation error",
    "could not open file",
    "bad instruction",
    "diverging solution",
    "insufficient number of phases",
    "insufficient number of independent phases",
    "phase loss",
    "slow convergence",
    "singular G matrix",
    "abnormally ill-conditioned problem",
    "invalid station code",
};


/*
 *
 * Local functions
 *
 */
static int OpenLogErrFiles(char *logfile, char *errfile);
static void PrintHelp();
/*
 *
 * main body
 *
 */
int main(int argc, char *argv[])
{
#ifdef PGSQL
    PGresult *res_set = (PGresult *)NULL;
    char sql[MAXBUF];
#endif
    FILE *instructfp;
    FILE *isfin = NULL, *isfout = NULL, *kml = NULL, *ekml = NULL;
    char *buffer = NULL, *homedir = NULL;
    char auxdir[FILENAMELEN];
    char instructfile[FILENAMELEN];
    char conninfo[FILENAMELEN];
    char *line = NULL, *instruction = NULL, *cmd = NULL;
    ssize_t nb = (ssize_t)0;
    size_t nbytes = 0;
    EVREC e;                                                 /* event record */
    SOLREC s;                                             /* solution record */
    HYPREC *h = (HYPREC *)NULL;                        /* hypocenter records */
    PHAREC *p = (PHAREC *)NULL;                             /* phase records */
    TT_TABLE *TTtables = (TT_TABLE *)NULL;      /* global travel-time tables */
    TT_TABLE *LocalTTtables = (TT_TABLE *)NULL;  /* local travel-time tables */
    EC_COEF *ec = (EC_COEF *)NULL;    /* ellipticity correction coefficients */
    VARIOGRAM variogram;                          /* generic variogram model */
    MAGQ mbQ;                                 /* magnitude attenuation table */
    FE fe;                        /* Flinn-Engdahl geographic region numbers */
    double **DepthGrid = (double **)NULL;              /* default depth grid */
    double *GrnDepth = (double *)NULL;              /* default depths by grn */
    short int **topo = (short int **)NULL;     /* ETOPO bathymetry/elevation */
    int total = 0, fail = 0, opt[7];                 /* counters for results */
    int ismbQ = 0;                           /* apply magnitude attenuation? */
    int isf = 0;                                     /* ISF text file choice */
    int db = 0;                                           /* database choice */
    int isbull = 0;                       /* KML bulletin file to be created */
    int ngrid = 0, i, j;
    int numECPhases = 0;
    double gres = 1.;
    double d = DEG_TO_RAD * MAX_RSTT_DIST;   /* max RSTT distance in radians */
    struct timeval t00;
    int MinDepthPhases_cf = 5, MindDepthPhaseAgencies_cf = 2;
    int MinLocalStations_cf = 1, MinSPpairs_cf = 5, MinCorePhases_cf = 5;
    double MaxLocalDistDeg_cf = 0.2, MaxSPDistDeg_cf = 3., MaxLocalTTDelta_cf = 3.;
    double MSPeriodRange_cf = 5., MagMaxTimeResidual_cf = 10.;
    int DoGridSearch_cf = 0, DoCorrelatedErrors_cf = 1;
    int UpdateDB_cf = 0;
    long iseed_cf = 5590;
    double NAsearchRadius_cf = 5., NAsearchDepth_cf = 300., NAsearchOT_cf = 30.;
    int NAinitialSample_cf = 700, NAnextSample_cf = 100, NAcells_cf = 25;
    int NAiterMax_cf = 5, MinNetmagSta_cf = 3;
    int UseRSTTPnSn_cf = 0, UseRSTTPgLg_cf = 0, UseRSTT_cf = 0;
    int depfix_cf = 0, surfix_cf = 0, hypofix_cf = 0, otfix_cf = 0;
    double startdepth_cf, startot_cf;
    int DoNotRenamePhase_cf = 0, MagnitudesOnly_cf = 0;
    int CalculateML_cf = 0, CalculatemB_cf = 0;
    char RSTTmodel_cf[FILENAMELEN];
    char vfile_cf[FILENAMELEN];
    char InputDB_cf[VALLEN], OutputDB_cf[VALLEN], NextidDB_cf[VALLEN];
    char magbloc[100*LINLEN];
/*
 *  set timezone to get epoch times right and other inits
 */
    setenv("TZ", "", 1);
    tzset();
    strcpy(ISFOutputFile, "");
    strcpy(KMLEventFile, "");
    strcpy(KMLBulletinFile, "");
    strcpy(vfile_cf, "");
    for (i = 0; i < 7; i++) opt[i] = 0;
    gettimeofday(&t00, NULL);
    gettimeofday(&t0, NULL);
    verbose = 0;
    errorcode = 0;
    DoNotRenamePhase = MagnitudesOnly = 0;
    PrevLat = PrevLon = 89.;
/*
 *  Get command line argument and open instruction file if any
 */
    if (argc == 1) {
/*
 *      no argument is given, abort!
 */
        PrintHelp();
        fprintf(stderr, "ABORT: No argument is given!\n");
        fprintf(stderr, "EVENT %.6f\n\n", secs(&t0));
        exit(1);
    }
    else if (streq(argv[1], "isc")) {
/*
 *      data will be read from an ISC PostgreSQL database schema
 *      connection info is read from environment variables
 *      PGDATABASE, PGUSER, PGPASSWORD
 */
        db = 1;
        strcpy(conninfo, "");
    }
/*    else if (streq(argv[1], "sc3niab")) {
 *
 *      data will be read from a SeisComp3 NiaB PostgreSQL database schema
 *       db = 2;
 *   }
 */
    else if (streq(argv[1], "niab")) {
/*
 *      data will be read from an IDC NiaB PostgreSQL database schema
 */
        db = 3;
    }
    else if (streq(argv[1], "seiscomp")) {
/*
 *      data will be read from a SeisComp3 MySQL database schema
 *      connection info should be in $HOME/.my.cnf
 */
        db = 4;
    }
    else if (streq(argv[1], "idc")) {
/*
 *      data will be read from an IDC Oracle database schema
 */
        db = 5;
    }
    else if (streq(argv[1], "isf2.1")) {
/*
 *      data will be read from an ISF input file
 */
        isf = 1;
    }
    else if (streq(argv[1], "isf2.0")) {
/*
 *      data will be read from an ISF2 input file
 */
        isf = 2;
    }
    else if (streq(argv[1], "ims")) {
/*
 *      data will be read from an IMS input file
 */
        isf = 3;
    }
    else {
/*
 *      invalid argument, abort!
 */
        PrintHelp();
        fprintf(stderr, "ABORT: Invalid first argument! (%s)\n", argv[1]);
        fprintf(stderr, "EVENT %.6f\n\n", secs(&t0));
        exit(1);
    }
    strcpy(instructfile, "stdin");
    instructfp = stdin;
/*
 *
 *  Get home directory name
 *      If the $ILOCROOT environment variable doesn't exist,
 *      auxdir defaults to the current working directory.
 *
 */
    if ((buffer = getenv("ILOCROOT"))) {
        sprintf(auxdir, "%s/auxdata", buffer);

    }
    else {
        strcpy(auxdir, ".");
    }
    homedir = getenv("HOME");
/*
 *
 *  Read configuration file from auxdir/iLocpars directory
 *
 */
    printf("ReadConfigFile: %s/iLocpars/Config.txt\n", auxdir);
    if (ReadConfigFile(auxdir, homedir)) {
        fprintf(stderr, "EVENT %.6f\n\n", secs(&t0));
        exit(1);
    }
/*
 *  save config pars that could be overriden by an instruction
 */
    MinDepthPhases_cf = MinDepthPhases;
    MindDepthPhaseAgencies_cf = MindDepthPhaseAgencies;
    MaxLocalDistDeg_cf = MaxLocalDistDeg;
    MinLocalStations_cf = MinLocalStations;
    MaxSPDistDeg_cf = MaxSPDistDeg;
    MinSPpairs_cf = MinSPpairs;
    MinCorePhases_cf = MinCorePhases;
    DoGridSearch_cf = DoGridSearch;
    NAsearchRadius_cf = NAsearchRadius;
    NAsearchDepth_cf = NAsearchDepth;
    NAsearchOT_cf = NAsearchOT;
    NAinitialSample_cf = NAinitialSample;
    NAnextSample_cf = NAnextSample;
    NAcells_cf = NAcells;
    NAiterMax_cf = NAiterMax;
    UpdateDB_cf = UpdateDB;
    iseed_cf = iseed;
    MSPeriodRange_cf = MSPeriodRange;
    MagMaxTimeResidual_cf = MagMaxTimeResidual;
    MinNetmagSta_cf = MinNetmagSta;
    DoCorrelatedErrors_cf = DoCorrelatedErrors;
    UseRSTTPnSn_cf = UseRSTTPnSn;
    UseRSTTPgLg_cf = UseRSTTPgLg;
    UseRSTT_cf = UseRSTT;
    MaxLocalTTDelta_cf = MaxLocalTTDelta;
    strcpy(RSTTmodel_cf, RSTTmodel);
    strcpy(vfile_cf, LocalVmodelFile);
    DoNotRenamePhase_cf = DoNotRenamePhase;
    MagnitudesOnly_cf = MagnitudesOnly;
    CalculateML_cf = CalculateML;
    CalculatemB_cf = CalculatemB;
    strcpy(InputDB_cf, InputDB);
    strcpy(OutputDB_cf, OutputDB);
    strcpy(NextidDB_cf, NextidDB);
/*
 *
 *  Open error and log files
 *
 */
    if (OpenLogErrFiles(logfile, errfile)) {
        fprintf(stderr, "EVENT %.6f\n\n", secs(&t0));
        exit(1);
    }
    if ((instruction = (char *)calloc(LINLEN, sizeof(char))) == NULL) {
        fprintf(errfp, "main: cannot allocate memory\n");
        exit(1);
    }
/*
 *
 *  ISF input file
 *      only the first line of the instruction file is interpreted
 *      can run without instruction file
 *
 */
    if (isf) {
        e.evid = 0;
        e.EventID[0] = '\0';
        e.DepthAgency[0] = '\0';
        e.EpicenterAgency[0] = '\0';
        e.OTAgency[0]     = '\0';
        e.HypocenterAgency[0]     = '\0';
        e.StartDepth = e.StartOT = startdepth_cf = startot_cf = NULLVAL;
        e.StartLat = e.StartLon = NULLVAL;
        e.FixDepthToZero = e.FixedHypocenter = 0;
        e.FixedOT = e.FixedDepth = 0;
        e.FixDepthToUser = e.FixedEpicenter = 0;
        e.FixDepthToDefault = 0;
        depfix_cf = surfix_cf = hypofix_cf = 0;
        if ((nb = getline(&line, &nbytes, instructfp)) > 0) {
            if ((j = nb - 2) >= 0) {
                if (line[j] == '\r') line[j] = '\n';   /* replace CR with LF */
            }
/*
 *          read instruction line
 */
            DropSpace(line, instruction);
            fprintf(logfp, "Instruction: %s", instruction);
            fprintf(errfp, "instruction: %s", instruction);
            if (ReadInstructionFile(instruction, &e, isf, auxdir, homedir)) {
                fprintf(logfp, "EVENT %.6f\n\n", secs(&t0));
                Free(line);
                Free(instruction);
                exit(1);
            }
            depfix_cf = e.FixedDepth;
            surfix_cf = e.FixDepthToZero;
            hypofix_cf = e.FixedHypocenter;
            otfix_cf = e.FixedOT;
            startdepth_cf = e.StartDepth;
            startot_cf = e.StartOT;
        }
        else {
            fprintf(errfp, "No instructions are given!\n");
            Free(line);
            Free(instruction);
            exit(1);
        }
        Free(line);
        MindDepthPhaseAgencies = 1; /* No agency info exists in an ISF file! */
/*
 *
 *      Open ISF input/output files if any
 *
 */
        if (ISFInputFile[0]) {
            if (streq(ISFInputFile, "stdin"))
                isfin = stdin;
            else if ((isfin = fopen(ISFInputFile, "r")) == NULL) {
                fprintf(errfp, "Cannot open isf input file %s\n", ISFInputFile);
                fprintf(logfp, "EVENT %.6f\n\n", secs(&t0));
                Free(instruction);
                exit(1);
            }
        }
        if (ISFOutputFile[0]) {
            if (streq(ISFOutputFile, "stdout"))
                isfout = stdout;
            else if ((isfout = fopen(ISFOutputFile, "w")) == NULL) {
                fprintf(errfp, "Cannot open isf output file %s\n", ISFOutputFile);
                fprintf(logfp, "EVENT %.6f\n\n", secs(&t0));
                Free(instruction);
                exit(1);
            }
        }
/*
 *      read ISF station information from file
 */
        if (verbose) fprintf(logfp, "    ReadStafileForISF isf=%d\n", isf);
        if (isf == 2) {
            if (ReadStafileForISF2(&StationList)) {
                fprintf(logfp, "EVENT %.6f\n\n", secs(&t0));
                Free(instruction);
                exit(1);
            }
        }
        else {
            if (ReadStafileForISF(&StationList)) {
                fprintf(logfp, "EVENT %.6f\n\n", secs(&t0));
                Free(instruction);
                exit(1);
            }
        }
/*
 *
 *      Open KML bulletin output file if any
 *
 */
        if (KMLEventFile[0])
            strcpy(KMLBulletinFile, KMLEventFile);
        if (KMLBulletinFile[0]) {
            if ((kml = fopen(KMLBulletinFile, "w")) == NULL) {
                fprintf(errfp, "Cannot open kml file %s\n", KMLBulletinFile);
                fprintf(logfp, "EVENT %.6f\n\n", secs(&t0));
                Free(instruction);
                exit(1);
            }
            isbull = 1;
            WriteKMLHeader(kml);
            WriteKMLVantagePoint(kml, KMLLatitude, KMLLongitude, KMLRange,
                                 KMLHeading, KMLTilt);
            WriteKMLStyleMap(kml);
        }
    }
/*
 *
 *  Read various data files from config directory
 *
 */
    if (ReadAuxDataFiles(auxdir, &ismbQ, &mbQ, &fe, &GrnDepth, &gres, &ngrid,
                        &DepthGrid, &topo, &numECPhases, &ec, &TTtables,
                        &variogram)) {
        fprintf(logfp, "EVENT %.6f\n\n", secs(&t0));
        if (isf) Free(StationList);
        Free(instruction);
        exit(1);
    }
    if (verbose) fprintf(logfp, "ReadAuxDataFiles (%.4f) done\n", secs(&t0));
/*
 *  generate static local TT tables from local velocity model if given
 */
//    if (UseLocalTT && !LocalTTfromRSTT) {
    if (UseLocalTT) {
        fprintf(logfp, "    read local velocity model: %s\n", LocalVmodelFile);
        if ((LocalTTtables = GenerateLocalTTtables(LocalVmodelFile,
                                                  s.lat, s.lon)) == NULL) {
            fprintf(errfp, "Cannot generate static local TT tables!\n");
            fprintf(logfp, "Cannot generate static local TT tables!\n");
            UseLocalTT = 0;
        }
    }
/*
 *
 *  Initialize SLBM and read RSTT model
 *
 */
    if (UseRSTT) {
        fprintf(logfp, "    Read RSTT model: %s\n", RSTTmodel);
        fprintf(logfp, "RSTT predictions for");
        if (UseRSTTPnSn) fprintf(logfp, " Pn/Sn");
        if (UseRSTTPgLg) fprintf(logfp, " Pg/Lg");
        fprintf(logfp, " will be used.\n");
//        if (LocalTTfromRSTT) fprintf(logfp, "Local travel time tables are generated from RSTT\n");
        slbm_shell_create();
        if (slbm_shell_loadVelocityModelBinary(RSTTmodel)) {
            fprintf(errfp, "ABORT: Cannot open RSTT model %s\n", RSTTmodel);
            fprintf(logfp, "EVENT %.6f\n\n", secs(&t0));
            slbm_shell_delete();
            if (isf) Free(StationList);
            goto abort;
        }
        slbm_shell_setMaxDistance(&d);
        strcpy(buffer, "NATUTAL_NEIGHBOR");
        slbm_shell_setInterpolatorType(buffer);
    }
/*
 *
 *  Read data from ISF input file
 *
 */
    if (isf) {
/*
 *      Event loop: Read next event from ISF file
 */
        while (!ReadISF(isfin, isf, &e, &h, &p, StationList, magbloc)) {
            if (verbose) fprintf(logfp, "    Locator (%.4f)\n", secs(&t0));
/*
 *          locate event
 */
            if (Locator(isf, db, &total, &fail, opt, &e, h, &s, p,
                         ismbQ, &mbQ, ec, TTtables, &LocalTTtables,
                         &variogram, gres, ngrid, DepthGrid, &fe, GrnDepth,
                         topo, isfout, magbloc)) {
                fprintf(logfp, "CAUTION: No solution found due to %s\n",
                        errorcodes[errorcode]);
                fprintf(errfp, "CAUTION: No solution found due to %s\n",
                        errorcodes[errorcode]);
            }
            fprintf(logfp, "EVENT %.6f %s %d\n", secs(&t0), e.EventID, s.numPhase);
            if (KMLBulletinFile[0])
                WriteKML(kml, e.EventID, e.numHypo, e.numSta, &s, h, p, isbull);
            Free(h); Free(p);
            fprintf(logfp, "\n\n");
            e.StartDepth = startdepth_cf;
            e.StartOT = startot_cf;
            e.FixedDepth = depfix_cf;
            e.FixDepthToZero = surfix_cf;
            e.FixedHypocenter = hypofix_cf;
            e.FixedOT = otfix_cf;
            PrevLat = s.lat;
            PrevLon = s.lon;
        }
/*
 *      End of event loop: close output files
 */
        fclose(isfin);
        if (ISFOutputFile[0])
            fclose(isfout);
        if (KMLBulletinFile[0]) {
            WriteKMLFooter(kml);
            fclose(kml);
            if (ZipKML) {
                strcpy(ISFOutputFile, KMLBulletinFile);
                ISFOutputFile[strlen(KMLBulletinFile) - 1] = 'z';
                cmd = (char *)calloc(LINLEN, sizeof(char));
                sprintf(cmd, "zip %s %s\n", ISFOutputFile, KMLBulletinFile);
                system(cmd);
                remove(KMLBulletinFile);
                Free(cmd);
            }
        }
        Free(StationList);
    }
/*
 *  End of ISF block
 */
#ifdef DB
    else {
/*
 *
 *      Read data from database
 *
 */
        if (verbose)
            fprintf(logfp, "    establish DB connection\n");
#ifdef PGSQL
        if (db == 1 || db == 2 || db == 3) {
/*
 *          Establish and test PostgreSQL DB connection
 */
            sprintf(conninfo, "user=%s password=%s dbname=%s",
                    DBuser, DBpasswd, DBname);
            if (PgsqlConnect(conninfo)) {
                fprintf(logfp, "ABORT: cannot establish DB connection!\n");
                fprintf(errfp, "ABORT: cannot establish DB connection!\n");
                fprintf(logfp, "EVENT %.6f\n\n", secs(&t0));
                goto abort;
            }
            sprintf(sql, "select now()");
            if ((res_set = PQexec(conn, sql)) == NULL)
                PrintPgsqlError("main: error: ");
            PQclear(res_set);
            if (conn == NULL) {
                fprintf(logfp, "ABORT: cannot establish DB connection!\n");
                fprintf(errfp, "ABORT: cannot establish DB connection!\n");
                fprintf(logfp, "EVENT %.6f\n\n", secs(&t0));
                goto abort;
            }
        }
#endif
#ifdef MYSQLDB
        if (db == 4) {
/*
 *          Establish MySQL DB connection
 */
            if (MysqlConnect()) {
                fprintf(logfp, "ABORT: cannot establish DB connection!\n");
                fprintf(errfp, "ABORT: cannot establish DB connection!\n");
                fprintf(logfp, "EVENT %.6f\n\n", secs(&t0));
                goto abort;
            }
        }
#endif
#ifdef ORASQL
        if (db == 5) {
/*
 *          Establish Oracle DB connection
 */
            if (OraConnect(DBuser, DBpasswd, DBname)) {
                fprintf(logfp, "ABORT: cannot establish DB connection!\n");
                fprintf(errfp, "ABORT: cannot establish DB connection!\n");
                fprintf(logfp, "EVENT %.6f\n\n", secs(&t0));
                goto abort;
            }
        }
#endif
/*
 *      Open KML bulletin output file if requested
 */
        if (KMLBulletinFile[0]) {
            if ((kml = fopen(KMLBulletinFile, "w")) == NULL) {
                fprintf(errfp, "Cannot open kml file %s\n", KMLBulletinFile);
                fprintf(logfp, "EVENT %.6f\n\n", secs(&t0));
                goto abort;
            }
            isbull = 1;
            WriteKMLHeader(kml);
            WriteKMLVantagePoint(kml, KMLLatitude, KMLLongitude, KMLRange,
                                 KMLHeading, KMLTilt);
            WriteKMLStyleMap(kml);
        }
/*
 *      Event loop: get next event instruction line
 */
        e.evid = 0;
        while ((nb = getline(&line, &nbytes, instructfp)) > 0) {
            if ((j = nb - 2) >= 0) {
                if (line[j] == '\r') line[j] = '\n';   /* replace CR with LF */
            }
            DropSpace(line, instruction);
            errorcode = 0;
            fprintf(logfp, "Instruction: %s", instruction);
            fprintf(errfp, "instruction: %s", instruction);
/*
 *          restore config pars that could be overriden by an instruction
 */
            MinDepthPhases = MinDepthPhases_cf;
            MindDepthPhaseAgencies = MindDepthPhaseAgencies_cf;
            MaxLocalDistDeg = MaxLocalDistDeg_cf;
            MinLocalStations = MinLocalStations_cf;
            MaxSPDistDeg = MaxSPDistDeg_cf;
            MinSPpairs = MinSPpairs_cf;
            MinCorePhases = MinCorePhases_cf;
            DoGridSearch = DoGridSearch_cf;
            NAsearchRadius = NAsearchRadius_cf;
            NAsearchDepth = NAsearchDepth_cf;
            NAsearchOT = NAsearchOT_cf;
            NAinitialSample = NAinitialSample_cf;
            NAnextSample = NAnextSample_cf;
            NAcells = NAcells_cf;
            NAiterMax = NAiterMax_cf;
            UpdateDB = UpdateDB_cf;
            iseed = iseed_cf;
            MSPeriodRange = MSPeriodRange_cf;
            MinNetmagSta = MinNetmagSta_cf;
            MagMaxTimeResidual = MagMaxTimeResidual_cf;
            DoCorrelatedErrors = DoCorrelatedErrors_cf;
            verbose = 0;
            strcpy(InputDB, InputDB_cf);
            strcpy(OutputDB, OutputDB_cf);
            strcpy(NextidDB, NextidDB_cf);
            strcpy(ISFOutputFile, "");
            WriteNAResultsToFile = 0;
            UseRSTTPnSn = UseRSTTPnSn_cf;
            UseRSTTPgLg = UseRSTTPgLg_cf;
            UseRSTT = UseRSTT_cf;
            MaxLocalTTDelta = MaxLocalTTDelta_cf;
            DoNotRenamePhase = DoNotRenamePhase_cf;
            MagnitudesOnly = MagnitudesOnly_cf;
            CalculateML = CalculateML_cf;
            CalculatemB = CalculatemB_cf;
            strcpy(RSTTmodel, RSTTmodel_cf);
/*
 *          parse instruction
 */
            e.EventID[0] = '\0';
            if (ReadInstructionFile(instruction, &e, isf, auxdir, homedir)) {
                fprintf(logfp, "WARNING: bad instruction line!\n");
                fprintf(logfp, "EVENT %.6f\n\n", secs(&t0));
                continue;
            }
/*
 *          generate static local TT tables from local velocity model if given
 */
            if (strcmp(LocalVmodelFile, vfile_cf)) {
                fprintf(logfp, "    read local velocity model: %s\n",
                        LocalVmodelFile);
                if ((LocalTTtables = GenerateLocalTTtables(LocalVmodelFile,
                                                  s.lat, s.lon)) == NULL) {
                    fprintf(errfp, "Cannot generate static local TT tables!\n");
                    fprintf(logfp, "Cannot generate static local TT tables!\n");
                    UseLocalTT = 0;
                }
                strcpy(vfile_cf, LocalVmodelFile);
            }
/*
 *          Open ISF output file if requested
 */
            if (ISFOutputFile[0]) {
                if (streq(ISFOutputFile, "stdout"))
                    isfout = stdout;
                else if ((isfout = fopen(ISFOutputFile, "w")) == NULL) {
                    fprintf(errfp, "Cannot open isf output file %s\n",
                            ISFOutputFile);
                    fprintf(logfp, "EVENT %.6f\n\n", secs(&t0));
                    continue;
                }
            }
/*
 *          Read hypocenters, phases, and associations from database
 */
#ifdef PGSQL
            if (db == 1) {
/*
 *              Read data from ISC PostgreSQL database
 */
#ifdef ISCDB
                if (verbose)
                    fprintf(logfp, "ReadEventFromISCPgsqlDatabase (%.4f)\n", secs(&t0));
                if (ReadEventFromISCPgsqlDatabase(&e, &h, &p, magbloc)) {
                    fprintf(logfp, "EVENT %.6f %d\n", secs(&t0), e.evid);
                    if (ISFOutputFile[0]) {
                        fclose(isfout);
                        remove(ISFOutputFile);
                    }
                    if (KMLBulletinFile[0]) {
                        fclose(kml);
                        remove(KMLBulletinFile);
                    }
                    continue;
                }
                if (verbose)
                    fprintf(logfp, "ReadEventFromISCPgsqlDatabase (%.4f) done\n", secs(&t0));
#endif
            }
            if (db == 2) {
#ifdef SC3DB
/*
 *              Read data from SeisComp3 PostgreSQL database
 */
                if (verbose)
                    fprintf(logfp, "ReadEventFromSC3NIABDatabase (%.4f)\n", secs(&t0));
                if (ReadEventFromSC3NIABDatabase(&e, &h, &p, magbloc)) {
                    fprintf(logfp, "EVENT %.6f %d\n", secs(&t0), e.evid);
                    if (ISFOutputFile[0]) {
                        fclose(isfout);
                        remove(ISFOutputFile);
                    }
                    if (KMLBulletinFile[0]) {
                        fclose(kml);
                        remove(KMLBulletinFile);
                    }
                    continue;
                }
                if (verbose)
                    fprintf(logfp, "ReadEventFromSC3NIABDatabase (%.4f) done\n", secs(&t0));
#endif
            }
            if (db == 3) {
#ifdef IDCDB
/*
 *              Read data from IDC PostgreSQL database
 */
                if (verbose)
                    fprintf(logfp, "ReadEventFromIDCNIABDatabase (%.4f)\n", secs(&t0));
                if (ReadEventFromIDCNIABDatabase(&e, &h, &p, magbloc)) {
                    fprintf(logfp, "EVENT %.6f %d\n", secs(&t0), e.evid);
                    if (ISFOutputFile[0]) {
                        fclose(isfout);
                        remove(ISFOutputFile);
                    }
                    if (KMLBulletinFile[0]) {
                        fclose(kml);
                        remove(KMLBulletinFile);
                    }
                    continue;
                }
                if (verbose)
                    fprintf(logfp, "ReadEventFromIDCNIABDatabase (%.4f) done\n", secs(&t0));
#endif
            }
#endif /* PGSQL */
#ifdef MYSQLDB
            if (db == 4) {
/*
 *              Read data from SeisComp3 MySQL database
 */
#ifdef SC3DB
                if (verbose)
                    fprintf(logfp, "ReadEventFromSC3MysqlDatabase (%.4f)\n", secs(&t0));
                if (ReadEventFromSC3MysqlDatabase(&e, &h, &p, magbloc)) {
                    fprintf(logfp, "EVENT %.6f %d\n", secs(&t0), e.evid);
                    if (ISFOutputFile[0]) {
                        fclose(isfout);
                        remove(ISFOutputFile);
                    }
                    if (KMLBulletinFile[0]) {
                        fclose(kml);
                        remove(KMLBulletinFile);
                    }
                    continue;
                }
                if (verbose)
                    fprintf(logfp, "ReadEventFromSC3MysqlDatabase (%.4f) done\n", secs(&t0));
#endif
            }
#endif /* MYSQLDB */
#ifdef ORASQL
            if (db == 5) {
/*
 *              Read data from IDC Oracle database
 */
#ifdef IDCDB
                if (verbose)
                    fprintf(logfp, "ReadEventFromIDCOraDatabase (%.4f)\n", secs(&t0));
                if (ReadEventFromIDCOraDatabase(&e, &h, &p, magbloc)) {
                    fprintf(logfp, "EVENT %.6f %d\n", secs(&t0), e.evid);
                    if (ISFOutputFile[0]) {
                        fclose(isfout);
                        remove(ISFOutputFile);
                    }
                    if (KMLBulletinFile[0]) {
                        fclose(kml);
                        remove(KMLBulletinFile);
                    }
                    continue;
                }
                if (verbose)
                    fprintf(logfp, "ReadEventFromIDCOraDatabase (%.4f) done\n", secs(&t0));
#endif
            }
#endif /* ORASQL */
/*
 *          locate event
 */
            if (Locator(isf, db, &total, &fail, opt, &e, h, &s, p,
                        ismbQ, &mbQ, ec, TTtables, &LocalTTtables,
                        &variogram, gres, ngrid, DepthGrid, &fe, GrnDepth,
                        topo, isfout, magbloc)) {
                fprintf(logfp, "CAUTION: No solution found due to %s\n",
                        errorcodes[errorcode]);
                fprintf(errfp, "CAUTION: No solution found due to %s\n",
                        errorcodes[errorcode]);
            }
            PrevLat = s.lat;
            PrevLon = s.lon;
            fprintf(logfp, "EVENT %.6f %s %d\n", secs(&t0), e.EventID, s.numPhase);
            if (ISFOutputFile[0])
                fclose(isfout);
/*
 *          Write KML output event file if requested
 */
            if (KMLBulletinFile[0])
                WriteKML(kml, e.EventID, e.numHypo, e.numSta, &s, h, p, isbull);
            if (KMLEventFile[0]) {
                if ((ekml = fopen(KMLEventFile, "w")) == NULL) {
                    fprintf(errfp, "Cannot open kml file %s\n", KMLEventFile);
                    fprintf(logfp, "EVENT %.6f\n\n", secs(&t0));
                    continue;
                }
                WriteKMLHeader(ekml);
                WriteKMLVantagePoint(ekml, s.lat, s.lon, KMLRange,
                                     KMLHeading, KMLTilt);
                WriteKMLStyleMap(ekml);
                WriteKML(ekml, e.EventID, e.numHypo, e.numSta, &s, h, p, 0);
                WriteKMLFooter(ekml);
                fclose(ekml);
                if (ZipKML) {
                    strcpy(ISFOutputFile, KMLEventFile);
                    ISFOutputFile[strlen(KMLEventFile) - 1] = 'z';
                    cmd = (char *)calloc(LINLEN, sizeof(char));
                    sprintf(cmd, "zip %s %s\n", ISFOutputFile, KMLEventFile);
                    system(cmd);
                    remove(KMLEventFile);
                    Free(cmd);
                }
            }
            Free(h); Free(p);
            fprintf(logfp, "\n\n");
        }
        Free(line);
/*
 *      End of event loop; close KML bulletin file, if any
 */
        if (KMLBulletinFile[0]) {
            WriteKMLFooter(kml);
            fclose(kml);
            if (ZipKML) {
                strcpy(ISFOutputFile, KMLBulletinFile);
                ISFOutputFile[strlen(KMLBulletinFile) - 1] = 'z';
                cmd = (char *)calloc(LINLEN, sizeof(char));
                sprintf(cmd, "zip %s %s\n", ISFOutputFile, KMLBulletinFile);
                system(cmd);
                remove(KMLBulletinFile);
                Free(cmd);
            }
        }
    }
/*
 *  End of database block
 */
#endif    /* DB */
/*
 *  Wrap it up
 */
abort:
    Free(instruction);
    if (strcmp(instructfile, "stdin"))
        fclose(instructfp);
/*
 *  disconnect from database
 */
    if (db) {
#ifdef PGSQL
        if (db == 1 || db == 2 || db == 3)
            PgsqlDisconnect();
#endif
#ifdef MYSQLDB
        if (db == 4)
            MysqlDisconnect();
#endif
#ifdef ORASQL
        if (db == 5)
            OraDisconnect();
#endif
    }
/*
 *  free default depth grid and etopo
 */
    FreeFlinnEngdahl(&fe);
    FreeFloatMatrix(DepthGrid);
    FreeShortMatrix(topo);
    Free(GrnDepth);
/*
 *  free travel-time tables
 */
    if (TTtables != NULL)
        FreeTTtables(TTtables);
    if (LocalTTtables != NULL)
        FreeLocalTTtables(LocalTTtables);
/*
 *  free ellipticity correction coefficients
 */
    if (ec != NULL)
        FreeEllipticityCorrectionTable(ec, numECPhases);
/*
 *  free mbQ
 */
    if (ismbQ) {
        Free(mbQ.deltas);
        Free(mbQ.depths);
        FreeFloatMatrix(mbQ.q);
    }
/*
 *  free variogram
 */
    FreeVariogram(&variogram);
/*
 *  delete SLBM instance
 */
    if (UseRSTT) {
        slbm_shell_delete();
    }
/*
 *  report on totals
 */
    fprintf(logfp, "\nTotals: option 0: %d 1: %d 2: %d 3: %d 4: %d 5: %d 6: %d",
            opt[0], opt[1], opt[2], opt[3], opt[4], opt[5], opt[6]);
    fprintf(logfp, " converged: %d failed: %d time %.2f\n",
            total, fail, secs(&t00));
/*
 *  Close output files, if any.
 */
    if (strcmp(logfile, "stderr") && strcmp(logfile, "stdout"))
        fclose(logfp);
    if (strcmp(errfile, "stderr") && strcmp(errfile, "stdout"))
        fclose(errfp);
    exit(0);
}

/*
 *  Title:
 *     OpenLogErrFiles
 *  Synopsis:
 *     Open files for log and error messages
 *  Input Arguments:
 *     logfile - pathname for logfile
 *     errfile - pathname for error log
 *  Return:
 *     0/1 for success/failure.
 */
static int OpenLogErrFiles(char *logfile, char *errfile)
{
    int i;
/*
 *  Open file for error messages
 */
    if (errfile[0]) {
        if (streq(errfile, "stdout"))
            errfp = stdout;
        else if (streq(errfile, "stderr"))
            errfp = stderr;
        else {
            /* First > is meaningless - always write to errfile. */
            if (errfile[0] == '>') {
                i = 0;
                while ((errfile[i] = errfile[i+1])) i++;
            }
            /* But if there are two then want append. */
            if (errfile[0] == '>') {
                i = 0;
                while ((errfile[i] = errfile[i+1])) i++;
                if ((errfp = fopen(errfile, "a")) == NULL) {
                    fprintf(stderr, "ERROR: Can't open error file\n");
                    return 1;
                }
            }
            else {
                if ((errfp = fopen(errfile, "w")) == NULL) {
                    fprintf(stderr, "ERROR: Can't open error file\n");
                    return 1;
                }
            }
        }
    }
    else {
        fprintf(stderr, "ERROR: no error file given\n");
        return 1;
    }
/*
 *  Open logfile
 *      If command line argument given then use that to form filename.
 *      Default to location in config file - could be stdout/stderr.
 */
    if (logfile[0]) {
        if (streq(logfile, "stdout"))
            logfp = stdout;
        else if (streq(logfile, "stderr"))
            logfp = stderr;
        else {
            /* First > is meaningless - always write to logfile. */
            if (logfile[0] == '>') {
                i = 0;
                while ((logfile[i] = logfile[i+1])) i++;
            }
            /* But if there are two then want append. */
            if (logfile[0] == '>') {
                i = 0;
                while ((logfile[i] = logfile[i+1])) i++;
                if ((logfp = fopen(logfile, "a")) == NULL) {
                    fprintf(errfp, "Can't open log file %s\n", logfile);
                    return 1;
                }
            }
            else {
                if ((logfp = fopen(logfile, "w")) == NULL) {
                    fprintf(errfp, "Can't open log file %s\n", logfile);
                    return 1;
                }
            }
        }
    }
    else {
        fprintf(errfp, "No log file given\n");
        return 1;
    }
    return 0;
}

/*
 *  Title:
 *     PrintHelp
 *  Synopsis:
 *     print help on iLoc parameters
 */
static void PrintHelp()
{
    printf("Usage:\n");
    printf("echo \"<instructions>\" | iLoc [isf2.1|isf2.0|ims|isc|seiscomp|idc|niab] > logfile\n");
    printf("iLoc [isf2.1|isf2.0|ims|isc|seiscomp|idc|niab] < instructionfile > logfile\n\n");
    printf("where:\n");
    printf("    isf2.1   indicates ISC ISF2.1 input file\n");
    printf("    isf2.0   indicates ISF2.0 input file\n");
    printf("    ims      indicates ISF1.0 or IMS1.0 input file\n");
    printf("    isc      indicates ISC PostgreSQL database schema I/O\n");
    printf("    seiscomp indicates SeisComp MySQL database schema I/O\n");
    printf("    idc      indicates IDC Oracle database schema I/O\n");
    printf("    niab     indicates IDC NDC-in-a-box PostgreSQL database schema I/O\n");
    printf("Examples:\n");
    printf("echo \"bud2016aceb UpdateDB=0 depth=10\" | iloc seiscomp\n");
    printf("echo \"ISFInputFile=Namibia.isf StationFile=./Namibia_isc_stalist\" | iloc ims\n");
    printf("Instructions:\n");
    printf("    Verbose            - verbosity level (0..5)\n");
    printf("    StartDepth         - an agency code or a number\n");
    printf("    StartLat           - an agency code or a number\n");
    printf("    StartLon           - an agency code or a number\n");
    printf("    StartOT            - an agency code or a number\n");
    printf("    FixDepth           - an agency code or a number\n");
    printf("    FixEpicenter       - an agency code\n");
    printf("    FixOriginTime      - an agency code or a number\n");
    printf("    FixHypocenter      - an agency code\n");
    printf("    FixDepthToDefault  - fix depth to default depth? [0/1]\n");
    printf("    ISFInputFile       - read ISF input from here\n");
    printf("    ISFOutputFile      - write ISF output to here\n");
    printf("    KMLEventFile       - write a kml file to here for Google Earth\n");
    printf("    WriteNAResultsToFile - write NA results to file\n");
    printf("    DoNotRenamePhase   - prevent iLoc reidentifying phases [0/1]\n");
    printf("    MagnitudesOnly     - calculate magnitudes only [0/1]\n");
    printf("Instructions that can temporarily override configuration parameters:\n");
    printf("    MinDepthPhases     - min # of depth phases for depth-phase depth\n");
    printf("    MindDepthPhaseAgencies - min # of agencies reporting depth phases\n");
    printf("    MaxLocalDistDeg    - max local distance (degs)\n");
    printf("    MinLocalStations   - min number of stations within MaxLocalDistDeg\n");
    printf("    MaxSPDistDeg       - max S-P distance (degs)\n");
    printf("    MinSPpairs         - min number of S-P phase pairs\n");
    printf("    MinCorePhases      - min number of core reflections (PcP/ScS)\n");
    printf("    iseed              - random number initial seed\n");
    printf("    UpdateDB           - update DB? [0/1]\n");
    printf("    InputDB            - input DB account\n");
    printf("    OutputDB           - output DB account\n");
    printf("    NextidDB           - get new unique ids from this DB account\n");
    printf("    ZipKML             - zip KMLEventFile and remove kml? [0/1]\n");
    printf("    DoGridSearch       - perform initial grid search? [0/1]\n");
    printf("    NAsearchRadius     - search radius [deg] around initial epicentre\n");
    printf("    NAsearchDepth      - search radius [km] around initial depth\n");
    printf("    NAsearchOT         - search radius [s] around initial origin time\n");
    printf("    NAinitialSample    - size of initial sample\n");
    printf("    NAnextSample       - size of subsequent samples\n");
    printf("    NAcells            - number of cells to be resampled\n");
    printf("    NAiterMax          - max number of iterations\n");
    printf("    DoCorrelatedErrors - account for correlated error structure? [0/1]\n");
    printf("    StationFile        - full pathname of station coordinates file\n");
    printf("    MSPeriodRange      - MSH period tolerance around MSZ period\n");
    printf("    MagMaxTimeResidual - max allowable timeres for magnitude phases\n");
    printf("    MinNetmagSta       - min number of stamags to calculate netmag\n");
    printf("    CalculateML        - calculate ML?\n");
    printf("    CalculatemB        - calculate broadband mB?\n");
    printf("    RSTTmodel          - full pathname of RSTT model file\n");
    printf("    UseRSTTPnSn        - use RSTT Pn/Sn predictions? [0/1]\n");
    printf("    UseRSTTPgLg        - use RSTT Pg/Lg predictions? [0/1]\n");
    printf("    MaxLocalTTDelta    - use local TT up to this distance\n");
//    printf("    LocalTTfromRSTT    - get local TT from RSTT model at epicentre\n");
    printf("    LocalVmodelFile    - pathname for local velocity model (non-RSTT)\n");
}
