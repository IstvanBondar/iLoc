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

/*
 * Functions:
 *    HumanToEpoch
 *    EpochToHuman
 *    EpochToHumanSC3
 *    HumanToEpochISF
 *    EpochToHumanISF
 *    EpochToJdate
 *    ReadHumanTime
 *    secs
 */

/*
 *  Title:
 *     HumanToEpoch
 *  Synopsis:
 *     Converts human time to epoch time.
 *  Input Arguments:
 *     htime - human time string 'YYYY-MM-DD HH:MI:SS'
 *     msec  - fractional seconds
 *     ismicro - fractional seconds are microseconds [1] or milliseconds [0]
 *  Return:
 *     epoch time
 *  Called by:
 *     GetSC3Hypocenter, GetSC3Phases
 */
double HumanToEpoch(char *htime, int msec, int ismicro)
{
    struct tm *ht = (struct tm *)NULL;
    char cutstr[25], *remnant;
    time_t et;
    int yyyy = 0, mm = 0, dd = 0, hh = 0, mi = 0, ss = 0;
    double t = 0.;
    ht = (struct tm *)calloc(1, sizeof(struct tm));
/*
 *  break down human time string
 */
    strcpy(cutstr, htime);

    cutstr[4] = '\0';
    yyyy = (int)strtod(&cutstr[0], &remnant);
    if (remnant[0]) return 0;

    cutstr[7] = '\0';
    mm = (int)strtod(&cutstr[5], &remnant);
    if (remnant[0]) return 0;

    cutstr[10] = '\0';
    dd = (int)strtod(&cutstr[8], &remnant);
    if (remnant[0]) return 0;

    cutstr[13] = '\0';
    hh = (int)strtod(&cutstr[11], &remnant);
    if (remnant[0]) return 0;

    cutstr[16] = '\0';
    mi = (int)strtod(&cutstr[14], &remnant);
    if (remnant[0]) return 0;

    cutstr[19] = '\0';
    ss = (int)strtod(&cutstr[17], &remnant);
    if (remnant[0]) return 0;
/*
 *  set time structure
 */
    ht->tm_year = yyyy - 1900;
    ht->tm_mon = mm - 1;
    ht->tm_mday = dd;
    ht->tm_hour = hh;
    ht->tm_min = mi;
    ht->tm_sec = ss;
    ht->tm_isdst = 0;

/*
 *  convert to epoch time
 */
    et = mktime(ht);
    if (ismicro) t = (double)et + (double)msec / 1000000.;
    else         t = (double)et + (double)msec / 1000.;
    Free(ht);
    return t;
}

/*
 *  Title:
 *     HumanToEpochISF
 *  Synopsis:
 *     Converts human time to epoch time.
 *  Input Arguments:
 *     yyyy, mm, dd, hh, mi, ss, msec
 *  Return:
 *     epoch time
 *  Called by:
 *     read_isf
 */
double HumanToEpochISF(int yyyy, int mm, int dd, int hh, int mi, int ss, int msec)
{
    struct tm *ht = (struct tm *)NULL;
    time_t et;
    double t = 0.;
    ht = (struct tm *)calloc(1, sizeof(struct tm));
/*
 *  set time structure
 */
    ht->tm_year = yyyy - 1900;
    ht->tm_mon = mm - 1;
    ht->tm_mday = dd;
    ht->tm_hour = hh;
    ht->tm_min = mi;
    ht->tm_sec = ss;
    ht->tm_isdst = 0;
/*
 *  convert to epoch time
 */
    et = mktime(ht);
    t = (double)et + (double)msec / 1000.;
    Free(ht);
    return t;
}

/*
 *  Title:
 *     EpochToHuman
 *  Synopsis:
 *     Converts epoch time to human time.
 *  Input Arguments:
 *     etime - epoch time (double, including fractional seconds)
 *  Output Arguments:
 *     htime - human time string 'YYYY-MM-DD HH:MI:SS.SSS'
 *  Called by:
 *     Locator, PrintSolution, PrintHypocenter, PrintPhases,
 *     PrintDefiningPhases, NASearch, WriteNAModels, WriteISFHypocenter
 */
