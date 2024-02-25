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
 * iLoc.h
 *  Istvan Bondar
 *  ibondar2014@gmail.com
 */
#ifndef ILOC_H
#define ILOC_H

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
/*
 * Grand Central Dispatch (MacOS)
 */
#ifdef GCD
#include <dispatch/dispatch.h>
#else
#define SERIAL
#endif
/*
 * Lapack (MacOS)
 */
#ifdef MACOSX
#include <Accelerate/Accelerate.h>
#endif
/*
 * RSTT
 */
#ifndef RSTT
#define RSTT
#endif
#ifndef SLBM_H
#define SLBM_H
#endif
#ifndef SLBM_C_SHELL_H
#include "slbm_C_shell.h"
#define SLBM_C_SHELL_H
#endif
/*
 * SeisComp3 database
 */
#ifdef SC3DB
#ifndef DB
#define DB
#endif
#endif
/*
 * IDC database
 */
#ifdef IDCDB
#ifndef DB
#define DB
#endif
#endif
/*
 * ISC database
 */
#ifdef ISCDB
#ifndef DB
#define DB
#endif
#endif
/*
 * MYSQL client
 */
#ifdef MYSQLDB
#include <mysql.h>
#endif
/*
 * PostgreSQL client
 */
#ifdef PGSQL
#include <libpq-fe.h>
#endif
/*
 * Oracle client using ODPI-C
 */
#ifdef ORASQL
#include "iLocDPI.h"
#endif

/*
 * String lengths
 */
#define LINLEN 2048                                      /* max line length */
#define PARLEN 64                              /* max parameter name length */
#define VALLEN 255                              /* max variable name length */
#define FILENAMELEN 512                             /* max file name length */
#define AGLEN 255                                 /* max agency name length */
#define STALEN 7                                 /* max station code length */
#define PHALEN 9                                   /* max phase name length */
#define MAXBUF 1024                                      /* max buffer size */
/*
 * tolerance values
 */
#define NULLVAL 9999999                                       /* null value */
#define SAMETIME_TOL 0.1              /* time tolerance for duplicate picks */
#define DEPSILON 1.e-8               /* for testing floating point equality */
#define CONV_TOL 1.e-8                             /* convergence tolerance */
#define ZERO_TOL 1.e-10                                   /* zero tolerance */
/*
 * limits (array sizes)
 */
#define MAXOPT 100                   /* max number of possible instructions */
#define MAXPHACODES 400         /* max number of IASPEI phase name mappings */
#define MAXAMP 10          /* max number of reported amplitudes for a phase */
#define MAXNUMPHA 200                   /* max number of IASPEI phase names */
#define MAXTTPHA 99                   /* max number of phases with TT table */
#define MAXLOCALTTPHA 11        /* max number of phases with local TT table */
#define MAXPHAINREADING 80             /* max number of phases in a reading */
#define MAXMAG 4               /* max number of magnitudes computed by iLoc */
/*
 *
 * Array sizes for SplineCoeffs interpolation routines
 *
 */
#define DELTA_SAMPLES 6      /* max number of TT samples in delta direction */
#define DEPTH_SAMPLES 4      /* max number of TT samples in depth direction */
#define MIN_SAMPLES 2            /* min number of samples for interpolation */
/*
 *
 * Array sizes for neighbourhood algorithm routines
 *
 */
#define NA_MAXND       4                  /* max number of model parameters */
#define NA_MAXBIT     30       /* max direction numbers for Sobol sequences */
#define NA_MAXDEG     10                  /* max degree for SAS polynomials */
/*
 * degree <-> rad conversions
 */
#define PI    M_PI                                           /* value of pi */
#define PI2   M_PI_2                                       /* value of pi/2 */
#define TWOPI 2 * PI                                        /* value of 2pi */
#define EARTH_RADIUS 6371.                                /* Earth's radius */
#define RAD_TO_DEG (180./PI)                   /* radian - degree transform */
#define DEG_TO_RAD (PI/180.)                   /* degree - radian transform */
#define DEG2KM (DEG_TO_RAD * EARTH_RADIUS)       /* degrees to km transform */
#define MAX_RSTT_DIST 15                        /* max delta for RSTT Pn/Sn */
#define EPIWALK 5.0      /* redo local TT tables if epicentre moves further */
/*
 * WGS84 ellipsoid
 */
#define FLATTENING     0.00335281066474                    /* f = 1/298.257 */

#define TRUE 1                                             /* logical true  */
#define FALSE 0                                            /* logical false */

#ifndef _FUNCS_
#define _FUNCS_
#define mabs(A)    ((A)<0 ? -(A):(A))         /* returns the abs value of A */
#define signum(A)  ((A)<0 ? -1 : 1)                /* returns the sign of A */
#define max(A,B)   ((A)>(B) ? (A):(B))        /* returns the max of A and B */
#define min(A,B)   ((A)<(B) ? (A):(B))        /* returns the min of A and B */
#define Sqrt(A)    ((A)>0 ? sqrt((A)):0.)               /* safe square root */
#define streq(A,B)  (strcmp ((A),(B)) == 0)    /* are two strings the same? */
#define swap(A,B)   { temp=(A);(A)=(B);(B)=temp; }           /* swap values */
#define swapi(A,B) { itemp=(A);(A)=(B);(B)=itemp; }          /* swap values */
#define sign(A,B)  ((B) < 0 ? ((A) < 0 ? (A) : -(A)) : ((A) < 0 ? -(A) : (A)))
#endif


