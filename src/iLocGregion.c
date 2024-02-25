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
 *    ReadFlinnEngdahl
 *    FreeFlinnEngdahl
 *    GregionNumber
 *    GregToSreg
 *    Gregion
 *    Sregion
 *    ReadDefaultDepthGrid
 *    ReadDefaultDepthGregion
 *    GetDefaultDepth
 */

/*
 * Local functions:
 *    DefaultDepthFromGrid
 */
static int DefaultDepthFromGrid(double lat, double lon, double gres,
                        int ngrid, double **DepthGrid, double *defdep);

/*
 *  Title:
 *     ReadFlinnEngdahl
 *  Synopsis:
 *     Reads Flinn-Engdahl regionalization file and stores it in FE structure.
 *
 *     Young, J.B., Presgrave, B.W., Aichele, H., Wiens, D.A. and Flinn, E.A.,
 *     1996, The Flinn-Engdahl Regionalisation Scheme: the 1995 revision,
 *     Physics of the Earth and Planetary Interiors, 96, 223-297.
 *
 *     For each latitude (from 90N to 90S) a set of longitude ranges
 *         is given (first part of the file). The second part of the file
 *         lists the geographic region numbers for each latitude for within
 *         the longitude ranges.
 *  Input Arguments:
 *     fname - pathname for the Flinn-Engdahl regionalization file
 *     fep   - pointer to FE structure
 *  Output Arguments:
 *     fep   - pointer to FE structure
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     ReadAuxDataFiles
 *  Calls:
 *     SkipComments, Free
 */
int ReadFlinnEngdahl(char *fname, FE *fep)
{
    FILE *fp;
    char buf[LINLEN], *s;
    int nlat = 0, i, j, n = 0;
/*
 *  open Flinn-Engdahl regionalization file
 */
    if ((fp = fopen(fname, "r")) == NULL) {
        fprintf(errfp, "ReadFlinnEngdahl: cannot open %s\n", fname);
        fprintf(logfp, "ReadFlinnEngdahl: cannot open %s\n", fname);
        errorcode = 2;
        return 1;
    }
/*
 *  number of latitudes
 */
    fgets(buf, LINLEN, fp);
    SkipComments(buf, fp);
    sscanf(buf, "%d", &nlat);
    fep->nlat = nlat;
/*
 *  memory allocations
 */
    fep->lon = (int **)calloc(nlat, sizeof(int *));
    fep->grn = (int **)calloc(nlat, sizeof(int *));
    if ((fep->nl = (int *)calloc(nlat, sizeof(int))) == NULL) {
        fprintf(logfp, "ReadFlinnEngdahl: cannot allocate memory\n");
        fprintf(errfp, "ReadFlinnEngdahl: cannot allocate memory\n");
        fclose(fp);
        Free(fep->grn);
        Free(fep->lon);
        errorcode = 1;
        return 1;
    }
/*
 *  number of longitudes at each latitude
 */
    SkipComments(buf, fp);
    s = strtok(buf, " ");
    fep->nl[0] = atoi(s);
    n = fep->nl[0];
    for (i = 1; i < nlat; i++) {
        s = strtok(NULL, " ");
        fep->nl[i] = atoi(s);
        n += fep->nl[i];
    }
/*
 *  memory allocations
 */
    if ((fep->lon[0] = (int *)calloc(nlat * n, sizeof(int))) == NULL) {
        fprintf(logfp, "ReadFlinnEngdahl: cannot allocate memory\n");
        fprintf(errfp, "ReadFlinnEngdahl: cannot allocate memory\n");
        fclose(fp);
        Free(fep->nl);
        Free(fep->grn);
        Free(fep->lon);
        errorcode = 1;
        return 1;
    }
    if ((fep->grn[0] = (int *)calloc((nlat - 1) * n, sizeof(int))) == NULL) {
        fprintf(logfp, "ReadFlinnEngdahl: cannot allocate memory\n");
        fprintf(errfp, "ReadFlinnEngdahl: cannot allocate memory\n");
        fclose(fp);
        Free(fep->nl);
        Free(fep->lon[0]);
        Free(fep->lon);
        Free(fep->grn);
        errorcode = 1;
        return 1;
    }
    for (i = 1; i < nlat; i++) {
        fep->lon[i] = fep->lon[i - 1] + fep->nl[i - 1];
        fep->grn[i] = fep->grn[i - 1] + fep->nl[i - 1] - 1;
    }
/*
 *  longitude ranges at each latitude
 */
    for (i = 0; i < nlat; i++) {
        SkipComments(buf, fp);
        s = strtok(buf, " ");
        n = atoi(s);
        for (j = 0; j < n; j++) {
            s = strtok(NULL, " ");
            fep->lon[i][j] = atoi(s);
        }
    }
/*
 *  geographic region numbers for corresponding longitude ranges per latitude
 */
    for (i = 0; i < nlat; i++) {
        SkipComments(buf, fp);
        s = strtok(buf, " ");
        n = atoi(s);
        for (j = 0; j < n; j++) {
            s = strtok(NULL, " ");
            fep->grn[i][j] = atoi(s);
        }
    }
    fclose(fp);
    return 0;
}

/*
 *  Title:
 *     FreeFlinnEngdahl
 *  Synopsis:
 *     Frees memory allocated to FE structure.
 *  Input Arguments:
 *     fep - pointer to FE structure
 *  Called by:
 *     ReadAuxDataFiles, main
 */
