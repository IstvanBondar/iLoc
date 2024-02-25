iLoc, a single-event earthquake location algorithm
==================================================

<!-- github-style badges/shields -->
<!-- <style type="text/css">p img{height:24px;}</style> -->
[![License](https://img.shields.io/badge/License-BSD%203--Clause-3da639?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyMy4yIj48ZGVmcy8+PHBhdGggZmlsbD0iI2VlZSIgZD0iTTAgMTIuMUMuMSA1LjUgNC43LjggMTAuNC4xYzYuNy0uOSAxMi40IDMuNyAxMy41IDkuNyAxIDUuOC0yLjEgMTEuMS03LjMgMTMuMy0uNC4yLS43LjEtLjktLjRMMTMgMTZjLS4xLS40IDAtLjYuMy0uOCAxLjItLjUgMS45LTEuNCAyLjEtMi43LjMtMS45LTEtMy43LTIuOS00aC0uMmMtMS44LS4yLTMuNSAxLjEtMy44IDIuOS0uMyAxLjYuNSAzLjEgMiAzLjguNS4yLjYuNC40LjlsLTIuNiA2LjhjLS4xLjMtLjQuNS0uOC4zLTIuNy0xLjEtNS0zLjEtNi4zLTUuOEMuMSAxNSAuMSAxMy4xIDAgMTIuMXoiLz48L3N2Zz4=)](https://opensource.org/licenses/BSD-3-Clause)
![C++](https://img.shields.io/badge/C++-11+-1069ac?logo=c%2B%2B)
![C](https://img.shields.io/badge/C-99+-7991b5?logo=c&logoColor=eee)

Tested on:

[![MacOS](https://img.shields.io/badge/MacOS-10.13%20%7C%2010.14%20%7C%2010.15-999999?logo=apple&logoColor=eee)](https://www.apple.com/)
[![Ubuntu](https://img.shields.io/badge/Ubuntu-16.04%20%7C%2018.04%20LTS-e95420?logo=ubuntu&logoColor=eee)](https://www.ubuntu.com/)
[![CentOS](https://img.shields.io/badge/CentOS-7%20%7C%208-262577?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA5MSA5MC41Ij48cGF0aCBmaWxsPSIjYjRiZWMxIiBzdHJva2U9IiNmZmYiIHN0cm9rZS13aWR0aD0iMi4wMDIiIGQ9Ik00NS44MSA0MS4yMWwtMTkuOC0xOS45IDE5LjgtMTkuOSAxOS44IDE5Ljl6Ii8+PHBhdGggZmlsbD0iIzM5NGQ1NCIgc3Ryb2tlPSIjZmZmIiBzdHJva2Utd2lkdGg9IjIuMDAyIiBkPSJNNjkuODEgNjUuMjFsLTE5LjgtMTkuOSAxOS44LTE5LjkgMTkuOCAxOS45eiIvPjxwYXRoIGZpbGw9IiNmY2U5NGYiIGQ9Ik0xNC43MSAxNC4xMWgyN3YyNi45aC0yN3oiLz48cGF0aCBmaWxsPSIjYjRiZWMxIiBkPSJNNDkuNzEgNDkuMjFoMjYuOHYyN2gtMjYuOHoiLz48cGF0aCBmaWxsPSIjNGY2YTc0IiBzdHJva2U9IiNmZmYiIHN0cm9rZS13aWR0aD0iMi4wMDIiIGQ9Ik00MS42MSA0NS4zMWwtMjAuMSAxOS44LTIwLjEtMTkuOCAyMC4xLTE5Ljh6Ii8+PHBhdGggZmlsbD0iIzRmNmE3NCIgZD0iTTQ5LjgxIDE0LjMxaDI2Ljl2MjYuOWgtMjYuOXoiLz48ZyBzdHJva2U9IiNmZmYiIHN0cm9rZS13aWR0aD0iMi4wMDIiPjxwYXRoIGZpbGw9IiM4YTlhYTAiIGQ9Ik02NS42MSA2OS4zMWwtMjAuMSAxOS44LTIwLjEtMTkuOCAyMC4xLTE5Ljh6TTEzLjcxIDEzLjIxaDI4djI3LjloLTI4eiIvPjxwYXRoIGZpbGw9Im5vbmUiIGQ9Ik00OS43MSA0OS4yMWgyNy44djI4aC0yNy44ek00OS44MSAxMy4zMWgyNy45djI3LjloLTI3Ljl6Ii8+PHBhdGggZmlsbD0iIzM5NGQ1NCIgZD0iTTEzLjcxIDQ5LjIxaDI4djI4aC0yOHoiLz48cGF0aCBmaWxsPSJub25lIiBkPSJNNDEuNjEgNDUuMzFsLTIwLjEgMTkuOC0yMC4xLTE5LjggMjAuMS0xOS44ek00NS44MSA0MS4yMWwtMTkuOC0xOS45IDE5LjgtMTkuOSAxOS44IDE5Ljl6TTY1LjYxIDY5LjMxbC0yMC4xIDE5LjgtMjAuMS0xOS44IDIwLjEtMTkuOHpNNjkuODEgNjUuMjFsLTE5LjgtMTkuOSAxOS44LTE5LjkgMTkuOCAxOS45eiIvPjwvZz48L3N2Zz4=)](https://www.centos.org/)

About iLoc
----------

iLoc is a single-event earthquake location algorithm that tackles the problem
that raypaths traversing unmodeled 3D velocity heterogeneities in the Earth
cause correlated travel-time predictions that can result in location bias and
underestimated location uncertainties.

### A brief history of iLoc

Original algorithm was developed under an AFRL contract (Bondár and McLaughlin, 2009a).
ISC location algorithm operational since 2010 (Bondár and Storchak, 2011). The ISC has relocated its entire bulletin (Storchak et al., 2017, 2020).
iLoc has additional features, and open source since 2014.
CsLoc at EMSC since 2019, (Steed et al., 2019; Bondár et al., 2020).
iLoc plugin in SeisComp since 2020.

### iLoc in a nutshell

- Can be used in a routine operational environment (fast).
- Assumes that an event is already formed, and the phases associated to the event.
- Accounts for correlated travel-time prediction errors.
- Initial hypocenter guess from Neighbourhood Algorithm search (Sambridge, 1999; Sambridge and Kennett, 2001).
- Linearised inversion using a priori estimate of the full data covariance matrix (Bondár and McLaughlin, 2009a).
- Attempts for free-depth solution only if there is depth resolution, otherwise sets default depth from a global grid of reliable free depth events from historical seismicity.
- Robust magnitude estimates with uncertainties.
- Gutenberg-Richter, Veith-Clawson and Murphy-Barker depth-distance corrections for mb and mB.
- Uses seismic, hydroacoustic and infrasound observations.
- Arrival time, slowness and azimuth measurements are used in the location.
- Identifies seismic phases w.r.t the initial guess, then the best hypocenter estimate from the NA grid search.
- Uses all valid ak135 (Kennett et al., 1995) phases in location.
- Elevation and ellipticity corrections (Dziewonski and Gilbert, 1976; Kennett and Gudmundsson, 1996).
- Depth-phase bounce point corrections (Engdahl et al., 1998).
- Uses RSTT travel-time predictions for Pn/Sn and Pg/Lg (Myers et al., 2010).
- RSTT provides its own uncertainty estimates (Begnaud et al., 2020, 2021).
- Optional use of a local velocity model.
- Predictions for local phases are calculated up to 3 degrees, beyond that iLoc switches to RSTT/ak135 predictions.
- Local phase travel time predictions are calculated for Pg/Sg, Pb/Sb, Pn/Sn.
- Supports IMS1.0/ISF1.0 and ISF2.0, ISF2.1 bulletin formats.
- Supports IDC oracle, NDC-in-a-Box PostgreSQL, ISC PostgreSQL and SeisComp MySQL database schemas.
- Optional Google Earth kmz output files.
- Performs Bondár and McLaughlin, (2009b) ground truth candidate event selection test for relocated events.
- Highly parameterizable (config files, command line arguments).

### Citations

- Bondár, I. and K. McLaughlin (2009a). Seismic location bias and uncertainty in the presence of correlated and non-Gaussian travel-time errors, Bull. Seism. Soc. Am., 99, 172-193, https://doi.org/10.1785/0120080922.
- Bondár, I., and D. Storchak (2011). Improved location procedures at the International Seismological Centre, Geophys. J. Int., 186, 1220-1244, https://doi.org/10.1111/j.1365-246X.2011.05107.x.
- Bondár I., R. Steed, J. Roch, R. Bossu, A. Heinloo, J. Saul and A. Strollo (2020). Accurate locations of felt earthquakes using crowdsource detections, Front. Earth Sci., 8, 272, https://doi.org/10.3389/feart.2020.00272.
- Bondár I., T. Šindelářová, D. Ghica, U. Mitterbauer, A. Liashchuk, J. Baše, J. Chum, C. Czanik, C. Ionescu, C. Neagoe, M. Pásztor, A. Le Pichon (2022). Central and Eastern European Infrasound Network: Contribution to Infrasound Monitoring, Geophys. J. Int., 230, 565-579, https://doi.org/10.1093/gji/ggac066.


Compiling iLoc from source
------------------------------

### Environment variables

The environment variable `$ILOCROOT` is required by the *Makefiles* during
the compilation process. Set it to your iLoc directory in your `.bashrc` file:

export ILOCROOT=$HOME/iLoc4.2


### Dependencies


#### MacOS

Below is a list of packages and software required to build iLoc from source.
Xcode or Command Line Tools need to be installed.

Software  | Purpose
:---------|:------------------------------
make      | Running compile scripts
gcc       | Compile C code
g++       | Build GeoTess and core RSTT libraries


#### Linux

Below is a list of packages and software required to build RSTT from source.

Software  | Purpose
:---------|:------------------------------
make      | Running compile scripts
gcc       | Build C library and tests
g++       | Build GeoTess and core RSTT libraries and tests
lapack    | Build lapack and blas libraries

The easiest way to satisfy these dependencies on Linux is, depending
on your distro and package manager, by running one of these sets of commands
in a termal window:

<style>
div.sxs div.sourceCode{display:inline-block;max-width:450px;vertical-align: top;}
</style>

```bash
# C++, C
$ sudo apt install build-essentials
$ sudo apt install g++-multilib

# lapack and blas libraries
$ sudo apt install liblapack3
```


```bash
# C++, C
$ sudo yum install kernel-devel gcc gcc-c++
$ sudo yum install g++-multilib

# lapack and blas libraries
$ sudo yum groupinstall 'Development Tools' -y
$ sudo yum install lapack.x86_64 -y
```


### Optional database clients

#### MySQL client

Only required if the standard SeisComp MySQL database interface is used.
For using mysql interactively, store the MySQL connection info in ~/.my.cnf.
iLoc reads the MySQL connection info from ~/.my.cnf.


##### MacOS

```bash
$ brew update
$ brew install mysql-client
```
##### Linux

```bash
$ sudo apt install libmysqlclient-dev
```


```bash
$ sudo yum install mysql-devel -y
```

#### PostgreSQL client

Only required if the CTBTO NDC-in-a-Box IDC or the ISC database interface
is to be used. For installation instructions see the PostgreSQL website:
https://www.postgresql.org/download/

Note that you need the developer package, because only that includes the
necessary include files (libpq-fe.h and postgres_ext.h) for the iLoc compilation.

iLoc expects that the PGPORT environment variable is set to the port
PostgreSQL uses (typically 5432). For using psql interactively, you may
want to store the PostgreSQL connection info in ~/.pgpass.

#### Oracle client

Only required if the CTBTO IDC database interface is to be used. Download
and install the Oracle instant client from
https://www.oracle.com/database/technologies/instant-client.html
or alternatively, the Oracle database express edition 11g from
https://www.oracle.com/database/technologies/xe-downloads.html

iLoc expects that the ORACLE_HOME, and ORACLE_SID environment variables are set.

Note that the IDC NIAB and Oracle databases have a unique index on the assoc table
by arid, which prevents iLoc to store multiple solutions for the same event. To
solve the problem, connect to the database with psql or sqlplus and delete the
assaridx index:

drop index assaridx;


### Build instructions

The iLoc installation process
- creates the ~/bin and ~/lib directories if they don’t exist yet
- compiles the RSTT libraries needed to run iLoc
- finds out whether MySQL, Postgresql or Oracle clients are installed
- creates symbolic links to the libraries needed by iLoc in ~/lib
- compiles the iLoc source code and builds various iLoc executables depending on
the database clients it found
- writes the executables to the ~/bin directory

iLoc includes only the necessary RSTT code needed to compile the libraries
libgeotesscpp, libslbm and libslbmCshell to run iLoc. If you need the whole RSTT
suite, you can download to from [sandia.gov/rstt][rstt].

To compile iLoc, use the commands below:

Command             | Description
:-------------------|:--------------------
`make`              | Make all libraries, and iLoc executables
`make all`          | Make all libraries, and iLoc executables
`make setup`        | Create the ~/bin and ~/lib folders if they do not exist yet
`make rstt`         | Make the RSST libraries required for iLoc (`geotess`, `slbm`, and `slbmCshell`)
`make iloc`         | Make the iLoc executables (`iLoc` and optionally `iLocSC`, `iLocNiaB`, `iLocIDC`, `iLocISC`)

For more granular-level control of the make process, you may use the following
commands using the Makefile in the rstt or in the src folder:


Command              | Description
:--------------------|:--------------------
`make geotess`       | Make the GeoTess library (required for RSTT)
`make slbm`          | Make the C++ library (main RSTT code)
`make slbmc`         | Make the C library

`make isf`           | Make the `iLoc` excecutable
`make seiscomp`      | Make the `iLocSC` excecutable with SeisComp MySQL interface
`make isc`           | Make the `iLocISC` excecutable with ISC PostgreSQL interface
`make niab`          | Make the `iLocNiaB` excecutable with IDC PostgreSQL interface
`make idc`           | Make the `iLocIDC` excecutable with IDC Oracle interface


Contact Information
-------------------

For questions/issues/comments about the software, please contact:

* [Istvan Bondar][slsiloc] (Seismic Location Services)

For questions/issues/comments about the RSTT package, please contact:

* [Brian Young][snl] (Sandia National Laboratories)


[//]:       # (list references below)
[brew]:     https://brew.sh/
[rstt]:     https://www.sandia.gov/rstt
[docs]:     http://www.sandia.gov/rstt/software/documentation/
[iloc]:     https://www.slsiloc.eu
[snl]:      mailto:byoung@sandia.gov
[slsiloc]:  mailto:istvan.bondar@slsiloc.eu
