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
 *    SVDdecompose
 *    SVDsolve
 *    SVDModelCovarianceMatrix
 *    SVDthreshold
 *    SVDrank
 *    SVDnorm
 *    ProjectionMatrix
 */

/*
 * Local functions:
 *    SVDreorder
 *    Pythagorean
 *    Wmatrix for parallelisation
 *    EigenDecompose
 *    dlamch
 */
static int SVDreorder(int n, int m, double **u, double w[], double **v);
static double Pythagorean(double a, double b);
static int Wmatrix(PHASELIST *plist, double pct, double **cov, double **w,
        int nunp, char **phundef, int ispchange);
static int EigenDecompose(int nd, double *avec, double **u, double *sv);
static double dlamch(char CMACH);

#ifndef MACOSX
extern void dsyevr_(char *jobz, char *range, char *uplo, int *n, double *a,
        int *lda, double *vl, double *vu, int *il, int *iu, double *abstol,
        int *m, double* w, double *z, int *ldz, int *isuppz, double *work,
        int *lwork, int *iwork, int *liwork, int *info);
extern double dlamch_(char *CMACHp);
#endif

/*
 * Singular value decomposition of an (NxM) matrix
 *    A = U * SV * transpose(V)
 *    The matrix U(N x M) replaces A on output.
 *    The diagonal matrix of singular values SV is output as a vector SV(M).
 *    The matrix V (not the transpose) is output as V(M x M).
 *    Adopted from Numerical Recipes
 *
 *    Input arguments:
 *       n  - number of data (rows)
 *       m  - number of model parameters (columns)
 *       u  - A matrix to be decomposed U(N x M)
 *       sv - singular values SV(M), descending order
 *       v  - orthonormal V matrix V(M x M). V * transpose(V) = I
 *    Output arguments:
 *       u  - U matrix U(N x M) (orthonormal if N >= M)
 *    Returns:
 *       0/1 on success/error
 *    Called by:
 *       LocateEvent
 *    Calls:
 *       SVDreorder, Pythagorean
 */
int SVDdecompose(int n, int m, double **u, double sv[], double **v)
{
    int flag = 0;
    int i, its = 0, j, jj, k, l, nm;
    double anorm = 0., c = 0., f = 0., g = 0., h = 0., s = 0.;
    double scale = 0., x = 0., y = 0., z = 0.;
    double *rv1 = (double *)NULL;
    if ((rv1 = (double *)calloc(m, sizeof(double))) == NULL) {
        fprintf(logfp, "SVDdecompose: cannot allocate memory\n");
        fprintf(errfp, "SVDdecompose: cannot allocate memory\n");
        errorcode = 1;
        return 1;
    }
/*
 *  Householder reduction to bidiagonal form
 */
    g = scale = anorm = 0.0;
    for (i = 0; i < m; i++) {
        l = i + 1;
        rv1[i] = scale * g;
        g = s = scale = 0.0;
        if (i < n) {
            for (k = i; k < n; k++) scale += fabs(u[k][i]);
            if (scale != 0.0) {
                for (k = i; k < n; k++) {
                    u[k][i] /= scale;
                    s += u[k][i] * u[k][i];
                }
                f = u[i][i];
                g = -sign(sqrt(s), f);
                h = f * g - s;
                u[i][i] = f - g;
                for (j = l; j < m; j++) {
                    s = 0.0;
                    for (k = i; k < n; k++) s += u[k][i] * u[k][j];
                    f = s / h;
                    for (k = i; k < n; k++) u[k][j] += f * u[k][i];
                }
                for (k = i; k < n; k++) u[k][i] *= scale;
            }
        }
        sv[i] = scale * g;
        g = s = scale = 0.0;
        if (i < n && i != (m - 1)) {
            for (k = l; k < m; k++) scale += fabs(u[i][k]);
            if (scale != 0.0) {
                for (k = l; k < m; k++) {
                    u[i][k] /= scale;
                    s += u[i][k] * u[i][k];
                }
                f = u[i][l];
                g = -sign(sqrt(s), f);
                h = f * g - s;
                u[i][l] = f - g;
                for (k = l; k < m; k++) rv1[k] = u[i][k] / h;
                for (j = l; j < n; j++) {
                    s = 0.0;
                    for (k = l; k < m; k++) s += u[j][k] * u[i][k];
                    for (k = l; k < m; k++) u[j][k] += s * rv1[k];
                }
                for (k = l; k < m; k++) u[i][k] *= scale;
            }
        }
        anorm = max(anorm, (fabs(sv[i]) + fabs(rv1[i])));
    }
/*
 *  accumulation of right-hand transformations
 */
    for (i = m - 1; i >= 0; i--) {
        if (i < (m - 1)) {
            if (g != 0.0) {
                for (j = l; j < m; j++)
                    v[j][i] = (u[i][j] / u[i][l]) / g;
                for (j = l; j < m; j++) {
                    s = 0.0;
                    for (k = l; k < m; k++) s += u[i][k] * v[k][j];
                    for (k = l; k < m; k++) v[k][j] += s * v[k][i];
                }
            }
            for (j = l; j < m; j++) v[i][j] = v[j][i] = 0.0;
        }
        v[i][i] = 1.0;
        g = rv1[i];
        l = i;
    }
/*
 *  accumulation of left-hand transformations
 */
    for (i = min(m, n) - 1; i >= 0; i--) {
        l = i + 1;
        g = sv[i];
        for (j = l; j < m; j++) u[i][j] = 0.0;
        if (g != 0.0) {
            g = 1.0 / g;
            for (j = l; j < m; j++) {
                s = 0.0;
                for (k = l; k < n; k++) s += u[k][i] * u[k][j];
                f =(s / u[i][i]) * g;
                for (k = i; k < n; k++) u[k][j] += f * u[k][i];
            }
            for (j = i; j < n; j++) u[j][i] *= g;
        }
        else
            for (j = i; j < n; j++) u[j][i] = 0.0;
        u[i][i] += 1.;
    }
/*
 *  diagonalization of the bidiagonal form
 */
    for (k = m - 1; k >= 0; k--) {
        for (its = 0; its < 30; its++) {
            flag = 1;
            for (l = k; l >= 0; l--) {
                nm = l - 1;
                if (l == 0 || fabs(rv1[l]) < DEPSILON) {
                    flag = 0;
                    break;
                }
                if (fabs(sv[nm]) < DEPSILON) break;
            }
            if (flag) {
/*
 *              cancellation of rv1[l] if l greater than 0
 */
                c = 0.0;
                s = 1.0;
                for (i = l; i < k + 1; i++) {
                    f = s * rv1[i];
                    rv1[i] = c * rv1[i];
                    if (fabs(f) < DEPSILON) break;
                    g = sv[i];
                    h = Pythagorean(f, g);
                    sv[i] = h;
                    if (h > ZERO_TOL) {
                        h = 1.0 / h;
                        c = g * h;
                        s = -f * h;
                    }
                    for (j = 0; j < n; j++) {
                        y = u[j][nm];
                        z = u[j][i];
                        u[j][nm] = y * c + z * s;
                        u[j][i]  = z * c - y * s;
                    }
                }
            }
/*
 *          test for convergence
 */
            z = sv[k];
            if (l == k) {
                if (z < 0.0) {
                    sv[k] = -z;
                    for (j = 0; j < m; j++) v[j][k] = -v[j][k];
                }
                break;
            }
            if (its == 29) {
                Free(rv1);
                fprintf(logfp, "SVDdecompose: max iteration reached!\n");
                fprintf(errfp, "SVDdecompose: max iteration reached!\n");
                return 2;
            }
/*
 *          shift from bottom 2 by 2 minor
 */
            x = sv[l];
            nm = k - 1;
            y = sv[nm];
            g = rv1[nm];
            h = rv1[k];
            f = ((y - z) * (y + z) + (g - h) * (g + h)) / (2.0 * h * y);
            g = Pythagorean(f, 1.0);
            f = ((x - z) * (x + z) + h * ((y / (f + sign(g, f))) - h)) / x;
            c = s = 1.0;
/*
 *          next QR transformation
 */
            for (j = l; j <= nm; j++) {
                i = j + 1;
                g = rv1[i];
                y = sv[i];
                h = s * g;
                g = c * g;
                z = Pythagorean(f, h);
                rv1[j] = z;
                if (z > ZERO_TOL) {
                    z = 1.0 / z;
                    c = f * z;
                    s = h * z;
                }
                f = x * c + g * s;
                g = g * c - x * s;
                h = y * s;
                y *= c;
                for (jj = 0; jj < m; jj++) {
                    x = v[jj][j];
                    z = v[jj][i];
                    v[jj][j] = x * c + z * s;
                    v[jj][i] = z * c - x * s;
                }
                z = Pythagorean(f, h);
                sv[j] = z;
/*
 *              rotation can be arbitrary if z is zero
 */
                if (z > ZERO_TOL) {
                    z = 1.0 / z;
                    c = f * z;
                    s = h * z;
                }
                f = c * g + s * y;
                x = c * y - s * g;
                for (jj = 0; jj < n; jj++) {
                    y = u[jj][j];
                    z = u[jj][i];
                    u[jj][j] = y * c + z * s;
                    u[jj][i] = z * c - y * s;
                }
            }
            rv1[l] = 0.0;
            rv1[k] = f;
            sv[k] = x;
        }
    }
    Free(rv1);
    if (SVDreorder(n, m, u, sv, v))
        return 1;
    return 0;
}

