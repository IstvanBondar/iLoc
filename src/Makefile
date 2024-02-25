#
# Copyright (c) 2021, Istvan Bondar,
# Written by Istvan Bondar, ibondar2014@gmail.com
#
# BSD Open Source License.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the <organization> nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#
# iloc Makefile
#
#
ILOC_VERSION := 4.2
SHELL:=/bin/bash
ILOC:=${ILOCROOT}
ORAH:=${ORACLE_HOME}
TARGETLIB := ${HOME}/lib
MAKE:=make
#
# set indicators
#
ISPG:=1
ISMY:=1
ISOR:=1
ISLA:=1
ifeq (,$(ORAH))
    ISOR:=0
endif
export MYSQLINC =
export PGSQLINC =
PLIB:=
MLIB:=
#
# color definitions
#
blue := $(shell tput setaf 6)
sgr0 := $(shell tput sgr0)
#
#
# Determine OS specifics
#
#
OS := $(shell uname -s | tr [:upper:] [:lower:])
#
# detect bit size
#
UNAME_M:=$(shell uname -m)
ifeq ($(UNAME_M),$(filter $(UNAME_M),x86_64 arm64))
	export ARCH := 64
else
	export ARCH := 32
endif
#
# get linux distribution
#
ifeq ($(OS),linux)
    LSB:=$(shell lsb_release -i | tr [:upper:] [:lower:])
    RL:=$(word 3, $(LSB))
    ifeq ($(RL),ubuntu)
        REL:=Ubuntu
    else ifeq ($(RL),debian)
        REL:=Ubuntu
    else ifeq ($(RL),centos)
        REL:=CentOS
    else ifeq ($(RL),redhat)
        REL:=CentOS
    else ifeq ($(RL),suse)
        REL:=CentOS
    else
        REL:=CentOS
    endif
endif
#
#
# Set OS-specific compiler variables
#
#
ifeq ($(OS),darwin)
#
# Mac OS X 10.6 and later:
#    - PostgreSQL client is part of the OS
#    - LAPACK is part of the Accelerate framework
#    - LD_LIBRARY_PATH is disabled; create ~/lib instead
#    - GCD turns Grand Central Dispatch multicore processing on
#      GeoTess does not work with GCD, Geotess is NOT threadsafe!!!
#
    export LAPACK =-framework Accelerate
#   MACOS=-DMACOSX=1 -DGCD=1
    export MACOS =-DMACOSX=1
    export LDOPTS =-dynamiclib
    export LIBEXT =dylib
#
#   check for the existence of postgresql client
#
    ifneq (,$(wildcard /usr/local/opt/libpq/lib/libpq.$(LIBEXT)))
        PLIB:=/usr/local/opt/libpq/lib/libpq.$(LIBEXT)
        export PGSQLINC =-I/usr/local/opt/libpq/include
    else ifneq (,$(wildcard /usr/local/libpq/lib/libpq.$(LIBEXT)))
        PLIB:=/usr/local/libpq/lib/libpq.$(LIBEXT)
        export PGSQLINC =-I/usr/local/libpq/include
    else ifneq (,$(wildcard /usr/local/lib/libpq.$(LIBEXT)))
        PLIB:=/usr/local/lib/libpq.$(LIBEXT)
        export PGSQLINC =-I/usr/local/include
    else
        ISPG:=0
    endif
#
#   check for the existence of MySQL client
#
    ifneq (,$(wildcard /usr/local/opt/mysql/lib/libmysqlclient.$(LIBEXT)))
        MLIB:=/usr/local/opt/mysql/lib/libmysqlclient.$(LIBEXT)
        export MYSQLINC =-I/usr/local/opt/mysql/include
    else ifneq (,$(wildcard /usr/local/mysql/lib/libmysqlclient.$(LIBEXT)))
        MLIB:=/usr/local/mysql/lib/libmysqlclient.$(LIBEXT)
        export MYSQLINC =-I/usr/local/mysql/include
    else ifneq (,$(wildcard /usr/local/lib/libmysqlclient.$(LIBEXT)))
        MLIB:=/usr/local/lib/libmysqlclient.$(LIBEXT)
        export MYSQLINC =-I/usr/local/include
    else
        ISMY:=0
    endif
else
#
#   Linux (tested on CentOS and Ubuntu):
#
    export LAPACK =-L$(TARGETLIB) -llapack -lblas
    export LDOPTS =-shared -z defs
    export LIBEXT =so
    export MYSQLINC =-I/usr/include/mysql
#
#   Ubuntu, Debian specific
#
    ifeq ($(REL),Ubuntu)
