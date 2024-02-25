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
extern char InAgency[VALLEN];                    /* author for input assocs */
extern char OutAgency[VALLEN];     /* author for new hypocentres and assocs */
extern int numSta;
extern int numAgencies;
extern char agencies[MAXBUF][AGLEN];
char isf_error[LINLEN];                      /* buffer for ISF parse errors */

/*
 * Functions:
 *    ReadISF
 *    ReadStafileForISF
 *    ReadStafileForISF2
 *    isInteger
 */

/*
 * Local functions
 */
static int ParseEventID(char *line, int *evid, char *EventID);
static int ParseOrigin(char *line, HYPREC *hp, int k,
       int *yyyy, int *mm, int *dd);
static int ParseArrivals(char *line, PHAREC *pp,
       int *hh, int *mi, int *ss, int *msec);
static int StaCompare(const void *sta1, const void *sta2);
static int FDSNCompare(const void *sta1, const void *sta2);
static int GetFDSNIndex(int nsta, STAREC stalist[], char *fdsn);
static int GetStaIndex(int nsta, STAREC stalist[], char *sta);

static int GetSubString(char *part, char *line, int offset, int numchars);
static int checkBlank(char *substr);
static int isAllBlank(char *s);
static int isFloat(char *substr);

/*
 *  Title:
 *     ReadStafileForISF
 *  Synopsis:
 *     Reads stations from ISF stafile into an array.
 *  Input Arguments:
 *     StationList[] - array of structures for station information.
 *  Returns:
 *     0/1 on success/error
 *  Called by:
 *     main
 *  Calls:
 *     StaCompare
 */
int ReadStafileForISF(STAREC *StationListp[])
{
    extern char StationFile[];        /* From config file */
    FILE *fp;
    STAREC *slist = (STAREC *)NULL;
    char *line = NULL, *s;
    ssize_t nb = (ssize_t)0;
    size_t nbytes = 0;
    int i, j, k;
    double elev = 0.;
/*
 *  Open station file an get number of stations
 */
    if ((fp = fopen(StationFile, "r")) == NULL) {
        fprintf(errfp, "ReadStafileForISF: Cannot open %s\n", StationFile);
        return 1;
    }
    i = 0;
    while ((nb = getline(&line, &nbytes, fp)) > 0) {
        if ((j = nb - 2) >= 0) i++;
    }
    numSta = i;
/*
 *  memory allocation
 */
    if ((*StationListp = (STAREC *)calloc(numSta, sizeof(STAREC))) == NULL) {
        fprintf(errfp, "ReadStafileForISF: cannot allocate memory\n");
        fclose(fp);
        Free(line);
        errorcode = 1;
        return 1;
    }
    slist = *StationListp;
/*
 *  read station info
 */
    rewind(fp);
    i = 0;
    while ((nb = getline(&line, &nbytes, fp)) > 0) {
        if ((j = nb - 2) < 0) continue;
        if (line[j] == '\r') line[j] = ' ';   /* remove CR */
        k = 0;
        s = strtok(line, ", ");
        strcpy(slist[i].sta, s);
        strcpy(slist[i].agency, "FDSN");
        strcpy(slist[i].deploy, "--");
        strcpy(slist[i].lcn, "--");
        while ((s = strtok(NULL, ", ")) != NULL) {
            switch (k) {
                case 0:
                    strcpy(slist[i].altsta, s);
                    sprintf(slist[i].fdsn, "%s.%s.%s.%s",
                            slist[i].agency, slist[i].deploy,
                            slist[i].sta, slist[i].lcn);
                    break;
                case 1:
                    slist[i].lat = atof(s);
                    break;
                case 2:
                    slist[i].lon = atof(s);
                    break;
                case 3:
                    slist[i].elev = NULLVAL;
                    elev = atof(s);
                    if (elev > -9999) slist[i].elev = elev;
                    break;
                default:
                    break;
            }
            k++;
        }
        i++;
    }
    fclose(fp);
    Free(line);
    qsort(slist, numSta, sizeof(STAREC), StaCompare);
    return 0;
}

/*
 *  Title:
 *     ReadStafileForISF2
 *  Synopsis:
 *     Reads stations from ISF2 stafile into an array.
 *  Input Arguments:
 *     StationList[] - array of structures for station information.
 *  Returns:
 *     0/1 on success/error
 *  Called by:
 *     main
 *  Calls:
 *     FDSNCompare
 */
int ReadStafileForISF2(STAREC *StationListp[])
{
    extern char StationFile[];        /* From config file */
    FILE *fp;
    STAREC *slist = (STAREC *)NULL;
    char *line = NULL, *s, *p;
    ssize_t nb = (ssize_t)0;
    size_t nbytes = 0;
    int i, j, k;
    double elev = 0.;
/*
 *  Open station file an get number of stations
 */
    if ((fp = fopen(StationFile, "r")) == NULL) {
        fprintf(errfp, "ReadStafileForISF2: Cannot open %s\n", StationFile);
        return 1;
    }
    i = 0;
    while ((nb = getline(&line, &nbytes, fp)) > 0) {
        if ((j = nb - 2) < 0) continue;
        if (line[j] == '\r') line[j] = ' ';   /* remove CR */
        if (strstr(line, "<EOE>"))
            i++;
    }
    numSta = i;
/*
 *  memory allocation
 */
    if ((*StationListp = (STAREC *)calloc(numSta, sizeof(STAREC))) == NULL) {
        fprintf(errfp, "ReadStafileForISF2: cannot allocate memory\n");
        fclose(fp);
        Free(line);
        errorcode = 1;
        return 1;
    }
    slist = *StationListp;
/*
 *  read station info
 */
    rewind(fp);
    i = 0;
    k = 1;
    while ((nb = getline(&line, &nbytes, fp)) > 0) {
        if ((j = nb - 2) < 0) continue;
        if (line[j] == '\r') line[j] = ' ';    /* remove CR */
        if (strstr(line, "<EO"))
            continue;
        s = strtok(line, ":");
        strcpy(slist[i].fdsn, s);
        s = strtok(NULL, ":");
        p = strtok(s, " ");
        slist[i].lat = atof(p);
        p = strtok(NULL, " ");
        slist[i].lon = atof(p);
        p = strtok(NULL, " ");
        slist[i].elev = NULLVAL;
        elev = atof(p);
        if (elev > -9999 && elev < 9999) slist[i].elev = elev;
        strcpy(s, slist[i].fdsn);
        p = strtok(s, ".");
        strcpy(slist[i].agency, p);
        p = strtok(NULL, ".");
        strcpy(slist[i].deploy, p);
        p = strtok(NULL, ".");
        strcpy(slist[i].sta, p);
        p = strtok(NULL, ".");
        strcpy(slist[i].lcn, p);
        sprintf(slist[i].altsta, "%05d", k);
        k++;
/*
 *      co-located stations get the same altsta for nearest-neighbour sort
 */
        for (j = 0; j < i; j++) {
            if (fabs(slist[i].lat - slist[j].lat) < DEPSILON &&
                fabs(slist[i].lon - slist[j].lon) < DEPSILON) {
                strcpy(slist[i].altsta, slist[j].altsta);
                k--;
                break;
            }
        }
        if (verbose > 5)
            fprintf(logfp, "%s %6.3f %7.3f %.1f %s\n",
                    slist[i].fdsn, slist[i].lat, slist[i].lon, slist[i].elev,
                    slist[i].altsta);
        i++;
    }
    fclose(fp);
    Free(line);
    qsort(slist, numSta, sizeof(STAREC), FDSNCompare);
    fprintf(logfp, "%d stations, %d distinct station locations\n",
            numSta, k);
    return 0;
}