/*
 * Order singular values
 *    Descending order of singular values and corresponding U and V matrices
 *    A = U * W * transpose(V)
 *    Adopted from Numerical Recipes
 *
 *    Input arguments:
 *       n  - number of data (rows)
 *       m  - number of model parameters (columns)
 *       u  - U matrix  U(N x M)
 *       w  - singular values SV(M), unordered
 *       v  - V matrix V(M x M)
 *    Output arguments:
 *       u  - U matrix U(N x M)
 *       w  - singular values SV(M), ordered
 *       v  - V matrix V(M x M)
 *    Returns:
 *       0/1 on success/error
 *    Called by:
 *       SVDdecompose
 */
static int SVDreorder(int n, int m, double **u, double w[], double **v)
{
    int i, j, k, s, inc = 1;
    double sw = 0.;
    double *su = (double *)NULL;
    double *sv = (double *)NULL;
    su = (double *)calloc(n, sizeof(double));
    if ((sv = (double *)calloc(m, sizeof(double))) == NULL) {
        Free(su);
        fprintf(logfp, "SVDreorder: cannot allocate memory\n");
        fprintf(errfp, "SVDreorder: cannot allocate memory\n");
        errorcode = 1;
        return 1;
    }
    do { inc *= 3; inc++; } while (inc <= m);
    do {
        inc /= 3;
        for (i = inc; i < m; i++) {
            sw = w[i];
            for (k = 0; k < n; k++) su[k] = u[k][i];
            for (k = 0; k < m; k++) sv[k] = v[k][i];
            j = i;
            while (w[j-inc] < sw) {
                w[j] = w[j-inc];
                for (k = 0; k < n; k++) u[k][j] = u[k][j-inc];
                for (k = 0; k < m; k++) v[k][j] = v[k][j-inc];
                j -= inc;
                if (j < inc) break;
            }
            w[j] = sw;
            for (k = 0; k < n; k++) u[k][j] = su[k];
            for (k = 0; k < m; k++) v[k][j] = sv[k];

        }
    } while (inc > 1);
    for (k = 0; k < m; k++) {
        s = 0;
        for (i = 0; i < n; i++) if (u[i][k] < 0.) s++;
        for (j = 0; j < m; j++) if (v[j][k] < 0.) s++;
        if (s > (m + n) / 2) {
            for (i = 0; i < n; i++) u[i][k] = -u[i][k];
            for (j = 0; j < m; j++) v[j][k] = -v[j][k];
        }
    }
    Free(sv);
    Free(su);
    return 0;
}

static double Pythagorean(double a, double b)
{
    double absa, absb;
    absa = fabs(a);
    absb = fabs(b);
    if (absa > absb)
        return absa * sqrt(1.0 + pow(absb / absa, 2));
    else if (absb < DEPSILON)
        return 0.;
    else
        return absb * sqrt(1.0 + pow(absa / absb, 2));
}