#
#       check for the existence of lapack and blas libraries
#
        ifeq (,$(wildcard $(TARGETLIB)/liblapack.$(LIBEXT)))
            ifneq (,$(wildcard /usr/lib/liblapack.$(LIBEXT)))
                LLIB:=/usr/lib/liblapack.$(LIBEXT)
                BLIB:=/usr/lib/libblas.$(LIBEXT)
            else ifneq (,$(wildcard /usr/lib/liblapack.$(LIBEXT).3))
                LLIB:=/usr/lib/liblapack.$(LIBEXT).3
                BLIB:=/usr/lib/libblas.$(LIBEXT).3
            else ifneq (,$(wildcard /usr/lib/x86_64-linux-gnu/liblapack.$(LIBEXT)))
                LLIB:=/usr/lib/x86_64-linux-gnu/liblapack.$(LIBEXT)
                BLIB:=/usr/lib/x86_64-linux-gnu/libblas.$(LIBEXT)
            else ifneq (,$(wildcard /usr/lib/x86_64-linux-gnu/liblapack.$(LIBEXT).3))
                LLIB:=/usr/lib/x86_64-linux-gnu/liblapack.$(LIBEXT).3
                BLIB:=/usr/lib/x86_64-linux-gnu/libblas.$(LIBEXT).3
            else
                ISLA:=0
            endif
        endif
#
#       check for the existence of postgresql client libpq.so.5
#
        export PGSQLINC =-I/usr/include/postgresql
        ifeq (,$(wildcard $(TARGETLIB)/libpq.$(LIBEXT)))
            ifneq (,$(wildcard /usr/lib/x86_64-linux-gnu/libpq.$(LIBEXT)))
                PLIB:=/usr/lib/x86_64-linux-gnu/libpq.$(LIBEXT)
            else ifneq (,$(wildcard /usr/lib/libpq.$(LIBEXT)))
                PLIB:=/usr/lib/libpq.$(LIBEXT)
            else ifneq (,$(wildcard /usr/local/lib/libpq.$(LIBEXT)))
                PLIB:=/usr/local/lib/libpq.$(LIBEXT)
            else ifneq (,$(wildcard /usr/lib/libpq.$(LIBEXT).5))
                PLIB:=/usr/lib/libpq.$(LIBEXT).5
            else ifneq (,$(wildcard /usr/lib/x86_64-linux-gnu/libpq.$(LIBEXT).5))
                PLIB:=/usr/lib/x86_64-linux-gnu/libpq.$(LIBEXT).5
            else ifneq (,$(wildcard /usr/local/lib/libpq.$(LIBEXT).5))
                PLIB:=/usr/local/lib/libpq.$(LIBEXT).5
            else
                ISPG:=0
            endif
        endif
#
#       check for the existence of MySQL client libmysqlclient.so.20
#
        ifeq (,$(wildcard $(TARGETLIB)/mysqlclient.$(LIBEXT)))
            ifneq (,$(wildcard /usr/lib/x86_64-linux-gnu/libmysqlclient.$(LIBEXT)))
                MLIB:=/usr/lib/x86_64-linux-gnu/libmysqlclient.$(LIBEXT)
            else ifneq (,$(wildcard /usr/lib/libmysqlclient.$(LIBEXT)))
                MLIB:=/usr/lib/libmysqlclient.$(LIBEXT) $(TARGETLIB); \
            else ifneq (,$(wildcard /usr/local/lib/libmysqlclient.$(LIBEXT)))
                MLIB:=/usr/local/lib/libmysqlclient.$(LIBEXT)
            else ifneq (,$(wildcard /usr/lib/x86_64-linux-gnu/libmysqlclient.$(LIBEXT).20))
                MLIB:=/usr/lib/x86_64-linux-gnu/libmysqlclient.$(LIBEXT).20
            else ifneq (,$(wildcard /usr/lib/libmysqlclient.$(LIBEXT).20))
                MLIB:=/usr/lib/libmysqlclient.$(LIBEXT).20
            else ifneq (,$(wildcard /usr/local/lib/libmysqlclient.$(LIBEXT).20))
                MLIB:=/usr/local/lib/libmysqlclient.$(LIBEXT).20
            else
                ISMY:=0
            endif
        endif
#
#   CentOS, Red Hat, SuSe
#
    else