/*
 * timing function to measure execution times
 */
double secs(struct timeval *t0);

/*
 *
 * C structure definitions
 *
 */

/*
 *
 * Event structure
 *
 */
typedef struct event_rec {
    char EventID[VALLEN];                               /* event identifier */
    int evid;                                        /* event id (isc_evid) */
    int PreferredOrid;                            /* id of preferred origin */
    int ISChypid;         /* for a new event not exists until location done */
    int OutputDBPreferredOrid;        /* id of preferred origin in OutputDB */
    int OutputDBISChypid;                           /* ISChypid in OutputDB */
    char prefmagid[VALLEN];               /* preferred magnitude identifier */
    char prefOriginID[VALLEN];               /* preferred origin identifier */
    char OutputDBprefOriginID[VALLEN];   /* preferred origin id in OutputDB */
    char etype[5];                    /* event type ([dfklsu ][eihlmnqrx ]) */
    int numHypo;                      /* number of hypocenters to this evid */
    int numPhase;                            /* number of associated phases */
    int numSta;                            /* number of associated stations */
    int numReading;                        /* number of associated readings */
    char DepthAgency[AGLEN];                    /* agency for initial depth */
    char EpicenterAgency[AGLEN];             /* agency for initial location */
    char OTAgency[AGLEN];                 /* agency for initial origin time */
    char HypocenterAgency[AGLEN];            /* agency for fixed hypocenter */
    double StartDepth;                       /* Value to start at or fix to */
    double StartLat;                         /* Value to start at or fix to */
    double StartLon;                         /* Value to start at or fix to */
    double StartOT;                          /* Value to start at or fix to */
    int FixDepthToZero;                         /* don't try options 2 or 3 */
    int FixedDepth;                             /* Flag that depth is fixed */
    int FixDepthToUser;               /* Flag that depth is fixed by editor */
    int FixDepthToDepdp;   /* Instruction to calculate depdp then fix on it */
    int FixDepthToDefault;                 /* Fix to regional default depth */
    int FixDepthToMedian;               /* Fix to median of reported depths */
    int FixedOT;                          /* Flag that origin time is fixed */
    int FixedEpicenter;                     /* Flag that epicentre is fixed */
    int FixedHypocenter;                   /* Flag that EVERYTHING is fixed */
    int numAgency;       /* number of agencies contributing arrivals to ISC */
} EVREC;
/*
 *
 * Hypocenter structure
 *
 */
typedef struct hyp_rec {
    int hypid;                                             /* hypocenter id */
    char origid[15];                                   /* origin identifier */
    double time;                            /* hypocenter origin epoch time */
    double lat;                           /* hypocenter geographic latitude */
    double lon;                          /* hypocenter geographic longitude */
    double depth;                                  /* hypocenter depth [km] */
    int nsta;                  /* number of readings NOT number of stations */
    int ndefsta;                             /* number of defining stations */
    int nass;                                /* number of associated phases */
    int ndef;                                  /* number of defining phases */
    double mindist;         /* distance to closest station (entire network) */
    double maxdist;        /* distance to furthest station (entire network) */
    double azimgap;                       /* azimuthal gap (entire network) */
    double sgap;                /* secondary azimuthal gap (entire network) */
    char etype[5];                    /* event type ([dfklsu ][eihlmnqrx ]) */
    char agency[AGLEN];                                      /* agency code */
    char rep[AGLEN];                                       /* reporter code */
    double sdobs;                               /* sigma * sqrt(N / (N -M)) */
    double stime;                                      /* origin time error */
    double sdepth;                                      /* depth time error */
    double smajax;                    /* error ellipse semi-major axis [km] */
    double sminax;                    /* error ellipse semi-minor axis [km] */
    double strike;                                  /* error ellipse strike */
    double depdp;                /* hypocenter depth from depth phases [km] */
    double dpderr;         /* hypocenter depth error from depth phases [km] */
    int depfix;                                              /* fixed depth */
    int epifix;                                          /* fixed epicenter */
    int timfix;                                        /* fixed origin time */
    int hypofix;                                        /* fixed hypocenter */
    int rank;                                        /* order of preference */
    int ignorehypo;             /* do not use in setting initial hypocentre */
} HYPREC;
/*
 *
 * Network magnitude structure
 *
 */
typedef struct netmag_rec {
    char agency[AGLEN];                                           /* agency */
    double magnitude;                                          /* magnitude */
    double uncertainty;                            /* magnitude uncertainty */
    int numMagnitudeAgency;  /* number of agencies contributing amps to mag */
    int numAssociatedMagnitude;             /* number of station magnitudes */
    int numDefiningMagnitude;      /* number of defining station magnitudes */
    int magid;                                              /* magnitude id */
    int mtypeid;                                       /* magnitude type id */
    char magtype[PHALEN];                                 /* magnitude type */
} NETMAG;
/*
 *
 * Solution structure
 *
 */