/*
 * Solve Ax = b with SVD
 *    Solve Ax = b for a vector x, where A = U * W * transpose(V)
 *    as returned by SVDdecompose and SVDreorder. N >= M is assumed.
 *    No input quantities are destroyed, so the routine may be called
 *    sequentially with different b's.
 *
 *    Input arguments:
 *       n  - number of data (rows)
 *       m  - number of model parameters (columns)
 *       u  - U matrix U(N x M)
 *       sv - singular values SV(M)
 *       v  - V matrix V(M x M)
 *       b  - b vector b(N)
 *       thres - threshold to zero out singular values
 *               if thres is negative a default value based on
 *               estimated roundoff is used
 *    Output arguments:
 *       x - x vector x(M)
 *    Returns:
 *       0/1 on success/error
 *    Called by:
 *       LocateEvent
 *    Calls:
 *       SVDthreshold
 */
int SVDsolve(int n, int m, double **u, double sv[], double **v,
              double *b, double *x, double thres)
{
    int i, j, jj;
    double s = 0., tsh = 0.;
    double *tmp = (double *)NULL;
    if ((tmp = (double *)calloc(m, sizeof(double))) == NULL) {
        fprintf(logfp, "SVDsolve: cannot allocate memory\n");
        fprintf(errfp, "SVDsolve: cannot allocate memory\n");
        errorcode = 1;
        return 1;
    }
    tsh = (thres >= 0.) ? thres : SVDthreshold(n, m, sv);
    for (j = 0; j < m; j++) {
        s = 0.0;
        if (sv[j] > tsh) {
            for (i = 0; i < n; i++) s += u[i][j] * b[i];
            s /= sv[j];
        }
        tmp[j] = s;
    }
    for (j = 0; j < m; j++) {
        s = 0.0;
        for (jj = 0; jj < m; jj++) s += v[j][jj] * tmp[jj];
        x[j] = s;
    }
    Free(tmp);
    return 0;
}

/*
 * Calculates a posteriori model covariance matrix
 *    ModCov(M x M) = V * (1 / SV^2) * transpose(V) =
 *                    Ginv * DataCov * transpose(Ginv)
 *    Ginv = V * 1/SV * transpose(U)
 *
 *    Input arguments:
 *       m     - number of model parameters
 *       thres - threshold to zero out singular values
 *       sv    - singular values SV(M)
 *       v     - V matrix V(M x M)
 *    Output arguments:
 *       mcov  - model covariance matrix MCOV(M x M)
 *    Called by:
 *       LocateEvent
 */
void SVDModelCovarianceMatrix(int m, double thres, double sv[], double **v,
                                 double mcov[][4])
{
    int i, j, k;
    double s = 0.;
    for (i = 0; i < m; i++) {
        for (j = 0; j < i + 1; j++) {
            mcov[i][j] = mcov[j][i] = 0.0;
            for (s = 0., k = 0; k < m; k++) {
                if (sv[k] > thres)
                    s += v[i][k] * v[j][k] / (sv[k] * sv[k]);
            }
            mcov[j][i] = mcov[i][j] = s;
        }
    }
    if (verbose > 2) {
        fprintf(logfp, "        SV(%d) :\n        ", m);
        for (i = 0; i < m; i++) fprintf(logfp, "%12.5f ", sv[i]);
        fprintf(logfp, "\n        V(%d x %d) matrix:\n", m, m);
        for (i = 0; i < m; i++) {
            fprintf(logfp, "        ");
            for (j = 0; j < m; j++) fprintf(logfp, "%12.5f ", v[i][j]);
            fprintf(logfp, "\n");
        }
        fprintf(logfp, "        MCOV(%d x %d) matrix:\n", m, m);
        for (i = 0; i < m; i++) {
            fprintf(logfp, "        ");
            for (j = 0; j < m; j++) fprintf(logfp, "%12.5f ", mcov[i][j]);
            fprintf(logfp, "\n");
        }
    }
}

/*
 * Get default threshold to zero out singular values
 *    Input arguments:
 *       n  - number of data (rows)
 *       m  - number of model parameters (columns)
 *       sv - singular values SV(M)
 *    Returns:
 *       threshold
 *    Called by:
 *       LocateEvent, SVDsolve, SVDrank
 */
double SVDthreshold(int n, int m, double sv[])
{
    return 0.5 * sqrt(n + m + 1.) * sv[0] * DEPSILON;
}


/*
 * Rank of A(N x M) after zeroing out singular values less than a threshold
 *    if thres is negative a default value based on estimated roundoff is used
 *    Input arguments:
 *       n  - number of data (rows)
 *       m  - number of model parameters (columns)
 *       sv - singular values SV(M)
 *       thres - threshold to zero out singular values
 *               if thres is negative a default value based on
 *               estimated roundoff is used
 *    Returns:
 *       rank
 *    Called by:
 *       LocateEvent
 *    Calls:
 *       SVDthreshold
 */
int SVDrank(int n, int m, double sv[], double thres)
{
    int j, nr = 0;
    double tsh = 0.;
    tsh = (thres >= 0.) ? thres : SVDthreshold(n, m, sv);
    for (j = 0; j < m; j++) if (sv[j] > tsh) nr++;
    return nr;
}

/*
 * Condition number and G matrix norm
 *    Input arguments:
 *       m     - number of model parameters )M)
 *       sv    - singular values SV(M)
 *       thres - threshold to zero out singular values
 *    Output arguments:
 *       cond   - condition number, largest / smallest singular value used
 *       isvmax - index of largest singular value
 *    Returns:
 *       G matrix norm = sum of squares of singular values
 *    Called by:
 *       LocateEvent
 */
double SVDnorm(int m, double sv[], double thres, double *cond)
{
    double norm = 0., cnum = 0.;
    int i;
    for (i = 0; i < m; i++) {
        if (sv[i] <= thres) break;
        norm += sv[i] * sv[i];
    }
    if (i == m) i--;
    cnum = (sv[0] <= 0. || sv[i] <= 0.) ? 99999. : sv[0] / sv[i];
    *cond = cnum;
    return norm;
}