/*
 *
 * StaCompare: compares two stalist records based on sta code
 *
 */
static int StaCompare(const void *sta1, const void *sta2)
{
    return strcmp(((STAREC *)sta1)->sta, ((STAREC *)sta2)->sta);
}

/*
 *
 * FDSNCompare: compares two stalist records based on fdsn code
 *
 */
static int FDSNCompare(const void *sta1, const void *sta2)
{
    return strcmp(((STAREC *)sta1)->fdsn, ((STAREC *)sta2)->fdsn);
}

/*
 *  Title:
 *     GetStaIndex
 *  Synopsis:
 *     Returns index of a station in the stalist array
 *  Input Arguments:
 *     nsta      - number of distinct stations
 *     stalist[] - array of starec structures
 *     sta       - station to find
 *  Return:
 *     station index or -1 on error
 *  Called by:
 *     ReadISF
 */
static int GetStaIndex(int nsta, STAREC stalist[], char *sta)
{
    int klo = 0, khi = nsta - 1, k = 0, i;
    if (nsta > 2) {
        while (khi - klo > 1) {
            k = (khi + klo) >> 1;
            if ((i = strcmp(stalist[k].sta, sta)) == 0)
                return k;
            else if (i > 0)
                khi = k;
            else
                klo = k;
        }
        if (khi == 1) {
            k = 0;
            if (streq(sta, stalist[k].sta)) return k;
            else return -1;
        }
        if (klo == nsta - 2) {
            k = nsta - 1;
            if (streq(sta, stalist[k].sta)) return k;
            else return -1;
        }
    }
    else if (nsta == 2) {
        if (streq(sta, stalist[0].sta)) return 0;
        else if (streq(sta, stalist[1].sta)) return 1;
        else return -1;
    }
    else {
        if (streq(sta, stalist[0].sta)) return 0;
        else return -1;
    }
    return -1;
}

/*
 *  Title:
 *     GetFDSNIndex
 *  Synopsis:
 *     Returns index of an IR2 station in the stalist array
 *  Input Arguments:
 *     nsta      - number of distinct stations
 *     stalist[] - array of starec structures
 *     fdsn      - station to find
 *  Return:
 *     station index or -1 on error
 *  Called by:
 *     ReadISF
 */
static int GetFDSNIndex(int nsta, STAREC stalist[], char *fdsn)
{
    int klo = 0, khi = nsta - 1, k = 0, i;
    if (nsta > 2) {
        while (khi - klo > 1) {
            k = (khi + klo) >> 1;
            if ((i = strcmp(stalist[k].fdsn, fdsn)) == 0)
                return k;
            else if (i > 0)
                khi = k;
            else
                klo = k;
        }
        if (khi == 1) {
            k = 0;
            if (streq(fdsn, stalist[k].fdsn)) return k;
            else return -1;
        }
        if (klo == nsta - 2) {
            k = nsta - 1;
            if (streq(fdsn, stalist[k].fdsn)) return k;
            else return -1;
        }
    }
    else if (nsta == 2) {
        if (streq(fdsn, stalist[0].fdsn)) return 0;
        else if (streq(fdsn, stalist[1].fdsn)) return 1;
        else return -1;
    }
    else {
        if (streq(fdsn, stalist[0].fdsn)) return 0;
        else return -1;
    }
    return -1;
}

/*
 *  Title:
 *    ReadISF
 *  Synopsis:
 *    Reads next event from an ISF bulletin format file.
 *    Assumes that the preferred origin hypocentre is the last one in the
 *    origin block.
 *    Allocates memory to hypocentre and phase structures.
 *  Input Arguments:
 *    infile  - file pointer to read from
 *    isf     - ISF version {1,2}
 *    ep      - pointer to event info
 *    pp      - pointer to phase structures
 *    hp      - pointer to hypocentre structures
 *    magbloc - reported magnitudes from ISF input file
 *  Return:
 *    0/1 on success/error.
 *  Called by:
 *     main
 *  Calls:
 *    ParseEventID, isAllBlank, ParseOrigin, ParseArrivals, HumanToEpochISF,
 *    GetStaIndex, GetFDSNIndex, SortPhasesFromISF, PrintPhases
 */