typedef struct sol_rec {
    int converged;                              /* convergent solution flag */
    int diverging;                               /* divergent solution flag */
    int numUnknowns;             /* number of model parameters to solve for */
    int numPhase;                      /* total number of associated phases */
    int hypid;                                             /* hypocenter id */
    double time;                            /* hypocenter origin epoch time */
    double lat;                           /* hypocenter geographic latitude */
    double lon;                          /* hypocenter geographic longitude */
    double depth;                                  /* hypocenter depth [km] */
    double depdp;                /* hypocenter depth from depth phases [km] */
    int ndp;      /* number of depth phases used in depth from depth phases */
    double depdp_error;    /* hypocenter depth error from depth phases [km] */
    double urms;                             /* unweighted RMS residual [s] */
    double wrms;                               /* weighted RMS residual [s] */
    double covar[4][4];                          /* model covariance matrix */
    double error[4];                                       /* uncertainties */
    double smajax;                    /* error ellipse semi-major axis [km] */
    double sminax;                    /* error ellipse semi-minor axis [km] */
    double strike;                                  /* error ellipse strike */
    double sdobs;                                /* urms * sqrt(N / (N -M)) */
    int nass;                                /* number of associated phases */
    int ndef;                                  /* number of defining phases */
    int ntimedef;                         /* number of time defining phases */
    int nazimdef;                      /* number of azimuth defining phases */
    int nslowdef;                     /* number of slowness defining phases */
    int prank;                            /* rank of data covariance matrix */
    int nreading;                                     /* number of readings */
    int nsta;                                /* number of distinct stations */
    int ndefsta;                             /* number of defining stations */
    double mindist;         /* distance to closest station (entire network) */
    double maxdist;        /* distance to furthest station (entire network) */
    double azimgap;                       /* azimuthal gap (entire network) */
    double sgap;                /* secondary azimuthal gap (entire network) */
    int epifix;                                          /* fixed epicenter */
    int timfix;                                        /* fixed origin time */
    int depfix;                                              /* fixed depth */
    int hypofix;                                        /* fixed hypocenter */
    int FixedDepthType;                         /* fixed depth type [0..10] */
    char OriginID[VALLEN];                             /* origin identifier */
    int nstamag;                            /* number of station magnitudes */
    int nnetmag;                            /* number of network magnitudes */
    NETMAG mag[MAXMAG];                               /* network magnitudes */
} SOLREC;
/*
 *
 * Network quality metrics structure
 *
 */
typedef struct network_qual {
    double gap;                              /* primary azimuthal gap [deg] */
    double sgap;                           /* secondary azimuthal gap [deg] */
    double du;                                    /* network quality metric */
    int ndefsta;                             /* number of defining stations */
    int ndef;                                  /* number of defining phases */
    double mindist;                                       /* min dist [deg] */
    double maxdist;                                       /* max dist [deg] */
} NETQUAL;
/*
 *
 * Hypocenter quality structure
 *
 */
typedef struct hypo_qual {
    int hypid;                                             /* hypocenter id */
    int numStaWithin10km;       /* number of defining stations within 10 km */
    int GT5candidate;                    /* 1 if GT5 candidate, 0 otherwise */
    NETQUAL LocalNetwork;               /* local (0-150 km) network quality */
    NETQUAL NearRegionalNetwork;/* near-regional (3-10 deg) network quality */
    NETQUAL TeleseismicNetwork; /* teleseismic (28-180 deg) network quality */
    NETQUAL FullNetwork;               /* whole (0-180 deg) network quality */
    double score;                          /* event score for ranking hypos */
} HYPQUAL;
/*
 *
 * Body wave magnitude attenuation Q(d,h) structure
 *
 */
typedef struct mag_atten {
    int numDistanceSamples;                   /* number of distance samples */
    int numDepthSamples;                         /* number of depth samples */
    double mindist;                                      /* min delta [deg] */
    double maxdist;                                      /* max delta [deg] */
    double mindepth;                                      /* min depth [km] */
    double maxdepth;                                      /* max depth [km] */
    double *deltas;                               /* distance samples [deg] */
    double *depths;                                   /* depth samples [km] */
    double **q;                                                   /* Q(d,h) */
} MAGQ;
/*
 *
 * Station magnitude structure
 *
 */
typedef struct stamag_rec {
    int rdid;                                                 /* reading id */
    char sta[STALEN];                                       /* station code */
    char prista[STALEN];                /* to deal with alternate sta codes */
    char agency[AGLEN];                        /* agency (station operator) */
    char deploy[AGLEN];                             /* deployment (network) */
    char lcn[3];                                                /* location */
    double magnitude;                                          /* magnitude */
    int magdef;                    /* 1 if used for the netmag, 0 otherwise */
    int mtypeid;                                       /* magnitude type id */
    char magtype[PHALEN];                                 /* magnitude type */
} STAMAG;
/*
 *
 * MS vertical/horizontal magnitude structure
 *
 */
typedef struct mszh_rec {
    int rdid;                                                 /* reading id */
    double msz;                                              /* vertical MS */
    int mszdef;                    /* 1 if used for the stamag, 0 otherwise */
    double msh;                                            /* horizontal MS */
    int mshdef;                    /* 1 if used for the stamag, 0 otherwise */
} MSZH;
/*
 *
 * Amplitude structure
 *
 */
typedef struct amp_rec {
    int ampid;                                              /* amplitude id */
    int magid;                                      /* network magnitude id */
    char amplitudeid[VALLEN];                       /* amplitude identifier */
    double amp;                              /* peak-to-peak amplitude [nm] */
    double per;                                               /* period [s] */
    double logat;                                               /* log(A/T) */
    double snr;                                    /* signal-to-noise ratio */
    char ach[9];                                  /* amplitude channel code */
    char comp;                                                 /* component */
    int ampdef;               /* 1 if used for the reading mag, 0 otherwise */
    double magnitude;                                          /* magnitude */
    int mtypeid;                                       /* magnitude type id */
    char magtype[PHALEN];                                 /* magnitude type */
} AMPREC;
/*
 *
 * Phase structure
 *
 */