void FreeFlinnEngdahl(FE *fep)
{
    Free(fep->nl);
    Free(fep->lon[0]);
    Free(fep->lon);
    Free(fep->grn[0]);
    Free(fep->grn);
}

/*
 *  Title:
 *     GregionNumber
 *  Synopsis:
 *     Returns Flinn-Engdahl geographic region number for a lat, lon pair.
 *  Input Arguments:
 *     lat - geographic latitude
 *     lon - geographic longitude
 *     fep - pointer to FE structure
 *  Return:
 *     grn - geographic region number
 *  Called by:
 *     Locator, GetDefaultDepth, WriteISCHypocenter
 *  Calls:
 *     IntegerBracket
 */
int GregionNumber(double lat, double lon, FE *fep)
{
    int ilo = 0, ihi = 0;
    int ilat = (int)(90. - lat);
    int ilon = floor(lon);
    int n = fep->nl[ilat];
    if (n == 2)
        return fep->grn[ilat][0];
    else {
        IntegerBracket(ilon, n, fep->lon[ilat], &ilo, &ihi);
        if (ilon < fep->lon[ilat][ihi]) return fep->grn[ilat][ilo];
        else if (ilon == 180)           return fep->grn[ilat][ilo];
        else                            return fep->grn[ilat][ihi];
    }
}

/*
 *  Title:
 *     GregToSreg
 *  Synopsis:
 *     Returns seismic region number for a geographic region number.
 *  Input Arguments:
 *     grn - geographic region number
 *  Return:
 *     srn - seismic region number or -1 on error
 *  Called by:
 *     WriteISCHypocenter
 *  Calls:
 *     IntegerBracket
 */
int GregToSreg(int grn)
{
    static int gstab[52] = { 0,
                 1, 18, 30, 47, 53, 72, 84, 102, 143, 147,
                 158, 169, 180, 183, 190, 196, 209, 211, 217, 231,
                 242, 248, 261, 273, 294, 302, 320, 326, 335, 357,
                 376, 402, 415, 438, 528, 532, 550, 588, 611, 633,
                 656, 667, 683, 693, 700, 703, 709, 713, 721, 727, 729
    };
    int ilo = 0, ihi = 0;
    if ((grn < 1) || (grn > 757))
        return -1;
    if (grn < 730) {
        IntegerBracket(grn, 51, gstab, &ilo, &ihi);
        if (grn < gstab[ihi]) return ilo;
        else                  return ihi;
    }
    else if (grn == 730) return  5;
    else if (grn == 731) return  7;
    else if (grn == 732) return 10;
    else if (grn > 732 && grn < 738) return 25;
    else if (grn > 737 && grn < 740) return 32;
    else if (grn > 739 && grn < 743) return 33;
    else if (grn > 742 && grn < 756) return 37;
    else if (grn == 756) return 43;
    else if (grn == 757) return 44;
    else return -1;
}

/*
 *  Title:
 *     GetDefaultDepth
 *  Synopsis:
 *     Determines default depth for a lat, lon pair.
 *  Input Arguments:
 *     lat       - latitude
 *     lon       - longitude
 *     ngrid     - number of grid points
 *     gres      - grid spacing
 *     DepthGrid - default depth grid
 *     fep       - FE structure
 *     GrnDepth  - geographic region default depths
 *  Output Arguments:
 *     isdefdep  - default depth grid point exists?
 *  Return:
 *     defdep    - default depth
 *  Called by:
 *     Locator
 *  Calls:
 *     DefaultDepthFromGrid, GregionNumber
 */
double GetDefaultDepth(SOLREC *sp, int ngrid, double gres,
                         double **DepthGrid, FE *fep, double *GrnDepth,
                         int *isdefdep)
{
    double defdep = 0.;
    int grn = 0;
/*
 *  get default depth from grid
 */
    if (DefaultDepthFromGrid(sp->lat, sp->lon, gres, ngrid,
                                DepthGrid, &defdep)) {
        sp->FixedDepthType = 5;
        *isdefdep = 1;
        fprintf(logfp, "Fix depth by default depth grid: %.2f\n", defdep);
    }
/*
 *  fix the depth to something else
 */
    else {
        if (sp->depth > 100.) {
/*
 *          deep event; fix the depth to the median of reported depths
 */
            sp->FixedDepthType = 6;
            defdep = sp->depth;
            *isdefdep = 0;
            fprintf(logfp, "No default depth grid point exists, ");
            fprintf(logfp, "fix depth to median reported depth: %.2f\n",
                    defdep);
        }
        else {
/*
 *          shallow event; fix the depth to GRN-dependent depth
 */
            sp->FixedDepthType = 7;
            grn = GregionNumber(sp->lat, sp->lon, fep);
            defdep = GrnDepth[grn - 1];
            fprintf(logfp, "No default depth grid point exists, ");
            fprintf(logfp, "fix depth by region: %.2f\n", defdep);
        }

    }
    return defdep;
}

/*
 *  Title:
 *     ReadDefaultDepthGregion
 *  Synopsis:
 *     Reads geographic region number and model specific default depths from
 *     file.
 *
 *     File contains default depths for each Flinn-Engdahl geographic
 *     region number.
 *     columns:
 *         grn:   Flinn-Engdahl geographic region number
 *         depth: depth for GRN
 *  Input Arguments:
 *     fname - pathname for the default depth file
 *  Return:
 *     GrnDepth - pointer to default depth vector
 *  Called by:
 *     ReadAuxDataFiles
 *  Calls:
 *     Free
 */
