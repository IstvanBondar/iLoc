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
#ifdef SC3DB
#ifdef MYSQLDB

#include "iLoc.h"
extern int verbose;                                         /* verbose level */
extern FILE *logfp;
extern FILE *errfp;
extern int errorcode;
extern struct timeval t0;
extern char InputDB[VALLEN];       /* read data from this DB account, if any */
extern char OutputDB[VALLEN];    /* write results to this DB account, if any */
extern char OutAgency[VALLEN];      /* author for new hypocentres and assocs */
extern char NextidDB[VALLEN];               /* get new ids from this account */
extern int UseRSTT;                                 /* use RSTT predictions? */
extern MYSQL *mysql;
extern int MinNetmagSta;                 /* min number of stamags for netmag */
/*
 * Functions:
 *    WriteEventToSC3MysqlDatabase
 *    UpdateMagnitudesInSC3MysqlDatabase
 */

/*
 * Local functions
 */
static int WriteSC3Origin(EVREC *ep, SOLREC *sp, char *gregname, int GT5candidate);
static int WriteSC3Netmag(SOLREC *sp);
static int WriteSC3Stamag(SOLREC *sp, PHAREC p[], STAMAG **stamag, STAMAG **rdmag);
static int WriteSC3Arrival(SOLREC *sp, PHAREC p[]);
static int WriteSC3PublicObject(int oid, char *publicid);
static int GetSC3NextID(int *nextid);

/*
 *  Title:
 *     WriteEventToSC3MysqlDatabase
 *  Synopsis:
 *     Writes solution to SeisComp3 MySQL database.
 *     Populates/updates Event table.
 *     Populates Origin, OriginReference, Arrival, Magnitude,
 *               StationMagnitude, StationMagnitudeContribution and
 *               PublicObject tables.
 *     Somewhat overloads SeisComp3/QuakeML schema logic by storing
 *         ampmags, readingmags, as well as stamags in the StationMagnitude
 *         table.
 *     The StationMagnitudeContribution and Magnitude tables are populated
 *         only if network magnitude(s) exist.
 *  Input Arguments:
 *     ep        - pointer to event info
 *     sp        - pointer to current solution
 *     p[]       - array of phase structures
 *     GT5candidate    - GT candidate? [0/1]
 *     stamag - array of station magnitude structures
 *     rdmag  - array of reading magnitude structures
 *     gregname  - Flinn_Engdahl region name
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     Locator
 *  Calls:
 *     WriteSC3Origin,  WriteSC3Arrival, WriteSC3Netmag, WriteSC3Stamag
 */
int WriteEventToSC3MysqlDatabase(EVREC *ep, SOLREC *sp, PHAREC p[], int GT5candidate,
                 STAMAG **stamag, STAMAG **rdmag, char *gregname)
{
   if (verbose)
        fprintf(logfp, "        WriteEventToSC3MysqlDatabase: EventID=%s\n", ep->EventID);
/*
 *  populate Event, EventDescription, Origin and OriginReference tables
 */
    if (verbose) fprintf(logfp, "        WriteSC3Origin\n");
    if (WriteSC3Origin(ep, sp, gregname, GT5candidate))
        return 1;
/*
 *  populate Arrival table
 */
    if (verbose) fprintf(logfp, "        WriteSC3Arrival\n");
    WriteSC3Arrival(sp, p);
/*
 *  populate Magnitude, StationMagnitude and StationMagnitudeContribution
 */
    if (sp->nnetmag) {
        if (verbose)
            fprintf(logfp, "        WriteSC3Netmag\n");
        if (WriteSC3Netmag(sp))
            return 1;
    }
/*
 *  populate StationMagnitude and StationMagnitudeContribution
 */
    if (sp->nstamag) {
        if (verbose)
            fprintf(logfp, "        WriteSC3Stamag\n");
        if (WriteSC3Stamag(sp, p, stamag, rdmag))
            return 1;
    }
    if (verbose)
        fprintf(logfp, "        WriteEventToSC3MysqlDatabase (%.2f)\n", secs(&t0));
    return 0;
}

/*
 *  Title:
 *     WriteSC3Origin
 *  Synopsis:
 *     Populates Event, EventDescription, Origin and OriginReference tables.
 *     Sets depfix flag:
 *         null: Free-depth solution
 *            A: Depth fixed by ISC Analyst
 *            S: Anthropogenic event; depth fixed to surface
 *            G: Depth fixed to ISC default depth grid
 *            R: Depth fixed to ISC default region depth
 *            M: Depth fixed to median depth of reported hypocentres
 *            B: Beyond depth limits; depth fixed to 0/600 km
 *            H: Depth fixed to depth of a reported hypocentre
 *            D: Depth fixed to depth-phase depth
 *  OriginID: Origin#@time/%Y%m%d%H%M%S.%f@.@id@
 *  Input Arguments:
 *     ep  - pointer to event info
 *     sp  - pointer to current solution
 *     gregname - Flinn_Engdahl region name
 *     GT5candidate - GT candidate?
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     WriteEventToSC3MysqlDatabase
 *  Calls:
 *     PrintMysqlError, DropSpace, GetSC3NextID
 */