void EpochToHuman(char *htime, double etime)
{
    struct tm *ht = (struct tm *)NULL;
    time_t et;
    int yyyy = 0, mm = 0, dd = 0, hh = 0, mi = 0, ss = 0;
    double t = 0., ft = 0., sec = 0.;
    int it = 0;
    char s[25];
    if (etime != NULLVAL) {
/*
 *      break down epoch time to day and msec used by ISC DB schema
 *      also take care of negative epoch times
 */
        sprintf(s, "%.3f", etime);
        t = atof(s);
        it = (int)t;
        sprintf(s, "%.3f\n", t - it);
        ft = atof(s);
        if (ft < 0.) {
            ft += 1.;
            t -= 1.;
        }
/*
 *      set time structure
 */
        ht = (struct tm *)calloc(1, sizeof(struct tm));
        et = (time_t)t;
        gmtime_r(&et, ht);
        yyyy = ht->tm_year + 1900;
        mm = ht->tm_mon + 1;
        dd = ht->tm_mday;
        hh = ht->tm_hour;
        mi = ht->tm_min;
        ss = ht->tm_sec;
        sec = (double)ht->tm_sec + ft;
/*
 *      human time string 'YYYY-MM-DD HH:MI:SS.SSS'
 */
        sprintf(htime, "%04d-%02d-%02d %02d:%02d:%06.3f",
                yyyy, mm, dd, hh, mi, sec);
        Free(ht);
    }
    else {
        strcpy(htime, "                       ");
    }
}

/*
 *  Title:
 *     EpochToHumanSC3
 *  Synopsis:
 *     Converts epoch time to human time for MySQL ids.
 *  Input Arguments:
 *     etime - epoch time (double, including fractional seconds)
 *  Output Arguments:
 *     htime - human time string 'YYYYMMDDHHMISS.SSSSSS'
 *  Called by:
 *
 */
void EpochToHumanSC3(char *htime, char *msec, double etime)
{
    struct tm *ht = (struct tm *)NULL;
    time_t et;
    int yyyy = 0, mm = 0, dd = 0, hh = 0, mi = 0, ss = 0;
    double t = 0., ft = 0., sec = 0.;
    int it = 0;
    char s[25];
    if (etime != NULLVAL) {
/*
 *      break down epoch time to day and msec used by ISC DB schema
 *      also take care of negative epoch times
 */
        sprintf(s, "%.6f", etime);
        t = atof(s);
        it = (int)t;
        sprintf(s, "%.6f\n", t - it);
        ft = atof(s);
        if (ft < 0.) {
            ft += 1.;
            t -= 1.;
        }
/*
 *      set time structure
 */
        ht = (struct tm *)calloc(1, sizeof(struct tm));
        et = (time_t)t;
        gmtime_r(&et, ht);
        yyyy = ht->tm_year + 1900;
        mm = ht->tm_mon + 1;
        dd = ht->tm_mday;
        hh = ht->tm_hour;
        mi = ht->tm_min;
        ss = ht->tm_sec;
        sec = (double)ht->tm_sec + ft;
/*
 *      human time string 'YYYY-MM-DD HH:MI:SS.SSS'
 */
        sprintf(htime, "%04d%02d%02d%02d%02d%02d",
                yyyy, mm, dd, hh, mi, ss);
        sprintf(msec, "%06.0f", 1000000. * ft);
        Free(ht);
    }
    else {
        strcpy(htime, "                   ");
        strcpy(msec, "      ");
    }
}

/*
 *  Title:
 *     EpochToHumanISF
 *  Synopsis:
 *     Converts epoch time to human time.
 *  Input Arguments:
 *     etime - epoch time (double, including fractional seconds)
 *  Output Arguments:
 *     hday - human time string YYYY/MM/DD
 *     htim - human time string HH:MM:SS.SSS
 *  Called by:
 *     WriteISFHypocenter
 */
void EpochToHumanISF(char *hday, char *htim, double etime)
{
    struct tm *ht = (struct tm *)NULL;
    time_t et;
    int yyyy = 0, mm = 0, dd = 0, hh = 0, mi = 0, ss = 0;
    double t = 0., ft = 0., sec = 0.;
    int it = 0;
    char s[25];
    if (etime != NULLVAL) {
/*
 *      break down epoch time to day and msec used by ISC DB schema
 *      also take care of negative epoch times
 */
        sprintf(s, "%.3f", etime);
        t = atof(s);
        it = (int)t;
        sprintf(s, "%.3f\n", t - it);
        ft = atof(s);
        if (ft < 0.) {
            ft += 1.;
            t -= 1.;
        }
/*
 *      set time structure
 */
        ht = (struct tm *)calloc(1, sizeof(struct tm));
        et = (time_t)t;
        gmtime_r(&et, ht);
        yyyy = ht->tm_year + 1900;
        mm = ht->tm_mon + 1;
        dd = ht->tm_mday;
        hh = ht->tm_hour;
        mi = ht->tm_min;
        ss = ht->tm_sec;
        sec = (double)ht->tm_sec + ft;
/*
 *      human time string 'YYYY/MM/DD HH:MI:SS.SSS'
 */
        sprintf(hday, "%04d/%02d/%02d", yyyy, mm, dd);
        sprintf(htim, "%02d:%02d:%06.3f", hh, mi, sec);
        Free(ht);
    }
    else {
        strcpy(hday, "          ");
        strcpy(htim, "            ");
    }
}