int ReadISF(FILE *infile, int isf, EVREC *ep, HYPREC *hp[], PHAREC *pp[],
        STAREC StationList[], char *magbloc)
{
    HYPREC *h = (HYPREC *)NULL;
    PHAREC *p = (PHAREC *)NULL;
    char EventID[AGLEN];
    char *line = NULL;
    fpos_t position;
    ssize_t nb = (ssize_t)0;
    size_t nbytes = 0;
    int yyyy, mm, dd, hh, mi, ss, msec;
    int evid = 0, rdid = 0;
    char PrevSta[STALEN], PrevAuth[AGLEN];
    int found = 0;
    int i, j, k, m, nstas = 0;
/*
 *
 *  find next event
 *
 */
    ep->EventID[0] = '\0';
    ep->prefOriginID[0] = '\0';
    ep->OutputDBprefOriginID[0] = '\0';
    ep->evid = NULLVAL;
    while ((nb = getline(&line, &nbytes, infile)) > 0) {
        if ((j = nb - 2) >= 0) {
            if (line[j] == '\r') line[j] = ' ';   /* remove CR */
        }
        if (ParseEventID(line, &evid, EventID) == 0) {
            ep->evid = evid;
            strcpy(ep->EventID, EventID);
            break;
        }
    }
    if (ep->evid == NULLVAL) {
        if (!feof(infile)) {
            fprintf(errfp, "ReadISF: no event line found\n");
            fprintf(logfp, "ReadISF: no event line found\n");
        }
        Free(line);
        return 1;
    }
    if (verbose) fprintf(logfp, "    found event id %d\n", ep->evid);
/*
 *
 *  origin block
 *
 */
    found = 0;
    while ((nb = getline(&line, &nbytes, infile)) > 0) {
        if ((j = nb - 2) >= 0) {
            if (line[j] == '\r') line[j] = ' ';   /* remove CR */
        }
        if (strstr(line, "Latitude Longitude")) {
            found = 1;
            break;
        }
    }
    if (!found) {
        fprintf(errfp, "ReadISF: no origin block found\n");
        fprintf(logfp, "ReadISF: no origin block found\n");
        Free(line);
        return 1;
    }
/*
 *  count origins in origin block
 */
    fgetpos(infile, &position);
    i = 0;
    while ((nb = getline(&line, &nbytes, infile)) > 0) {
        if ((j = nb - 2) >= 0) {
            if (line[j] == '\r') line[j] = ' ';   /* remove CR */
        }
/*
 *      Reference, Magnitude, Phase, or Stop lines terminate the origin block
 */
        if (!strncmp(line, "Year", 4) || !strncmp(line, "Magn", 4) ||
            !strncmp(line, "Sta", 3)  || !strncmp(line, "STOP", 4))
            break;
/*
 *      skip ISF comment and blank lines
 */
        if (!strncmp(line, " (", 2) || isAllBlank(line))
            continue;
        i++;
    }
    if ((ep->numHypo = i) == 0) {
        fprintf(errfp, "ReadISF: no hypocentres found\n");
        fprintf(logfp, "ReadISF: no hypocentres found\n");
        Free(line);
        return 1;
    }
/*
 *  allocate memory for hypocentres
 */
    if (verbose) fprintf(logfp, "    %d hypocentres\n", ep->numHypo);
    *hp = (HYPREC *)calloc(ep->numHypo, sizeof(HYPREC));
    if (*hp == NULL) {
        fprintf(errfp, "ReadISF: cannot allocate memory\n");
        fprintf(logfp, "ReadISF: cannot allocate memory\n");
        Free(line);
        errorcode = 1;
        return 1;
    }
    h = *hp;
/*
 *  parse origin block
 *  the last origin in the block is assumed to be the preferred origin, h[0]
 */
    fsetpos(infile, &position);
    k = 1;
    while ((nb = getline(&line, &nbytes, infile)) > 0) {
        if ((j = nb - 2) >= 0) {
            if (line[j] == '\r') line[j] = ' ';    /* remove CR */
        }
/*
 *      Reference, Magnitude, Phase, or Stop lines terminate the origin block
 */
        if (!strncmp(line, "Year", 4) || !strncmp(line, "Magn", 4) ||
            !strncmp(line, "Sta", 3)  || !strncmp(line, "STOP", 4))
            break;
/*
 *      skip ISF comment and blank lines
 */
        if (!strncmp(line, " (", 2) || isAllBlank(line))
            continue;
/*
 *      parse origin line
 */
        j = k;
        if (k == ep->numHypo) j = 0;
        if (ParseOrigin(line, &h[j], j, &yyyy, &mm, &dd) == 0)
            k++;
        else {
            fprintf(errfp, "ReadISF: origin: %s\n", isf_error);
            fprintf(logfp, "ReadISF: origin: %s\n", isf_error);
        }
    }
    if ((ep->numHypo = k - 1) == 0) {
        fprintf(errfp, "ReadISF: no hypocentres found\n");
        fprintf(logfp, "ReadISF: no hypocentres found\n");
        Free(*hp);
        Free(line);
        return 1;
    }
    if (verbose) fprintf(logfp, "    %d hypocentres read\n", ep->numHypo);
    h[0].depfix = h[0].epifix = h[0].timfix = 0;
    numAgencies = 2;
    strcpy(agencies[0], h[0].agency);
    strcpy(agencies[1], "FDSN");
    if (streq(h[0].agency, OutAgency)) {
        ep->PreferredOrid = h[0].hypid;
        strcpy(ep->prefOriginID, h[0].origid);
    }
/*
 *
 *  skip reference (if any) blocks
 *
 */
    if (!strncmp(line, "Year", 4)) {
        while ((nb = getline(&line, &nbytes, infile)) > 0) {
            if ((j = nb - 2) >= 0) {
                if (line[j] == '\r') line[j] = ' ';    /* remove CR */
            }
/*
 *          Magnitude, Phase, or Stop lines terminate the origin block
 */
            if (!strncmp(line, "Magn", 4) ||
                !strncmp(line, "Sta", 3)  || !strncmp(line, "STOP", 4))
                break;
        }
    }
/*
 *
 *  find magnitude (if any) and phase blocks
 *
 */
    found = 0;
    strcpy(magbloc, "");
    if (!strncmp(line, "Magn", 4)) {
/*
 *      accumulate magnitude block in a string for output
 */
        while ((nb = getline(&line, &nbytes, infile)) > 0) {
            if ((j = nb - 2) >= 0) {
                if (line[j] == '\r') line[j] = ' ';    /* remove CR */
            }
/*
 *          Phase or Stop lines terminate the magnitude block
 */
            if (!strncmp(line, "Sta", 3) || !strncmp(line, "STOP", 4))
                break;
/*
 *          skip ISF comment and blank lines
 */
            if (!strncmp(line, " (", 2) || isAllBlank(line))
                continue;
/*
 *          skip magnitudes from OutAgency, we recalculate them, anyway
 */
            if (strstr(line, OutAgency))
                continue;
            strcat(magbloc, line);
        }
    }
    else if (!strncmp(line, "Sta", 3)) {
        found = 1;
    }
    else {
        fprintf(errfp, "ReadISF: no phase block found\n");
        fprintf(logfp, "ReadISF: no phase block found\n");
        Free(line);
        Free(*hp);
        return 1;
    }
/*
 *
 *  count phases in phase block
 *
 */
    fgetpos(infile, &position);
    i = 0;
    while ((nb = getline(&line, &nbytes, infile)) > 0) {
        if ((j = nb - 2) >= 0) {
            if (line[j] == '\r') line[j] = ' ';    /* remove CR */
        }
/*
 *      a blank line terminates the phase block
 */
        if (isAllBlank(line))
            break;
/*
 *      skip ISF comment lines
 */
        if (!strncmp(line, " (", 2))
            continue;
        i++;
    }
    if ((ep->numPhase = i) == 0) {
        fprintf(errfp, "ReadISF: no phases read\n");
        fprintf(logfp, "ReadISF: no phases read\n");
        Free(*hp);
        Free(line);
        return 1;
    }
/*
 *  allocate memory for phases
 */
    if (verbose) fprintf(logfp, "    %d phases\n", ep->numPhase);
    *pp = (PHAREC *)calloc(ep->numPhase, sizeof(PHAREC));
    if (*pp == NULL) {
        fprintf(errfp, "ReadISF: cannot allocate memory\n");
        fprintf(logfp, "ReadISF: cannot allocate memory\n");
        Free(*hp);
        Free(line);
        errorcode = 1;
        return 1;
    }
    p = *pp;
/*
 *  parse phase block
 */
    fsetpos(infile, &position);
    k = 0;
    while ((nb = getline(&line, &nbytes, infile)) > 0) {
        if ((j = nb - 2) >= 0) {
            if (line[j] == '\r') line[j] = ' ';    /* remove CR */
        }
/*
 *      a blank line terminates the phase block
 */
        if (isAllBlank(line))
            break;
/*
 *      skip ISF comment lines
 */
        if (!strncmp(line, " (", 2))
            continue;
/*
 *      parse phase line
 */
        if (ParseArrivals(line, &p[k], &hh, &mi, &ss, &msec) == 0) {
            p[k].hypid = h[0].hypid;
            p[k].phid  = k + 1;
/*
 *          arrival epoch time: take yyyy,mm,dd from preferred origin
 */
            if (hh == NULLVAL)
                p[k].time = NULLVAL;
            else {
                p[k].time = HumanToEpochISF(yyyy, mm, dd, hh, mi, ss, msec);
/*
 *              Only way phase could be before origin is if day changes
 */
                if (p[k].time < h[0].time - 3600.)
                    p[k].time += 86400.;
            }
/*
 *          check station existence and calculate delta, esaz, seaz
 *          ISF2.1 contains the station coordinates in the phase line
 */
            if (isf > 1) {
                if (isf == 2) {
/*
 *                  ISF2.0
 */
                    m = GetFDSNIndex(numSta, StationList, p[k].fdsn);
                    if (m < 0) {
                        fprintf(errfp, "ReadISF: missing station %s\n", p[k].fdsn);
                        fprintf(logfp, "ReadISF: missing station %s\n", p[k].fdsn);
                        continue;
                    }
                }
                else {
/*
 *                  IMS1.0
 */
                    m = GetStaIndex(numSta, StationList, p[k].sta);
                    if (m < 0) {
                        fprintf(errfp, "ReadISF: missing station %s\n", p[k].sta);
                        fprintf(logfp, "ReadISF: missing station %s\n", p[k].sta);
                        continue;
                    }
                }
                strcpy(p[k].prista, StationList[m].altsta);
                p[k].StaLat  = StationList[m].lat;
                p[k].StaLon  = StationList[m].lon;
                p[k].StaElev = StationList[m].elev;
            }
            p[k].delta = DistAzimuth(p[k].StaLat, p[k].StaLon, h[0].lat, h[0].lon,
                        &p[k].seaz, &p[k].esaz);
            k++;
        }
        else {
            fprintf(errfp, "ReadISF: phase: %s\n", isf_error);
            fprintf(logfp, "ReadISF: phase: %s\n", isf_error);
        }
    }
    if ((ep->numPhase = k) == 0) {
        fprintf(errfp, "ReadISF: no phases read\n");
        fprintf(logfp, "ReadISF: no phases read\n");
        Free(*hp);
        Free(*pp);
        Free(line);
        return 1;
    }
    if (verbose) fprintf(logfp, "    %d phases read\n", ep->numPhase);
/*
 *  skip rest of the lines belonging to the event (NEIC PDE ISF2)
 */
    if (isf == 2) {
        while ((nb = getline(&line, &nbytes, infile)) > 0) {
            if ((j = nb - 2) >= 0) {
                if (line[j] == '\r') line[j] = ' ';    /* remove CR */
            }
            if (strstr(line, "DATA_TYPE BULLETIN"))
                break;
        }
    }
    Free(line);
/*
 *  Sort phase structures by increasing delta, sta, time, auth
 */
    SortPhasesFromISF(ep->numPhase, p);
/*
 *  Split phases into readings and use station array to assign lat, lon.
 */
    PrevSta[0] = '\0';
    strcpy(PrevAuth, InAgency);
    rdid = 1;
    for (nstas = 0, i = 0; i < ep->numPhase; i++) {
        if (strcmp(p[i].sta, PrevSta)) {
            p[i].rdid = rdid++;
            p[i].init = 1;
            nstas++;
        }
        else if (strcmp(p[i].auth, PrevAuth)) {
            p[i].rdid = rdid++;
            p[i].init = 1;
            nstas++;
        }
        else {
            p[i].rdid = p[i-1].rdid;
        }
        strcpy(PrevSta, p[i].sta);
        strcpy(PrevAuth, p[i].auth);
    }
    ep->numReading = rdid - 1;
/*
 *  count number of distinct stations
 */
    PrevSta[0] = '\0';
    m = 0;
    for (i = 0; i < ep->numPhase; i++) {
        if (streq(p[i].prista, PrevSta)) continue;
        strcpy(PrevSta, p[i].prista);
        m++;
    }
    ep->numSta = m;
    if (verbose) fprintf(logfp, "    %d readings from %d stations\n", ep->numReading, ep->numSta);
    strcpy(ep->etype, h[0].etype);
    ep->numAgency = 1;
    if (!ep->FixedDepth)     ep->FixedDepth = h[0].depfix;
    if (!ep->FixedEpicenter) ep->FixedEpicenter = h[0].epifix;
    if (!ep->FixedOT)        ep->FixedOT = h[0].timfix;
    if (verbose) {
        fprintf(logfp, "ReadISF done.\n");
        PrintPhases(ep->numPhase, p);
    }
    return 0;
}

