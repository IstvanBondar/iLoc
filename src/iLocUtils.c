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
 *  Title:
 *     SkipComments
 *  Synopsis:
 *     Skips comment lines in file. A comment line starts with #.
 *  Input Arguments:
 *     buf - current line from file
 *     fp  - file pointer
 *  Output Arguments:
 *     buf - next non-comment line
 */
void SkipComments(char *buf, FILE *fp)
{
    do {
        fgets(buf, LINLEN, fp);
        RightTrim(buf);                               /* cut trailing spaces */
    } while (buf[0] == '#' || buf[0] == '\0');
}

/*
 *  Title:
 *     RightTrim
 *  Synopsis:
 *     Cuts trailing spaces from string.
 *  Input Arguments:
 *     buf - input string
 *  Return:
 *     buf - input string, with trailing spaces removed
 */
char *RightTrim(char *buf)
{
    int n = (int)strlen(buf) - 1;
    if (buf[n] == '\n') n--;
    if (buf[n] == '\r') n--;
    while (buf[n] == ' ') n--;
    buf[n+1] = '\0';
    return buf;
}

/*
 *  Title:
 *     DropSpace
 *  Synopsis:
 *     Removes extra white space characters from string.
 *     Also removes spaces around equal sign and CR character
 *  Input Arguments:
 *     str1 - string to be de-white-spaced
 *  Output Arguments:
 *     str2 - string with white space characters removed
 *  Return:
 *     length of resulting string
 */
int DropSpace(char *str1, char *str2)
{
    int i, j, n, isch = 0;
    char c;
    n = strlen(str1);
    for (j = 0, i = 0; i < n; i++) {
        c = str1[i];
        if (c == '\r') continue;
        if (isblank(c)) {
            if (isch)
                str2[j++] = c;
            isch = 0;
        }
        else {
/*
 *          remove spaces around equal sign
 */
            if (c == '=') {
                if (i) {
                    if (isblank(str1[i-1]))
                        j--;
                }
                if (i < n - 1) {
                    if (isblank(str1[i+1]))
                        i++;
                }
            }
            str2[j++] = c;
            isch = 1;
        }
    }
    str2[j] = '\0';
    return j;
}

/*
 *
 * Free: a smart free
 *
 */
void Free(void *ptr)
{
    if (ptr != NULL)
        free(ptr);
}

/*
 *  Title:
 *     AllocateFloatMatrix
 *  Synopsis:
 *     Allocates memory to a double matrix.
 *  Input Arguments:
 *     nrow - number of rows
 *     ncol - number of columns
 *  Returns:
 *     pointer to matrix
 */
double **AllocateFloatMatrix(int nrow, int ncol)
{
    double **matrix = (double **)NULL;
    int i;
    if ((matrix = (double **)calloc(nrow, sizeof(double *))) == NULL) {
        fprintf(logfp, "AllocateFloatMatrix: cannot allocate memory\n");
        fprintf(errfp, "AllocateFloatMatrix: cannot allocate memory\n");
        errorcode = 1;
        return (double **)NULL;
    }
    if ((matrix[0] = (double *)calloc(nrow * ncol, sizeof(double))) == NULL) {
        fprintf(logfp, "AllocateFloatMatrix: cannot allocate memory\n");
        fprintf(errfp, "AllocateFloatMatrix: cannot allocate memory\n");
        Free(matrix);
        errorcode = 1;
        return (double **)NULL;
    }
    for (i = 1; i < nrow; i++)
        matrix[i] = matrix[i - 1] + ncol;
    return matrix;
}

/*
 *  Title:
 *     FreeFloatMatrix
 *  Synopsis:
 *     Frees memory allocated to a matrix.
 *  Input Arguments:
 *     matrix - matrix
 */
void FreeFloatMatrix(double **matrix)
{
    if (matrix != NULL) {
        Free(matrix[0]);
        Free(matrix);
    }
}

/*
 *  Title:
 *     AllocateShortMatrix
 *  Synopsis:
 *     Allocates memory to a short integer matrix.
 *  Input Arguments:
 *     nrow - number of rows
 *     ncol - number of columns
 *  Returns:
 *     pointer to matrix
 */
short int **AllocateShortMatrix(int nrow, int ncol)
{
    short int **matrix = (short int **)NULL;
    int i;
    if ((matrix = (short int **)calloc(nrow, sizeof(short int *))) == NULL) {
        fprintf(logfp, "AllocateShortMatrix: cannot allocate memory\n");
        fprintf(errfp, "AllocateShortMatrix: cannot allocate memory\n");
        errorcode = 1;
        return (short int **)NULL;
    }
    if ((matrix[0] = (short int *)calloc(nrow * ncol, sizeof(short int))) == NULL) {
        fprintf(logfp, "AllocateShortMatrix: cannot allocate memory\n");
        fprintf(errfp, "AllocateShortMatrix: cannot allocate memory\n");
        Free(matrix);
        errorcode = 1;
        return (short int **)NULL;
    }
    for (i = 1; i < nrow; i++)
        matrix[i] = matrix[i - 1] + ncol;
    return matrix;
}

/*
 *  Title:
 *     FreeShortMatrix
 *  Synopsis:
 *     Frees memory allocated to a short integer matrix.
 *  Input Arguments:
 *     matrix - matrix
 */
void FreeShortMatrix(short int **matrix)
{
    if (matrix != NULL) {
        Free(matrix[0]);
        Free(matrix);
    }
}

/*
 *  Title:
 *     AllocateLongMatrix
 *  Synopsis:
 *     Allocates memory to an unsigned long matrix.
 *  Input Arguments:
 *     nrow - number of rows
 *     ncol - number of columns
 *  Returns:
 *     pointer to matrix
 */
unsigned long **AllocateLongMatrix(int nrow, int ncol)
{
    unsigned long **matrix = (unsigned long **)NULL;
    int i;
    if ((matrix = (unsigned long **)calloc(nrow, sizeof(unsigned long *))) == NULL) {
        fprintf(stderr, "AllocateLongMatrix: cannot allocate memory\n");
        return (unsigned long **)NULL;
    }
    if ((matrix[0] = (unsigned long *)calloc(nrow * ncol, sizeof(unsigned long))) == NULL) {
        fprintf(stderr, "iLoc_AllocateLongMatrix: cannot allocate memory\n");
        Free(matrix);
        return (unsigned long **)NULL;
    }
    for (i = 1; i < nrow; i++)
        matrix[i] = matrix[i - 1] + ncol;
    return matrix;
}

/*
 *  Title:
 *     FreeLongMatrix
 *  Synopsis:
 *     Frees memory allocated to an unsigned long matrix.
 *  Input Arguments:
 *     matrix - matrix
 */
void FreeLongMatrix(unsigned long **matrix)
{
    if (matrix != NULL) {
        Free(matrix[0]);
        Free(matrix);
    }
}

/*
 *  Title:
 *     CompareInt
 *  Synopsis:
 *     compares two ints
 *  Returns:
 *     -1 if x < y, 1 if x > y and 0 if x == y
 */
int CompareInt(const void *x, const void *y)
{
    if (*(int *)x < *(int *)y)
        return -1;
    if (*(int *)x > *(int *)y)
        return 1;
    return 0;
}

/*
 *  Title:
 *     CompareDouble
 *  Synopsis:
 *     compares two doubles
 *  Returns:
 *     -1 if x < y, 1 if x > y and 0 if x == y
 */
int CompareDouble(const void *x, const void *y)
{
    if (*(double *)x < *(double *)y)
        return -1;
    if (*(double *)x > *(double *)y)
        return 1;
    return 0;
}

/*  EOF  */