/*
 *  Projection matrix to project Gm = d into eigen system: WGm = Wd
 *
 *     Bondár, I., and K. McLaughlin, 2009,
 *        Seismic location bias and uncertainty in the presence of correlated
 *        and non-Gaussian travel-time errors,
 *        Bull. Seism. Soc. Am., 99, 172-193.
 *     Bondár, I., and D. Storchak, 2011,
 *        Improved location procedures at the International Seismological
 *        Centre,
 *        Geophys. J. Int., doi: 10.1111/j.1365-246X.2011.05107.x.
 *
 *    W(N x N) = 1 / sqrt(SV) * transpose(U) = Binv
 *           B = U * sqrt(SV)
 *           C = B * transpose(B) = U * SV * transpose(V)
 *        Cinv = transpose(W) * W = V * 1/SV * transpose(U)
 *
 *    Since different phases travel along different ray paths,
 *    they are uncorrelated. This makes the data covariance matrix
 *    block-diagonal (when ordered by phases). Furthermore, if the
 *    observations are ordered by the nearest-neighbour station order
 *    the phase blocks themselves exhibit a block-diagonal structure.
 *    To improve efficiency and speed, the data covariance matrix is inverted
 *    block by block instead of doing one monster inversion.
 *
 *    Input arguments:
 *       numPhase   - number of associated phases
 *       p         - array of phase structures
 *       n         - number of defining phases
 *       pctvar    - percentage of total variance to be explained
 *       cov       - data covariance matrix C(N x N)
 *       nunp      - number of distinct phases made non-defining
 *       phundef   - list of distinct phases made non-defining
 *       ispchange - was there a change in phase names?
 *    Output arguments:
 *       prank     - rank of G matrix at pctvar level
 *       w         - projection matrix (N x N)
 *    Returns:
 *       0/1 on success/error
 *    Called by:
 *       LocateEvent
 *    Calls:
 *       GetPhaseList, FreePhaseList, Wmatrix
 */
int ProjectionMatrix(int numPhase, PHAREC p[], int nd, double pctvar,
                      double **cov, double **w, int *prank, int nunp,
                      char **phundef, int ispchange)
{
    int i, j, knull = 0, nphases = 0;
    PHASELIST plist[MAXTTPHA];
    PHASELIST *plistp = plist;
    double sum = 0., pct = 0.;
/*
 *  populate plist structure
 */
    if ((nphases = GetPhaseList(numPhase, p, plist)) == 0)
        return 1;
    pct = pctvar / 100.;
/*
 *  calculate projection matrix
 */
#ifdef GCD
/*
 *  use GCD (Mac OS) to parallelize the calculation of projection matrix
 *  each phase block of data covariance matrix is processed concurrently
 */
    if (nphases > 500) {
        if (verbose > 3) fprintf(logfp, "ProjectionMatrix: GCD is on\n");
        dispatch_apply(nphases, dispatch_get_global_queue(0, 0),
                       ^(size_t j){Wmatrix((plistp + j), pct, cov, w,
                                   nunp, phundef, ispchange); });
    }
    else {
        for (j = 0; j < nphases; j++) {
            Wmatrix((plistp + j), pct, cov, w, nunp, phundef, ispchange);
        }
    }
#else
/*
 *  single core
 */
    for (j = 0; j < nphases; j++) {
        Wmatrix((plistp + j), pct, cov, w, nunp, phundef, ispchange);
    }
#endif
    FreePhaseList(nphases, plist);
    if (errorcode)
        return 1;
/*
 *  calculate effective rank from projection matrix
 */
    if (verbose > 2) fprintf(logfp, "        Projection matrix and row sums\n");
    knull = 0;
    for (i = 0; i < nd; i++) {
        if (verbose > 2) fprintf(logfp, "          %4d ", i);
        sum = 0.;
        for (j = 0; j < nd; j++) {
            sum += w[i][j];
            if (verbose > 2) fprintf(logfp, "%13.4f ", w[i][j]);
        }
        if (fabs(sum) < 1.e-5) knull++;
        if (verbose > 2) fprintf(logfp, " | %12.4f %d\n", sum, knull);
    }
    if (verbose > 1) {
        fprintf(logfp, "    Projection matrix W(%d x %d):\n", nd, nd);
        fprintf(logfp, "      %d observations are projected ", knull);
        fprintf(logfp, "to the null space (rank = %d)\n", nd - knull);
    }
    *prank = nd - knull;
    return 0;
}

/*
 * Calculate the projection matrix W for a phase block
 *        W = 1 / sqrt(SV) * transpose(U)
 *    Exploits the block-diagonal structure of a phase block
 *    by inverting the covariance matrix block by block.
 *    Uses Lapack routines to obtain the eigenvalue decomposition
 *    of the symmetric, positive semi-definite covariance matrix.
 *    Input arguments:
 *       plist     - PHASELIST structure for a phase
 *       pct       - percentage of total variance to be explained
 *       cov       - data covariance matrix C(N x N)
 *       w         - projection matrix (N x N)
 *       nunp      - number of distinct phases made non-defining
 *       phundef   - list of distinct phases made non-defining
 *       ispchange - was there a change in phase names?
 *    Output arguments:
 *       prank     - rank of G matrix at pctvar level
 *       w         - projection matrix (N x N)
 *    Returns:
 *       0/1 on success/error
 *    Called by:
 *       ProjectionMatrix
 *    Calls:
 *       AllocateFloatMatrix, FreeFloatMatrix, EigenDecompose, SVDthreshold
 */
