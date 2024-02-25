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
#define MAXKMLPOLYPOINTS 361
extern double KMLLatitude;                /* KML bulletin viewpoint latitude */
extern double KMLLongitude;              /* KML bulletin viewpoint longitude */
extern double KMLRange;                  /* KML bulletin viewpoint elevation */
extern double KMLHeading;                  /* KML bulletin viewpoint azimuth */
extern double KMLTilt;                        /* KML bulletin viewpoint tilt */
extern int MinNetmagSta;                 /* min number of stamags for netmag */
/*
 * Functions:
 *    WriteKML
 *    WriteKMLVantagePoint
 *    WriteKMLHeader
 *    WriteKMLFooter
 *    WriteKMLStyleMap
 */

/*
 * Local functions
 *    WriteKMLFolderHeader
 *    WriteKMLFolderFooter
 *    WriteKMLStations
 *    WriteKMLEpicenters
 *    WriteKMLSolution
 *    WriteKMLErrorEllipse
 */
static void WriteKMLFolderHeader(FILE *kml, char *title, char *Description);
static void WriteKMLFolderFooter(FILE *kml);
static void WriteKMLStations(FILE *kml, int numSta, int numPhase, PHAREC p[],
        double evlat, double evlon);
static void WriteKMLRayPaths(FILE *kml, int numPhase, PHAREC p[],
        double evlat, double evlon);
static void WriteKMLEpicenters(FILE *kml, int numHypo, HYPREC h[]);
static void WriteKMLSolution(FILE *kml, SOLREC *sp, char *Eventid);
static void WriteKMLErrorEllipse(FILE *kml, char *Styleid, char *auth,
        double evlat, double evlon, double smajax, double sminax, double strike);
static void WriteKMLLine(FILE *kml, char *Styleid,
        double evlat, double evlon, double slat, double slon);


void WriteKML(FILE *kml, char *Eventid, int numHypo, int numSta,
        SOLREC *sp, HYPREC h[], PHAREC p[], int isbull)
{
    char hday[25], htim[25];
    if (sp->converged) {
        if (isbull) {
            EpochToHumanISF(hday, htim, sp->time);
            WriteKMLFolderHeader(kml, hday, Eventid);
        }
        WriteKMLSolution(kml, sp, Eventid);
        WriteKMLEpicenters(kml, numHypo, h);
        WriteKMLStations(kml, numSta, sp->numPhase, p, sp->lat, sp->lon);
        WriteKMLRayPaths(kml, sp->numPhase, p, sp->lat, sp->lon);
        if (isbull) WriteKMLFolderFooter(kml);
    }
}

