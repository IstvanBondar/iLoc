################################################################################
# initialization
################################################################################

# set some defaults if this is not being called from the main makefile
ifndef CALLED_FROM_MAIN

	# compilers
	CXX := g++#      # C++ compiler
	DOX := doxygen#  # C, C++, Fortran documentation

	# aliases
	MV     := mv -v
	RM     := rm -rfv

	# color definitions
	blue := $(shell tput setaf 6)
	sgr0 := $(shell tput sgr0)

	# detect os (linux, darwin)
	OS := $(shell uname -s | tr '[:upper:]' '[:lower:]')

	# detect bit size
	UNAME_M := $(shell uname -m)
	ifeq ($(UNAME_M),$(filter $(UNAME_M),x86_64 arm64))
		ARCH := 64
	else
		ARCH := 32
	endif

	# declare the library file extension for your system
	ifeq ($(OS),darwin)
		# MacOS
		export LIBEXT := dylib
	else
		# linux
		export LIBEXT := so
	endif

endif


################################################################################
# compilation parameters
################################################################################

# define output file
OUT := libslbmCshell.$(LIBEXT)
OUTDIR := ${HOME}/lib

# includes
INC := -Iinclude -I../GeoTessCPP/include -I../SLBM/include

# libraries
LIBS := -lm -lstdc++ -L$(OUTDIR) -lgeotesscpp -lslbm

# compiling parameters
ifeq ($(UNAME_M),arm64)
	CXXFLAGS := -mcpu=native
else
	CXXFLAGS := -march=native
endif
CXXFLAGS += -D$(OS) -m$(ARCH) -O1 -fPIC -std=c++11

# specific LD_FLAGS on Mac
ifeq ($(OS),darwin)

	# if Xcode version is >= 15, we need to add -Wl,-ld_classic to the linker
	# see: https://developer.apple.com/documentation/xcode-release-notes/xcode-15-release-notes#Linking
	XCODE_VERSION := $(shell ld -version_details | grep 'LLVM' | grep -oE '[0-9\.]+' | cut -d '.' -f 1)
	ifeq ($(shell expr $(XCODE_VERSION) \>= 15), 1)
		OTHER_LDFLAGS += -Wl,-ld_classic
	endif

	# if using clang instead of gnu compiler, set additional LD_FLAGS
	CLANG := $(shell $(CXX) -v 2>&1 | grep -qi clang; echo $$?)
	ifeq ($(CLANG),0)
		CXXFLAGS += -Wno-deprecated-declarations
		OTHER_LDFLAGS += -undefined dynamic_lookup -rpath @loader_path
	endif

endif

# list of objects to compile
OBJS = src/slbm_C_shell.o


################################################################################
# recipes
################################################################################

# recipe to make everything and run tests
all: initialize slbmc finalize

# command that tells us which recipes don't build source files
.PHONY: all initialize msg_intro slbmc msg_docs docs finalize clean cleanall


# initialize compilation script
initialize: msg_intro

# print intro message
msg_intro:
	@echo "$(blue)------------------------------------------------------------$(sgr0)"
	@echo "$(blue)Making RSTT C library...$(sgr0)"
	@echo "$(blue)------------------------------------------------------------$(sgr0)"


# to build any *.o file, compile its corresponding *.cc file
%.o: %.cc
	@printf "$(blue)(Building obj) $(sgr0)"
	$(CXX) $(CXXFLAGS) $(INC) -o $@ -c $<


# build library after building each prerequisite in $(OBJS)
slbmc: initialize $(OUT)

# MacOS
ifeq ($(OS),darwin)
$(OUT): $(OBJS)
	@printf "$(blue)(Building lib) $(sgr0)"
	$(CXX) $(CXXFLAGS) -dynamiclib $(OTHER_LDFLAGS) -install_name $(OUTDIR)/$(OUT) -o $(OUT) $(OBJS) $(LIBS)

# linux
else
$(OUT): $(OBJS)
	@printf "$(blue)(Building lib) $(sgr0)"
	$(CXX) $(CXXFLAGS) -shared -Wl,-rpath,'$$ORIGIN' -o $(OUT) $(OBJS) $(LIBS)

endif


# print docs msg
msg_docs:
	@echo "$(blue)------------------------------------------------------------$(sgr0)"
	@echo "$(blue)Making RSTT C documentation...$(sgr0)"
	@echo "$(blue)------------------------------------------------------------$(sgr0)"

# documentation
docs: msg_docs doc/html
doc/html: doc/Doxyfile
ifeq (, $(shell which $(DOX)))  # doxygen is not installed
	@printf "$(blue)(Building HTML docs) $(sgr0)"
	@echo "$(DOX) is not installed. Skipping..."

else  # doxygen is installed
	@printf "$(blue)(Building HTML docs) $(sgr0)"
	cd doc; $(DOX) Doxyfile

endif


# move files and clean up
finalize:
	@printf "$(blue)(Moving files) $(sgr0)"
	$(MV) $(OUT) $(OUTDIR)

clean:
	@printf "$(blue)(Clean *.o) $(sgr0)"
	$(RM) src/*.o  src/*.d

cleanall: clean
	@printf "$(blue)(Clean docs) $(sgr0)"
	$(RM) doc/html
	@printf "$(blue)(Clean libs) $(sgr0)"
	$(RM) $(OUTDIR)/$(OUT)