static int WriteSC3Origin(EVREC *ep, SOLREC *sp, char *gregname, int GT5candidate)
{
    MYSQL_RES *res_set = (MYSQL_RES *)NULL;
    MYSQL_ROW row;
    char sql[4096], psql[8192];
    char s[100], ms[10], depfix[2], magnitudeid[255], gname[255];
    int i, j, k, n, nextid = 0, isgreg = 0;
/*
 *  get a new id for Origin
 */
    if (GetSC3NextID(&nextid))
        return 1;
    if (verbose > 2) fprintf(logfp, "            new _oid=%d\n", nextid);
    sp->hypid = ep->ISChypid = ep->PreferredOrid = nextid;
/*
 *  generate preferredOriginID and preferredMagnitudeId
 */
    EpochToHumanSC3(s, ms, sp->time);
    sprintf(sp->OriginID, "Origin#%s.%s.%d", s, ms, ep->PreferredOrid);
    strcpy(magnitudeid, "");
    if (strcmp(ep->prefmagid, "X"))
        strcpy(magnitudeid, ep->prefmagid);
    for (i = 0; i < MAXMAG; i++) {
        if (sp->mag[i].numDefiningMagnitude < MinNetmagSta) continue;
        if (sp->mag[i].mtypeid == 4)
            sprintf(magnitudeid, "%s#netmag.%s", sp->OriginID, sp->mag[i].magtype);
        if (sp->mag[i].mtypeid == 1)
            sprintf(magnitudeid, "%s#netmag.%s", sp->OriginID, sp->mag[i].magtype);
        if (sp->mag[i].mtypeid == 3)
            sprintf(magnitudeid, "%s#netmag.%s", sp->OriginID, sp->mag[i].magtype);
        if (sp->mag[i].mtypeid == 2)
            sprintf(magnitudeid, "%s#netmag.%s", sp->OriginID, sp->mag[i].magtype);
    }
/*
 *  populate PublicObject table
 */
    if (WriteSC3PublicObject(ep->PreferredOrid, sp->OriginID))
        return 1;
    if (verbose) fprintf(logfp, "        WriteSC3Origin\n");
    if (strcmp(InputDB, OutputDB) && !ep->OutputDBPreferredOrid) {
/*
 *      populate Event table in OutputDB if necessary
 */
        sprintf(psql, "insert into %sEvent                                \
                              (_oid, _parent_oid, _last_modified, type,   \
                              preferredOriginID, preferredMagnitudeID,    \
                              creationInfo_agencyID, creationInfo_author, \
                              creationInfo_creationTime,                  \
                              creationInfo_modificationTime,              \
                              creationInfo_used)                          \
                       values (%d, %d, now(), '%s', '%s', '%s', '%s',     \
                              'iLoc', now(),  now(), 1)",
                OutputDB, ep->evid, 1, ep->etype, sp->OriginID, magnitudeid,
                OutAgency);
        DropSpace(psql, sql);
        if (verbose > 2) fprintf(logfp, "            WriteSC3Origin: %s\n", sql);
        if (mysql_query(mysql, sql)) {
            PrintMysqlError("WriteSC3Origin: error: Event");
            return 1;
        }
    }
    else {
/*
 *      update Event table
 */
        sprintf(psql, "update %sEvent set                           \
                              _last_modified = now(), type='%s',    \
                              preferredOriginID = '%s',             \
                              preferredMagnitudeID = '%s',          \
                              creationInfo_author = 'iLoc',         \
                              creationInfo_modificationTime = now() \
                        where _oid = %d",
                OutputDB, ep->etype, sp->OriginID, magnitudeid, ep->evid);
        DropSpace(psql, sql);
        if (verbose > 2) fprintf(logfp, "            WriteSC3Origin: %s\n", sql);
        if (mysql_query(mysql, sql)) {
            PrintMysqlError("WriteSC3Origin: error: Event");
            return 1;
        }
    }
/*
 *  update or insert EventDescription table
 *      fix for apostrophes in gregname (e.g. Cote d'Ivoire)
 */
    if (strstr(gregname, "'")) {
        n = (int)strlen(gregname);
        for (k = 0, j = 0; j < n; j++) {
            if (gregname[j] == '\'')
                gname[k++] = '\'';
            gname[k++] = gregname[j];
        }
        gname[k] = '\0';
    }
    else
        strcpy(gname, gregname);
    sprintf(psql, "select count(*) from %sEventDescription \
                    where type = 'region name' and _parent_oid = %d",
            OutputDB, ep->evid);
    DropSpace(psql, sql);
    if (mysql_query(mysql, sql))
        PrintMysqlError("WriteSC3Origin: error: EventDescription");
    else {
        res_set = mysql_use_result(mysql);
        row = mysql_fetch_row(res_set);
        isgreg = atoi(row[0]);
    }
    mysql_free_result(res_set);
    if (isgreg) {
        sprintf(psql, "update %sEventDescription                  \
                          set _last_modified = now(), text = '%s' \
                        where _parent_oid = %d                    \
                          and type = 'region name'",
                OutputDB, gname, ep->evid);
        DropSpace(psql, sql);
        if (verbose > 2) fprintf(logfp, "            WriteSC3Origin: %s\n", sql);
        if (mysql_query(mysql, sql)) {
            PrintMysqlError("WriteSC3Origin: error: EventDescription");
            return 1;
        }
    }
    else {
        if (GetSC3NextID(&nextid))
            return 1;
        sprintf(psql, "insert into %sEventDescription                         \
                              (_oid, _parent_oid, _last_modified, type, text) \
                       values (%d, %d, now(), 'region name', '%s')",
                OutputDB, nextid, ep->evid, gname);
        DropSpace(psql, sql);
        if (verbose > 2) fprintf(logfp, "            WriteSC3Origin: %s\n", sql);
        if (mysql_query(mysql, sql)) {
            PrintMysqlError("WriteSC3Origin: error: EventDescription");
            return 1;
        }
    }
/*
 *  populate OriginReference table
 */
    if (GetSC3NextID(&nextid))
        return 1;
    sprintf(psql, "insert into %sOriginReference                        \
                          (_oid, _parent_oid, _last_modified, originID) \
                   values (%d, %d, now(), '%s')",
                   OutputDB, nextid, ep->evid, sp->OriginID);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            WriteSC3Origin: %s\n", sql);
    if (mysql_query(mysql, sql)) {
        PrintMysqlError("WriteSC3Origin: error: OriginReference");
        return 1;
    }
    if      (sp->FixedDepthType == 8) strcpy(depfix, "A"); /* analyst */
    else if (sp->FixedDepthType == 1) strcpy(depfix, "B"); /* beyond limit */
    else if (sp->FixedDepthType == 2) strcpy(depfix, "H"); /* hypocentre */
    else if (sp->FixedDepthType == 3) strcpy(depfix, "D"); /* depdp */
    else if (sp->FixedDepthType == 4) strcpy(depfix, "S"); /* surface */
    else if (sp->FixedDepthType == 5) strcpy(depfix, "G"); /* depth grid */
    else if (sp->FixedDepthType == 6) strcpy(depfix, "M"); /* median depth */
    else if (sp->FixedDepthType == 7) strcpy(depfix, "R"); /* GRN depth */
    else                              strcpy(depfix, "F"); /* free depth */
/*
 *  populate Origin table
 */
    sprintf(psql, "insert into %sOrigin                                     \
                          (_oid, _parent_oid, _last_modified,               \
                          time_value, time_value_ms, time_uncertainty,      \
                          latitude_value, longitude_value,                  \
                          depth_value, depth_uncertainty,                   \
                          depth_used, depthType,                            \
                          timeFixed, epicenterFixed,                        \
                          methodID, earthModelID,                           \
                          quality_associatedPhaseCount,                     \
                          quality_usedPhaseCount,                           \
                          quality_associatedStationCount,                   \
                          quality_usedStationCount,                         \
                          quality_depthPhaseCount,                          \
                          quality_standardError,                            \
                          quality_azimuthalGap,                             \
                          quality_secondaryAzimuthalGap,                    \
                          quality_groundTruthLevel,                         \
                          quality_maximumDistance, quality_minimumDistance, \
                          quality_used,                                     \
                          uncertainty_maxHorizontalUncertainty,             \
                          uncertainty_minHorizontalUncertainty,             \
                          uncertainty_azimuthMaxHorizontalUncertainty,      \
                          uncertainty_confidenceEllipsoid_used,             \
                          uncertainty_preferredDescription,                 \
                          uncertainty_used, evaluationMode,                 \
                          creationInfo_agencyID, creationInfo_author,       \
                          creationInfo_creationTime,                        \
                          creationInfo_modificationTime,                    \
                          creationInfo_used)                                \
                   values (%d, '1', now(), '%s', '%s', %f, %f, %f, %f, ",
            OutputDB, ep->PreferredOrid, s, ms, sp->error[0], sp->lat, sp->lon,
            sp->depth);
    if (sp->error[3] == NULLVAL)     sprintf(psql, "%s NULL, 0, ", psql);
    else if (sp->error[3] > 9999.99) sprintf(psql, "%s %f, 1, ", psql, 9999.999);
    else                        sprintf(psql, "%s %f, 1, ", psql, sp->error[3]);
    if (sp->timfix) sprintf(psql, "%s '%s', %d, ", psql, depfix, sp->timfix);
    else            sprintf(psql, "%s '%s', NULL, ", psql, depfix);
    if (sp->epifix) sprintf(psql, "%s %d, ", psql, sp->epifix);
    else            sprintf(psql, "%s NULL, ", psql);
    sprintf(psql, "%s 'iLoc', ", psql);
    if (UseRSTT) sprintf(psql, "%s 'RSTT', ", psql);
    else         sprintf(psql, "%s NULL, ", psql);
    sprintf(psql, "%s %d, %d, %d, %d, %d, ",
            psql, sp->nass, sp->ndef, sp->nreading, sp->ndefsta, sp->ndp);
    if (sp->sdobs == NULLVAL)     sprintf(psql, "%s NULL, ", psql);
    else if (sp->sdobs > 9999.99) sprintf(psql, "%s %f, ", psql, 9999.999);
    else                          sprintf(psql, "%s %f, ", psql, sp->sdobs);
    sprintf(psql, "%s %f, %f, ", psql, sp->azimgap, sp->sgap);
    if (GT5candidate) sprintf(psql, "%s '5', ", psql);
    else              sprintf(psql, "%s NULL, ", psql);
    if (sp->mindist == NULLVAL) sprintf(psql, "%s NULL, NULL, 1, ", psql);
    else   sprintf(psql, "%s %f, %f, 1, ", psql, sp->maxdist, sp->mindist);
    if (sp->smajax == NULLVAL)
        sprintf(psql, "%s NULL, NULL, NULL, 0, ", psql);
    else {
        if (sp->sminax > 9999.99)
            sprintf(psql, "%s %f, %f, %f, 1, ",
                    psql, 9999.999, 9999.999, sp->strike);
        else if (sp->smajax > 9999.99)
            sprintf(psql, "%s %f, %f, %f, 1, ",
                    psql, 9999.999, sp->sminax, sp->strike);
        else
            sprintf(psql, "%s %f, %f, %f, 1, ",
                    psql, sp->smajax, sp->sminax, sp->strike);
    }
    sprintf(psql, "%s '90 percent confidence error ellipse', ", psql);
    sprintf(psql, "%s 1, 'manual', '%s', 'iLoc', now(), now(), 1)",
            psql, OutAgency);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            WriteSC3Origin: %s\n", sql);
    if (mysql_query(mysql, sql)) {
        PrintMysqlError("WriteSC3Origin: error: Origin");
        return 1;
    }
    if (verbose) fprintf(logfp, "    WriteSC3Origin: done\n");
    return 0;
}