void WriteKMLHeader(FILE *kml)
{
    fprintf(kml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(kml, "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n");
    fprintf(kml, "<Document>\n");
}

void WriteKMLFooter(FILE *kml)
{
    fprintf(kml, "</Document>\n");
    fprintf(kml, "</kml>");
}

void WriteKMLStyleMap(FILE *kml)
{
/*
 *  solution
 */
    fprintf(kml, "<Style id=\"Solution\">\n");
    fprintf(kml, "  <IconStyle>\n");
    fprintf(kml, "    <color>ff0000ff</color>\n");
    fprintf(kml, "    <Icon id=\"sol\">\n");
    fprintf(kml, "      <href>http://maps.google.com/mapfiles/kml/shapes/placemark_circle.png</href>\n");
    fprintf(kml, "    </Icon>\n");
    fprintf(kml, "  </IconStyle>\n");
    fprintf(kml, "  <LabelStyle>\n");
    fprintf(kml, "    <scale>1</scale>\n");
    fprintf(kml, "  </LabelStyle>\n");
    fprintf(kml, "</Style>\n");
/*
 *  other hypocenters
 */
    fprintf(kml, "<Style id=\"Hypocenter\">\n");
    fprintf(kml, "  <IconStyle>\n");
    fprintf(kml, "    <color>ffffbf00</color>\n");
    fprintf(kml, "    <Icon id=\"hypo\">\n");
    fprintf(kml, "      <href>http://maps.google.com/mapfiles/kml/shapes/placemark_circle.png</href>\n");
    fprintf(kml, "    </Icon>\n");
    fprintf(kml, "  </IconStyle>\n");
    fprintf(kml, "  <LabelStyle>\n");
    fprintf(kml, "    <scale>1</scale>\n");
    fprintf(kml, "  </LabelStyle>\n");
    fprintf(kml, "</Style>\n");
/*
 *  Stations
 */
    fprintf(kml, "<Style id=\"Station\">\n");
    fprintf(kml, "  <IconStyle>\n");
    fprintf(kml, "    <color>ff00ffff</color>\n");
    fprintf(kml, "    <scale>0.75</scale>\n");
    fprintf(kml, "    <Icon id=\"sta\">\n");
    fprintf(kml, "      <href>http://maps.google.com/mapfiles/kml/shapes/triangle.png</href>\n");
    fprintf(kml, "    </Icon>\n");
    fprintf(kml, "  </IconStyle>\n");
    fprintf(kml, "  <LabelStyle>\n");
    fprintf(kml, "    <scale>1</scale>\n");
    fprintf(kml, "  </LabelStyle>\n");
    fprintf(kml, "</Style>\n");
/*
 *  Error ellipses
 */
    fprintf(kml, "<Style id=\"Ellipse\">\n");
    fprintf(kml, "  <LineStyle>\n");
    fprintf(kml, "    <width>1.5</width>\n");
    fprintf(kml, "    <color>ffffbf00</color>\n");
    fprintf(kml, "  </LineStyle>\n");
    fprintf(kml, "</Style>\n");
/*
 *  iLoc error ellipse
 */
    fprintf(kml, "<Style id=\"iLocEllipse\">\n");
    fprintf(kml, "  <LineStyle>\n");
    fprintf(kml, "    <width>3</width>\n");
    fprintf(kml, "    <color>ff0000ff</color>\n");
    fprintf(kml, "  </LineStyle>\n");
    fprintf(kml, "</Style>\n");
/*
 *  non-defining raypath
 */
    fprintf(kml, "<Style id=\"NonDefining\">\n");
    fprintf(kml, "  <LineStyle>\n");
    fprintf(kml, "    <width>1</width>\n");
    fprintf(kml, "    <color>f700ffff</color>\n");
    fprintf(kml, "  </LineStyle>\n");
    fprintf(kml, "</Style>\n");
/*
 *  defining ray path
 */
    fprintf(kml, "<Style id=\"Defining\">\n");
    fprintf(kml, "  <LineStyle>\n");
    fprintf(kml, "    <width>1</width>\n");
    fprintf(kml, "    <color>ff00ff00</color>\n");
    fprintf(kml, "  </LineStyle>\n");
    fprintf(kml, "</Style>\n");
}

void WriteKMLVantagePoint(FILE *kml, double lat, double lon, double elev,
        double heading, double tilt)
{
    fprintf(kml, "  <LookAt id=\"docroot\">\n");
    fprintf(kml, "    <heading>%f</heading>\n", heading);
    fprintf(kml, "    <tilt>%f</tilt>\n", tilt);
    fprintf(kml, "    <latitude>%f</latitude>\n", lat);
    fprintf(kml, "    <longitude>%f</longitude>\n", lon);
    fprintf(kml, "    <range>%f</range>\n", elev);
    fprintf(kml, "  </LookAt>\n");
}

static void WriteKMLFolderHeader(FILE *kml, char *title, char *Description)
{
    fprintf(kml, "<Folder id=\"ID\">\n");
    fprintf(kml, "<name>%s</name>\n", title);
    fprintf(kml, "<open>0</open>\n");
    fprintf(kml, "<description>\n");
    fprintf(kml, "  <![CDATA[%s]]>\n", Description);
    fprintf(kml, "</description>\n");
}

static void WriteKMLFolderFooter(FILE *kml)
{
    fprintf(kml, "</Folder>\n\n");
}

static void WriteKMLRayPaths(FILE *kml, int numPhase, PHAREC p[],
        double evlat, double evlon)
{
    char PrevSta[STALEN];
    double slat, slon;
    int i, isdef = 0;
    fprintf(kml, "<Folder id=\"ID\">\n");
    fprintf(kml, "<name>RayPaths</name>\n");
    fprintf(kml, "<open>0</open>\n");
    strcpy(PrevSta, "");
    for (i = 0; i < numPhase; i++) {
        if (strcmp(p[i].prista, PrevSta)) {
/*
 *          new station
 */
            if (i) {
/*
 *              draw ray path for previous station
 */
                if (isdef) {
                    WriteKMLLine(kml, "Defining", evlat, evlon, slat, slon);
                }
                else {
                    WriteKMLLine(kml, "NonDefining", evlat, evlon, slat, slon);
                }
                isdef = 0;
            }
            if (p[i].timedef || p[i].azimdef || p[i].slowdef) isdef = 1;
        }
        else {
/*
 *      still the same station
 */
            if (p[i].timedef || p[i].azimdef || p[i].slowdef) isdef = 1;
        }
        strcpy(PrevSta, p[i].prista);
        slat = p[i].StaLat;
        slon = p[i].StaLon;
    }
/*
 *  close Placemark for last station
 */
    if (isdef) {
        WriteKMLLine(kml, "Defining", evlat, evlon, slat, slon);
    }
    else {
        WriteKMLLine(kml, "NonDefining", evlat, evlon, slat, slon);
    }
    fprintf(kml, "</Folder>\n\n");
}

static void WriteKMLStations(FILE *kml, int numSta, int numPhase, PHAREC p[],
        double evlat, double evlon)
{
    char hday[25], htim[25], PrevSta[STALEN];
    double u, slat, slon;
    int i;
    fprintf(kml, "<Folder id=\"ID\">\n");
    fprintf(kml, "<name>Stations</name>\n");
    fprintf(kml, "<open>0</open>\n");
    strcpy(PrevSta, "");
    for (i = 0; i < numPhase; i++) {
        if (strcmp(p[i].prista, PrevSta)) {
/*
 *          new station
 */
            if (i) {
/*
 *              close Placemark for previous station
 */
                fprintf(kml, "    </table>]]>\n");
                fprintf(kml, "  </description>\n");
                fprintf(kml, "  <Style>\n");
                fprintf(kml, "    <BalloonStyle>\n");
                fprintf(kml, "      <bgColor>7fffffff</bgColor>\n");
                fprintf(kml, "      <textColor>ff000000</textColor>\n");
                fprintf(kml, "      <text>\n");
                fprintf(kml, "        <h3 align=\"center\">$[name]</h3>\n");
                fprintf(kml, "        $[description]\n");
                fprintf(kml, "      </text>\n");
                fprintf(kml, "      <displayMode>default</displayMode>\n");
                fprintf(kml, "    </BalloonStyle>\n");
                fprintf(kml, "  </Style>\n");
                fprintf(kml, "</Placemark>\n");
            }
/*
 *          new Placemark for station
 */
            fprintf(kml, "<Placemark>\n");
            fprintf(kml, "  <name>%s</name>\n", p[i].prista);
            fprintf(kml, "  <Snippet maxLines=\"1\">%s</Snippet>\n",
                    p[i].prista);
            fprintf(kml, "  <visibility>1</visibility>\n");
            fprintf(kml, "  <styleUrl>#Station</styleUrl>\n");
            WriteKMLVantagePoint(kml, p[i].StaLat, p[i].StaLon, 50000, 0, 0);
            fprintf(kml, "  <Point>\n");
            fprintf(kml, "    <coordinates>%f,%f,0</coordinates>\n",
                        p[i].StaLon, p[i].StaLat);
            fprintf(kml, "  </Point>\n");
            fprintf(kml, "  <description>\n");
            fprintf(kml, "    <![CDATA[<table width=\"600\">\n");
            fprintf(kml, "    <tr><td>Lat=%.3f</td><td>Lon=%.3f</td>",
                    p[i].StaLat, p[i].StaLon);
            fprintf(kml, "<td>Elev=%.1f</td>", p[i].StaElev);
            fprintf(kml, "<td>Delta=%6.2f</td><td>Esaz=%5.1f</td></tr>\n",
                    p[i].delta, p[i].esaz);
            fprintf(kml, "    <tr><td>Phase</td>");
            fprintf(kml, "<td>Arrival time</td><td>TimeRes</td>");
            fprintf(kml, "<td>Azimuth</td><td>AzimRes</td>");
            fprintf(kml, "<td>Slowness</td><td>SRes</td><td>TAS</td></tr>\n");
            EpochToHumanISF(hday, htim, p[i].time);
            fprintf(kml, "    <tr><td>%s</td><td>%s %s</td>",
                    p[i].phase, hday, htim);
            if (p[i].timeres == NULLVAL)
                fprintf(kml, "<td></td>");
            else {
                u = p[i].timeres;
                if (u >  9999) u =  9999;
                if (u < -9999) u = -9999;
                fprintf(kml, "<td>%6.2f</td>", u);
            }
            if (p[i].azim == NULLVAL)
                fprintf(kml, "<td></td><td></td>");
            else
                fprintf(kml, "<td>%5.1f</td><td>%5.1f</td>", p[i].azim, p[i].azimres);
            if (p[i].slow == NULLVAL)
                fprintf(kml, "<td></td><td></td>");
            else
                fprintf(kml, "<td>%6.2f</td><td>%6.2f</td>", p[i].slow, p[i].slowres);
            if (p[i].timedef) fprintf(kml, "<td>T");
            else              fprintf(kml, "<td>_");
            if (p[i].azimdef) fprintf(kml, "A");
            else              fprintf(kml, "_");
            if (p[i].slowdef) fprintf(kml, "S</td>\n");
            else              fprintf(kml, "_</td></tr>\n");
        }
/*
 *      still the same station
 */
        else {
            EpochToHumanISF(hday, htim, p[i].time);
            fprintf(kml, "    <tr><td>%s</td><td>%s %s</td>",
                    p[i].phase, hday, htim);
            if (p[i].timeres == NULLVAL)
                fprintf(kml, "<td></td>");
            else {
                u = p[i].timeres;
                if (u >  9999) u =  9999;
                if (u < -9999) u = -9999;
                fprintf(kml, "<td>%5.1f</td>", u);
            }
            if (p[i].azim == NULLVAL)
                fprintf(kml, "<td></td><td></td>");
            else
                fprintf(kml, "<td>%5.1f</td><td>%5.1f</td>", p[i].azim, p[i].azimres);
            if (p[i].slow == NULLVAL)
                fprintf(kml, "<td></td><td></td>");
            else
                fprintf(kml, "<td>%6.2f</td><td>%6.2f</td>", p[i].slow, p[i].slowres);
            if (p[i].timedef) fprintf(kml, "<td>T");
            else              fprintf(kml, "<td>_");
            if (p[i].azimdef) fprintf(kml, "A");
            else              fprintf(kml, "_");
            if (p[i].slowdef) fprintf(kml, "S</td>\n");
            else              fprintf(kml, "_</td></tr>\n");
        }
        strcpy(PrevSta, p[i].prista);
        slat = p[i].StaLat;
        slon = p[i].StaLon;
    }
/*
 *  close Placemark for last station
 */
    fprintf(kml, "    </table>]]>\n");
    fprintf(kml, "  </description>\n");
    fprintf(kml, "  <Style>\n");
    fprintf(kml, "    <BalloonStyle>\n");
    fprintf(kml, "      <bgColor>7fffffff</bgColor>\n");
    fprintf(kml, "      <textColor>ff000000</textColor>\n");
    fprintf(kml, "      <text>\n");
    fprintf(kml, "        <h3 align=\"center\">$[name]</h3>\n");
    fprintf(kml, "        $[description]\n");
    fprintf(kml, "      </text>\n");
    fprintf(kml, "      <displayMode>default</displayMode>\n");
    fprintf(kml, "    </BalloonStyle>\n");
    fprintf(kml, "  </Style>\n");
    fprintf(kml, "</Placemark>\n");
    fprintf(kml, "</Folder>\n\n");
}

static void WriteKMLEpicenters(FILE *kml, int numHypo, HYPREC h[])
{
    int i;
    char hday[25], htim[25];
    fprintf(kml, "<Folder id=\"ID\">\n");
    fprintf(kml, "<name>Origins</name>\n");
    fprintf(kml, "<open>0</open>\n");
    for (i = 0; i < numHypo; i++) {
        fprintf(kml, "<Placemark>\n");
        WriteKMLVantagePoint(kml, h[i].lat, h[i].lon, 50000, 0, 0);
        fprintf(kml, "  <name>%s</name>\n", h[i].agency);
        fprintf(kml, "  <Snippet maxLines=\"1\">%s</Snippet>\n", h[i].rep);
        fprintf(kml, "  <visibility>1</visibility>\n");
        fprintf(kml, "  <description>\n");
        EpochToHumanISF(hday, htim, h[i].time);
        fprintf(kml, "    <![CDATA[<b>%s</b> OT=%s %s ",
                h[i].origid, hday, htim);
        fprintf(kml, "Lat=%.3f Lon=%.3f Depth=%.1f<br>\n",
                h[i].lat, h[i].lon, h[i].depth);
        fprintf(kml, "    nass=%d ndef=%d sdobs=%.2f]]>\n",
                h[i].nass, h[i].ndef, h[i].sdobs);
        fprintf(kml, "  </description>\n");
        fprintf(kml, "  <Style>\n");
        fprintf(kml, "    <BalloonStyle>\n");
        fprintf(kml, "      <bgColor>7fffffff</bgColor>\n");
        fprintf(kml, "      <textColor>ff000000</textColor>\n");
        fprintf(kml, "      <text><table width=\"450\">\n");
        fprintf(kml, "        <th align=\"center\">$[name]</th>\n");
        fprintf(kml, "        <tr><td>$[description]</td></tr>\n");
        fprintf(kml, "      </table></text>\n");
        fprintf(kml, "      <displayMode>default</displayMode>\n");
        fprintf(kml, "    </BalloonStyle>\n");
        fprintf(kml, "  </Style>\n");
        fprintf(kml, "  <styleUrl>#Hypocenter</styleUrl>\n");
        fprintf(kml, "  <Point>\n");
        fprintf(kml, "    <coordinates>%.3f,%.3f,0</coordinates>\n",
                h[i].lon, h[i].lat);
        fprintf(kml, "  </Point>\n");
        fprintf(kml, "</Placemark>\n");
        WriteKMLErrorEllipse(kml, "Ellipse", h[i].agency, h[i].lat, h[i].lon,
                             h[i].smajax, h[i].sminax, h[i].strike);
    }
    fprintf(kml, "</Folder>\n\n");
}

static void WriteKMLSolution(FILE *kml, SOLREC *sp, char *Eventid)
{
    int i;
    char hday[25], htim[25];
    fprintf(kml, "<Placemark>\n");
    WriteKMLVantagePoint(kml, sp->lat, sp->lon, 50000, 0, 0);
    fprintf(kml, "  <name>iLoc</name>\n");
    fprintf(kml, "  <Snippet maxLines=\"1\">%s</Snippet>\n", Eventid);
    fprintf(kml, "  <visibility>1</visibility>\n");
    fprintf(kml, "  <description>\n");
    EpochToHumanISF(hday, htim, sp->time);
    fprintf(kml, "    <![CDATA[<b>%s</b> OT=%s %s ", Eventid, hday, htim);
    fprintf(kml, "Lat=%.3f Lon=%.3f Depth=%.1f<br>\n", sp->lat, sp->lon, sp->depth);
    fprintf(kml, "    sdobs=%.2f ", sp->sdobs);
    if (sp->error[0] == NULLVAL) fprintf(kml, "stime=N/A ");
    else                         fprintf(kml, "stime=%.2f ", sp->error[0]);
    if (sp->error[3] == NULLVAL) fprintf(kml, "sdepth=N/A ");
    else                         fprintf(kml, "sdepth=%.2f ", sp->error[3]);
    fprintf(kml, "smajax=%.1f sminax=%.1f strike=%.0f<br>\n",
            sp->smajax, sp->sminax, sp->strike);
    fprintf(kml, "    nass=%d ndef=%d nrank=%d nsta=%d ndefsta=%d<br>\n",
            sp->nass, sp->ndef, sp->prank, sp->nsta, sp->ndefsta);
    for (i = 0; i < MAXMAG; i++) {
        if (sp->mag[i].numDefiningMagnitude < MinNetmagSta) continue;
        fprintf(kml, "    %-5s %4.1f err=%3.1f nsta=%d<br>\n",
                sp->mag[i].magtype, sp->mag[i].magnitude,
                sp->mag[i].uncertainty, sp->mag[i].numDefiningMagnitude);
    }
    fprintf(kml, "  ]]>\n");
    fprintf(kml, "  </description>\n");
    fprintf(kml, "  <Style>\n");
    fprintf(kml, "    <BalloonStyle>\n");
    fprintf(kml, "      <bgColor>7fffffff</bgColor>\n");
    fprintf(kml, "      <textColor>ff000000</textColor>\n");
    fprintf(kml, "      <text><table width=\"450\">\n");
    fprintf(kml, "        <th align=\"center\">$[name]</th>\n");
    fprintf(kml, "        <tr><td>$[description]</td></tr>\n");
    fprintf(kml, "      </table></text>\n");
    fprintf(kml, "      <displayMode>default</displayMode>\n");
    fprintf(kml, "    </BalloonStyle>\n");
    fprintf(kml, "  </Style>\n");
    fprintf(kml, "  <styleUrl>#Solution</styleUrl>\n");
    fprintf(kml, "  <Point>\n");
    fprintf(kml, "    <coordinates>%.3f,%.3f,0</coordinates>\n",
            sp->lon, sp->lat);
    fprintf(kml, "  </Point>\n");
    fprintf(kml, "</Placemark>\n");
    WriteKMLErrorEllipse(kml, "iLocEllipse", "iLoc", sp->lat, sp->lon,
                         sp->smajax, sp->sminax, sp->strike);
}

static void WriteKMLErrorEllipse(FILE *kml, char *Styleid, char *auth,
        double evlat, double evlon, double smajax, double sminax, double strike)
{
    double step = TWOPI / (MAXKMLPOLYPOINTS - 1);
    double azim = DEG_TO_RAD * strike;
    double saz, caz, t, u, v, x, y, azi, delta, lat, lon;
    int i;
    if (smajax == NULLVAL) return;
    fprintf(kml, "<Placemark>\n");
    fprintf(kml, "  <name>%s</name>\n", Styleid);
    fprintf(kml, "  <description>%s</description>\n", auth);
    fprintf(kml, "  <visibility>1</visibility>\n");
    fprintf(kml, "  <styleUrl>#%s</styleUrl>\n", Styleid);
    fprintf(kml, "  <extrude>0</extrude>\n");
    fprintf(kml, "  <tessellate>1</tessellate>\n");
    fprintf(kml, "  <altitudeMode>clampToGround</altitudeMode>\n");
    fprintf(kml, "  <LinearRing>\n");
    fprintf(kml, "    <coordinates>\n");
    saz = sin(azim);
    caz = cos(azim);
    for (i = 0; i < MAXKMLPOLYPOINTS; i++) {
        t = (double)i * step;
        u = smajax * cos(t);
        v = sminax * sin(t);
		x = u * caz - v * saz;
		y = u * saz + v * caz;
		delta = sqrt(x * x + y * y) / DEG2KM;
        azi = RAD_TO_DEG * atan2(y, x);
        if (azi < 0.0) azi += 360.0;
		PointAtDeltaAzimuth(evlat, evlon, delta, azi, &lat, &lon);
		fprintf(kml, "    %f,%f,0\n", lon, lat);
	}
    fprintf(kml, "    </coordinates>\n");
    fprintf(kml, "  </LinearRing>\n");
    fprintf(kml, "</Placemark>\n");
}

static void WriteKMLLine(FILE *kml, char *Styleid,
        double evlat, double evlon, double slat, double slon)
{
    fprintf(kml, "<Placemark>\n");
    fprintf(kml, "  <visibility>0</visibility>\n");
    fprintf(kml, "  <styleUrl>#%s</styleUrl>\n", Styleid);
    fprintf(kml, "  <extrude>0</extrude>\n");
    fprintf(kml, "  <tessellate>1</tessellate>\n");
    fprintf(kml, "  <altitudeMode>clampToGround</altitudeMode>\n");
    fprintf(kml, "  <LineString>\n");
    fprintf(kml, "    <coordinates>\n");
	fprintf(kml, "      %f,%f,0 %f,%f,0\n", evlon, evlat, slon, slat);
    fprintf(kml, "    </coordinates>\n");
    fprintf(kml, "  </LineString>\n");
    fprintf(kml, "</Placemark>\n");
}