typedef struct pha_rec {
    int hypid;                                             /* hypocenter id */
    int phid;                                                   /* phase id */
    int rdid;                                                 /* reading id */
    int init;                           /* initial phase in a reading [0/1] */
    int repid;                            /* report id from association row */
    char arrid[15];                                   /* arrival identifier */
    char pickid[VALLEN];                                 /* pick identifier */
    char auth[AGLEN];                                           /* observer */
    char rep[AGLEN];                                            /* reporter */
    char fdsn[30];        /* fdsn station code: agency.sta.network.location */
    char sta[STALEN];                                       /* station code */
    char prista[STALEN];                /* to deal with alternate sta codes */
    char agency[AGLEN];                        /* agency (station operator) */
    char deploy[AGLEN];                             /* deployment (network) */
    char lcn[3];                                                /* location */
    double StaLat;                                /* station latitude [deg] */
    double StaLon;                               /* station longitude [deg] */
    double StaElev;                                /* station elevation [m] */
    double StaDepth;                  /* instrument depth below surface [m] */
    char ReportedPhase[PHALEN];                           /* reported phase */
    char phase[PHALEN];                      /* phase mapped to IASPEI code */
    char prevphase[PHALEN];                /* phase from previous iteration */
    int phase_fixed;              /* 1 to stop iscloc reidentifying a phase */
    int force_undef; /* 1 if forced to be undefining by editor, 0 otherwise */
    char pch[9];                                      /* phase channel code */
    char comp;                                                 /* component */
    char sp_fm;                                       /* first motion [cd ] */
    char lp_fm;                           /* long period first motion [cd ] */
    char detchar;                             /* detection character [eiq ] */
    double delta;                              /* delta cf current solution */
    double esaz;            /* event-to-station azimuth cf current solution */
    double seaz;            /* station-to-event azimuth cf current solution */
    double azim;                                  /* measured azimuth [deg] */
    double azimres;                               /* azimuth residual [deg] */
    int azimdef;                  /* 1 if used in the location, 0 otherwise */
    int prevazimdef;                     /* azimdef from previous iteration */
    double delaz;      /* a priori azimuth measurement error estimate [deg] */
    int CovIndAzim;        /* position in covariance matrix azimuth section */
    double slow;                               /* measured slowness [s/deg] */
    double slowres;                            /* slowness residual [s/deg] */
    int slowdef;                  /* 1 if used in the location, 0 otherwise */
    int prevslowdef;                     /* slowdef from previous iteration */
    double delslo;  /* a priori slowness measurement error estimate [s/deg] */
    int CovIndSlow;       /* position in covariance matrix slowness section */
    double time;                                  /* arrival epoch time [s] */
    double timeres;                                    /* time residual [s] */
    int timedef;                  /* 1 if used in the location, 0 otherwise */
    int prevtimedef;                     /* timedef from previous iteration */
    double ttime;                       /* travel time with corrections [s] */
    double dtdd;                             /* horizontal slowness [s/deg] */
    double dtdh;                                /* vertical slowness [s/km] */
    double d2tdd;                                /* second time derivatives */
    double d2tdh;                                /* second time derivatives */
    double bpdel;                /* depth phase bounce point distance [deg] */
    double deltim;          /* a priori time measurement error estimate [s] */
    double rsttTotalErr;    /* RSTT path-dependent (model + pick) error [s] */
    double rsttPickErr;                              /* RSTT pick error [s] */
    int CovIndTime;           /* position in covariance matrix time section */
    char vmod[6];                                 /* travel time table type */
    double snr;                                    /* signal-to-noise ratio */
    int firstP;              /* 1 if first arriving defining P, 0 otherwise */
    int firstS;                       /* 1 if first arriving S, 0 otherwise */
    int hasdepthphase;        /* 1 if firstP and reading has depth phase(s) */
    int pPindex;                     /* index pointer to pP in this reading */
    int pwPindex;                   /* index pointer to pwP in this reading */
    int pSindex;                     /* index pointer to pS in this reading */
    int sPindex;                     /* index pointer to sP in this reading */
    int sSindex;                     /* index pointer to sS in this reading */
    double dupsigma;            /* extra variance factor for duplicates [s] */
    int duplicate;                           /* 1 if duplicate, 0 otherwise */
    int numamps;                           /* number of reported amplitudes */
    AMPREC a[MAXAMP];                                  /* amplitude records */
} PHAREC;
/*
 *
 * Reading structure (phase indices belonging to a reading)
 *
 */
typedef struct rdidx {
    int start;                          /* start phase index in the reading */
    int npha;                            /* number of phases in the reading */
} READING;
/*
 *
 * Phase map structure (map reported phase codes to IASPEI phase codes)
 *
 */
typedef struct PhaseMapRec {
    char ReportedPhase[PHALEN];                           /* reported phase */
    char phase[PHALEN];                                     /* IASPEI phase */
} PHASEMAP;
/*
 *
 * A priori measurement error estimates structure
 *
 */
typedef struct phaseweight {
    char phase[PHALEN];                                       /* phase name */
    double delta1;                                    /* min distance [deg] */
    double delta2;                                    /* max distance [deg] */
    double deltim;          /* a priori time measurement error estimate [s] */
    double delaz;      /* a priori azimuth measurement error estimate [deg] */
    double delslo;  /* a priori slowness measurement error estimate [s/deg] */
} PHASEWEIGHT;
/*
 *
 * Station info structure
 *
 */