static int Wmatrix(PHASELIST *plist, double pct, double **cov, double **w,
        int nunp, char **phundef, int ispchange)
{
    int i, k, m, np = 0, mp = 0, ii, jj;
    int knull = 0, nr = 0, isfound = 0;
    double sum = 0., esum = 0., psum = 0., ths = 0., x = 0.;
    double **u = (double **)NULL;
    double **z = (double **)NULL;
    double *sv = (double *)NULL;
    double *avec = (double *)NULL;
/*
 *  deal only with those phases that were made non-defining
 *  if phase name changes have occured, build W from scratch
 */
    isfound = 0;
    for (i = 0; i < nunp; i++)
        if (streq(phundef[i], plist->phase))
            isfound = 1;
    if (isfound == 0 && ispchange == 0)
        return 0;
/*
 *  number of time observations for this phase
 */
    np = plist->nTime;
    if (np) {
#ifdef SERIAL
        if (verbose > 3) {
            fprintf(logfp, "        Correlated errors: ");
            fprintf(logfp, "phase %s, %d time observations\n",
                    plist->phase, plist->nTime);
        }
#endif
/*
 *      only a single time observation for this phase
 */
        if (np == 1) {

            ii = plist->indTime[0];
            w[ii][ii] = 0.;
            if (cov[ii][ii] > ZERO_TOL)
                w[ii][ii] = 1. / sqrt(cov[ii][ii]);
#ifdef SERIAL
            if (verbose > 4) {
                fprintf(logfp, "          Covariance matrix C(%d x %d):\n", np, np);
                fprintf(logfp, "          %4d %12.4f\n", ii, cov[ii][ii]);
                fprintf(logfp, "          Projection matrix W(%d x %d):\n", np, np);
                fprintf(logfp, "               %12.4f\n", w[ii][ii]);
            }
#endif
        }
/*
 *      multiple time observations
 */
        else {
            if ((z = AllocateFloatMatrix(np, np)) == NULL) {
                fprintf(logfp, "Wmatrix: cannot allocate memory\n");
                fprintf(errfp, "wn_matrix: cannot allocate memory\n");
                errorcode = 1;
                return 1;
            }
/*
 *          build covariance block for this phase
 */
            for (k = 0; k < np; k++) {
                ii = plist->indTime[k];
                z[k][k] = cov[ii][ii];
                for (m = 0; m < k; m++) {
                    jj = plist->indTime[m];
                    z[k][m] = cov[ii][jj];
                    z[m][k] = cov[jj][ii];
                }
            }
/*
 *          find diagonal sub-blocks in the covariance block for this phase
 */
            for (k = 0; k < np; k++) {
                mp = np - 1;
                while (z[k][mp] < ZERO_TOL) mp--;
/*
 *              only a single observation in this sub-block
 */
                if (mp == k) {
                    ii = plist->indTime[k];
                    w[ii][ii] = 0.;
                    if (z[k][k] > ZERO_TOL)
                        w[ii][ii] = 1. / sqrt(z[k][k]);
#ifdef SERIAL
                    if (verbose > 4) {
                        fprintf(logfp, "          Covariance matrix C(1 x 1):\n");
                        fprintf(logfp, "          %4d %12.4f\n", ii, z[k][k]);
                        fprintf(logfp, "          Projection matrix W(1 x 1):\n");
                        fprintf(logfp, "               %12.4f\n", w[ii][ii]);
                    }
#endif
                    continue;
                }
/*
 *              multiple observations in this sub-block
 */
                else {
                    for (i = k + 1; i < mp; i++) {
                        m = np - 1;
                        while (z[i][m] < ZERO_TOL) m--;
                        if (m > mp) mp = m;
                    }
                    mp = mp - k + 1;
                    u = AllocateFloatMatrix(mp, mp);
                    avec = (double *)calloc(mp * mp, sizeof(double));
                    if ((sv = (double *)calloc(mp, sizeof(double))) == NULL) {
                        fprintf(logfp, "Wmatrix: cannot allocate memory\n");
                        fprintf(errfp, "Wmatrix: cannot allocate memory\n");
                        FreeFloatMatrix(u); FreeFloatMatrix(z);
                        Free(avec);
                        errorcode = 1;
                        return 1;
                    }
/*
 *                  copy the covariance matrix of this sub-block into avec
 */
#ifdef SERIAL
                    if (verbose > 4)
                        fprintf(logfp, "      Covariance matrix C(%d x %d):\n",
                                mp, mp);
#endif
                    for (ii = 0, i = k; ii < mp; ii++, i++) {
#ifdef SERIAL
                        if (verbose > 4)
                            fprintf(logfp, "      %4d", plist->indTime[i]);
#endif
                        for (m = 0, jj = k; m < mp; m++, jj++) {
                            avec[ii + m * mp] = z[i][jj];
#ifdef SERIAL
                            if (verbose > 4) fprintf(logfp, "%12.4f ", z[i][jj]);
#endif
                        }
#ifdef SERIAL
                        if (verbose > 4) fprintf(logfp, "\n");
#endif
                    }
/*
 *                  Eigenvalue decomposition
 */
                    if (EigenDecompose(mp, avec, u, sv)) {
                        FreeFloatMatrix(u); FreeFloatMatrix(z);
                        Free(avec); Free(sv);
                        errorcode = 1;
                        return 1;
                    }
                    ths = SVDthreshold(mp, mp, sv);
/*
 *                  get effective rank that explains
 *                  pct percent of total variance
 */
                    for (esum = 0., m = 0; m < mp; m++) {
                        if (sv[m] <= ths) break;
                            esum += sv[m];
                    }
                    nr = m;
                    for (psum = 0., i = 0; i < nr; i++) {
                        psum += sv[i] / esum;
                        if (psum > pct) break;
                    }
                    m = min(i, nr - 1);
                    ths = sv[m];
/*
 *                  projection matrix:
 *                      W(N x N) = (1 / sqrt(SV) * transpose(U)
 *
 *                  a zero rowsum in W indicates the projection of perfectly
 *                  correlated observations to the null space
 */
#ifdef SERIAL
                    if (verbose > 3)
                        fprintf(logfp, "          Projection matrix W(%d x %d):\n",
                                mp, mp);
#endif
                    for (knull = 0, m = 0; m < mp; m++) {
                        ii = plist->indTime[m+k];
                        sum = 0.;
#ifdef SERIAL
                        if (verbose > 3) fprintf(logfp, "          %4d", ii);
#endif
                        for (i = 0; i < mp; i++) {
                            jj = plist->indTime[i+k];
                            x = 0.;
                            if (sv[m] >= ths) {
                                x = u[i][m] / Sqrt(sv[m]);
                                if (fabs(x) < ZERO_TOL) x = 0.;
                                sum += x;
                            }
                            w[ii][jj] = x;
#ifdef SERIAL
                            if (verbose > 3) fprintf(logfp, "%12.4f ", x);
#endif
                        }
                        if (fabs(sum) < 1.e-5) knull++;
#ifdef SERIAL
                        if (verbose > 3) fprintf(logfp, "| %12.4f\n", sum);
#endif
                    }
#ifdef SERIAL
                    if (verbose > 3) {
                        if (knull) {
                            fprintf(logfp, "          %d observations are ", knull);
                            fprintf(logfp, "projected to the null space\n");
                        }
                        fprintf(logfp, "          Eigenvalue spectrum:\n");
                        fprintf(logfp, "          threshold = %.3f rank = %d\n",
                                ths, mp - knull);
                        fprintf(logfp, "          ");
                        for (m = 0; m < mp; m++)
                            fprintf(logfp, "%10.3f ", sv[m]);
                        fprintf(logfp, "\n          ");
                        for (psum = 0., m = 0; m < nr; m++) {
                            psum += 100. * sv[m] / esum;
                            fprintf(logfp, "   %7.3f ", psum);
                        }
                        for (m = nr; m < mp; m++)
                            fprintf(logfp, "   100.0   ");
                        fprintf(logfp, "\n");
                    }
#endif
                    FreeFloatMatrix(u);
                    Free(avec); Free(sv);
                }
                k += mp - 1;
            }
            FreeFloatMatrix(z);
        }
    }
/*
 *  number of azimuth observations for this phase
 */
    np = plist->nAzim;
    if (np) {
#ifdef SERIAL
        if (verbose > 3) {
            fprintf(logfp, "        Correlated errors: ");
            fprintf(logfp, "phase %s, %d azimuth observations\n",
                    plist->phase, plist->nAzim);
        }
#endif
/*
 *      only a single azimuth observation for this phase
 */
        if (np == 1) {

            ii = plist->indAzim[0];
            w[ii][ii] = 0.;
            if (cov[ii][ii] > ZERO_TOL)
                w[ii][ii] = 1. / sqrt(cov[ii][ii]);
#ifdef SERIAL
            if (verbose > 4) {
                fprintf(logfp, "          Covariance matrix C(%d x %d):\n", np, np);
                fprintf(logfp, "          %4d %12.4f\n", ii, cov[ii][ii]);
                fprintf(logfp, "          Projection matrix W(%d x %d):\n", np, np);
                fprintf(logfp, "               %12.4f\n", w[ii][ii]);
            }
#endif
        }
/*
 *      multiple azimuth observations
 */
        else {
            if ((z = AllocateFloatMatrix(np, np)) == NULL) {
                fprintf(logfp, "Wmatrix: cannot allocate memory\n");
                fprintf(errfp, "wn_matrix: cannot allocate memory\n");
                errorcode = 1;
                return 1;
            }
/*
 *          build covariance block for this phase
 */
            for (k = 0; k < np; k++) {
                ii = plist->indAzim[k];
                z[k][k] = cov[ii][ii];
                for (m = 0; m < k; m++) {
                    jj = plist->indAzim[m];
                    z[k][m] = cov[ii][jj];
                    z[m][k] = cov[jj][ii];
                }
            }
/*
 *          find diagonal sub-blocks in the covariance block for this phase
 */
            for (k = 0; k < np; k++) {
                mp = np - 1;
                while (z[k][mp] < ZERO_TOL) mp--;
/*
 *              only a single observation in this sub-block
 */
                if (mp == k) {
                    ii = plist->indAzim[k];
                    w[ii][ii] = 0.;
                    if (z[k][k] > ZERO_TOL)
                        w[ii][ii] = 1. / sqrt(z[k][k]);
#ifdef SERIAL
                    if (verbose > 4) {
                        fprintf(logfp, "          Covariance matrix C(1 x 1):\n");
                        fprintf(logfp, "          %4d %12.4f\n", ii, z[k][k]);
                        fprintf(logfp, "          Projection matrix W(1 x 1):\n");
                        fprintf(logfp, "               %12.4f\n", w[ii][ii]);
                    }
#endif
                    continue;
                }
/*
 *              multiple observations in this sub-block
 */
                else {
                    for (i = k + 1; i < mp; i++) {
                        m = np - 1;
                        while (z[i][m] < ZERO_TOL) m--;
                        if (m > mp) mp = m;
                    }
                    mp = mp - k + 1;
                    u = AllocateFloatMatrix(mp, mp);
                    avec = (double *)calloc(mp * mp, sizeof(double));
                    if ((sv = (double *)calloc(mp, sizeof(double))) == NULL) {
                        fprintf(logfp, "Wmatrix: cannot allocate memory\n");
                        fprintf(errfp, "Wmatrix: cannot allocate memory\n");
                        FreeFloatMatrix(u); FreeFloatMatrix(z);
                        Free(avec);
                        errorcode = 1;
                        return 1;
                    }
/*
 *                  copy the covariance matrix of this sub-block into avec
 */
#ifdef SERIAL
                    if (verbose > 4)
                        fprintf(logfp, "      Covariance matrix C(%d x %d):\n",
                                mp, mp);
#endif
                    for (ii = 0, i = k; ii < mp; ii++, i++) {
#ifdef SERIAL
                        if (verbose > 4)
                            fprintf(logfp, "      %4d", plist->indAzim[i]);
#endif
                        for (m = 0, jj = k; m < mp; m++, jj++) {
                            avec[ii + m * mp] = z[i][jj];
#ifdef SERIAL
                            if (verbose > 4) fprintf(logfp, "%12.4f ", z[i][jj]);
#endif
                        }
#ifdef SERIAL
                        if (verbose > 4) fprintf(logfp, "\n");
#endif
                    }
/*
 *                  Eigenvalue decomposition
 */
                    if (EigenDecompose(mp, avec, u, sv)) {
                        FreeFloatMatrix(u); FreeFloatMatrix(z);
                        Free(avec); Free(sv);
                        errorcode = 1;
                        return 1;
                    }
                    ths = SVDthreshold(mp, mp, sv);
/*
 *                  get effective rank that explains
 *                  pct percent of total variance
 */
                    for (esum = 0., m = 0; m < mp; m++) {
                        if (sv[m] <= ths) break;
                            esum += sv[m];
                    }
                    nr = m;
                    for (psum = 0., i = 0; i < nr; i++) {
                        psum += sv[i] / esum;
                        if (psum > pct) break;
                    }
                    m = min(i, nr - 1);
                    ths = sv[m];
/*
 *                  projection matrix:
 *                      W(N x N) = (1 / sqrt(SV) * transpose(U)
 *
 *                  a zero rowsum in W indicates the projection of perfectly
 *                  correlated observations to the null space
 */
#ifdef SERIAL
                    if (verbose > 3)
                        fprintf(logfp, "          Projection matrix W(%d x %d):\n",
                                mp, mp);
#endif
                    for (knull = 0, m = 0; m < mp; m++) {
                        ii = plist->indAzim[m+k];
                        sum = 0.;
#ifdef SERIAL
                        if (verbose > 3) fprintf(logfp, "          %4d", ii);
#endif
                        for (i = 0; i < mp; i++) {
                            jj = plist->indAzim[i+k];
                            x = 0.;
                            if (sv[m] >= ths) {
                                x = u[i][m] / Sqrt(sv[m]);
                                if (fabs(x) < ZERO_TOL) x = 0.;
                                sum += x;
                            }
                            w[ii][jj] = x;
#ifdef SERIAL
                            if (verbose > 3) fprintf(logfp, "%12.4f ", x);
#endif
                        }
                        if (fabs(sum) < 1.e-5) knull++;
#ifdef SERIAL
                        if (verbose > 3) fprintf(logfp, "| %12.4f\n", sum);
#endif
                    }
#ifdef SERIAL
                    if (verbose > 3) {
                        if (knull) {
                            fprintf(logfp, "          %d observations are ", knull);
                            fprintf(logfp, "projected to the null space\n");
                        }
                        fprintf(logfp, "          Eigenvalue spectrum:\n");
                        fprintf(logfp, "          threshold = %.3f rank = %d\n",
                                ths, mp - knull);
                        fprintf(logfp, "          ");
                        for (m = 0; m < mp; m++)
                            fprintf(logfp, "%10.3f ", sv[m]);
                        fprintf(logfp, "\n          ");
                        for (psum = 0., m = 0; m < nr; m++) {
                            psum += 100. * sv[m] / esum;
                            fprintf(logfp, "   %7.3f ", psum);
                        }
                        for (m = nr; m < mp; m++)
                            fprintf(logfp, "   100.0   ");
                        fprintf(logfp, "\n");
                    }
#endif
                    FreeFloatMatrix(u);
                    Free(avec); Free(sv);
                }
                k += mp - 1;
            }
            FreeFloatMatrix(z);
        }
    }
/*
 *  number of slowness observations for this phase
 */
    np = plist->nSlow;
    if (np) {
#ifdef SERIAL
        if (verbose > 3) {
            fprintf(logfp, "        Correlated errors: ");
            fprintf(logfp, "phase %s, %d slowness observations\n",
                    plist->phase, plist->nSlow);
        }
#endif
/*
 *      only a single slowness observation for this phase
 */
        if (np == 1) {

            ii = plist->indSlow[0];
            w[ii][ii] = 0.;
            if (cov[ii][ii] > ZERO_TOL)
                w[ii][ii] = 1. / sqrt(cov[ii][ii]);
#ifdef SERIAL
            if (verbose > 4) {
                fprintf(logfp, "          Covariance matrix C(%d x %d):\n", np, np);
                fprintf(logfp, "          %4d %12.4f\n", ii, cov[ii][ii]);
                fprintf(logfp, "          Projection matrix W(%d x %d):\n", np, np);
                fprintf(logfp, "               %12.4f\n", w[ii][ii]);
            }
#endif
        }
/*
 *      multiple slowness observations
 */
        else {
            if ((z = AllocateFloatMatrix(np, np)) == NULL) {
                fprintf(logfp, "Wmatrix: cannot allocate memory\n");
                fprintf(errfp, "wn_matrix: cannot allocate memory\n");
                errorcode = 1;
                return 1;
            }
/*
 *          build covariance block for this phase
 */
            for (k = 0; k < np; k++) {
                ii = plist->indSlow[k];
                z[k][k] = cov[ii][ii];
                for (m = 0; m < k; m++) {
                    jj = plist->indSlow[m];
                    z[k][m] = cov[ii][jj];
                    z[m][k] = cov[jj][ii];
                }
            }
/*
 *          find diagonal sub-blocks in the covariance block for this phase
 */
            for (k = 0; k < np; k++) {
                mp = np - 1;
                while (z[k][mp] < ZERO_TOL) mp--;
/*
 *              only a single observation in this sub-block
 */
                if (mp == k) {
                    ii = plist->indSlow[k];
                    w[ii][ii] = 0.;
                    if (z[k][k] > ZERO_TOL)
                        w[ii][ii] = 1. / sqrt(z[k][k]);
#ifdef SERIAL
                    if (verbose > 4) {
                        fprintf(logfp, "          Covariance matrix C(1 x 1):\n");
                        fprintf(logfp, "          %4d %12.4f\n", ii, z[k][k]);
                        fprintf(logfp, "          Projection matrix W(1 x 1):\n");
                        fprintf(logfp, "               %12.4f\n", w[ii][ii]);
                    }
#endif
                    continue;
                }
/*
 *              multiple observations in this sub-block
 */
                else {
                    for (i = k + 1; i < mp; i++) {
                        m = np - 1;
                        while (z[i][m] < ZERO_TOL) m--;
                        if (m > mp) mp = m;
                    }
                    mp = mp - k + 1;
                    u = AllocateFloatMatrix(mp, mp);
                    avec = (double *)calloc(mp * mp, sizeof(double));
                    if ((sv = (double *)calloc(mp, sizeof(double))) == NULL) {
                        fprintf(logfp, "Wmatrix: cannot allocate memory\n");
                        fprintf(errfp, "Wmatrix: cannot allocate memory\n");
                        FreeFloatMatrix(u); FreeFloatMatrix(z);
                        Free(avec);
                        errorcode = 1;
                        return 1;
                    }
/*
 *                  copy the covariance matrix of this sub-block into avec
 */
#ifdef SERIAL
                    if (verbose > 4)
                        fprintf(logfp, "      Covariance matrix C(%d x %d):\n",
                                mp, mp);
#endif
                    for (ii = 0, i = k; ii < mp; ii++, i++) {
#ifdef SERIAL
                        if (verbose > 4)
                            fprintf(logfp, "      %4d", plist->indSlow[i]);
#endif
                        for (m = 0, jj = k; m < mp; m++, jj++) {
                            avec[ii + m * mp] = z[i][jj];
#ifdef SERIAL
                            if (verbose > 4) fprintf(logfp, "%12.4f ", z[i][jj]);
#endif
                        }
#ifdef SERIAL
                        if (verbose > 4) fprintf(logfp, "\n");
#endif
                    }
/*
 *                  Eigenvalue decomposition
 */
                    if (EigenDecompose(mp, avec, u, sv)) {
                        FreeFloatMatrix(u); FreeFloatMatrix(z);
                        Free(avec); Free(sv);
                        errorcode = 1;
                        return 1;
                    }
                    ths = SVDthreshold(mp, mp, sv);
/*
 *                  get effective rank that explains
 *                  pct percent of total variance
 */
                    for (esum = 0., m = 0; m < mp; m++) {
                        if (sv[m] <= ths) break;
                            esum += sv[m];
                    }
                    nr = m;
                    for (psum = 0., i = 0; i < nr; i++) {
                        psum += sv[i] / esum;
                        if (psum > pct) break;
                    }
                    m = min(i, nr - 1);
                    ths = sv[m];
/*
 *                  projection matrix:
 *                      W(N x N) = (1 / sqrt(SV) * transpose(U)
 *
 *                  a zero rowsum in W indicates the projection of perfectly
 *                  correlated observations to the null space
 */
#ifdef SERIAL
                    if (verbose > 3)
                        fprintf(logfp, "          Projection matrix W(%d x %d):\n",
                                mp, mp);
#endif
                    for (knull = 0, m = 0; m < mp; m++) {
                        ii = plist->indSlow[m+k];
                        sum = 0.;
#ifdef SERIAL
                        if (verbose > 3) fprintf(logfp, "          %4d", ii);
#endif
                        for (i = 0; i < mp; i++) {
                            jj = plist->indSlow[i+k];
                            x = 0.;
                            if (sv[m] >= ths) {
                                x = u[i][m] / Sqrt(sv[m]);
                                if (fabs(x) < ZERO_TOL) x = 0.;
                                sum += x;
                            }
                            w[ii][jj] = x;
#ifdef SERIAL
                            if (verbose > 3) fprintf(logfp, "%12.4f ", x);
#endif
                        }
                        if (fabs(sum) < 1.e-5) knull++;
#ifdef SERIAL
                        if (verbose > 3) fprintf(logfp, "| %12.4f\n", sum);
#endif
                    }
#ifdef SERIAL
                    if (verbose > 3) {
                        if (knull) {
                            fprintf(logfp, "          %d observations are ", knull);
                            fprintf(logfp, "projected to the null space\n");
                        }
                        fprintf(logfp, "          Eigenvalue spectrum:\n");
                        fprintf(logfp, "          threshold = %.3f rank = %d\n",
                                ths, mp - knull);
                        fprintf(logfp, "          ");
                        for (m = 0; m < mp; m++)
                            fprintf(logfp, "%10.3f ", sv[m]);
                        fprintf(logfp, "\n          ");
                        for (psum = 0., m = 0; m < nr; m++) {
                            psum += 100. * sv[m] / esum;
                            fprintf(logfp, "   %7.3f ", psum);
                        }
                        for (m = nr; m < mp; m++)
                            fprintf(logfp, "   100.0   ");
                        fprintf(logfp, "\n");
                    }
#endif
                    FreeFloatMatrix(u);
                    Free(avec); Free(sv);
                }
                k += mp - 1;
            }
            FreeFloatMatrix(z);
        }
    }
    return 0;
}