/*
 *  Title:
 *     WriteSC3Netmag
 *  Synopsis:
 *     Populates Magnitude table with network magnitudes
 *  Input Arguments:
 *     sp - pointer to current solution
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     WriteEventToSC3MysqlDatabase
 *  Uses:
 *     PrintMysqlError, DropSpace, GetSC3NextID
 */
static int WriteSC3Netmag(SOLREC *sp)
{
    char sql[2048], psql[2048], magnitudeid[255];
    int i, nextid = 0;
    for (i = 0; i < MAXMAG; i++) {
        if (sp->mag[i].numDefiningMagnitude < MinNetmagSta) continue;
        if (GetSC3NextID(&nextid))
            return 1;
        sp->mag[i].magid = nextid;
/*
 *      populate PublicObject table
 */
        sprintf(magnitudeid, "%s#netmag.%s", sp->OriginID, sp->mag[i].magtype);
        if (WriteSC3PublicObject(sp->mag[i].magid, magnitudeid))
            return 1;
/*
 *      populate Magnitude table
 */
        sprintf(psql, "insert into %sMagnitude                            \
                              (_oid, _parent_oid, _last_modified,         \
                              magnitude_value, magnitude_uncertainty,     \
                              type, originID, methodID, stationCount,     \
                              creationInfo_agencyID, creationInfo_author, \
                              creationInfo_creationTime,                  \
                              creationInfo_modificationTime,              \
                              creationInfo_used)                          \
                       values (%d, %d, now(), %f, %f, '%s', '%s',         \
                              'trimmed median (20)', %d, '%s', 'iLoc',    \
                              now(), now(), 1)",
                OutputDB, sp->mag[i].magid, sp->hypid, sp->mag[i].magnitude,
                sp->mag[i].uncertainty, sp->mag[i].magtype, sp->OriginID,
                sp->mag[i].numDefiningMagnitude, OutAgency);
        DropSpace(psql, sql);
        if (verbose > 2) fprintf(logfp, "            put_sc3_mag: %s\n", sql);
        if (mysql_query(mysql, sql)) {
            PrintMysqlError("WriteSC3Origin: error: Magnitude");
            return 1;
        }
    }
    return 0;
}