typedef struct sta_rec {
    char fdsn[30];        /* fdsn station code: agency.sta.network.location */
    char sta[STALEN];                                       /* station code */
    char altsta[STALEN];                        /* alternative station code */
    char agency[AGLEN];                        /* agency (station operator) */
    char deploy[AGLEN];                             /* deployment (network) */
    char lcn[3];                                                /* location */
    double lat;                                                 /* latitude */
    double lon;                                                /* longitude */
    double elev;                                               /* elevation */
} STAREC;
/*
 *
 * travel time table structure
 *     TT tables are generated using libtau software
 *
 */
typedef struct TTtables {
    char phase[PHALEN];                                            /* phase */
    int isbounce;                         /* surface reflection or multiple */
    int ndel;                                 /* number of distance samples */
    int ndep;                                    /* number of depth samples */
    double *depths;                                   /* depth samples [km] */
    double *deltas;                               /* distance samples [deg] */
    double **tt;                                   /* travel-time table [s] */
    double **bpdel;        /* depth phase bounce point distance table [deg] */
    double **dtdd;                     /* horizontal slowness table [s/deg] */
    double **dtdh;                        /* vertical slowness table [s/km] */
} TT_TABLE;
/*
 *
 * ak135 ellipticity correction coefficients structure
 *     Note: the tau corrections are stored at 5 degree intervals in distance
 *           and at the depths 0, 100, 200, 300, 500, 700 km.
 *
 */
typedef struct ec_coef {
    char phase[PHALEN];                                            /* phase */
    int numDistanceSamples;                   /* number of distance samples */
    int numDepthSamples;                         /* number of depth samples */
    double mindist;                               /* minimum distance [deg] */
    double maxdist;                               /* maximum distance [deg] */
    double depth[6];                                  /* depth samples [km] */
    double *delta;                                /* distance samples [deg] */
    double **t0;                                         /* t0 coefficients */
    double **t1;                                         /* t1 coefficients */
    double **t2;                                         /* t2 coefficients */
} EC_COEF;
/*
 *
 * Flinn-Engdahl geographic region numbers (1995)
 *
 */
typedef struct FlinnEngdahl {
    int nlat;                                 /* number of latitude samples */
    int *nl;                /* number of longitude samples at each latitude */
    int **lon;                         /* longitude ranges at each latitude */
    int **grn;                  /* grn in longitude ranges at each latitude */
} FE;
/*
 *
 * NA search space
 *
 */
typedef struct NASearchspace {
    int nd;                                   /* number of model parameters */
    int epifix, otfix, depfix;                     /* fixed parameter flags */
    double lat, lon, ot, depth;                   /* center point of search */
    double lpnorm;         /* p = [1,2] for L1, L2 norm or anything between */
    double range[NA_MAXND][2];                       /* search space limits */
    double ranget[NA_MAXND][2];           /* normalized search space limits */
    double scale[NA_MAXND+1];                                    /* scaling */
} NASPACE;
/*
 *
 * Sobol-Antonov-Saleev coefficients for quasi-random sequences
 *
 */
typedef struct sobol_coeff {
    int n;                           /* max number of independent sequences */
    int *mdeg;                                                    /* degree */
    unsigned long *pol;                                       /* polynomial */
    unsigned long **iv;                              /* initializing values */
} SOBOL;
/*
 *
 * Variogram structure
 *
 */
typedef struct variogram {
    int n;                                             /* number of samples */
    double sill;                        /* sill (background variance) [s^2] */
    double maxsep;          /* max station separation to be considered [km] */
    double *x;                                  /* station separations [km] */
    double *y;                                    /* variogram values [s^2] */
    double *d2y;     /* second derivatives for natural spline interpolation */
} VARIOGRAM;
/*
 *
 * Phaselist structure for defining phases
 *
 */
typedef struct phaselst {
    char phase[PHALEN];                                        /* phase name */
    int nTime;                                          /* number of samples */
    int nAzim;                                          /* number of samples */
    int nSlow;                                          /* number of samples */
    int *indTime;/* permutation vector to block-diagonalize data covariances */
    int *indAzim;/* permutation vector to block-diagonalize data covariances */
    int *indSlow;/* permutation vector to block-diagonalize data covariances */
} PHASELIST;

/*
 *
 * Nearest-neighbour station order
 *
 */
typedef struct staord {
    int index;
    int x;
} STAORDER;

/*
 *
 * node stucture from single-linkage clustering
 *
 */
typedef struct node {
    int left;
    int right;
    double linkdist;
} NODE;

/*
 *
 * Local velocity model
 *
 */
typedef struct velmodel {
    int n;
    double *h;
    double *vp;
    double *vs;
    int iconr;
    int imoho;
} VMODEL;

/*
 *
 * function declarations
 *
 */
/*
 * iLocCluster.c
 */
int HierarchicalCluster(int nsta, double **distmatrix, STAORDER staorder[]);
/*
 * iLocDataCovariance.c
 */
STAREC *GetStalist(int numPhase, PHAREC p[], int *nsta);
int StarecCompare(const void *sta1, const void *sta2);
int GetStationIndex(int nsta, STAREC stalist[], char *sta);
double **GetDistanceMatrix(int nsta, STAREC stalist[]);
double **GetDataCovarianceMatrix(int nsta, int numPhase, int nd, PHAREC p[],
        STAREC stalist[], double **distmatrix, VARIOGRAM *variogramp);