/*
 *  Parses an event title line.
 *     EventID could be either an integer or a string
 *     Returns 0/1 on success/error
 */
static int ParseEventID(char *line, int *evid, char *EventID)
{
    char *s;
    int i;
    i = *evid;
/*
 *  Chars 1-5:  Event
 */
    s = strtok(line, " ");
    if (streq(s, "Event") || streq(s, "EVENT")) {
        s = strtok(NULL, " ");
        strcpy(EventID, s);
        if (isInteger(s))
            *evid = i + 1;
        else
            *evid = atoi(EventID);
        return 0;
    }
    else
        return 1;
}


/*
 *  Parses an origin line.
 *     Returns 0/1 on success/error
 */
static int ParseOrigin(char *line, HYPREC *hp, int k,
                       int *yyyy, int *mm, int *dd)
{
    int hh, mi, ss, msec;
    char substr[LINLEN];
/*
 *  initializations
 */
    hp->stime = hp->sdobs = hp->sdepth = hp->azimgap = hp->sgap = NULLVAL;
    hp->smajax = hp->sminax = hp->strike = hp->dpderr = NULLVAL;
    hp->time = hp->lat = hp->lon = hp->depth = hp->depdp = NULLVAL;
    hp->ndef = hp->nsta = hp->mindist = hp->maxdist = NULLVAL;
/*
 *  Chars 1-4: year
 */
    if (!GetSubString(substr, line, 0, 4)) {
        sprintf(isf_error, "missing year: %s", line);
        return 1;
    }
    if (isInteger(substr)) {
        sprintf(isf_error, "bad year: %s", line);
        return 1;
    }
    *yyyy = atoi(substr);
/*
 *  Chars 6-7: month
 */
    if (!GetSubString(substr, line, 5, 2)) {
        sprintf(isf_error, "missing month: %s", line);
        return 1;
    }
    if (isInteger(substr)) {
        sprintf(isf_error, "bad month: %s", line);
        return 1;
    }
    *mm = atoi(substr);
/*
 *  Chars 9-10: day
 */
    if (!GetSubString(substr, line, 8, 2)) {
        sprintf(isf_error, "missing day: %s", line);
        return 1;
    }
    if (isInteger(substr)) {
        sprintf(isf_error, "bad day: %s", line);
        return 1;
    }
    *dd = atoi(substr);
/*
 *   Chars 12-13: hour
 */
    if (!GetSubString(substr, line, 11, 2)) {
        sprintf(isf_error, "missing hour: %s", line);
        return 1;
    }
    if (isInteger(substr)) {
        sprintf(isf_error, "bad hour: %s", line);
        return 1;
    }
    hh = atoi(substr);
/*
 *  Chars 15-16: minute
 */
    if (!GetSubString(substr, line, 14, 2)) {
        sprintf(isf_error, "missing minute: %s", line);
        return 1;
    }
    if (isInteger(substr)) {
        sprintf(isf_error, "bad minute: %s", line);
        return 1;
    }
    mi = atoi(substr);
/*
 *  Chars 18,19: integral second
 */
    if (!GetSubString(substr, line, 17, 2)) {
        sprintf(isf_error, "missing second: %s", line);
        return 1;
    }
    if (isInteger(substr)) {
        sprintf(isf_error, "bad second: %s", line);
        return 1;
    }
    ss = atoi(substr);
/*
 *  Chars 20-22: msec or spaces
 *  Allow decimal place with no numbers after it.
 */
    if (GetSubString(substr, line, 20, 2)) {
/*
 *      Char 20: '.' character
 */
        if (line[19] != '.') {
            sprintf(isf_error, "bad date: %s", line);
            return 1;
        }
/*
 *      Chars 21-22: 10s of msec
 */
        if (!isdigit(line[20])) {
            sprintf(isf_error, "bad date: %s", line);
            return 1;
        }
        msec = (line[20] - '0') * 100;
        if (isdigit(line[21]))
            msec += (line[21] - '0') * 10;
        else if (line[21] != ' ') {
            sprintf(isf_error, "bad date: %s", line);
            return 1;
        }
    }
    else {
/*
 *      Char 20: '.' character or space
 */
        if (line[19] != '.' && line[19] != ' ') {
            sprintf(isf_error, "bad date: %s", line);
            return 1;
        }
        msec = 0;
    }
    hp->time = HumanToEpochISF(*yyyy, *mm, *dd, hh, mi, ss, msec);
/*
 *  Char 23: timfix - either f or space
 */
    hp->timfix = 0;
    if (line[22] == 'f') hp->timfix = 1;
/*
 *  Chars 25-29: origin time error
 */
    if (GetSubString(substr, line, 24, 5)) {
        if (isFloat(substr)) {
            sprintf(isf_error, "bad stime: %s", line);
            return 1;
        }
        hp->stime = atof(substr);
    }
/*
 *  Chars 31-35: rms (sdobs)
 */
    if (GetSubString(substr, line, 30, 5)) {
        if (isFloat(substr)) {
            sprintf(isf_error, "bad sdobs: %s", line);
            return 1;
        }
        hp->sdobs = atof(substr);
    }
/*
 *  Chars 37-44: latitude
 */
    if (!GetSubString(substr, line, 36, 8)) {
        sprintf(isf_error, "missing latitude: %s", line);
        return 1;
    }
    if (isFloat(substr)) {
        sprintf(isf_error, "bad latitude: %s", line);
        return 1;
    }
    hp->lat = atof(substr);
/*
 *  Chars 46-54: longitude
 */
    if (!GetSubString(substr, line, 45, 9)) {
        sprintf(isf_error, "missing longitude: %s", line);
        return 1;
    }
    if (isFloat(substr)) {
        sprintf(isf_error, "bad longitude: %s", line);
        return 1;
    }
    hp->lon = atof(substr);
/*
 *  Char 55: epifix - either f or space.
 */
    hp->epifix = 0;
    if (line[54] == 'f') hp->epifix = 1;
/*
 *  Chars 56-60: semi-major axis of error ellipse
 */
    if (GetSubString(substr, line, 55, 5)) {
        if (isFloat(substr)) {
            sprintf(isf_error, "bad smaj: %s", line);
            return 1;
        }
        hp->smajax = atof(substr);
    }
/*
 *  Chars 62-66: semi-minor axis of error ellipse
 */
    if (GetSubString(substr, line, 61, 5)) {
        if (isFloat(substr)) {
            sprintf(isf_error, "bad smin: %s", line);
            return 1;
        }
        hp->sminax = atof(substr);
    }
/*
 *  Chars 68-70: strike
 */
    if (GetSubString(substr, line, 67, 3)) {
        if (isInteger(substr)) {
            sprintf(isf_error, "bad strike: %s", line);
            return 1;
        }
        hp->strike = atoi(substr);
    }
/*
 *  Chars 72-76: depth
 */
    if (GetSubString(substr, line, 71, 5)) {
        if (isFloat(substr)) {
            sprintf(isf_error, "bad depth: %s", line);
            return 1;
        }
        hp->depth = atof(substr);
    }
/*
 *  Char 77: depfix
 */
    hp->depfix = 0;
    if (line[76] == 'f') hp->depfix = 1;
/*
 *  Chars 79-82: depth error
 */
    if (GetSubString(substr, line, 78, 4)) {
        if (isFloat(substr)) {
            sprintf(isf_error, "bad sdepth: %s", line);
            return 1;
        }
        hp->sdepth = atof(substr);
    }
/*
 *  Chars 84-87: ndef
 */
    if (GetSubString(substr, line, 83, 4)) {
        if (isInteger(substr)) {
            sprintf(isf_error, "bad ndef: %s", line);
            return 1;
        }
        hp->ndef = atoi(substr);
    }
/*
 *  Chars 89-92: nsta
 */
    if (GetSubString(substr, line, 88, 4)) {
        if (isInteger(substr)) {
            sprintf(isf_error, "bad nsta: %s", line);
            return 1;
        }
        hp->nsta = atoi(substr);
    }
/*
 *  Chars 94-96: gap
 */
    if (GetSubString(substr, line, 93, 3)) {
        if (isInteger(substr)) {
            sprintf(isf_error, "bad gap: %s", line);
            return 1;
        }
        hp->azimgap = atoi(substr);
    }
    else hp->azimgap = NULLVAL;
/*
 *  Chars 98-103: minimum distance
 */
    if (GetSubString(substr, line, 97, 6)) {
        if (isFloat(substr)) {
            sprintf(isf_error, "bad mindist: %s", line);
            return 1;
        }
        hp->mindist = atof(substr);
    }
/*
 *  Chars 105-110: maximum distance
 */
    if (GetSubString(substr, line, 104, 6)) {
        if (isFloat(substr)) {
            sprintf(isf_error, "bad maxdist: %s", line);
            return 1;
        }
        hp->maxdist = atof(substr);
    }
/*
 *  Chars 116-117: event type
 */
    if (!GetSubString(substr, line, 115, 2))
        strcpy(substr, "  ");
    else if (strlen(substr) != 2) {
        sprintf(isf_error, "bad etype: %s", line);
        return 1;
    }
    strcpy(hp->etype, substr);
/*
 *  Chars 119-127: author
 */
    if (!GetSubString(substr, line, 118, 9)) {
        sprintf(isf_error, "missing author: %s", line);
        return 1;
    }
    if (checkBlank(substr)) {
        sprintf(isf_error, "bad author: %s", line);
        return 1;
    }
    strcpy(hp->agency, substr);
/*
 *  Chars 129-139: origin ID
 */
    if (GetSubString(substr, line, 128, 11)) {
        if (!isInteger(substr))
            hp->hypid = atoi(substr);
        else
            hp->hypid = k;
    }
    else {
        sprintf(isf_error, "missing origid: %s", line);
        return 1;
    }
    strcpy(hp->origid, substr);
/*
 *  Chars 141-145: reporter
 */
    strcpy(hp->rep, "");
    if (GetSubString(substr, line, 140, 5)) {
        if (checkBlank(substr)) {
            sprintf(isf_error, "bad reporter: %s", line);
            return 1;
        }
        strcpy(hp->rep, substr);
    }
/*
 *  Chars 147-151: depth-phase depth
 */
    if (GetSubString(substr, line, 146, 5)) {
        if (!isFloat(substr))
            hp->depdp = atof(substr);
    }
/*
 *  Chars 153-157: depth-phase depth error
 */
    if (GetSubString(substr, line, 152, 5)) {
        if (!isFloat(substr))
            hp->dpderr = atof(substr);
    }
/*
 *  Chars 159-161: sgap
 */
    if (GetSubString(substr, line, 158, 3)) {
        if (!isInteger(substr))
            hp->sgap = atoi(substr);
    }
    return 0;
}