double *ReadDefaultDepthGregion(char *fname)
{
    FILE *fp;
    double *GrnDepth = (double *)NULL;
    double depth = 0.;
    int ngreg = 757, i, k;
/*
 *  memory allocation
 */
    if ((GrnDepth = (double *)calloc(ngreg, sizeof(double))) == NULL) {
        fprintf(logfp, "read_DefaultDepth: cannot allocate memory\n");
        fprintf(errfp, "read_DefaultDepth: cannot allocate memory\n");
        return (double *)NULL;
    }
/*
 *  open default depth file
 */
    if ((fp = fopen(fname, "r")) == NULL) {
        fprintf(errfp, "read_DefaultDepth: cannot open %s\n", fname);
        fprintf(logfp, "read_DefaultDepth: cannot open %s\n", fname);
        Free(GrnDepth);
        return (double *)NULL;
    }
    for (i = 0; i < ngreg; i++) {
        fscanf(fp, "%d%lf", &k, &depth);
        GrnDepth[i] = depth;
    }
    fclose(fp);
    return GrnDepth;
}

/*
 *  Title:
 *     ReadDefaultDepthGrid
 *  Synopsis:
 *     Reads default depth grid from file.
 *
 *     The default depth grid follows gridline registration, i.e. the nodes are
 *     centered on the grid line intersections and the data points represent
 *     the median value in a cell of dimensions (gres x gres) centered on the
 *     nodes.
 *
 *      lat(i-1) +--------+--------+--------+
 *               |        |        |        |
 *               |        |        |        |
 *               |    #########    |        |
 *               |    #   |   #    |        |
 *      lat(i)   +----#---o---#----+--------+
 *               |    #   |   #    |        |
 *               |    #########    |        |
 *               |        |        |        |
 *               |        |        |        |
 *      lat(i+1) +--------+--------+--------+
 *            lon(j-1)  lon(j)   lon(j+1) lon(j+2)
 *
 *     columns:
 *        lat, lon: center of grid cells
 *        depth: median depth in the cell
 *        min:   minimum depth in the cell
 *        25Q:   25th percentile depth in the cell
 *        75Q:   75th percentile depth in the cell
 *        max:   maximum depth in the cell
 *        N:     number of observations in the cell
 *        range: quartile range (75Q - 25Q)
 *     rows ordered by descending latitude and increasing longitude
 *  Input Arguments:
 *     fname - pathname for the default depth file
 *  Output Arguments:
 *     gres  - grid spacing
 *     ngrid - number of grid points
 *  Return:
 *     DepthGrid - pointer to default depth grid (lat, lon, depth)
 *  Called by:
 *     ReadAuxDataFiles
 *  Calls:
 *     AllocateFloatMatrix, SkipComments
 */
double **ReadDefaultDepthGrid(char *fname, double *gres, int *ngrid)
{
    FILE *fp;
    double **DepthGrid = (double **)NULL;
    double lat = 0., lon = 0., depth = 0., res = 1.;
    double q25 = 0., q75 = 0., lo = 0., hi = 0., range = 0.;
    char buf[LINLEN];
    int n = 0, i, m = 0, k = 0;
/*
 *  open default depth grid file
 */
    if ((fp = fopen(fname, "r")) == NULL) {
        fprintf(errfp, "ReadDefaultDepthGrid: cannot open %s\n", fname);
        fprintf(logfp, "ReadDefaultDepthGrid: cannot open %s\n", fname);
        errorcode = 2;
        return (double **)NULL;
    }
/*
 *  skip comments and get grid spacing
 */
    fgets(buf, LINLEN, fp);
    SkipComments(buf, fp);
    sscanf(buf, "%lf", &res);
/*
 *  number of gridpoints
 */
    fscanf(fp, "%d", &n);
/*
 *  memory allocation
 */
    if ((DepthGrid = AllocateFloatMatrix(n, 3)) == NULL)
        return (double **)NULL;
/*
 *  read default depth grid
 */
    k = 0;
    for (i = 0; i < n; i++) {
        fscanf(fp, "%lf%lf%lf%lf%lf%lf%lf%d%lf",
               &lat, &lon, &depth, &lo, &q25, &q75, &hi, &m, &range);
/*
 *      ignore estimates with large uncertainties
 */
        if (range > 100.) continue;
/*
 *      populate default depth grid
 */
        DepthGrid[k][0] = lat;
        DepthGrid[k][1] = lon;
        DepthGrid[k][2] = depth;
        k++;
    }
    fclose(fp);
    *ngrid = k;
    *gres = res;
    return DepthGrid;
}

/*
 *  Title:
 *     DefaultDepthFromGrid
 *  Synopsis:
 *     Sets default depth from default depth grid if grid cell exists.
 *
 *     Default depth grid follows gridline registration, i.e. the nodes are
 *     centered on the grid line intersections and the data points represent
 *     the median value in a cell of dimensions (gres x gres) centered on the
 *     nodes.
 *
 *       i-1 +--------+--------+--------+
 *           |        |        |        |
 *           |        |        |        |
 *           |    #########    |        |
 *           |    #   |   #    |        |
 *       i   +----#---o---#----+--------+
 *           |    #   |   #    |        |
 *           |    #########    |        |
 *           |        |        |        |
 *           |        |        |        |
 *       i+1 +--------+--------+--------+
 *          j-1       j       j+1      j+2
 *
 *     Therefore, a point (lat,lon) falls in a grid cell if
 *         (abs(lat - grid_lat) <= gres/2 && abs(lon - grid_lon) <= gres/2)
 *  Input Arguments:
 *     lat   - latitude
 *     lon   - longitude
 *     gres  - grid spacing
 *     ngrid - number of grid points
 *     DepthGrid - default depth grid
 *  Output Arguments:
 *     defdep - default depth from depth grid
 *  Return:
 *     1 if found a grid cell, 0 otherwise
 *  Called by:
 *     GetDefaultDepth
 */