int ReadVariogram(char *fname, VARIOGRAM *variogramp);
void FreeVariogram(VARIOGRAM *variogramp);
/*
 * iLocDepthPhases.c
 */
int DepthResolution(SOLREC *sp, READING *rdindx, PHAREC p[], int isverbose);
int DepthPhaseCheck(SOLREC *sp, READING *rdindx, PHAREC p[], int isverbose);
int DepthPhaseStack(SOLREC *sp, PHAREC p[], TT_TABLE *TTtables,
        short int **topo);
/*
 * iLocDistAzimuth.c
 */
void GetDeltaAzimuth(SOLREC *sp, PHAREC p[], int verbosethistime);
double DistAzimuth(double slat, double slon, double elat, double elon,
        double *azi, double *baz);
void PointAtDeltaAzimuth(double lat1, double lon1, double delta, double azim,
        double *lat2, double *lon2);
/*
 * iLocEllipticityCorrection.c
 */
EC_COEF *ReadEllipticityCorrectionTable(int *numECPhases, char *filename);
void FreeEllipticityCorrectionTable(EC_COEF *ec, int numECPhases);
double GetEllipticityCorrection(EC_COEF *ec, char *phase, double ecolat,
        double delta, double depth, double esaz);
/*
 * iLocGregion.c
 */
int ReadFlinnEngdahl(char *fname, FE *fep);
void FreeFlinnEngdahl(FE *fep);
int GregionNumber(double lat, double lon, FE *fep);
int GregToSreg(int grn);
int Gregion(int number, char *gregname);
int Sregion(int number, char *sregname);
double **ReadDefaultDepthGrid(char *fname, double *gres, int *ngrid);
double *ReadDefaultDepthGregion(char *fname);
double GetDefaultDepth(SOLREC *sp, int ngrid, double gres, double **DepthGrid,
        FE *fep, double *GrnDepth, int *isdefdep);
/*
 * iLocInitializations.c
 */
int InitializeEvent(EVREC *ep, HYPREC h[]);
int InitialHypocenter(EVREC *ep, HYPREC h[], HYPREC *starthyp);
int InitialSolution(SOLREC *sp, EVREC *ep, HYPREC *hp);
/*
 * iLocInterpolate.c
 */
void SplineCoeffs(int n, double *x, double *y, double *d2y, double *tmp);
double SplineInterpolation(double xp, int n, double *x, double *y, double *d2y,
        int isderiv, double *dydx, double *d2ydx);
void FloatBracket(double xp, int n, double *x, int *jlo, int *jhi);
void IntegerBracket(int xp, int n, int *x, int *jlo, int *jhi);
double BilinearInterpolation(double xp1, double xp2, int nx1, int nx2,
        double *x1, double *x2, double **y);
/*
 * iLocLocalTT.c
 */
TT_TABLE *GenerateLocalTTtables(char *filename, double lat, double lon);
/*
 * iLocLocationQuality.c
 */
int LocationQuality(int numPhase, PHAREC p[], HYPQUAL *hq);
double GetdUGapSgap(int nsta, double *esaz, double *gap, double *sgap);
/*
 * iLocLocator.c
 */
int Locator(int isf, int database, int *total, int *fail, int *opt, EVREC *e,
        HYPREC h[], SOLREC *s, PHAREC p[], int ismbQ, MAGQ *mbQ, EC_COEF *ec,
        TT_TABLE *TTtables, TT_TABLE *LocalTTtables[], VARIOGRAM *variogram,
        double gres, int ngrid, double **DepthGrid, FE *fe, double *GrnDepth,
        short int **topo, FILE *isfout, char *magbloc);
void Synthetic(EVREC *ep, HYPREC *hp, SOLREC *sp, READING *rdindx, PHAREC p[],
        EC_COEF *ec, TT_TABLE *TTtables, TT_TABLE *LocalTTtables[],
        short int **topo, int database, int isf);
void ResidualsForFixedHypocenter(EVREC *ep, HYPREC *hp, SOLREC *sp,
        READING *rdindx, PHAREC p[], EC_COEF *ec, TT_TABLE *TTtables,
        TT_TABLE *LocalTTtables[], short int **topo);
int LocateEvent(int option, int nsta, int has_depdpres, SOLREC *sp,
        READING *rdindx, PHAREC p[], EC_COEF *ec, TT_TABLE *TTtables,
        TT_TABLE *LocalTTtable, STAREC stalist[], double **distmatrix,
        VARIOGRAM *variogramp, STAORDER staorder[], short int **topo,
        int is2nderiv);
int GetPhaseList(int numPhase, PHAREC p[], PHASELIST plist[]);
void FreePhaseList(int nphases, PHASELIST plist[]);
void Readings(int numPhase, int nreading, PHAREC p[], READING *rdindx);
/*
 * iLocMagnitudes.c
 */
int NetworkMagnitudes(SOLREC *sp, READING *rdindx, PHAREC p[],
        STAMAG **stamag, STAMAG **rdmag, MSZH *mszh, int ismbQ, MAGQ *mbQ);
int ReadMagnitudeQ(char *filename, MAGQ * magq);
/*
 * iLocNA.c
 */
int SetNASearchSpace(SOLREC *sp, NASPACE *nasp);
int NASearch(int nsta, SOLREC *sp, PHAREC p[], TT_TABLE *TTtables,
        TT_TABLE *LocalTTtable, EC_COEF *ec, short int **topo, STAREC stalist[],
        double **distmatrix, VARIOGRAM *variogramp, STAORDER staorder[],
        NASPACE *nasp, char *filename, int is2nderiv);