/*
 *  Title:
 *     WriteSC3Stamag
 *  Synopsis:
 *     Populates StationMagnitude and StationMagnitudeContribution tables.
 *     Somewhat overloads SeisComp3/QuakeML schema logic by storing
 *         ampmags, readingmags, as well as stamags in the StationMagnitude
 *         table.
 *     The StationMagnitudeContribution and Magnitude tables are populated
 *         only if network magnitude(s) exist.
 *  Input Arguments:
 *     sp       - pointer to current solution
 *     p[]      - array of phase structures
 *     stamag   - array of stamag structures
 *     rdmag    - array of rdmag structures
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     WriteEventToSC3MysqlDatabase
 *  Calls:
 *     PrintMysqlError, DropSpace, GetSC3NextID
 */
static int WriteSC3Stamag(SOLREC *sp, PHAREC p[], STAMAG **stamag, STAMAG **rdmag)
{
    char sql[2048], psql[2048], magnitudeid[255];
    STAMAG *smag = (STAMAG *)NULL;
    int i, j, nextid = 0;
/*
 *  magnitudes from amplitude measurements (ampmag)
 */
    for (i = 0; i < sp->numPhase; i++) {
        for (j = 0; j < p[i].numamps; j++) {
/*
 *          get a new id for a magnitude from amplitude measurements
 */
            if (GetSC3NextID(&nextid))
                return 1;
/*
 *          populate PublicObject table
 */
            sprintf(magnitudeid, "%s#ampMag.%s",
                    p[i].a[j].amplitudeid, p[i].a[j].magtype);
            if (WriteSC3PublicObject(nextid, magnitudeid))
                return 1;
/*
 *          populate StationMagnitude table (ampmag)
 */
            sprintf(psql, "insert into %sStationMagnitude         \
                                  (_oid, _parent_oid,             \
                                  _last_modified,                 \
                                  magnitude_value, type,          \
                                  originID, amplitudeID, methodID, \
                                  waveformID_networkCode,         \
                                  waveformID_stationCode,         \
                                  waveformID_locationCode,        \
                                  waveformID_channelCode,         \
                                  waveformID_used,                \
                                  creationInfo_agencyID,          \
                                  creationInfo_author,            \
                                  creationInfo_creationTime,      \
                                  creationInfo_modificationTime,  \
                                  creationInfo_used)              \
                           values (%d, %d, now(), %f, '%s', '%s', \
                                  '%s', 'iLoc', '%s', '%s', '%s', \
                                  '%s', %d, '%s', 'iLoc', now(), now(), 1)",
                    OutputDB, nextid, sp->hypid, p[i].a[j].magnitude,
                    p[i].a[j].magtype, sp->OriginID, magnitudeid,
                    p[i].deploy, p[i].sta, p[i].lcn, p[i].a[j].ach,
                    p[i].a[j].ampdef, OutAgency);
            DropSpace(psql, sql);
            if (verbose > 2)
                fprintf(logfp, "            put_sc3_mag: %s\n", sql);
            if (mysql_query(mysql, sql)) {
                PrintMysqlError("put_sc3_mag: error: ampmag");
                return 1;
            }
        }
    }
/*
 *  reading magnitudes from ampmags
 */
    for (j = 0; j < MAXMAG; j++) {
        smag = rdmag[j];
        for (i = 0; i < sp->mag[j].numAssociatedMagnitude; i++) {
/*
 *          get a new id for a reading magnitude
 */
            if (GetSC3NextID(&nextid))
                return 1;
/*
 *          populate PublicObject table
 */
            sprintf(magnitudeid, "%s#rdMag.%s#%s.%s",
                sp->OriginID, smag[i].magtype, smag[i].deploy, smag[i].sta);
            if (WriteSC3PublicObject(nextid, magnitudeid))
                return 1;
/*
 *          populate StationMagnitude table (readingmag)
 */
            sprintf(psql, "insert into %sStationMagnitude            \
                                  (_oid, _parent_oid,                \
                                  _last_modified,                    \
                                  magnitude_value, type,             \
                                  originID, amplitudeID, methodID,   \
                                  waveformID_networkCode,            \
                                  waveformID_stationCode,            \
                                  waveformID_locationCode,           \
                                  waveformID_used,                   \
                                  creationInfo_agencyID,             \
                                  creationInfo_author,               \
                                  creationInfo_creationTime,         \
                                  creationInfo_modificationTime,     \
                                  creationInfo_used)                 \
                           values (%d, %d, now(), %f, '%s', '%s',    \
                                  '%s', 'iLoc', '%s', '%s', '%s', 0, \
                                  '%s', 'iLoc', now(), now(), 1)",
                    OutputDB, nextid, sp->hypid, smag[i].magnitude,
                    smag[i].magtype, sp->OriginID, magnitudeid,
                    smag[i].deploy, smag[i].sta, smag[i].lcn,
                    OutAgency);
            DropSpace(psql, sql);
            if (verbose > 2)
                fprintf(logfp, "            put_sc3_mag: %s\n", sql);
            if (mysql_query(mysql, sql)) {
                PrintMysqlError("put_sc3_mag: error: readingmag");
                return 1;
            }
        }
    }
/*
 *  station magnitudes from reading magnitudes
 */
    for (j = 0; j < MAXMAG; j++) {
        smag = stamag[j];
        for (i = 0; i < sp->mag[j].numDefiningMagnitude; i++) {
/*
 *          get a new id for a station magnitude
 */
            if (GetSC3NextID(&nextid))
                return 1;
/*
 *          populate PublicObject table
 */
            sprintf(magnitudeid, "%s#staMag.%s#%s.%s",
                sp->OriginID, smag[i].magtype, smag[i].deploy, smag[i].sta);
            if (WriteSC3PublicObject(nextid, magnitudeid))
                return 1;
/*
 *          populate StationMagnitude table (stamag)
 */
            sprintf(psql, "insert into %sStationMagnitude            \
                                  (_oid, _parent_oid,                \
                                  _last_modified,                    \
                                  magnitude_value, type,             \
                                  originID, amplitudeID, methodID,   \
                                  waveformID_networkCode,            \
                                  waveformID_stationCode,            \
                                  waveformID_locationCode,           \
                                  waveformID_used,                   \
                                  creationInfo_agencyID,             \
                                  creationInfo_author,               \
                                  creationInfo_creationTime,         \
                                  creationInfo_modificationTime,     \
                                  creationInfo_used)                 \
                           values (%d, %d, now(), %f, '%s', '%s',    \
                                  '%s', 'iLoc', '%s', '%s', '%s', 0, \
                                  '%s', 'iLoc', now(), now(), 1)",
                    OutputDB, nextid, sp->hypid, smag[i].magnitude,
                    smag[i].magtype, sp->OriginID, magnitudeid,
                    smag[i].deploy, smag[i].sta, smag[i].lcn,
                    OutAgency);
            DropSpace(psql, sql);
            if (verbose > 2)
                fprintf(logfp, "            put_sc3_mag: %s\n", sql);
            if (mysql_query(mysql, sql)) {
                PrintMysqlError("put_sc3_mag: error: stamag");
                return 1;
            }
/*
 *          if no network magnitude was computed, skip stamag contributions
 */
            if (sp->mag[j].magid == NULLVAL || sp->mag[j].magid == 0)
                continue;
/*
 *          populate StationMagnitudeContribution table
 */
            if (GetSC3NextID(&nextid))
                return 1;
            sprintf(psql, "insert into %sStationMagnitudeContribution \
                                  (_oid, _parent_oid, _last_modified, \
                                  stationMagnitudeID,  weight)        \
                           values (%d, %d, now(), '%s', %d)",
                    OutputDB, nextid, sp->mag[j].magid, magnitudeid, smag[i].magdef);
            DropSpace(psql, sql);
            if (verbose > 2)
                fprintf(logfp, "            put_sc3_mag: %s\n", sql);
            if (mysql_query(mysql, sql)) {
                PrintMysqlError("put_sc3_mag: error: stamag");
                return 1;
            }
        }
    }
    return 0;
}