static int DefaultDepthFromGrid(double lat, double lon, double gres,
                         int ngrid, double **DepthGrid, double *defdep)
{
    int i;
    char s[100];
    for (i = 0; i < ngrid; i++) {
/*
 *      find corresponding grid point
 *      depth grid has gridline registration!
 */
        if (fabs(lat - DepthGrid[i][0]) > 0.5 * gres  ||
            fabs(lon - DepthGrid[i][1]) > 0.5 * gres)
            continue;
/*
 *      round default depth to the nearest km
 */
        sprintf(s, "%.0f\n", DepthGrid[i][2]);
        *defdep = atof(s);
        return 1;
    }
/*
 *  no grid cell exists for this lat, lon
 */
    return 0;
}

/*
 *  Title:
 *     Gregion
 *  Synopsis:
 *     Returns Flinn-Engdahl geographic region name for a geographic region
 *        number.
 *  Input Arguments:
 *     grn - geographic region number
 *  Output Arguments:
 *     gregname - Flinn-Engdahl geographic region name
 *  Return:
 *     0/1 on success/error
 *  Called by:
 *     PrintSolution, WriteISF
 */
int Gregion(int number, char *gregname)
{
    static char *gregs[] = {
        "Central Alaska",
        "Southern Alaska",
        "Bering Sea",
        "Komandorsky Islands region",
        "Near Islands",
        "Rat Islands",
        "Andreanof Islands",
        "Pribilof Islands",
        "Fox Islands",
        "Unimak Island region",
        "Bristol Bay",
        "Alaska Peninsula",
        "Kodiak Island region",
        "Kenai Peninsula",
        "Gulf of Alaska",
        "South of Aleutian Islands",
        "South of Alaska",
        "Southern Yukon Territory",
        "Southeastern Alaska",
        "Off coast of southeastern Alaska",
        "West of Vancouver Island",
        "Queen Charlotte Islands region",
        "British Columbia",
        "Alberta",
        "Vancouver Island region",
        "Off coast of Washington",
        "Near coast of Washington",
        "Washington-Oregon border region",
        "Washington",
        "Off coast of Oregon",
        "Near coast of Oregon",
        "Oregon",
        "Western Idaho",
        "Off coast of northern California",
        "Near coast of northern California",
        "Northern California",
        "Nevada",
        "Off coast of California",
        "Central California",
        "California-Nevada border region",
        "Southern Nevada",
        "Western Arizona",
        "Southern California",
        "California-Arizona border region",
        "California-Baja California border region",
        "Western Arizona-Sonora border region",
        "Off west coast of Baja California",
        "Baja California",
        "Gulf of California",
        "Sonora",
        "Off coast of central Mexico",
        "Near coast of central Mexico",
        "Revilla Gigedo Islands region",
        "Off coast of Jalisco",
        "Near coast of Jalisco",
        "Near coast of Michoacan",
        "Michoacan",
        "Near coast of Guerrero",
        "Guerrero",
        "Oaxaca",
        "Chiapas",
        "Mexico-Guatemala border region",
        "Off coast of Mexico",
        "Off coast of Michoacan",
        "Off coast of Guerrero",
        "Near coast of Oaxaca",
        "Off coast of Oaxaca",
        "Off coast of Chiapas",
        "Near coast of Chiapas",
        "Guatemala",
        "Near coast of Guatemala",
        "Honduras",
        "El Salvador",
        "Near coast of Nicaragua",
        "Nicaragua",
        "Off coast of central America",
        "Off coast of Costa Rica",
        "Costa Rica",
        "North of Panama",
        "Panama-Costa Rica border region",
        "Panama",
        "Panama-Colombia border region",
        "South of Panama",
        "Yucatan Peninsula",
        "Cuba region",
        "Jamaica region",
        "Haiti region",
        "Dominican Republic region",
        "Mona Passage",
        "Puerto Rico region",
        "Virgin Islands",
        "Leeward Islands",
        "Belize",
        "Caribbean Sea",
        "Windward Islands",
        "Near north coast of Colombia",
        "Near coast of Venezuela",
        "Trinidad",
        "Northern Colombia",
        "Lake Maracaibo",
        "Venezuela",
        "Near west coast of Colombia",
        "Colombia",
        "Off coast of Ecuador",
        "Near coast of Ecuador",
        "Colombia-Ecuador border region",
        "Ecuador",
        "Off coast of northern Peru",
        "Near coast of northern Peru",
        "Peru-Ecuador border region",
        "Northern Peru",
        "Peru-Brazil border region",
        "Western Brazil",
        "Off coast of Peru",
        "Near coast of Peru",
        "Central Peru",
        "Southern Peru",
        "Peru-Bolivia border region",
        "Northern Bolivia",
        "Central Bolivia",
        "Off coast of northern Chile",
        "Near coast of northern Chile",
        "Northern Chile",
        "Chile-Bolivia border region",
        "Southern Bolivia",
        "Paraguay",
        "Chile-Argentina border region",
        "Jujuy Province",
        "Salta Province",
        "Catamarca Province",
        "Tucuman Province",
        "Santiago del Estero Province",
        "Northeastern Argentina",
        "Off coast of central Chile",
        "Near coast of central Chile",
        "Central Chile",
        "San Juan Province",
        "La Rioja Province",
        "Mendoza Province",
        "San Luis Province",
        "Cordoba Province",
        "Uruguay",
        "Off coast of southern Chile",
        "Southern Chile",
        "Southern Chile-Argentina border region",
        "Southern Argentina",
        "Tierra del Fuego",
        "Falkland Islands region",
        "Drake Passage",
        "Scotia Sea",
        "South Georgia Island region",
        "South Georgia Rise",
        "South Sandwich Islands region",
        "South Shetland Islands",
        "Antarctic Peninsula",
        "Southwestern Atlantic Ocean",
        "Weddell Sea",
        "Off west coast of North Island",
        "North Island",
        "Off east coast of North Island",
        "Off west coast of South Island",
        "South Island",
        "Cook Strait",
        "Off east coast of South Island",
        "North of Macquarie Island",
        "Auckland Islands region",
        "Macquarie Island region",
        "South of New Zealand",
        "Samoa Islands region",
        "Samoa Islands",
        "South of Fiji Islands",
        "West of Tonga Islands (REGION NOT IN USE)",
        "Tonga Islands",
        "Tonga Islands region",
        "South of Tonga Islands",
        "North of New Zealand",
        "Kermadec Islands region",
        "Kermadec Islands",
        "South of Kermadec Islands",
        "North of Fiji Islands",
        "Fiji Islands region",
        "Fiji Islands",
        "Santa Cruz Islands region",
        "Santa Cruz Islands",
        "Vanuatu Islands region",
        "Vanuatu Islands",
        "New Caledonia",
        "Loyalty Islands",
        "Southeast of Loyalty Islands",
        "New Ireland region",
        "North of Solomon Islands",
        "New Britain region",
        "Bougainville - Solomon Islands region",
        "D'Entrecasteaux Islands region",
        "South of Solomon Islands",
        "Irian Jaya region",
        "Near north coast of Irian Jaya",
        "Ninigo Islands region",
        "Admiralty Islands region",
        "Near north coast of New Guinea",
        "Irian Jaya",
        "New Guinea",
        "Bismarck Sea",
        "Aru Islands region",
        "Near south coast of Irian Jaya",
        "Near south coast of New Guinea",
        "Eastern New Guinea region",
        "Arafura Sea",
        "Western Caroline Islands",
        "South of Mariana Islands",
        "Southeast of Honshu",
        "Bonin Islands region",
        "Volcano Islands region",
        "West of Mariana Islands",
        "Mariana Islands region",
        "Mariana Islands",
        "Kamchatka Peninsula",
        "Near east coast of Kamchatka Peninsula",
        "Off east coast of Kamchatka Peninsula",
        "Northwest of Kuril Islands",
        "Kuril Islands",
        "East of Kuril Islands",
        "Eastern Sea of Japan",
        "Hokkaido region",
        "Off southeast coast of Hokkaido",
        "Near west coast of eastern Honshu",
        "Eastern Honshu",
        "Near east coast of eastern Honshu",
        "Off east coast of Honshu",
        "Near south coast of eastern Honshu",
        "South Korea",
        "Western Honshu",
        "Near south coast of western Honshu",
        "Northwest of Ryukyu Islands",
        "Kyushu",
        "Shikoku",
        "Southeast of Shikoku",
        "Ryukyu Islands",
        "Southeast of Ryukyu Islands",
        "West of Bonin Islands",
        "Philippine Sea",
        "Near coast of southeastern China",
        "Taiwan region",
        "Taiwan",
        "Northeast of Taiwan",
        "Southwestern Ryukyu Islands",
        "Southeast of Taiwan",
        "Philippine Islands region",
        "Luzon",
        "Mindoro",
        "Samar",
        "Palawan",
        "Sulu Sea",
        "Panay",
        "Cebu",
        "Leyte",
        "Negros",
        "Sulu Archipelago",
        "Mindanao",
        "East of Philippine Islands",
        "Borneo",
        "Celebes Sea",
        "Talaud Islands",
        "North of Halmahera",
        "Minahassa Peninsula, Sulawesi",
        "Northern Molucca Sea",
        "Halmahera",
        "Sulawesi",
        "Southern Molucca Sea",
        "Ceram Sea",
        "Buru",
        "Seram",
        "Southwest of Sumatera",
        "Southern Sumatera",
        "Java Sea",
        "Sunda Strait",
        "Jawa",
        "Bali Sea",
        "Flores Sea",
        "Banda Sea",
        "Tanimbar Islands region",
        "South of Jawa",
        "Bali region",
        "South of Bali",
        "Sumbawa region",
        "Flores region",
        "Sumba region",
        "Savu Sea",
        "Timor region",
        "Timor Sea",
        "South of Sumbawa",
        "South of Sumba",
        "South of Timor",
        "Myanmar-India border region",
        "Myanmar-Bangladesh border region",
        "Myanmar",
        "Myanmar-China border region",
        "Near south coast of Myanmar",
        "Southeast Asia (REGION NOT IN USE)",
        "Hainan Island",
        "South China Sea",
        "Eastern Kashmir",
        "Kashmir-India border region",
        "Kashmir-Xizang border region",
        "Western Xizang-India border region",
        "Xizang",
        "Sichuan",
        "Northern India",
        "Nepal-India border region",
        "Nepal",
        "Sikkim",
        "Bhutan",
        "Eastern Xizang-India border region",
        "Southern India",
        "India-Bangladesh border region",
        "Bangladesh",
        "Northeastern India",
        "Yunnan",
        "Bay of Bengal",
        "Kyrgyzstan-Xinjiang border region",
        "Southern Xinjiang",
        "Gansu",
        "Western Nei Mongol",
        "Kashmir-Xinjiang border region",
        "Qinghai",
        "Southwestern Siberia",
        "Lake Baykal region",
        "East of Lake Baykal",
        "Eastern Kazakhstan",
        "Lake Issyk-Kul region",
        "Kazakhstan-Xinjiang border region",
        "Northern Xinjiang",
        "Tuva-Buryatia-Mongolia border region",
        "Mongolia",
        "Ural Mountains region",
        "Western Kazakhstan",
        "Eastern Caucasus",
        "Caspian Sea",
        "Northwestern Uzbekistan",
        "Turkmenistan",
        "Iran-Turkmenistan border region",
        "Turkmenistan-Afghanistan border region",
        "Turkey-Iran border region",
        "Iran-Armenia-Azerbaijan border region",
        "Northwestern Iran",
        "Iran-Iraq border region",
        "Western Iran",
        "Northern and central Iran",
        "Northwestern Afghanistan",
        "Southwestern Afghanistan",
        "Eastern Arabian Peninsula",
        "Persian Gulf",
        "Southern Iran",
        "Southwestern Pakistan",
        "Gulf of Oman",
        "Off coast of Pakistan",
        "Ukraine - Moldova - Southwestern Russia region",
        "Romania",
        "Bulgaria",
        "Black Sea",
        "Crimea region",
        "Western Caucasus",
        "Greece-Bulgaria border region",
        "Greece",
        "Aegean Sea",
        "Turkey",
        "Turkey-Georgia-Armenia border region",
        "Southern Greece",
        "Dodecanese Islands",
        "Crete",
        "Eastern Mediterranean Sea",
        "Cyprus region",
        "Dead Sea region",
        "Jordan - Syria region",
        "Iraq",
        "Portugal",
        "Spain",
        "Pyrenees",
        "Near south coast of France",
        "Corsica",
        "Central Italy",
        "Adriatic Sea",
        "Northwestern Balkan Peninsula",
        "West of Gibraltar",
        "Strait of Gibraltar",
        "Balearic Islands",
        "Western Mediterranean Sea",
        "Sardinia",
        "Tyrrhenian Sea",
        "Southern Italy",
        "Albania",
        "Greece-Albania border region",
        "Madeira Islands region",
        "Canary Islands region",
        "Morocco",
        "Northern Algeria",
        "Tunisia",
        "Sicily",
        "Ionian Sea",
        "Central Mediterranean Sea",
        "Near coast of Libya",
        "North Atlantic Ocean",
        "Northern Mid-Atlantic Ridge",
        "Azores Islands region",
        "Azores Islands",
        "Central Mid-Atlantic Ridge",
        "North of Ascension Island",
        "Ascension Island region",
        "South Atlantic Ocean",
        "Southern Mid-Atlantic Ridge",
        "Tristan da Cunha region",
        "Bouvet Island region",
        "Southwest of Africa",
        "Southeastern Atlantic Ocean",
        "Eastern Gulf of Aden",
        "Socotra region",
        "Arabian Sea",
        "Lakshadweep region",
        "Northeastern Somalia",
        "North Indian Ocean",
        "Carlsberg Ridge",
        "Maldive Islands region",
        "Laccadive Sea",
        "Sri Lanka",
        "South Indian Ocean",
        "Chagos Archipelago region",
        "Mauritius - Reunion region",
        "Southwest Indian Ridge",
        "Mid-Indian Ridge",
        "South of Africa",
        "Prince Edward Islands region",
        "Crozet Islands region",
        "Kerguelen Islands region",
        "Broken Ridge",
        "Southeast Indian Ridge",
        "Southern Kerguelen Plateau",
        "South of Australia",
        "Saskatchewan",
        "Manitoba",
        "Hudson Bay",
        "Ontario",
        "Hudson Strait region",
        "Northern Quebec",
        "Davis Strait",
        "Labrador",
        "Labrador Sea",
        "Southern Quebec",
        "Gaspe Peninsula",
        "Eastern Quebec",
        "Anticosti Island",
        "New Brunswick",
        "Nova Scotia",
        "Prince Edward Island",
        "Gulf of St. Lawrence",
        "Newfoundland",
        "Montana",
        "Eastern Idaho",
        "Hebgen Lake region, Montana",
        "Yellowstone region",
        "Wyoming",
        "North Dakota",
        "South Dakota",
        "Nebraska",
        "Minnesota",
        "Iowa",
        "Wisconsin",
        "Illinois",
        "Michigan",
        "Indiana",
        "Southern Ontario",
        "Ohio",
        "New York",
        "Pennsylvania",
        "Vermont - New Hampshire region",
        "maine",
        "Southern New England",
        "Gulf of maine",
        "Utah",
        "Colorado",
        "Kansas",
        "Iowa-Missouri border region",
        "Missouri-Kansas border region",
        "Missouri",
        "Missouri-Arkansas border region",
        "Missouri-Illinois border region",
        "New Madrid region, Missouri",
        "Cape Girardeau region, Missouri",
        "Southern Illinois",
        "Southern Indiana",
        "Kentucky",
        "West Virginia",
        "Virginia",
        "Chesapeake Bay region",
        "New Jersey",
        "Eastern Arizona",
        "New Mexico",
        "Northwestern Texas-Oklahoma border region",
        "Western Texas",
        "Oklahoma",
        "Central Texas",
        "Arkansas-Oklahoma border region",
        "Arkansas",
        "Louisiana-Texas border region",
        "Louisiana",
        "Mississippi",
        "Tennessee",
        "Alabama",
        "Western Florida",
        "Georgia",
        "Florida-Georgia border region",
        "South Carolina",
        "North Carolina",
        "Off east coast of United States",
        "Florida Peninsula",
        "Bahama Islands",
        "Eastern Arizona-Sonora border region",
        "New Mexico-Chihuahua border region",
        "Texas-Mexico border region",
        "Southern Texas",
        "Near coast of Texas",
        "Chihuahua",
        "Northern Mexico",
        "Central Mexico",
        "Jalisco",
        "Veracruz",
        "Gulf of Mexico",
        "Bay of Campeche",
        "Brazil",
        "Guyana",
        "Suriname",
        "French Guiana",
        "Eire",
        "United Kingdom",
        "North Sea",
        "Southern Norway",
        "Sweden",
        "Baltic Sea",
        "France",
        "Bay of Biscay",
        "The Netherlands",
        "Belgium",
        "Denmark",
        "Germany",
        "Switzerland",
        "Northern Italy",
        "Austria",
        "Czech and Slovak Republics",
        "Poland",
        "Hungary",
        "Northwest Africa (REGION NOT IN USE)",
        "Southern Algeria",
        "Libya",
        "Egypt",
        "Red Sea",
        "Western Arabian Peninsula",
        "Chad region",
        "Sudan",
        "Ethiopia",
        "Western Gulf of Aden",
        "Northwestern Somalia",
        "Off south coast of northwest Africa",
        "Cameroon",
        "Equatorial Guinea",
        "Central African Republic",
        "Gabon",
        "Congo",
        "Zaire",
        "Uganda",
        "Lake Victoria region",
        "Kenya",
        "Southern Somalia",
        "Lake Tanganyika region",
        "Tanzania",
        "Northwest of Madagascar",
        "Angola",
        "Zambia",
        "Malawi",
        "Namibia",
        "Botswana",
        "Zimbabwe",
        "Mozambique",
        "Mozambique Channel",
        "Madagascar",
        "South Africa",
        "Lesotho",
        "Swaziland",
        "Off coast of South Africa",
        "Northwest of Australia",
        "West of Australia",
        "Western Australia",
        "Northern Territory",
        "South Australia",
        "Gulf of Carpentaria",
        "Queensland",
        "Coral Sea",
        "Northwest of New Caledonia",
        "New Caledonia region",
        "Southwest of Australia",
        "Off south coast of Australia",
        "Near coast of South Australia",
        "New South Wales",
        "Victoria",
        "Near southeast coast of Australia",
        "Near east coast of Australia",
        "East of Australia",
        "Norfolk Island region",
        "Northwest of New Zealand",
        "Bass Strait",
        "Tasmania region",
        "Southeast of Australia",
        "North Pacific Ocean",
        "Hawaiian Islands region",
        "Hawaiian Islands",
        "Eastern Caroline Islands region",
        "Marshall Islands region",
        "Enewetak Atoll region",
        "Bikini Atoll region",
        "Gilbert Islands region",
        "Johnston Island region",
        "Line Islands region",
        "Palmyra Island region",
        "Kiritimati region",
        "Tuvalu region",
        "Phoenix Islands region",
        "Tokelau Islands region",
        "Northern Cook Islands",
        "Cook Islands region",
        "Society Islands region",
        "Tubuai Islands region",
        "Marquesas Islands region",
        "Tuamotu Archipelago region",
        "South Pacific Ocean",
        "Lomonosov Ridge",
        "Arctic Ocean",
        "Near north coast of Kalaallit Nunaat",
        "Eastern Kalaallit Nunaat",
        "Iceland region",
        "Iceland",
        "Jan Mayen Island region",
        "Greenland Sea",
        "North of Svalbard",
        "Norwegian Sea",
        "Svalbard region",
        "North of Franz Josef Land",
        "Franz Josef Land",
        "Northern Norway",
        "Barents Sea",
        "Novaya Zemlya",
        "Kara Sea",
        "Near coast of northwestern Siberia",
        "North of Severnaya Zemlya",
        "Severnaya Zemlya",
        "Near coast of northern Siberia",
        "East of Severnaya Zemlya",
        "Laptev Sea",
        "Southeastern Siberia",
        "Primorye-Northeastern China border region",
        "Northeastern China",
        "North Korea",
        "Sea of Japan",
        "Primorye",
        "Sakhalin Island",
        "Sea of Okhotsk",
        "Southeastern China",
        "Yellow Sea",
        "Off east coast of southeastern China",
        "North of New Siberian Islands",
        "New Siberian Islands",
        "Eastern Siberian Sea",
        "Near north coast of eastern Siberia",
        "Eastern Siberia",
        "Chukchi Sea",
        "Bering Strait",
        "St. Lawrence Island region",
        "Beaufort Sea",
        "Northern Alaska",
        "Northern Yukon Territory",
        "Queen Elizabeth Islands",
        "Northwest Territories",
        "Western Kalaallit Nunaat",
        "Baffin Bay",
        "Baffin Island region",
        "Southeast Central Pacific Ocean",
        "Southern East Pacific Rise",
        "Easter Island region",
        "West Chile Rise",
        "Juan Fernandez Islands region",
        "East of North Island",
        "Chatham Islands region",
        "South of Chatham Islands",
        "Pacific-Antarctic Ridge",
        "Southern Pacific Ocean",
        "East Central Pacific Ocean",
        "Central East Pacific Rise",
        "West of Galapagos Islands",
        "Galapagos Islands region",
        "Galapagos Islands",
        "Southwest of Galapagos Islands",
        "Southeast of Galapagos Islands",
        "South of Tasmania",
        "West of Macquarie Island",
        "Balleny Islands region",
        "Andaman Islands region",
        "Nicobar Islands region",
        "Off west coast of northern Sumatera",
        "Northern Sumatera",
        "Malay Peninsula",
        "Gulf of Thailand",
        "Southeastern Afghanistan",
        "Pakistan",
        "Southwestern Kashmir",
        "India-Pakistan border region",
        "Central Kazakhstan",
        "Southeastern Uzbekistan",
        "Tajikistan",
        "Kyrgyzstan",
        "Afghanistan-Tajikistan border region",
        "Hindu Kush region",
        "Tajikistan-Xinjiang border region",
        "Northwestern Kashmir",
        "Finland",
        "Norway-Murmansk border region",
        "Finland-Karelia border region",
        "Baltic States - Belarus - Northwestern Russia",
        "Northwestern Siberia",
        "Northern and central Siberia",
        "Victoria Land",
        "Ross Sea",
        "Antarctica",
        "Northern East Pacific Rise",
        "North of Honduras",
        "East of South Sandwich Islands",
        "Thailand",
        "Laos",
        "Cambodia",
        "Vietnam",
        "Gulf of Tongking",
        "Reykjanes Ridge",
        "Azores-Cape St. Vincent Ridge",
        "Owen Fracture Zone region",
        "Indian Ocean Triple Junction",
        "Western Indian-Antarctic Ridge",
        "Western Sahara",
        "Mauritania",
        "Mali",
        "Senegal - Gambia region",
        "Guinea region",
        "Sierra Leone",
        "Liberia region",
        "Cote d'Ivoire",
        "Burkina Faso",
        "Ghana",
        "Benin - Togo region",
        "Niger",
        "Nigeria",
        "Southeast of Easter Island",
        "Galapagos Triple Junction region",
    };
    if (number < 1 || number > 757) {
        strcpy(gregname, "UNKNOWN GEOGRAPHIC REGION");
        return 1;
    }
    strcpy(gregname, gregs[number - 1]);
    return 0;
}