#
#       check for the existence of lapack and blas libraries
#
        ifeq (,$(wildcard $(TARGETLIB)/liblapack.$(LIBEXT)))
            ifneq (,$(wildcard /usr/lib64/liblapack.$(LIBEXT)))
                LLIB:=/usr/lib64/liblapack.$(LIBEXT)
                BLIB:=/usr/lib64/libblas.$(LIBEXT)
            else ifneq (,$(wildcard /usr/lib64/liblapack.$(LIBEXT).3))
                LLIB:=/usr/lib64/liblapack.$(LIBEXT).3
                BLIB:=/usr/lib64/libblas.$(LIBEXT).3
            else ifneq (,$(wildcard /usr/lib/x86_64-linux-gnu/liblapack.$(LIBEXT)))
                LLIB:=/usr/lib/x86_64-linux-gnu/liblapack.$(LIBEXT)
                BLIB:=/usr/lib/x86_64-linux-gnu/libblas.$(LIBEXT)
            else ifneq (,$(wildcard /usr/lib/x86_64-linux-gnu/liblapack.$(LIBEXT).3))
                LLIB:=/usr/lib/x86_64-linux-gnu/liblapack.$(LIBEXT).3
                BLIB:=/usr/lib/x86_64-linux-gnu/libblas.$(LIBEXT).3
            else
                ISLA:=0
            endif
        endif
#
#      check for the existence of postgresql client libpq.so.5
#
        export PGSQLINC =-I/usr/include/pgsql
        ifeq (,$(wildcard $(TARGETLIB)/libpq.$(LIBEXT)))
            ifneq (,$(wildcard /usr/lib64/x86_64-linux-gnu/libpq.$(LIBEXT)))
                PLIB:=/usr/lib64/x86_64-linux-gnu/libpq.$(LIBEXT)
            else ifneq (,$(wildcard /usr/lib64/libpq.$(LIBEXT)))
                PLIB:=/usr/lib64/libpq.$(LIBEXT)
            else ifneq (,$(wildcard /usr/local/lib/libpq.$(LIBEXT)))
                PLIB:=/usr/local/lib/libpq.$(LIBEXT)
            else ifneq (,$(wildcard /usr/lib64/x86_64-linux-gnu/libpq.$(LIBEXT).5))
                PLIB:=/usr/lib64/x86_64-linux-gnu/libpq.$(LIBEXT).5
            else ifneq (,$(wildcard /usr/lib64/libpq.$(LIBEXT).5))
                PLIB:=/usr/lib64/libpq.$(LIBEXT).5
            else ifneq (,$(wildcard /usr/local/lib/libpq.$(LIBEXT).5))
                PLIB:=/usr/local/lib/libpq.$(LIBEXT).5
            else
                ISPG:=0
            endif
        endif
#
#       check for the existence of MySQL client libmysqlclient.so.20
#
        ifeq (,$(wildcard $(TARGETLIB)/mysqlclient.$(LIBEXT)))
            ifneq (,$(wildcard /usr/lib64/x86_64-linux-gnu/libmysqlclient.$(LIBEXT)))
                MLIB:=/usr/lib64/x86_64-linux-gnu/libmysqlclient.$(LIBEXT)
            else ifneq (,$(wildcard /usr/lib64/libmysqlclient.$(LIBEXT)))
                MLIB:=/usr/lib64/libmysqlclient.$(LIBEXT) $(TARGETLIB); \
            else ifneq (,$(wildcard /usr/local/lib/libmysqlclient.$(LIBEXT)))
                MLIB:=/usr/local/lib/libmysqlclient.$(LIBEXT)
            else ifneq (,$(wildcard /usr/lib64/x86_64-linux-gnu/libmysqlclient.$(LIBEXT).20))
                MLIB:=/usr/lib64/x86_64-linux-gnu/libmysqlclient.$(LIBEXT).20
            else ifneq (,$(wildcard /usr/lib64/libmysqlclient.$(LIBEXT).20))
                MLIB:=/usr/lib64/libmysqlclient.$(LIBEXT).20
            else ifneq (,$(wildcard /usr/local/lib/libmysqlclient.$(LIBEXT).20))
                MLIB:=/usr/local/lib/libmysqlclient.$(LIBEXT).20
            else
                ISMY:=0
            endif
        endif
    endif
endif

################################################################################
# recipes
################################################################################


all: checks isf seiscomp niab isc idc clean
.PHONY: all clean checks isf seiscomp niab isc idc

checks: clean
	@echo "$(blue)----------------------------------------$(sgr0)"
	@echo "$(blue)Building iLoc v$(ILOC_VERSION)$(sgr0)"
	@echo "$(blue)Checking libraries... $(sgr0)"
	@echo "$(blue)----------------------------------------$(sgr0)"
#
# exit with error if lapack and blas libraries are missing
#
	@echo "$(blue)liblapack and libblas $(sgr0)"
ifeq ($(ISLA),0)
	$(error lapack and blas libraries are not found!)
else
#
# create symbolic links of lapack and blas libraries to ~/lib
#
ifeq ($(OS),linux)
ifeq (,$(wildcard $(TARGETLIB)/liblapack.$(LIBEXT)))
	ln -srf $(LLIB) $(TARGETLIB)
	ln -srf $(BLIB) $(TARGETLIB)
