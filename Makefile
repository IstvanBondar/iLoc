#
# Copyright (c) 2021, Istvan Bondar,
# Written by Istvan Bondar, ibondar2014@gmail.com
#
# BSD Open Source License.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#   * Neither the name of the <organization> nor the
#     names of its contributors may be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
#
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#
# iLoc master Makefile
#
#
################################################################################
# initializations
################################################################################
ILOC_VERSION := 4.2
#
# get environment variables
#
ILOC:= ${ILOCROOT}
TARGETLIB := ${HOME}/lib
MAKE:=make
#export DEBUG = -g
export DEBUG = -O2
#
#
# Determine OS specifics
#
#
OS := $(shell uname -s | tr '[:upper:]' '[:lower:]')
ifeq ($(OS),darwin)
    LIBEXT:=dylib
else
    LIBEXT:=so
endif
# color definitions
blue := $(shell tput setaf 6)
sgr0 := $(shell tput sgr0)
################################################################################
# recipes
################################################################################
all: setup rstt iLoc

# command that tells us which recipes don't build source files
.PHONY: all setup rstt iLoc

setup:
#
#   create target directories if necessary
#
	@echo "$(blue)-----------------------$(sgr0)"
	@echo "$(blue)Building iLoc v$(ILOC_VERSION)$(sgr0)"
	@echo "$(blue)Creating directories.. $(sgr0)"
	@echo "$(blue)-----------------------$(sgr0)"
	@ [ -d $(TARGETLIB) ] || mkdir $(TARGETLIB)
	@ [ -d $(HOME)/bin ] || mkdir $(HOME)/bin
#
#   compile RSTT libraries: geotess, C++ and C interface
#
rstt:
	@echo "$(blue)--------------------------------------$(sgr0)"
	@echo "$(blue)Creating RSTT C interface libraries.. $(sgr0)"
	@echo "$(blue)--------------------------------------$(sgr0)"
	$(MAKE) -C rstt cleanall
	$(MAKE) -C rstt geotess
	$(MAKE) -C rstt slbm
	$(MAKE) -C rstt slbmc
	$(MAKE) -C rstt clean

#
#	compile iLoc executables
#
iLoc:
	@echo "$(blue)-----------------------------$(sgr0)"
	@echo "$(blue)Compiling iLoc executables.. $(sgr0)"
	@echo "$(blue)-----------------------------$(sgr0)"
	$(MAKE) -C src all
	$(MAKE) -C src clean
	@echo "$(blue)---------$(sgr0)"
	@echo "$(blue)Finished compiling iLoc$(sgr0)"
	@echo "$(blue)---------$(sgr0)"
	@echo "$(blue)done $(sgr0)"