/*
 *  Parses a phase block data line.
 *     Only those values are parsed that are used by the locator.
 *     Returns 0/1 on success/error
 */
static int ParseArrivals(char *line, PHAREC *pp,
                         int *hh, int *mi, int *ss, int *msec)
{
    char substr[LINLEN], c = '_';
    double amp = NULLVAL, per = NULLVAL;
    char ach[5];
/*
 *  initializations
 */
    *hh = *mi = *ss = *msec = 0;
    pp->delta = pp->time = pp->slow = pp->azim = pp->snr = NULLVAL;
    pp->numamps = 0;
    strcpy(pp->sta, "");
    strcpy(pp->ReportedPhase, "");
    strcpy(pp->rep, InAgency);
    strcpy(pp->auth, InAgency);
    strcpy(pp->agency, "FDSN");
    strcpy(pp->deploy, "IR");
    strcpy(pp->lcn, "--");
    strcpy(pp->arrid, "");
    strcpy(pp->pch, "???");
    strcpy(ach, "???");
    pp->StaDepth = 0;
/*
 *  Chars 1-5: station code
 */
    if (!GetSubString(substr, line, 0, 5)) {
        sprintf(isf_error, "missing sta: %s", line);
        return 1;
    }
    if (checkBlank(substr)) {
        sprintf(isf_error, "bad sta: %s", line);
        return 1;
    }
    strcpy(pp->sta, substr);
    strcpy(pp->prista, substr);
/*
 *  Chars 7-12: distance
 */
    if (GetSubString(substr, line, 6, 6)) {
        if (isFloat(substr)) {
            sprintf(isf_error, "bad distance: %s", line);
            return 1;
        }
        pp->delta = atof(substr);
    }
/*
 *  Chars 20-27: phase code - can be null
 */
    if (GetSubString(substr, line, 19, 8)) {
        if (checkBlank(substr)) {
            sprintf(isf_error, "bad phase: %s", line);
            return 1;
        }
        strcpy(pp->ReportedPhase, substr);
    }
/*
 *  Chars 29-40: arrival time - can be null
 */
    if (GetSubString(substr, line, 28, 12)) {
/*
 *      Chars 29,30: hour
 */
        if (!GetSubString(substr, line, 28, 2)) {
            sprintf(isf_error, "missing hour: %s", line);
            return 1;
        }
        if (isInteger(substr)) {
            sprintf(isf_error, "bad hour: %s", line);
            return 1;
        }
        *hh = atoi(substr);
/*
 *      Chars 32,33: minute
 */
        if (!GetSubString(substr, line, 31, 2)) {
            sprintf(isf_error, "missing minute: %s", line);
            return 1;
        }
        if (isInteger(substr)) {
            sprintf(isf_error, "bad minute: %s", line);
            return 1;
        }
        *mi = atoi(substr);
/*
 *      Chars 35,36: second
 */
        if (!GetSubString(substr, line, 34, 2)) {
            sprintf(isf_error, "missing second: %s", line);
            return 1;
        }
        if (isInteger(substr)) {
            sprintf(isf_error, "bad second: %s", line);
            return 1;
        }
        *ss = atoi(substr);
/*
 *      Char 37-40: msec or spaces
 *      Allow decimal place without any numbers after it.
 */
        if (GetSubString(substr, line, 37, 3)) {
/*
 *          Char 37: '.' character
 */
            if (line[36] != '.') {
                sprintf(isf_error, "bad time: %s", line);
                return 1;
            }
/*
 *          Chars 38-40: msec.
 */
            if (!isdigit(line[37])) {
                sprintf(isf_error, "bad time: %s", line);
                return 1;
            }
            *msec = (line[37] - '0') * 100;
            if (isdigit(line[38]))
                *msec += (line[38] - '0') * 10;
            else if (line[38] != ' ' || line[39] != ' ') {
                sprintf(isf_error, "bad time: %s", line);
                return 1;
            }
            if (isdigit(line[39]))
                *msec += (line[39] - '0');
            else if (line[39] != ' ') {
                sprintf(isf_error, "bad time: %s", line);
                return 1;
            }
        }
        else {
/*
 *          Char 37: '.' character or space
 */
            if (line[36] != '.' && line[36] != ' ') {
                sprintf(isf_error, "bad time: %s", line);
                return 1;
            }
            *msec = 0;
        }
    }
/*
 *  Chars 48-52: observed azimuth
 */
    if (GetSubString(substr, line, 47, 5)) {
        if (isFloat(substr)) {
            sprintf(isf_error, "bad azim: %s", line);
            return 1;
        }
        pp->azim = atof(substr);
    }
/*
 *  Chars 60-65: slowness
 */
    if (GetSubString(substr, line, 59, 6)) {
        if (isFloat(substr)) {
            sprintf(isf_error, "bad slow: %s", line);
            return 1;
        }
        pp->slow = atof(substr);
    }
/*
 *  Chars 78-82: snr
 */
    if (GetSubString(substr, line, 77, 5)) {
        if (isFloat(substr)) {
            sprintf(isf_error, "bad snr: %s", line);
            return 1;
        }
        pp->snr = atof(substr);
    }
/*
 *  Chars 84-92: amplitude
 */
    if (GetSubString(substr, line, 83, 9)) {
        if (isFloat(substr)) {
            sprintf(isf_error, "bad amp: %s", line);
            return 1;
        }
        amp = atof(substr);
        pp->numamps = 1;
    }
    else
        pp->numamps = 0;
/*
 *  Chars 94-98: period
 */
    if (GetSubString(substr, line, 93, 5)) {
        if (isFloat(substr)) {
            sprintf(isf_error, "bad per: %s", line);
            return 1;
        }
        per = atof(substr);
    }
/*
 *  Char 101: sp_fm
 */
    c = tolower(line[100]);
    if (c == 'c' || c == 'd') pp->sp_fm = c;
    else                      pp->sp_fm = '_';
/*
 *  Char 102: detection character
 */
    c = tolower(line[101]);
    if (c == 'i' || c == 'e' || c == 'q') pp->detchar = c;
    else                                  pp->detchar = '_';
/*
 *  ISF2 extra columns
 */
/*
 *  Chars 115-125: arrival id
 */
    if (GetSubString(substr, line, 114, 11)) {
        if (checkBlank(substr)) {
            sprintf(isf_error, "bad agency: %s", line);
            return 1;
        }
        strcpy(pp->arrid, substr);
    }
/*
 *  Chars 127-131: agency code
 */
    if (GetSubString(substr, line, 126, 5)) {
        if (checkBlank(substr)) {
            sprintf(isf_error, "bad agency: %s", line);
            return 1;
        }
        strcpy(pp->agency, substr);
    }
/*
 *  Chars 133-140: deployment code
 */
    if (GetSubString(substr, line, 132, 8)) {
        if (checkBlank(substr)) {
            sprintf(isf_error, "bad deployment: %s", line);
            return 1;
        }
        strcpy(pp->deploy, substr);
    }
/*
 *  Chars 142-143: location code
 */
    if (GetSubString(substr, line, 141, 2)) {
        if (checkBlank(substr)) {
            sprintf(isf_error, "bad location: %s", line);
            return 1;
        }
        strcpy(pp->lcn, substr);
    }
/*
 *  Chars 145-149: author code
 */
    if (GetSubString(substr, line, 144, 5)) {
        if (checkBlank(substr)) {
            sprintf(isf_error, "bad author: %s", line);
            return 1;
        }
        strcpy(pp->auth, substr);
    }
/*
 *  Chars 151-155: reporter code
 */
    if (GetSubString(substr, line, 150, 5)) {
        if (checkBlank(substr)) {
            sprintf(isf_error, "bad reporter: %s", line);
            return 1;
        }
        strcpy(pp->rep, substr);
    }
/*
 *  Chars 157-159: pick channel code
 */
    if (GetSubString(substr, line, 156, 3)) {
        if (checkBlank(substr)) {
            sprintf(isf_error, "bad phase channel: %s", line);
            return 1;
        }
        strcpy(pp->pch, substr);
    }
/*
 *  Chars 161-163: amplitude channel code
 */
    if (GetSubString(ach, line, 160, 3)) {
        if (checkBlank(ach)) {
            sprintf(isf_error, "bad amplitude channel: %s", line);
            return 1;
        }
    }
/*
 *  Char 165: long period first motion
 */
    c = tolower(line[164]);
    if (c == 'd' || c == 'c') pp->lp_fm = c;
    else                      pp->lp_fm = '_';
/*
 *  station code
 */
    sprintf(pp->fdsn, "%s.%s.%s.%s", pp->agency, pp->deploy, pp->sta, pp->lcn);
/*
 *  Chars 167-174: station latitude
 */
    if (GetSubString(substr, line, 166, 8)) {
        if (isFloat(substr)) {
            sprintf(isf_error, "bad station latitude: %s", line);
            return 1;
        }
        pp->StaLat = atof(substr);
    }
/*
 *  Chars 176-184: station longitude
 */
    if (GetSubString(substr, line, 175, 9)) {
        if (isFloat(substr)) {
            sprintf(isf_error, "bad station longitude: %s", line);
            return 1;
        }
        pp->StaLon = atof(substr);
    }
/*
 *  Chars 186-192: station elevation [m]
 */
    if (GetSubString(substr, line, 185, 7)) {
        if (isFloat(substr)) {
            sprintf(isf_error, "bad station elevation: %s", line);
            return 1;
        }
        pp->StaElev = atof(substr);
    }
/*
 *  Chars 194-199: instrument depth below surface [m]
 */
    if (GetSubString(substr, line, 193, 6)) {
        if (isFloat(substr)) {
            sprintf(isf_error, "bad instrument depth: %s", line);
            return 1;
        }
        pp->StaDepth = atof(substr);
    }
/*
 *  amplitude and periods
 */
    if (pp->numamps) {
        pp->a[0].amp = amp;
        pp->a[0].per = per;
        strcpy(pp->a[0].ach, ach);
        if (ach[2] == 'Z' || ach[2] == 'E' || ach[2] == 'N')
            pp->a[0].comp = ach[2];
        else {
            pp->a[0].comp = ' ';
            if (streq(pp->ReportedPhase, "LRZ") ||
                streq(pp->ReportedPhase, "LZ") ||
                streq(pp->ReportedPhase, "PMZ"))
                pp->a[0].comp = 'Z';
            if (streq(pp->ReportedPhase, "LRE") ||
                streq(pp->ReportedPhase, "LE"))
                pp->a[0].comp = 'E';
            if (streq(pp->ReportedPhase, "LRN") ||
                streq(pp->ReportedPhase, "LN"))
                pp->a[0].comp = 'N';
        }
    }
    return 0;
}