/*
 *  Title:
 *     Sregion
 *  Synopsis:
 *     Returns seismic region name for a seismic region number.
 *  Input Arguments:
 *     srn - seismic region number
 *  Output Arguments:
 *     sregname - seismic geographic name
 *  Return:
 *     0/1 on success/error
 */
int Sregion(int number, char *sregname)
{
    static char *sregs[] = {
        "ALASKA - ALEUTIAN ARC", "E. ALASKA TO VANCOUVER ISLAND",
        "CALIFORNIA - NEVADA REGION", "BAJA CALIF. AND GULF OF CALIF.",
        "MEXICO - GUATEMALA AREA", "CENTRAL AMERICA", "CARIBBEAN LOOP",
        "ANDEAN SOUTH AMERICA", "EXTREME SOUTH AMERICA", "SOUTHERN ANTILLES",
        "NEW ZEALAND REGION", "KERMADEC - TONGA - SAMOA AREA",
        "FIJI ISLANDS AREA", "NEW HEBRIDES ISLANDS",
        "BISMARCK AND SOLOMON ISLANDS", "NEW GUINEA",
        "CAROLINE ISLANDS TO GUAM", "GUAM TO JAPAN",
        "JAPAN - KURILES - KAMCHATKA", "S.W. JAPAN AND RYUKYU ISLANDS",
        "TAIWAN", "PHILIPPINES", "BORNEO - CELEBES", "SUNDA ARC",
        "BURMA AND SOUTHEAST ASIA", "INDIA - TIBET - SZECHWAN - YUNAN",
        "SOUTHERN SINKIANG TO KANSU", "ALMA-ATA TO LAKE BAIKAL",
        "WESTERN ASIA", "MIDDLE EAST - CRIMEA - BALKANS",
        "WESTERN MEDITERRANEAN AREA", "ATLANTIC OCEAN", "INDIAN OCEAN",
        "EASTERN NORTH AMERICA", "EASTERN SOUTH AMERICA",
        "NORTHWESTERN EUROPE", "AFRICA", "AUSTRALIA", "PACIFIC BASIN",
        "ARCTIC ZONE", "EASTERN ASIA", "NE. ASIA,N. ALASKA TO GREENLAND",
        "S.E. AND ANTARCTIC PACIFIC", "GALAPAGOS AREA", "MACQUARIE LOOP",
        "ANDAMAN ISLANDS TO SUMATRA", "BALUCHISTAN", "HINDU KUSH AND PAMIR",
        "NORTHERN ASIA", "ANTARCTICA"
    };
    if (number < 1 || number > 50) {
        strcpy(sregname, "UNKNOWN SEISMIC REGION");
        return 1;
    }
    strcpy(sregname, sregs[number - 1]);
    return 0;
}

