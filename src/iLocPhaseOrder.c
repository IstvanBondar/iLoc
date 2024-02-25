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
 *    SortPhasesFromISF
 *    SortPhasesFromDatabase
 *    SortPhasesForNA
 */

/*
 *  Title:
 *     SortPhasesFromISF
 *  Synopsis:
 *     Sorts phase structures by increasing delta, prista, time, auth.
 *  Input Arguments:
 *     numPhase    - number of associated phases
 *     p[]        - array of phase structures.
 *  Called by:
 *     ReadISF
 */
void SortPhasesFromISF(int numPhase, PHAREC p[])
{
    int i, j;
    PHAREC temp;
    for (i = 1; i < numPhase; i++) {
        for (j = i - 1; j > -1; j--) {
            if (strcmp(p[j].auth, p[j+1].auth) > 0) {
                swap(p[j], p[j+1]);
            }
        }
    }
    for (i = 1; i < numPhase; i++) {
        for (j = i - 1; j > -1; j--) {
            if ((p[j].time > p[j+1].time && p[j+1].time != NULLVAL) ||
                 p[j].time == NULLVAL) {
                swap(p[j], p[j+1]);
            }
        }
    }
    for (i = 1; i < numPhase; i++) {
        for (j = i - 1; j > -1; j--) {
            if (strcmp(p[j].prista, p[j+1].prista) > 0) {
                swap(p[j], p[j+1]);
            }
        }
    }
    for (i = 1; i < numPhase; i++) {
        for (j = i - 1; j > -1; j--) {
            if (p[j].delta > p[j+1].delta) {
                swap(p[j], p[j+1]);
            }
        }
    }
}

/*
 *  Title:
 *     SortPhasesFromDatabase
 *  Synopsis:
 *     Sorts phase structures by increasing delta, prista, rdid, time.
 *  Input Arguments:
 *     numPhase    - number of associated phases
 *     p[]        - array of phase structures.
 *  Called by:
 *     GetISCPhases, GetSC3Phases, LocateEvent
 */
void SortPhasesFromDatabase(int numPhase, PHAREC p[])
{
    int i, j;
    PHAREC temp;
    for (i = 1; i < numPhase; i++) {
        for (j = i - 1; j > -1; j--) {
            if ((p[j].time > p[j+1].time && p[j+1].time != NULLVAL) ||
                 p[j].time == NULLVAL) {
                swap(p[j], p[j+1]);
            }
        }
    }
    for (i = 1; i < numPhase; i++) {
        for (j = i - 1; j > -1; j--) {
            if (p[j].rdid > p[j+1].rdid) {
                swap(p[j], p[j+1]);
            }
        }
    }
    for (i = 1; i < numPhase; i++) {
        for (j = i - 1; j > -1; j--) {
            if (strcmp(p[j].prista, p[j+1].prista) > 0) {
                swap(p[j], p[j+1]);
            }
        }
    }
    for (i = 1; i < numPhase; i++) {
        for (j = i - 1; j > -1; j--) {
            if (p[j].delta > p[j+1].delta) {
                swap(p[j], p[j+1]);
            }
        }
    }
}

/*
 *  Title:
 *     SortPhasesForNA
 *  Synopsis:
 *     Sort phase structures by increasing staorder, rdid, time.
 *     Ensures that phase records are ordered by the nearest-neighbour
 *     ordering, thus block-diagonalizing the data covariance matrix.
 *  Input Arguments:
 *     numPhase    - number of associated phases
 *     nsta       - number of distinct stations
 *     p[]        - array of phase structures.
 *     stalist[]  - array of starec structures
 *     staorder[] - array of staorder structures
 *  Called by:
 *     LocateEvent, NASearch
 *  Calls:
 *     GetStationIndex
 */
void SortPhasesForNA(int numPhase, int nsta, PHAREC p[], STAREC stalist[],
        STAORDER staorder[])
{
    int i, j, k, kp, m;
    PHAREC temp;
/*
 *  sort by arrival time
 */
    for (i = 1; i < numPhase; i++) {
        for (j = i - 1; j > -1; j--) {
            if ((p[j].time > p[j+1].time && p[j+1].time != NULLVAL) ||
                 p[j].time == NULLVAL) {
                swap(p[j], p[j+1]);
            }
        }
    }
/*
 *  sort by rdid
 */
    for (i = 1; i < numPhase; i++) {
        for (j = i - 1; j > -1; j--) {
            if (p[j].rdid > p[j+1].rdid) {
                swap(p[j], p[j+1]);
            }
        }
    }
/*
 *  sort by nearest-neighbour station order
 */
    for (i = 1; i < numPhase; i++) {
        for (j = i - 1; j > -1; j--) {
            m = GetStationIndex(nsta, stalist, p[j].prista);
            kp = staorder[m].index;
            m = GetStationIndex(nsta, stalist, p[j+1].prista);
            k = staorder[m].index;
            if (kp > k) {
                swap(p[j], p[j+1]);
            }
        }
    }
}

/*  EOF  */
