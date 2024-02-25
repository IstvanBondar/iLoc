RSTT v3.2.1 User Manual
===========================

<!-- github-style badges/shields -->
<style type="text/css">p img{height:24px;}</style>
[![License](https://img.shields.io/badge/License-BSD%203--Clause-3da639?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyMy4yIj48ZGVmcy8+PHBhdGggZmlsbD0iI2VlZSIgZD0iTTAgMTIuMUMuMSA1LjUgNC43LjggMTAuNC4xYzYuNy0uOSAxMi40IDMuNyAxMy41IDkuNyAxIDUuOC0yLjEgMTEuMS03LjMgMTMuMy0uNC4yLS43LjEtLjktLjRMMTMgMTZjLS4xLS40IDAtLjYuMy0uOCAxLjItLjUgMS45LTEuNCAyLjEtMi43LjMtMS45LTEtMy43LTIuOS00aC0uMmMtMS44LS4yLTMuNSAxLjEtMy44IDIuOS0uMyAxLjYuNSAzLjEgMiAzLjguNS4yLjYuNC40LjlsLTIuNiA2LjhjLS4xLjMtLjQuNS0uOC4zLTIuNy0xLjEtNS0zLjEtNi4zLTUuOEMuMSAxNSAuMSAxMy4xIDAgMTIuMXoiLz48L3N2Zz4=)](https://opensource.org/licenses/BSD-3-Clause)
![C++](https://img.shields.io/badge/C++-11+-1069ac?logo=c%2B%2B)
![C](https://img.shields.io/badge/C-99+-7991b5?logo=c&logoColor=eee)
![Fortran](https://img.shields.io/badge/Fortran-90+-744e97?logo=data:image/svg+xml;base64,PHN2ZyByb2xlPSJpbWciIHZpZXdCb3g9IjAgMCAyNCAyNCIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj48cGF0aCBkPSJNLjg3NSAyMS4zNjVoMS45NjljMS4wOTEgMCAyLjE2NS0uNjE3IDIuMTY1LTIuMTQ5VjQuNzg0YzAtMS45NDItLjY0OC0yLjE1LTIuNjU5LTIuMTVILjk5OFYwaDIyLjEyN3Y5LjU3NWgtMi41OGMwLTIuMjg4LS4yNjEtMy45Ni0uOTE1LTUuMjA3LS43Mi0xLjM4Ny0yLjE5NS0xLjczMy00Ljc3OS0xLjczM0g5LjU5djcuNjM0aDEuMDJjMi45ODIgMCAzLjc3Mi0uNzQ2IDMuNzEtNC43MjNoMi4zMjd2MTIuMzg4SDE0LjMyYy0uMjA4LTMuOTg3LS40MzEtNS40MS00LjE2OC01LjM2NmgtLjYyNHY2LjY0OGMwIDEuOTQxLjgwNSAyLjE1IDIuODE2IDIuMTVoMS40NDVWMjRILjg3NXYtMi42MzV6IiBmaWxsPSIjZWVlIi8+PC9zdmc+)
![Java](https://img.shields.io/badge/Java-8+-b07219?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PGcgZmlsbD0iI2VlZSI+PHBhdGggZD0iTTkuOCAxNS41YzMuMS4xIDUuOS0uNCA4LjQtMS43LTEuMy4yLTMuNi43LTYuMS44IDcuMS0zLjYtMy44LTQuOSA1LjUtOS40LS4xIDAtLjMtLjEtLjQtLjEtMS45LjItNy43IDMuMy00LjIgNi42IDEuMSAxLjItLjMgMi40LTEgMi45LTIgLjEtMy43LjItNC45LS4ydi0uMWMuMy0uMyAyLjQtLjkgMy4yLTEtMS43LS4zLTQuNC40LTUgMS4xLS4yLjEtLjIuMi0uMS4zLjIuNSAxLjQuOCA0LjYuOHoiLz48cGF0aCBkPSJNMTIuMyAxMy45Yy0uNi0xLjYtMi0yLjgtMi4yLTQuMVY5LjJjLjEtLjcuNy0xLjQgMS44LTIuNCAyLjctMS45IDQtNSAxLjktNi44IDIgNi45LTExLjIgNi40LTEuNSAxMy45em04LjQgN0MxOS4zIDI0IDguOCAyMy40IDYuNCAyM2wtLjUtLjEuMy4yLS4yLjFjMSAuMS44LjUgMy44LjcuNyAwIDEuNi4xIDIuNC4xIDQuMi0uMSA4LjgtLjMgOC41LTMuMXptLTMuOS0zLjRzLS4xLjEtLjEuMnYuM2M2LjctMS4zIDMuNC03LjIuMy00IDMuMS0xLjQgMy4yIDIuMi0uMiAzLjV6bS02LjkuNmMtNC4xLjctMS44IDIuNyAxLjYgMi41IDEuNCAwIDMuMS0uMyA0LjUtMS0uNi0uMi0xLjYtLjctMS42LS44LTEuNC41LTggLjktNC41LS43eiIvPjxwYXRoIGQ9Ik0xOC4xIDE5LjljNC4yIDIuNy0xMi42IDIuMy0xMy4yIDEuMy4yLS40IDIuMS0uOCAyLjYtLjYtLjgtLjktMi43LS4zLTMuOS40LS4xLjItLjIuMy0uMy41LS4xLjEtLjEuMyAwIC40LjYgMS4zIDYuNS45IDcuNCAxLjEgNS4zLS4yIDExLjctMS4zIDcuNC0zLjF6TTE2IDE3Yy0uNi0uMi0uOS0uNS0xLS42LTEuOC40LTkuMiAxLjEtNi0uOS0zLjguOS0yIDIuNiAxLjYgMi40IDIgMCA0LjEtLjMgNS44LS44TDE2IDE3eiIvPjwvZz48L3N2Zz4=)
![Python](https://img.shields.io/badge/Python-3.6+-fcc624?logo=python&logoColor=eee)

Tested on:

[![MacOS](https://img.shields.io/badge/MacOS-10.13%20%7C%2010.14%20%7C%2010.15-999999?logo=apple&logoColor=eee)](https://www.apple.com/)
[![WSL](https://img.shields.io/badge/WSL-Ubuntu%2018.04-e95420?logo=windows)](https://docs.microsoft.com/windows/wsl/about)
[![Ubuntu](https://img.shields.io/badge/Ubuntu-16.04%20%7C%2018.04%20LTS-e95420?logo=ubuntu&logoColor=eee)](https://www.ubuntu.com/)
[![Fedora](https://img.shields.io/badge/Fedora-31-294172?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHhtbG5zOnhsaW5rPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5L3hsaW5rIiB2aWV3Qm94PSIwIDAgMjY3IDI2NyI+PGRlZnMvPjxwYXRoIGZpbGw9IiM3ZjdmN2YiIGQ9Ik0yNjYuNjI2IDEzMy41MDZjMC03My41MjUtNTkuNjA0LTEzMy4xMy0xMzMuMTI4LTEzMy4xM0M2MC4wMDcuMzc2LjQyNiA1OS45My4zNzQgMTMzLjQxMnYxMDMuMDIxYy4wNCAxNi42ODUgMTMuNTcxIDMwLjE5MSAzMC4yNjYgMzAuMTkxaDEwMi45MTJjNzMuNS0uMDI5IDEzMy4wNzQtNTkuNjEzIDEzMy4wNzQtMTMzLjExOCIvPjxwYXRoIGlkPSJhIiBmaWxsPSJub25lIiBzdHJva2U9IiNiMmIyYjIiIHN0cm9rZS13aWR0aD0iMjkuMjEiIGQ9Ik03Ny4xMjYgMTQyLjA5OGg0Ny44NDV2NDcuODQ0YzAgMjYuNDEtMjEuNDM0IDQ3Ljg0NS00Ny44NDUgNDcuODQ1LTI2LjQxIDAtNDcuODQ0LTIxLjQzNC00Ny44NDQtNDcuODQ1IDAtMjYuNDEgMjEuNDM0LTQ3Ljg0NCA0Ny44NDQtNDcuODQ0eiIvPjx1c2UgdHJhbnNmb3JtPSJyb3RhdGUoMTgwIDEyNC44NTYgMTQyLjE0NCkiIHhsaW5rOmhyZWY9IiNhIi8+PHBhdGggaWQ9ImIiIGZpbGw9IiNmZmYiIGQ9Ik0xMzkuNjA3IDEyNy41M3Y2Mi4zNDVjMCAzNC41MDQtMjcuOTc1IDYyLjQ4LTYyLjQ4IDYyLjQ4LTUuMjM1IDAtOC45NTctLjU5LTEzLjgwMy0xLjg1Ny03LjA2Ni0xLjg1LTEyLjg0LTcuNjQzLTEyLjg0Mi0xNC4zODEgMC04LjE0NiA1LjkxMi0xNC4wNyAxNC43NS0xNC4wNyA0LjIwNiAwIDUuNzMzLjgwNyAxMS44OTUuODA3IDE4LjE5MSAwIDMyLjk0Ny0xNC43MzUgMzIuOTgtMzIuOTI2VjE2MS4yN2E0LjY1NCA0LjY1NCAwIDAwLTQuNjYtNC42NDhsLTIxLjY2NS0uMDA1Yy04LjA3MiAwLTE0LjU5NC02LjQzOC0xNC41OTQtMTQuNTMyLS4wMDQtOC4xNDUgNi41OS0xNC41NTcgMTQuNzQ5LTE0LjU1NyIvPjx1c2UgdHJhbnNmb3JtPSJyb3RhdGUoMTgwIDEyNC44NTYgMTQyLjE0NCkiIHhsaW5rOmhyZWY9IiNiIi8+PC9zdmc+)](https://getfedora.org/)
[![CentOS](https://img.shields.io/badge/CentOS-7%20%7C%208-262577?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA5MSA5MC41Ij48cGF0aCBmaWxsPSIjYjRiZWMxIiBzdHJva2U9IiNmZmYiIHN0cm9rZS13aWR0aD0iMi4wMDIiIGQ9Ik00NS44MSA0MS4yMWwtMTkuOC0xOS45IDE5LjgtMTkuOSAxOS44IDE5Ljl6Ii8+PHBhdGggZmlsbD0iIzM5NGQ1NCIgc3Ryb2tlPSIjZmZmIiBzdHJva2Utd2lkdGg9IjIuMDAyIiBkPSJNNjkuODEgNjUuMjFsLTE5LjgtMTkuOSAxOS44LTE5LjkgMTkuOCAxOS45eiIvPjxwYXRoIGZpbGw9IiNmY2U5NGYiIGQ9Ik0xNC43MSAxNC4xMWgyN3YyNi45aC0yN3oiLz48cGF0aCBmaWxsPSIjYjRiZWMxIiBkPSJNNDkuNzEgNDkuMjFoMjYuOHYyN2gtMjYuOHoiLz48cGF0aCBmaWxsPSIjNGY2YTc0IiBzdHJva2U9IiNmZmYiIHN0cm9rZS13aWR0aD0iMi4wMDIiIGQ9Ik00MS42MSA0NS4zMWwtMjAuMSAxOS44LTIwLjEtMTkuOCAyMC4xLTE5Ljh6Ii8+PHBhdGggZmlsbD0iIzRmNmE3NCIgZD0iTTQ5LjgxIDE0LjMxaDI2Ljl2MjYuOWgtMjYuOXoiLz48ZyBzdHJva2U9IiNmZmYiIHN0cm9rZS13aWR0aD0iMi4wMDIiPjxwYXRoIGZpbGw9IiM4YTlhYTAiIGQ9Ik02NS42MSA2OS4zMWwtMjAuMSAxOS44LTIwLjEtMTkuOCAyMC4xLTE5Ljh6TTEzLjcxIDEzLjIxaDI4djI3LjloLTI4eiIvPjxwYXRoIGZpbGw9Im5vbmUiIGQ9Ik00OS43MSA0OS4yMWgyNy44djI4aC0yNy44ek00OS44MSAxMy4zMWgyNy45djI3LjloLTI3Ljl6Ii8+PHBhdGggZmlsbD0iIzM5NGQ1NCIgZD0iTTEzLjcxIDQ5LjIxaDI4djI4aC0yOHoiLz48cGF0aCBmaWxsPSJub25lIiBkPSJNNDEuNjEgNDUuMzFsLTIwLjEgMTkuOC0yMC4xLTE5LjggMjAuMS0xOS44ek00NS44MSA0MS4yMWwtMTkuOC0xOS45IDE5LjgtMTkuOSAxOS44IDE5Ljl6TTY1LjYxIDY5LjMxbC0yMC4xIDE5LjgtMjAuMS0xOS44IDIwLjEtMTkuOHpNNjkuODEgNjUuMjFsLTE5LjgtMTkuOSAxOS44LTE5LjkgMTkuOCAxOS45eiIvPjwvZz48L3N2Zz4=)](https://www.centos.org/)


Compiling RSTT from source
------------------------------

In RSTT v3.2.1, no paths should need to be specified in any environment
variables to compile and run. During the compilation process, the *Makefiles*
will point to the correct library paths within the default directory structure.
If you are having difficulty with strange errors related to paths during
compilation, try and temporarily remove any remnants of past RSTT versions in
environment variables such as `$PATH`, `$LD_LIBRARY_PATH`, `$DYLD_LIBRARY_PATH`,
and `$SLBM_ROOT` or `$SLBM_HOME`.

When writing your own codes that utilize the RSTT libraries, you may find it
useful to set one of the following environmental variables to the root directory
of RSTT:

1. `$RSTT_ROOT`
2. `$RSTT_HOME`
3. `$SLBM_ROOT`
4. `$SLBM_HOME`

In both the Java and Python interfaces, RSTT will first attempt to load the
appropriate libraries through the system paths. If that fails, they will fall
back to searching the path provided in the above variables, in order.


### Dependencies


#### MacOS

Below is a list of packages and software required to build RSTT from source. The
version numbers listed are not necessarily required to compile without error;
they are simply those that were used and tested in development.

Software  | Version        | Purpose
:---------|:---------------|:------------------------------
make      | 3.81           | Running compile scripts
gcc       | 4.2.1 (XCode)  | Build C library and tests
g++       | 4.2.1 (XCode)  | Build GeoTess and core RSTT libraries and tests
gfortran  | 9.3.0          | Build Fortran library and tests
java      | openjdk 13.0.2 | Build Java JNI library and tests
javac     | openjdk 13.0.2 | Build Java JNI library and tests
jar       | openjdk 13.0.2 | Build Java JNI library and tests 
javadoc   | openjdk 13.0.2 | Build documentation for Java code
doxygen   | 1.8.17         | Build documentation for C++, C, and Fortran code
python    | 3.8.3          | Run Python library and tests
pip       | 20.1.1         | Build Python module
sphinx    | 3.3.1          | Build documentation for Python code

The easiest way to satisfy these dependencies on a Mac is to install the
[Homebrew][brew] package manager. Following that, you can install the necessary
dependencies by running these commands in a terminal window:

```bash
$ brew update
$ brew install gcc gfortran openjdk doxygen python3 pybind11 sphinx-doc
$ python3 -m pip install sphinx-autodoc-typehints
```

NOTE: You can also install [pybind11][pybind11], [Sphinx][sphinx], and related
modules directly through `pip`.

```bash
$ python3 -m pip install pybind11 sphinx sphinx-autodoc-typehints
```

If you are having trouble with installing things with `pip` due to disk/folder
permissions, try adding `--user` to the `pip` flags in order to install modules
to a local directory.

```bash
$ python3 -m pip install --user pybind11 sphinx sphinx-autodoc-typehints
```


#### Linux

Below is a list of packages and software required to build RSTT from source. The
version numbers listed are not necessarily required to compile without error;
they are simply those that were used and tested in development.

Software  | Version           | Purpose
:---------|:------------------|:------------------------------
make      | 4.2.1             | Running compile scripts
gcc       | 9.3.1             | Build C library and tests
g++       | 9.3.1             | Build GeoTess and core RSTT libraries and tests
gfortran  | 9.3.1             | Build Fortran library and tests
java      | openjdk 1.8.0_242 | Build Java JNI library and tests
javac     | openjdk 1.8.0_242 | Build Java JNI library and tests
jar       | openjdk 1.8.0_242 | Build Java JNI library and tests 
javadoc   | openjdk 1.8.0_242 | Build documentation for Java code
doxygen   | 1.8.15            | Build documentation for C++, C, and Fortran code
python    | 3.6.8             | Run Python library and tests
pip       | 8.1.1             | Build Python module
sphinx    | 3.3.1             | Build documentation for Python code

The easiest way to satisfy these dependencies on Linux is, depending
on your distro and package manager, by running one of these sets of commands
in a termal window:

<style>
div.sxs div.sourceCode{display:inline-block;max-width:450px;vertical-align: top;}
</style>

```bash
# C++, C
$ sudo apt install build-essentials

# Fortran
$ sudo apt install gfortran

# Java
$ sudo apt install default-jdk

# Python
$ sudo apt install python3-pip python3-devel python3-pybind11

# documentation
$ sudo apt install doxygen python3-sphinx python3-sphinx-autodoc-typehints
```

```bash
# C++, C
$ sudo dnf install kernel-devel gcc gcc-c++

# Fortran
$ sudo dnf install gcc-gfortran

# Java
$ sudo dnf install java-devel

# Python
$ sudo dnf install python3-devel
$ python3 -m pip install pybind11

# documentation
$ sudo dnf config-manager --set-enabled powertools
$ sudo dnf install doxygen python3-sphinx
$ python3 -m pip install sphinx-autodoc-typehints
```

```bash
# C++, C
$ sudo yum install kernel-devel gcc gcc-c++

# Fortran
$ sudo yum install gcc-gfortran

# Java
$ sudo yum install java-devel

# Python
$ sudo yum install python3-devel
$ python3 -m pip install pybind11

# documentation
$ sudo yum config-manager --set-enabled PowerTools
$ sudo yum install doxygen python3-sphinx
$ python3 -m pip install sphinx-autodoc-typehints
```

NOTE: On any distro, you can install [pybind11][pybind11], [Sphinx][sphinx], and
related modules directly through `pip`.

```bash
$ python3 -m pip install pybind11 sphinx sphinx-autodoc-typehints
```

If you are having trouble with installing things with `pip` due to disk/folder
permissions, try adding `--user` to the `pip` flags in order to install modules
to a local directory.

```bash
$ python3 -m pip install --user pybind11 sphinx sphinx-autodoc-typehints
```


### Build instructions

RSTT is developed in C++, but it also has interfaces in C, Fortran, Java, and
Python. RSTT is distributed in various precompiled forms on [sandia.gov/rstt][rstt],
but if you prefer, or need, to compile it from source, yourself, the `Makefiles`
have been designed so that you need only compile the minimum amount of code
necessary to use RSTT with your language of choice. Do note, however, that
because RSTT is developed in C++, the core C++ libraries will have to be
compiled regardless of whether or not you intend to use only the C, Fortran,
Java, or Python interfaces.

To generally compile RSTT in any of its languages, use the commands, below:

Command             | Description
:-------------------|:--------------------
`make`              | Make all libraries, tests, and run tests (C++, C, Fortran, Java, Python)
`make all`          | Make all libraries, documentation, tests, and run tests (C++, C, Fortran, Java, Python)
`make cpp`          | Make the components required for the C++ library and run tests (`geotess`, `slbm`, and `slbm_test`)
`make c`            | Make the components required for the C library and run tests (`geotess`, `slbm`, `slmbc`, and `slbmc_test`)
`make fortran`      | Make the components required for the Fortran library and run tests (`geotess`, `slbm`, `slbmfort`, and `slbmfort_test`)
`make java`         | Make the components required for the Java library and run tests (`geotess`, `slbm`, `slbmjni`, and `slbmjni_test`)
`make python`       | Make the components required for the Python library and run tests (`geotess`, `slbm`, `slbmpy`, and `slbmpy_test`)
`make docs`         | Make the documentation for all the libraries (C++, C, Fortran, Java, Python)
`make cpp_docs`     | Make the documentation for the C++ library (`slbm_docs`)
`make c_docs`       | Make the documentation for the C library (`slbmc_docs`)
`make fortran_docs` | Make the documentation for the Fortran library (`slbmfort_docs`)
`make java_docs`    | Make the documentation for the Java library (`slbmjni_docs`)
`make python_docs`  | Make the documentation for the Python library (`slbmpy_docs`)

For more granular-level control of the make process, you may use the following
commands:

Command              | Description
:--------------------|:--------------------
`make geotess`       | Make the GeoTess library (required for RSTT)
`make slbm`          | Make the C++ library (main RSTT code)
`make slbmc`         | Make the C library
`make slbmfort`      | Make the Fortran library
`make slbmjni`       | Make the Java library
`make slbmpy`        | Make the Python library
`make geotess_docs`  | Make the documentation for the GeoTess library
`make slbm_docs`     | Make the documentation for the C++ library
`make slbmc_docs`    | Make the documentation for the C library
`make slbmfort_docs` | Make the documentation for the Fortran library
`make slbmjni_docs`  | Make the documentation for the Java library
`make slbmpy_docs`   | Make the documentation for the Python library
`make slbm_test`     | Make and run a test of the C++ library
`make slbmc_test`    | Make and run a test of the C library
`make slbmfort_test` | Make and run a test of the Fortran library
`make slbmjni_test`  | Make and run a test of the Java library
`make slbmpy_test`   | Make and run a test of the Python library


Usage examples
------------------

In the `usage_examples` directory, there is one example program for each of the
languages supported by RSTT.

Language | File
:--------|:--------------------
C++      | `cpp_example.cc`
C        | `c_example.c`
Fortran  | `fortran_example.f90`
Java     | `java_example.java`
Python   | `python_example.py`

Each example program performs the same set of tasks and is written to be as
simple as possible with extensive commenting. These programs are meant to be a
starting point from which users may explore the [documentation][docs] and write
their own code.

To build and run each example, `cd` to `usage_examples` and run `make.sh` with
any combination of arguments:

* `cpp`
* `c`
* `fortran`
* `java`
* `python`
* `all`

To see an example of how to build and run your own progam, open `make.sh` and
view with your favorite text or source code editor. This shell script is
extensively commented, and each language is sorted into *compile* and *run*
segments.


Path troubleshooting
-----------------------

Care was taken to make linking to the RSTT libraries as painless as possible.
For any of the languages, you can run `usage_examples/make.sh`, and it will
print the compiling and run commands as they are executed so that you can see
how the base libraries are included and linked during compilation. For example,
these are the commands used to link and compile the Fortran usage example on
MacOS,
```bash
# NOTE: $RSTT_ROOT = /path/to/rstt
$ gfortran -mcpu=native -Ddarwin -m64 -O1 -fno-underscoring -cpp -ffree-line-length-none -I$RSTT_ROOT/GeoTessCPP/include -I$RSTT_ROOT/SLBM/include -I$RSTT_ROOT/SLBM_Fort_shell/include -o fortran_example.o -c fortran_example.f90
$ gfortran -mcpu=native -Ddarwin -m64 -O1 -fno-underscoring -cpp -ffree-line-length-none -Wl,-rpath,$RSTT_ROOT/lib -o fortran_example fortran_example.o -lm -lstdc++ -L$RSTT_ROOT/lib -lgeotesscpp -lslbm -lslbmFshell
```

For Java programs, you have to ensure that `libslbmjni` is in your
`java.library.path`, and that `slbmjni.jar` is in your classpath. When invoking
your `java` command, this can be accomplished by
```bash
$ java -classpath .:/path/to/rstt/lib/slbmjni.jar -Djava.library.path=/path/to/rstt/lib example_java
```

For Python programs, you should either ensure that your Python module search
path contains the RSTT `lib` folder, e.g.,
```python
import sys
sys.path.append('/path/to/rstt/lib')
```
or you should install the compiled RSTT Python module,
```bash
$ python3 -m pip install --no-index --find-links=/path/to/rstt/SLBM_Py_shell/wheel rstt
```
The `rstt` Python module contains copies of `libgeotesscpp` and `libslbm` within
the module structure, itself, and unless something goes wrong, it should load
those internal copies by default.

Both the Python and Java usage examples also contain blocks of code to attempt
to read the appropriate libraries using paths from environment variables if they
fail to find them any other way. These code blocks are well-commented and are
located in the `static {}` block in `java_example.java` and before the
`if __name__ == "__main__"` in `python_example.py`. Both bits of code work the
same way, which is to first attempt to load the correct libraries using a simple
`loadLibrary` or `import` command, and if that fails they will search the paths
delineated in the following environment variables, in order,

1. `$RSTT_ROOT`
2. `$RSTT_HOME`
3. `$SLBM_ROOT`
4. `$SLBM_HOME`

If you set one of these environment variables in your terminal
(`$ export RSTT_ROOT=/path/to/rstt`) and simply copy and paste the described
code blocks in the `usage_examples/java_example.java` and `python_example.py`
into your own programs, you will not have to provide `-Djava.library.path` when
invoking your `java` command, nor will you have to `pip install` the `rstt`
module in Python—instead the `rstt` module will be imported from the `rstt`
folder inside of `/path/to/rstt/lib`.


Contact Information
-----------------------

For questions/issues/comments about the software, please contact:

* [Brian Young][snl] (Sandia National Laboratories)

For questions/issues/comments about the results returned by RSTT, please
contact:

* [Steve Myers][llnl] (Lawrence Livermore National Laboratory)

* [Mike Begnaud][lanl] (Los Alamos National Laboratory)


[//]:       # (list references below)
[brew]:     https://brew.sh/
[pybind11]: https://github.com/pybind/pybind11
[sphinx]:   https://www.sphinx-doc.org/
[rstt]:     https://www.sandia.gov/rstt
[docs]:     http://www.sandia.gov/rstt/software/documentation/
[snl]:      mailto:byoung@sandia.gov
[llnl]:     mailto:smyers@llnl.gov
[lanl]:     mailto:mbegnaud@lanl.gov