endif
endif
endif
#
# create symbolic link of postgresql library to ~/lib
#
	@echo "$(blue)libpq $(sgr0)"
ifeq ($(ISPG),1)
ifeq (,$(wildcard $(TARGETLIB)/libpq.$(LIBEXT)))
	ln -srf $(PLIB) $(TARGETLIB)
endif
else
	@echo "$(blue)libpq is not found!$(sgr0)"
endif
#
# create symbolic link of myssqlclient library to ~/lib
#
	@echo "$(blue)libmysqlclient $(sgr0)"
ifeq ($(ISMY),1)
ifeq (,$(wildcard $(TARGETLIB)/libmysqlclient.$(LIBEXT)))
	ln -srf $(MLIB) $(TARGETLIB)
endif
else
	@echo "$(blue)libmysqlclient is not found!$(sgr0)"
endif
#
# create symbolic link of clntsh library to ~/lib
#
	@echo "$(blue)libclntsh $(sgr0)"
ifeq ($(ISOR),1)
ifeq (,$(wildcard $(TARGETLIB)/libclntsh.$(LIBEXT)))
	ln -srf $(ORAH)/lib/libclntsh.$(LIBEXT) $(TARGETLIB)
endif
else
	@echo "$(blue)libclntsh is not found!$(sgr0)"
endif

#
#   iLoc with ISF I/O (default)
#
isf:
	@echo "$(blue)----------------------------------------$(sgr0)"
	@echo "$(blue)Compiling iLoc, default ISF input files $(sgr0)"
	@echo "$(blue)----------------------------------------$(sgr0)"
	$(MAKE) -f Makefile.default iLoc
	rm -f *.o
	@echo


#
#  Optional MYSQL client
#    required for SeisComp default MySQL db interface
#
seiscomp:
ifeq ($(ISMY),1)
	@echo "$(blue)--------------------------------------------$(sgr0)"
	@echo "$(blue)Compiling iLocSC, SeisComp MySQL DB schema  $(sgr0)"
	@echo "$(blue)--------------------------------------------$(sgr0)"
	$(MAKE) -f Makefile.seiscomp iLocSC
else
	@echo "$(blue)-----------------------------------------$(sgr0)"
	@echo "$(blue)MySQL client is not installed, skipping  $(sgr0)"
	@echo "$(blue)-----------------------------------------$(sgr0)"
endif

#
#  Optional PostgreSQL client
#    required for CTBTO NDC-in-a-Box db interfaces
#
niab:
ifeq ($(ISPG),1)
	@echo "$(blue)----------------------------------------------$(sgr0)"
	@echo "$(blue)Compiling iLocNiaB, PostgreSQL IDC DB schema  $(sgr0)"
	@echo "$(blue)----------------------------------------------$(sgr0)"
	$(MAKE) -f Makefile.niab iLocNiaB
else
	@echo "$(blue)----------------------------------------------$(sgr0)"
	@echo "$(blue)PostgreSQL client is not installed, skipping  $(sgr0)"
	@echo "$(blue)----------------------------------------------$(sgr0)"
endif

#
#  Optional PostgreSQL client
#    required for ISC db interfaces
#
isc:
ifeq ($(ISPG),1)
	@echo "$(blue)---------------------------------------------$(sgr0)"
	@echo "$(blue)Compiling iLocISC, PostgreSQL ISC DB schema  $(sgr0)"
	@echo "$(blue)---------------------------------------------$(sgr0)"
	$(MAKE) -f Makefile.isc iLocISC
else
	@echo "$(blue)----------------------------------------------$(sgr0)"
	@echo "$(blue)PostgreSQL client is not installed, skipping  $(sgr0)"
	@echo "$(blue)----------------------------------------------$(sgr0)"
endif

#
#  Optional Oracle instant client libraries
#    required for IDC Oracle db interface
#
idc:
ifeq ($(ISOR),1)
	@echo "$(blue)------------------------------------------$(sgr0)"
	@echo "$(blue)Compiling iLocIDC, IDC Oracle DB schema   $(sgr0)"
	@echo "$(blue)------------------------------------------$(sgr0)"
	$(MAKE) -f Makefile.idc iLocIDC
else
	@echo "$(blue)------------------------------------------$(sgr0)"
	@echo "$(blue)Oracle client is not installed, skipping  $(sgr0)"
	@echo "$(blue)------------------------------------------$(sgr0)"
endif

#
# clean up
#
clean:
	@echo "$(blue)Cleaning up object files $(sgr0)"
	rm -f *.o
#
#
