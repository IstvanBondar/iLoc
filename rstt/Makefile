# -Wl,-rpath,@loader_path
#-Wunknown-warning-option
#../GeoTessCPP/include/EarthShape.h:432:4: warning: 'sprintf' is deprecated: This function is provided for compatibility reasons only.  Due to security concerns inherent in the design of sprintf(3), it is highly recommended that you use snprintf(3) instead. [-Wdeprecated-declarations]
# src/Brents.cc:189:7: warning: 'register' storage class specifier is deprecated and incompatible with C++17 [-Wdeprecated-register]

# -Wno-deprecated-register

################################################################################
# configuration
################################################################################

# paths to relevant compilers and shells
# (don't worry if you do not have one installed; it will not matter unless you
#  try to compile an rstt interface in that language)
export MAKE    := make#           # GNU Makefiles
export CC      := gcc#            # C compiler
export CXX     := g++#            # C++ compiler
export DOX     := doxygen#        # C, C++, Fortran documentation


################################################################################
# initialization
################################################################################

# define RSTT version
RSTT_VERSION := 3.2.1
TARGETLIB := $(HOME)/lib

# aliases
export MKDIR  := mkdir -p
export CP     := cp -rv
export MV     := mv -v
export RM     := rm -rfv

# color definitions
export blue := $(shell tput setaf 6)
export sgr0 := $(shell tput sgr0)

# detect os (linux, darwin)
export OS := $(shell uname -s | tr '[:upper:]' '[:lower:]')

# detect bit size
export UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),$(filter $(UNAME_M),x86_64 arm64))
	export ARCH := 64
else
	export ARCH := 32
endif

# declare the library file extension for your system
ifeq ($(OS),darwin)
	# MacOS
	export LIBEXT := dylib
	export LIBEXT_JAVA := jnilib
else
	# linux
	export LIBEXT := so
	export LIBEXT_JAVA := so
endif


# set a flag indicating to child Makefiles that they've been called from the main Makefile
export CALLED_FROM_MAIN := true


################################################################################
# recipes
################################################################################

# default make command
default: initialize geotess slbm slbmc clean

# recipe to make everything (except documentation) and run tests
all: initialize geotess slbm slbmc geotess_docs slbm_docs slbmc_docs clean

# language-specific
cpp:     cc
cc:      initialize slbm         slbm_test clean
c:       initialize slbmc       slbmc_test clean
docs:    initialize geotess_docs slbm_docs slbmc_docs
cpp_docs:           cc_docs
cc_docs:          slbm_docs
c_docs:          slbmc_docs

# command that tells us which recipes don't build source files
.PHONY: default all initialize msg_intro make_dirs clean cleanall \
        cpp cc c docs cpp_docs cc_docs c_docs geotess slbm slbmc

# initialize some folders and print status message
initialize: msg_intro make_dirs

msg_intro:
	@echo "$(blue)------------------------------------------------------------$(sgr0)"
	@echo "$(blue)Building Regional Seismic Travel Time v$(RSTT_VERSION)$(sgr0)"
	@echo "$(blue)------------------------------------------------------------$(sgr0)"
	@ [ -d $(TARGETLIB) ] || mkdir $(TARGETLIB)

# geotess library and docs
geotess: $(TARGETLIB)/libgeotesscpp.$(LIBEXT)
$(TARGETLIB)/libgeotesscpp.$(LIBEXT):
	@printf "$(blue)(Making GeoTessCPP) $(sgr0)"
	$(MAKE) -C GeoTessCPP

geotess_docs:
	@printf "$(blue)(Making docs) $(sgr0)"
	$(MAKE) -C GeoTessCPP docs

# slbm c++ library, and docs
slbm: geotess $(TARGETLIB)/libslbm.$(LIBEXT)
$(TARGETLIB)/libslbm.$(LIBEXT):
	@printf "$(blue)(Making SLBM) $(sgr0)"
	$(MAKE) -C SLBM

slbm_docs:
	@printf "$(blue)(Making docs) $(sgr0)"
	$(MAKE) -C SLBM docs

# slbm c library, and docs
slbmc: slbm $(TARGETLIB)/libslbmCshell.$(LIBEXT)
$(TARGETLIB)/libslbmCshell.$(LIBEXT):
	@printf "$(blue)(Making SLBM_C_shell) $(sgr0)"
	$(MAKE) -C SLBM_C_shell

slbmc_docs:
	@printf "$(blue)(Making docs) $(sgr0)"
	$(MAKE) -C SLBM_C_shell docs


# clean up object files
clean:
	@echo "$(blue)------------------------------------------------------------$(sgr0)"
	@echo "$(blue)Cleaning up objects...$(sgr0)"
	@echo "$(blue)------------------------------------------------------------$(sgr0)"
	@printf "$(blue)(Cleaning GeoTessCPP) $(sgr0)"
	$(MAKE) -C GeoTessCPP clean
	@printf "$(blue)(Cleaning SLBM) $(sgr0)"
	$(MAKE) -C SLBM clean
	@printf "$(blue)(Cleaning SLBM_C_shell) $(sgr0)"
	$(MAKE) -C SLBM_C_shell clean

cleanall: clean
	@echo "$(blue)------------------------------------------------------------$(sgr0)"
	@echo "$(blue)Cleaning up everything...$(sgr0)"
	@echo "$(blue)------------------------------------------------------------$(sgr0)"
	@printf "$(blue)(Cleaning GeoTessCPP docs) $(sgr0)"
	$(RM) GeoTessCPP/doc/html
	@printf "$(blue)(Cleaning SLBM docs) $(sgr0)"
	$(RM) SLBM/doc/html
	@printf "$(blue)(Cleaning SLBM_C_shell docs) $(sgr0)"
	$(RM) SLBM_C_shell/doc/html