/*
 *  Title:
 *     WriteSC3Arrival
 *  Synopsis:
 *     Populates Arrival table
 *  Input Arguments:
 *     sp  - pointer to current solution
 *     p[] - array of phase structures
 *  Called by:
 *     WriteEventToSC3MysqlDatabase
 *  Calls:
 *     PrintMysqlError, DropSpace, GetSC3NextID
 */
static int WriteSC3Arrival(SOLREC *sp, PHAREC p[])
{
    char sql[2048], psql[2048];
    int i, j, k, n;
    char phase[11];
    double timeres = 0., weight = 0.;
    int nextid = 0, timeres_ind;
/*
 *
 *  populate Arrival table
 *
 */
    for (i = 0; i < sp->numPhase; i++) {
        timeres_ind = 1;
        if      (p[i].timeres == NULLVAL)   timeres_ind = 0;
        else if (isnan(p[i].timeres))       timeres_ind = 0;
        else if (p[i].timeres >= 1000000.)  timeres = 999999.;
        else if (p[i].timeres <= -1000000.) timeres = -999999.;
        else                                timeres = p[i].timeres;
/*
 *      fix for apostrophes in phasenames (e.g. P'P'df)
 */
        if (strstr(p[i].phase, "'")) {
            n = (int)strlen(p[i].phase);
            for (k = 0, j = 0; j < n; j++) {
                if (p[i].phase[j] == '\'')
                    phase[k++] = '\'';
                phase[k++] = p[i].phase[j];
            }
            phase[k] = '\0';
        }
        else
            strcpy(phase, p[i].phase);
/*
 *      get a new id for Arrival
 */
        if (GetSC3NextID(&nextid))
            return 1;
/*
 *      insert Arrival record
 */
        sprintf(psql, "insert into %sArrival                              \
                              (_oid, _parent_oid, _last_modified, pickID, \
                              phase_code, azimuth, distance,              \
                              timeResidual, timeUsed, weight,             \
                              creationInfo_agencyID, creationInfo_author, \
                              creationInfo_creationTime,                  \
                              creationInfo_modificationTime,              \
                              creationInfo_used)                          \
                       values (%d, %d, now(), '%s', ",
                OutputDB, nextid, sp->hypid, p[i].pickid);
        if (phase[0])        sprintf(psql, "%s '%s', ", psql, phase);
        else                 sprintf(psql, "%s 'x', ", psql);
        sprintf(psql, "%s %f, %f, ", psql, p[i].esaz, p[i].delta);
        if (timeres_ind)     sprintf(psql, "%s %f, ", psql, timeres);
        else                 sprintf(psql, "%s NULL, ", psql);
        if (p[i].timedef) {
            if (p[i].deltim == NULLVAL)
                sprintf(psql, "%s 1, NULL, ", psql);
            else {
                weight = 1. / p[i].deltim;
                sprintf(psql, "%s 1, %f, ", psql, weight);
            }
        }
        else sprintf(psql, "%s 0, NULL,", psql);
        sprintf(psql, "%s '%s', 'iLoc', now(), now(), 1)", psql, OutAgency);
        DropSpace(psql, sql);
        if (verbose > 2)
            fprintf(logfp, "            WriteSC3Arrival: %s\n", sql);
        if (mysql_query(mysql, sql)) {
            PrintMysqlError("WriteSC3Arrival: error: Arrival");
        }
    }
    if (verbose) fprintf(logfp, "    WriteSC3Arrival: done\n");
    return 0;
}