/*
 * Calculate the eigenvalues and eigenvectors of an NxN symmetric matrix A.
 *        A = U * SV * transpose(U)
 *    The A matrix has to be in Fortran vector format (column order).
 *    Uses the Lapack dsyevr routine to obtain the eigenvalue decomposition
 *    of the symmetric, positive semi-definite covariance matrix.
 *    Input arguments:
 *       nd   - number of data
 *       avec - A matrix in Fortran vector format
 *    Output arguments:
 *       u    - eigenvector matrix (N x N)
 *       sv   - eigenvalue vector in descending order
 *    Returns:
 *       0/1 on success/error
 *    Called by:
 *       Wmatrix
 *    Calls:
 *       dlamch, dsyevr_
 */
static int EigenDecompose(int nd, double *avec, double **u, double *sv)
{
    double *work = (double *)NULL;
    double *uvec = (double *)NULL;
    int *isuppz = (int *)NULL;
    int *iwork = (int *)NULL;
    int n = nd, lda = nd, ldz = nd, m = nd, il = 0, iu = 0;
    int info, lwork = -1, liwork = -1, iwkopt, i, j;
    double abstol = dlamch('S');
    double vl = 0., vu = 0., wkopt;
/*
 *  allocate memory
 */
    uvec = (double *)calloc(n * n, sizeof(double));
    if ((isuppz = (int *)calloc(2 * n, sizeof(int))) == NULL) {
        fprintf(stderr, "EigenDecompose: cannot allocate memory\n");
        Free(uvec);
        return 1;
    }
/*
 *  query and allocate the optimal workspace
 */
    dsyevr_("Vectors", "All", "Upper", &n, avec, &lda, &vl, &vu, &il, &iu,
            &abstol, &m, sv, uvec, &ldz, isuppz, &wkopt, &lwork, &iwkopt,
            &liwork, &info);
    lwork = (int)wkopt;
    liwork = iwkopt;
    work = (double *)calloc(lwork, sizeof(double));
    if ((iwork = (int *)calloc(liwork, sizeof(int))) == NULL) {
        fprintf(stderr, "EigenDecompose: cannot allocate memory\n");
        Free(isuppz); Free(uvec); Free(work);
        return 1;
    }
/*
 *  eigenvalue decomposition
 */
    dsyevr_("Vectors", "All", "Upper", &n, avec, &lda, &vl, &vu, &il, &iu,
            &abstol, &m, sv, uvec, &ldz, isuppz, work, &lwork, iwork,
            &liwork, &info);
    if (info) {
        fprintf(stderr, "EigenDecompose: failed to compute eigenvalues\n");
        Free(isuppz); Free(uvec); Free(work); Free(iwork);
        return 1;
    }
/*
 *  sort eigenvalues/eigenvectors in descending order
 */
    for (i = 0; i < nd; i++)
        for (j = 0; j < nd; j++)
            u[i][nd - j - 1] = uvec[i + j * nd];
    for (i = 0; i < nd; i++) avec[i] = sv[nd - i - 1];
    for (i = 0; i < nd; i++) sv[i] = avec[i];
    Free(isuppz); Free(uvec); Free(work); Free(iwork);
    return 0;
}

/*
 * Wrapper for Lapack machine precision function
 */
static double dlamch(char CMACH)
{
  return dlamch_(&CMACH);
}