/*
 * iLocPhaseIdentification.c
 */
int IdentifyPhases(SOLREC *sp, READING *rdindx, PHAREC p[], EC_COEF *ec,
        TT_TABLE *TTtables, TT_TABLE *LocalTTtable, short int **topo,
        int *is2nderiv);
int ReIdentifyPhases(SOLREC *sp, READING *rdindx, PHAREC p[], EC_COEF *ec,
        TT_TABLE *TTtables, TT_TABLE *LocalTTtable, short int **topo,
        int is2nderiv);
void IdentifyPFAKE(SOLREC *sp, PHAREC p[], EC_COEF *ec,
        TT_TABLE *TTtables, TT_TABLE *LocalTTtable, short int **topo);
void RemovePFAKE(SOLREC *sp, PHAREC p[]);
int DuplicatePhases(SOLREC *sp, PHAREC p[], EC_COEF *ec,
        TT_TABLE *TTtables, TT_TABLE *LocalTTtable, short int **topo);
void ResidualsForReportedPhases(SOLREC *sp, PHAREC *pp, EC_COEF *ec,
        TT_TABLE *TTtables, TT_TABLE *LocalTTtable, short int **topo);
int NumTimeDef(int numPha, PHAREC p[]);
/*
 * iLocPhaseOrder.c
 */
void SortPhasesFromISF(int numPhase, PHAREC p[]);
void SortPhasesFromDatabase(int numPhase, PHAREC p[]);
void SortPhasesForNA(int numPhase, int nsta, PHAREC p[], STAREC stalist[],
        STAORDER staorder[]);
/*
 * iLocSVD.c
 */
int SVDdecompose(int n, int m, double **u, double sv[], double **v);
int SVDsolve(int n, int m, double **u, double sv[], double **v, double *b,
        double *x, double thres);
void SVDModelCovarianceMatrix(int m, double thres, double sv[], double **v,
        double mcov[][4]);
double SVDthreshold(int n, int m, double sv[]);
int SVDrank(int n, int m, double sv[], double thres);
double SVDnorm(int m, double sv[], double thres, double *cond);
int ProjectionMatrix(int numPhase, PHAREC p[], int nd, double pctvar,
        double **cov, double **w, int *prank, int nunp, char **phundef,
        int ispchange);
/*
 * iLocTimeFuncs.c
 */
double HumanToEpoch(char *htime, int msec, int ismicro);
double HumanToEpochISF(int yyyy, int mm, int dd, int hh, int mi, int ss,
        int msec);
void EpochToHuman(char *htime, double etime);
void EpochToHumanSC3(char *htime, char *msec, double etime);
void EpochToHumanISF(char *hday, char *htim, double etime);
int EpochToJdate(double etime);
double ReadHumanTime(char *timestr);
/*
 * iLocTravelTimes.c
 */
TT_TABLE *ReadTTtables(char *dirname);
void FreeTTtables(TT_TABLE *TTtables);
void FreeLocalTTtables(TT_TABLE *TTtables);
short int **ReadEtopo1(char *filename);
int GetPhaseIndex(char *phase);
int GetLocalPhaseIndex(char *phase);
int TravelTimeResiduals(SOLREC *sp, PHAREC p[], char mode[4], EC_COEF *ec,
        TT_TABLE *TTtables, TT_TABLE *LocalTTtable,
        short int **topo, int iszderiv, int is2nderiv);
int GetTravelTimePrediction(SOLREC *sp, PHAREC *pp, EC_COEF *ec,
        TT_TABLE *TTtables, TT_TABLE *LocalTTtable,
        short int **topo, int iszderiv, int isfirst, int is2nderiv);
double GetTravelTimeTableValue(TT_TABLE *tt_tablep, double depth, double delta,
        int iszderiv, double *dtdd, double *dtdh, double *bpdel,
        int is2nderiv, double *d2tdd, double *d2tdh);
double GetEtopoCorrection(int ips, double rayp, double bplat, double bplon,
        short int **topo, double *tcorw);
/*
 * iLocUncertainties.c
 */
int Uncertainties(SOLREC *sp, PHAREC p[]);
/*
 * iLocUtils.c
 */
void SkipComments(char *buf, FILE *fp);
char *RightTrim(char *buf);
int DropSpace(char *str1, char *str2);
double **AllocateFloatMatrix(int nrow, int ncol);
short int **AllocateShortMatrix(int nrow, int ncol);
unsigned long **AllocateLongMatrix(int nrow, int ncol);
void FreeFloatMatrix(double **matrix);
void FreeShortMatrix(short int **matrix);
void FreeLongMatrix(unsigned long **matrix);
void Free(void *ptr);
int CompareInt(const void *x, const void *y);
int CompareDouble(const void *x, const void *y);
/*
 *
 * I/O functions, text files
 *
 */
/*
 * iLocPrintEvent.c
 */
void PrintSolution(SOLREC *sp, int grn);
void PrintHypocenter(EVREC *ep, HYPREC h[]);
void PrintPhases(int numPhase, PHAREC p[]);
void PrintDefiningPhases(int numPhase, PHAREC p[]);
/*
 * iLocReadConfig.c
 */
int ReadConfigFile(char *auxdir, char *homedir);
int ReadInstructionFile(char *instruction, EVREC *ep, int isf,
        char *auxdir, char *homedir);