/*
 *  Title:
 *     UpdateMagnitudesInSC3MysqlDatabase
 *  Synopsis:
 *     Writes magnitudes to SeisComp3 MySQL database.
 *     Populates Magnitude StationMagnitude, StationMagnitudeContribution and
 *               PublicObject tables.
 *     Somewhat overloads SeisComp3/QuakeML schema logic by storing
 *         ampmags, readingmags, as well as stamags in the StationMagnitude
 *         table.
 *     The StationMagnitudeContribution and Magnitude tables are populated
 *         only if network magnitude(s) exist. In that case the Event table
 *         is also updated.
 *  Input Arguments:
 *     ep        - pointer to event info
 *     sp        - pointer to current solution
 *     p[]       - array of phase structures
 *     stamag - array of station magnitude structures
 *     rdmag  - array of reading magnitude structures
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     Locator
 *  Calls:
 *     WriteSC3Netmag, WriteSC3Stamag
 */
int UpdateMagnitudesInSC3MysqlDatabase(EVREC *ep, SOLREC *sp, PHAREC p[],
                                       STAMAG **stamag, STAMAG **rdmag)
{
    char sql[2048], psql[2048], magnitudeid[VALLEN];
    int i, j, nextid = 0, magids[MAXMAG];
    MYSQL_RES *res_set = (MYSQL_RES *)NULL;
    MYSQL_ROW row;
    if (verbose)
        fprintf(logfp, "        UpdateMagnitudesInSC3MysqlDatabase");
/*
 *
 *  delete previous magnitudes
 *
 */
    sprintf(psql, "select _oid from %sMagnitude where _parent_oid = %d",
                   OutputDB, sp->hypid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            UpdateMagnitudes: %s\n", sql);
    if (mysql_query(mysql, sql))
        PrintMysqlError("UpdateMagnitudes: error: Magnitude");
    else {
        res_set = mysql_use_result(mysql);
        i = 0;
        while ((row = mysql_fetch_row(res_set))) {
            magids[i] = atoi(row[0]);
            i++;
        }
    }
    mysql_free_result(res_set);
/*
 *  delete StationMagnitudeContribution entries
 */
    for (j = 0; j < i; j++) {
        sprintf(psql, "delete from %sStationMagnitudeContribution \
                        where _parent_oid = %d",
                OutputDB, magids[j]);
        DropSpace(psql, sql);
        if (verbose > 2) fprintf(logfp, "            UpdateMagnitudes: %s\n", sql);
        if (mysql_query(mysql, sql)) {
            PrintMysqlError("UpdateMagnitudes: error: StationMagnitudeContribution");
            return 1;
        }
    }
/*
 *  delete StationMagnitude entries
 */
    sprintf(psql, "delete from %sStationMagnitude where _parent_oid = %d",
            OutputDB, sp->hypid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            UpdateMagnitudes: %s\n", sql);
    if (mysql_query(mysql, sql)) {
        PrintMysqlError("UpdateMagnitudes: error: StationMagnitude");
        return 1;
    }
/*
 *  delete Magnitude entries
 */
    sprintf(psql, "delete from %sMagnitude where _parent_oid = %d",
            OutputDB, sp->hypid);
    DropSpace(psql, sql);
    if (verbose > 2) fprintf(logfp, "            UpdateMagnitudes: %s\n", sql);
    if (mysql_query(mysql, sql)) {
        PrintMysqlError("UpdateMagnitudes: error: Magnitude");
        return 1;
    }
/*
 *
 *  insert magnitudes
 *
 */
    if (sp->nnetmag) {
/*
 *      populate Magnitude table
 */
        if (verbose)
            fprintf(logfp, "        WriteSC3Netmag\n");
        if (WriteSC3Netmag(sp))
            return 1;
        for (i = 0; i < MAXMAG; i++) {
            if (sp->mag[i].numDefiningMagnitude < MinNetmagSta) continue;
            if (sp->mag[i].mtypeid == 4)
                sprintf(magnitudeid, "%s#netmag.%s", sp->OriginID, sp->mag[i].magtype);
            if (sp->mag[i].mtypeid == 1)
                sprintf(magnitudeid, "%s#netmag.%s", sp->OriginID, sp->mag[i].magtype);
            if (sp->mag[i].mtypeid == 3)
                sprintf(magnitudeid, "%s#netmag.%s", sp->OriginID, sp->mag[i].magtype);
            if (sp->mag[i].mtypeid == 2)
                sprintf(magnitudeid, "%s#netmag.%s", sp->OriginID, sp->mag[i].magtype);
        }
/*
 *      update Event table
 */
        sprintf(psql, "update %sEvent set _last_modified = now(),   \
                              preferredMagnitudeID = '%s',          \
                              creationInfo_author = 'iLoc',         \
                              creationInfo_modificationTime = now() \
                        where _oid = %d",
                OutputDB, magnitudeid, ep->evid);
        DropSpace(psql, sql);
        if (verbose > 2) fprintf(logfp, "            UpdateMagnitudes: %s\n", sql);
        if (mysql_query(mysql, sql)) {
            PrintMysqlError("UpdateMagnitudes: error: Event");
            return 1;
        }
    }
/*
 *  populate StationMagnitude and StationMagnitudeContribution tables
 */
    if (sp->nstamag) {
        if (verbose)
            fprintf(logfp, "        WriteSC3Stamag\n");
        if (WriteSC3Stamag(sp, p, stamag, rdmag))
            return 1;
    }
    return 0;
}

/*
 *  Title:
 *      WriteSC3PublicObject
 *  Synopsis:
 *      populates PublicObject table
 *  Input Arguments:
 *      oid      - id
 *      publicid - public id string
 *  Return:
 *      0/1 on success/error
 *  Called by:
 *      WriteSC3Origin, WriteSC3Netmag, WriteSC3Stamag, WriteSC3Arrival
 *  Calls:
 *      PrintMysqlError
 */
static int WriteSC3PublicObject(int oid, char *publicid)
{
    char sql[MAXBUF];
    sprintf(sql, "replace into %sPublicObject (_oid, publicID) \
                  values (%d, '%s')",
            NextidDB, oid, publicid);
    if (verbose > 2) fprintf(logfp, "            WriteSC3PublicObject: %s\n", sql);
    if (mysql_query(mysql, sql)) {
        PrintMysqlError("WriteSC3PublicObject: ");
        return 1;
    }
    return 0;
}

/*
 *  Title:
 *      GetSC3NextID
 *  Synopsis:
 *      Gets next unique id from the Object table.
 *  Output Arguments:
 *      nextid - unique id
 *  Return:
 *      0/1 on success/error
 *  Called by:
 *      WriteSC3Origin, WriteSC3Netmag, WriteSC3Stamag, WriteSC3Arrival
 *  Calls:
 *      PrintMysqlError
 *
 */
static int GetSC3NextID(int *nextid)
{
    MYSQL_RES *res_set = (MYSQL_RES *)NULL;
    char sql[MAXBUF];
    sprintf(sql, "insert into %sObject (_timestamp) values (now())", NextidDB);
    if (verbose > 2) fprintf(logfp, "            GetSC3NextID: %s\n", sql);
    if (mysql_query(mysql, sql)) {
        PrintMysqlError("GetSC3NextID: error: nextid");
        return 1;
    }
    else {
        res_set = mysql_store_result(mysql);
        *nextid = mysql_insert_id(mysql);
        mysql_free_result(res_set);
    }
    if (verbose > 2) fprintf(logfp, "            new id=%d\n", *nextid);
    return 0;
}

#endif
#endif /* SC3DB */

/*  EOF  */