/*
 *  Title:
 *     EpochToJdate
 *  Synopsis:
 *     Converts epoch time to jdate.
 *  Input Arguments:
 *     etime - epoch time (double, including fractional seconds)
 *  Output Arguments:
 *     jdate - YYYYDDD
 *  Called by:
 *     WriteIDCOrigin
 */
int EpochToJdate(double etime)
{
    struct tm *ht = (struct tm *)NULL;
    time_t et;
    int yyyy = 0, yday = 0;
    double t = 0., ft = 0.;
    int it = 0;
    char s[25];
    int jdate = -1;
    if (etime == NULLVAL) return jdate;
/*
 *  break down epoch time to day and msec used by ISC DB schema
 *  also take care of negative epoch times
 */
    sprintf(s, "%.3f", etime);
    t = atof(s);
    it = (int)t;
    sprintf(s, "%.3f\n", t - it);
    ft = atof(s);
    if (ft < 0.) {
        ft += 1.;
        t -= 1.;
    }
/*
 *  set time structure
 */
    ht = (struct tm *)calloc(1, sizeof(struct tm));
    et = (time_t)t;
    gmtime_r(&et, ht);
    yyyy = ht->tm_year + 1900;
    yday = ht->tm_yday + 1;
    Free(ht);
/*
 *  day of year 'YYYYDOY'
 */
    sprintf(s, "%04d%03d", yyyy, yday);
    jdate = atoi(s);
    return jdate;
}

/*
 *  Title:
 *     secs
 *  Synopsis:
 *     Returns elapsed time since t0 in seconds.
 *  Input Arguments:
 *     t0 - timeval structure
 *  Return:
 *     number of seconds.
 */
double secs(struct timeval *t0)
{
    struct timeval t1;
    gettimeofday(&t1, NULL);
    return t1.tv_sec - t0->tv_sec + (double)(t1.tv_usec - t0->tv_usec)/1000000.;
}

/*
 *  Title:
 *     ReadHumanTime
 *  Synopsis:
 *     Converts time from database format string to seconds since epoch.
 *  Input Arguments:
 *     date/time as a string 'yyyy-mm-dd_hh:mi:ss.sss'
 *  Return:
 *     time - time since epoch as a double or 0 on error.
 *  Called by:
 *     ReadInstructionFile
 *  Calls:
 *     HumanToEpoch
 */
double ReadHumanTime(char *timestr)
{
    int yyyy = 0, mm = 0, dd = 0, hh = 0, mi = 0, ss = 0, msec = 0, n = 0;
    char cutstr[24], q[2];
    char *remnant;

    strcpy(cutstr, timestr);

    cutstr[4] = '\0';
    yyyy = (int)strtod(&cutstr[0], &remnant);
    if (remnant[0]) return 0;

    cutstr[7] = '\0';
    mm = (int)strtod(&cutstr[5], &remnant);
    if (remnant[0]) return 0;

    cutstr[10] = '\0';
    dd = (int)strtod(&cutstr[8], &remnant);
    if (remnant[0]) return 0;

    cutstr[13] = '\0';
    hh = (int)strtod(&cutstr[11], &remnant);
    if (remnant[0]) return 0;

    cutstr[16] = '\0';
    mi = (int)strtod(&cutstr[14], &remnant);
    if (remnant[0]) return 0;

    cutstr[19] = '\0';
    ss = (int)strtod(&cutstr[17], &remnant);
    if (remnant[0]) return 0;
    n = strlen(timestr);
    if (n > 20) {
        cutstr[23] = '\0';
        q[0] = cutstr[22]; q[1] = '\0';
        if (streq(q, "") || streq(q, " ")) cutstr[22] = '0';
        q[0] = cutstr[21]; q[1] = '\0';
        if (streq(q, "") || streq(q, " ")) cutstr[21] = '0';
        q[0] = cutstr[20]; q[1] = '\0';
        if (streq(q, "") || streq(q, " ")) cutstr[20] = '0';
        msec = (int)strtod(&cutstr[20], &remnant);
        if (remnant[0]) return 0;
    }
    return HumanToEpochISF(yyyy, mm, dd, hh, mi, ss, msec);
}