/*
 *  Get a substring, removing leading and trailing white space.
 *  Expects a string, an offset from the start of the string, and a maximum
 *  length for the resulting substring. If this length is 0 it will take up
 *  to the end of the input string.
 *
 *  Need to allow for ')' to come after the required field at the end of a
 *  comment.  Discard ')' at end of a string  as long as there's no '('
 *  before it anywhere.
 *
 *  Returns the length of the resulting substring.
 */
static int GetSubString(char *part, char *line, int offset, int numchars)
{
    int i, j, len;
    int bracket = 0;
    len = (int)strlen(line);
    if (len < offset) return 0;
    if (numchars == 0) numchars = len - offset;
    for (i = offset, j = 0; i < offset + numchars; i++) {
        if (j == 0 && (line[i] == ' ' || line[i] == '\t')) continue;
        if (line[i] == '\0' || line[i] == '\n') break;
        part[j++] = line[i];
        if (line[i] == '(') bracket = 1;
    }
    if (!bracket) {
        while (--j != -1 && (part[j] == ' ' ||
                             part[j] == '\t' || part[j] == ')'));
        part[++j] = '\0';
    }
    else if (j) {
        while (part[--j] == ' ' || part[j] == '\t');
        part[++j] = '\0';
    }
    return j;
}

/*
 *  Check that a string has no spaces in it.
 *  Returns 0 if there are no spaces or 1 if there is a space.
 */