/*
 * iLocReadAuxDataFiles.c
 */
int ReadAuxDataFiles(char *auxdir, int *ismbQ, MAGQ *mbQp, FE *fep,
        double **GrnDepth, double *gres, int *ngrid, double ***DepthGrid,
        short int ***topo, int *numECPhases, EC_COEF *ec[],
        TT_TABLE *TTtables[], VARIOGRAM *variogramp);
/*
 * ISF/ISF2: iLocReadISF.c, iLocWriteISF.c
 */
int ReadISF(FILE *infile, int isf, EVREC *ep, HYPREC *hp[], PHAREC *pp[],
        STAREC StationList[], char *magbloc);
void WriteISF(FILE *ofp, EVREC *ep, SOLREC *sp, HYPREC h[], PHAREC p[],
        STAMAG **stamag, STAMAG **rdmag, int grn, char *magbloc);
int ReadStafileForISF(STAREC *StationListp[]);
int ReadStafileForISF2(STAREC *StationListp[]);
int isInteger(char *substr);
/*
 * iLocWriteKML.c
 */
void WriteKML(FILE *kml, char *Eventid, int numHypo, int numSta,
        SOLREC *sp, HYPREC h[], PHAREC p[], int isbull);
void WriteKMLVantagePoint(FILE *kml, double lat, double lon, double elev,
        double heading, double tilt);
void WriteKMLHeader(FILE *kml);
void WriteKMLFooter(FILE *kml);
void WriteKMLStyleMap(FILE *kml);
/*
 *
 * I/O functions, database
 *
 */
/*
 * iLocMysqlFuncs.c
 */
#ifdef MYSQLDB
void PrintMysqlError(char *message);
int MysqlConnect(void);
void MysqlDisconnect(void);
#endif
/*
 * iLocPgsqlFuncs.c
 */
#ifdef PGSQL
void PrintPgsqlError(char *message);
int PgsqlConnect(char *conninfo);
void PgsqlDisconnect(void);
#endif
/*
 * iLocOracleFuncs.c
 */
#ifdef ORASQL
int PrintOraError(dpiStmt *stmt, char *message);
int OraConnect(char *user, char *passwd, char *constr);
void OraDisconnect(void);
#endif
/*
 * ISC schema
 */
#ifdef ISCDB
#ifdef PGSQL
int ReadEventFromISCPgsqlDatabase(EVREC *ep, HYPREC *hp[], PHAREC *pp[],
        char *magbloc);
int WriteEventToISCPgsqlDatabase(EVREC *ep, SOLREC *sp, PHAREC p[],
        HYPQUAL *hq, STAMAG **stamag, STAMAG **rdmag, MSZH *mszh, FE *fep);
int UpdateMagnitudesInISCPgsqlDatabase(EVREC *ep, SOLREC *sp, PHAREC p[],
        STAMAG **stamag, STAMAG **rdmag, MSZH *mszh);
void RemoveISCHypocenter(EVREC *ep);
void ReplaceISCPrime(EVREC *ep, HYPREC *hp);
void ReplaceISCAssociation(EVREC *ep, PHAREC p[], HYPREC *hp);
#endif
#endif /* ISCDB */
/*
 * SeisComp schema
*/
#ifdef SC3DB
#ifdef MYSQLDB
int ReadEventFromSC3MysqlDatabase(EVREC *ep, HYPREC *hp[], PHAREC *pp[],
        char *magbloc);
int WriteEventToSC3MysqlDatabase(EVREC *ep, SOLREC *sp, PHAREC p[],
        int GT5candidate, STAMAG **stamag, STAMAG **rdmag, char *gregname);
int UpdateMagnitudesInSC3MysqlDatabase(EVREC *ep, SOLREC *sp, PHAREC p[],
        STAMAG **stamag, STAMAG **rdmag);
#endif
#ifdef PGSQL
int ReadEventFromSC3NIABDatabase(EVREC *ep, HYPREC *hp[], PHAREC *pp[],
        char *magbloc);
int WriteEventToSC3NIABDatabase(EVREC *ep, SOLREC *sp, PHAREC p[],
        int GT5candidate, STAMAG **stamag, STAMAG **rdmag, char *gregname);
int UpdateMagnitudesInSC3NIABDatabase(EVREC *ep, SOLREC *sp, PHAREC p[],
        STAMAG **stamag, STAMAG **rdmag);
#endif
#endif /* SC3DB */
/*
 * IDC schema
 */
#ifdef IDCDB
#ifdef PGSQL
int ReadEventFromIDCNIABDatabase(EVREC *ep, HYPREC *hp[], PHAREC *pp[],
        char *magbloc);
int WriteEventToIDCNIABDatabase(EVREC *ep, SOLREC *sp, PHAREC p[],
        STAMAG **stamag, FE *fep);
int UpdateMagnitudesInIDCNIABDatabase(EVREC *ep, SOLREC *sp, STAMAG **stamag);
#endif
#ifdef ORASQL
int ReadEventFromIDCOraDatabase(EVREC *ep, HYPREC *hp[], PHAREC *pp[],
        char *magbloc);
int WriteEventToIDCOraDatabase(EVREC *ep, SOLREC *sp, PHAREC p[],
        STAMAG **stamag, FE *fep);
int UpdateMagnitudesInIDCOraDatabase(EVREC *ep, SOLREC *sp, STAMAG **stamag);
#endif
#endif /* IDCDB */

#endif /* ILOC_H */