static int checkBlank(char *substr)
{
    int i, n;
    n = strlen(substr);
    for (i = 0; i < n; i++) {
        if (substr[i] == ' ' || substr[i] == '\t')
            return 1;
    }
    return 0;
}

/*
 *  Check if a string is composed entirely of white space or not.
 *  Returns 1 if it is, 0 if it isn't.
 */
static int isAllBlank(char *s)
{
    int i;
    for (i = 0; s[i] == ' ' || s[i] == '\t'; i++);
    if (s[i] == '\n' || s[i] == '\0') return 1;
    return 0;
}

/*
 *  Check that a string contains only sign/number characters
 *  atoi itself does no checking.
 *  Returns 0 if OK,  1 if not.
 */
int isInteger(char *substr)
{
    int i, n;
    n = strlen(substr);
    for (i = 0; i < n; i++) {
        if (isdigit(substr[i])) continue;
        if (i == 0)
            if (substr[i] == '+' || substr[i] == '-') continue;
        return 1;
    }
    return 0;
}

/*
 *  Check that a string contains only sign/number/decimal point characters
 *  atof itself does no checking.
 *  Returns 0 if OK,  1 if not.
 */
static int isFloat(char *substr)
{
    int i, n;
    n = strlen(substr);
    for (i=0; i < n; i++) {
        if (isdigit(substr[i]) || substr[i] == '.') continue;
        if (i == 0)
            if (substr[i] == '+' || substr[i] == '-') continue;
        return 1;
    }
    return 0;
}

/*  EOF  */
