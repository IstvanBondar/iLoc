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

# define output file (libgeotesscpp.so or .dylib)
OUT := libgeotesscpp.$(LIBEXT)
OUTDIR := ${HOME}/lib

# includes
INC := -Iinclude

# libraries
LIBS := -lm -lstdc++

# compiling parameters
ifeq ($(UNAME_M),arm64)
	CXXFLAGS := -mcpu=native
else
	CXXFLAGS := -march=native
endif
CXXFLAGS += -D$(OS) -m$(ARCH) -O1 -fPIC -Wno-terminate -MD -std=c++11

# specific LD_FLAGS on Mac
ifeq ($(OS),darwin)

	# if Xcode version is >= 15, we need to add -Wl,-ld_classic to the linker
	# see: https://developer.apple.com/documentation/xcode-release-notes/xcode-15-release-notes#Linking
	XCODE_VERSION := $(shell ld -version_details | grep 'LLVM' | grep -oE '[0-9\.]+' | cut -d '.' -f 1)
	ifeq ($(shell expr $(XCODE_VERSION) \>= 15), 1)
		OTHER_LDFLAGS += -Wl,-ld_classic
	endif

	# if using clang instead of gnu compiler, set additional flags
	CLANG := $(shell $(CXX) -v 2>&1 | grep -qi clang; echo $$?)
	ifeq ($(CLANG),0)
		CXXFLAGS += -Wno-unknown-warning-option -Wno-deprecated-declarations -Wno-exceptions
		OTHER_LDFLAGS += -undefined dynamic_lookup -rpath @loader_path
	endif

endif


# list of objects to compile
OBJS = src/ArrayReuse.o \
       src/CPPUtils.o \
       src/CpuTimer.o \
       src/DataType.o \
       src/EarthShape.o \
       src/EnumType.o \
       src/GeoTessDataArray.o \
       src/GeoTessData.o \
       src/GeoTessDataValue.o \
       src/GeoTessException.o \
       src/GeoTessGreatCircle.o \
       src/GeoTessGrid.o \
       src/GeoTessHorizon.o \
       src/GeoTessHorizonDepth.o \
       src/GeoTessHorizonLayer.o \
       src/GeoTessHorizonRadius.o \
       src/GeoTessInterpolatorType.o \
       src/GeoTessMetaData.o \
       src/GeoTessModel.o \
       src/GeoTessModelUtils.o \
       src/GeoTessOptimizationType.o \
       src/GeoTessPointMap.o \
       src/GeoTessPolygon3D.o \
       src/GeoTessPolygon.o \
       src/GeoTessPosition.o \
       src/GeoTessPositionLinear.o \
       src/GeoTessPositionNaturalNeighbor.o \
       src/GeoTessProfile.o \
       src/GeoTessProfileConstant.o \
       src/GeoTessProfileEmpty.o \
       src/GeoTessProfileNPoint.o \
       src/GeoTessProfileSurface.o \
       src/GeoTessProfileSurfaceEmpty.o \
       src/GeoTessProfileThin.o \
       src/GeoTessProfileType.o \
       src/GeoTessUtils.o \
       src/IFStreamAscii.o \
       src/IFStreamBinary.o


################################################################################
# recipes
################################################################################

# recipe to make everything and run tests
all: initialize geotess finalize

# command that tells us which recipes don't build source files
.PHONY: all initialize msg_intro geotess msg_docs docs finalize clean cleanall


# initialize compilation script
initialize: msg_intro

# print intro message
msg_intro:
	@echo "$(blue)------------------------------------------------------------$(sgr0)"
	@echo "$(blue)Making GeoTessCPP library...$(sgr0)"
	@echo "$(blue)------------------------------------------------------------$(sgr0)"


# to build any *.o file, compile its corresponding *.cc file
%.o: %.cc
	@printf "$(blue)(Building obj) $(sgr0)"
	$(CXX) $(CXXFLAGS) $(INC) -o $@ -c $<


# build library after building each prerequisite in $(OBJS)
geotess: initialize $(OUT)

# MacOS
ifeq ($(OS),darwin)
$(OUT): $(OBJS)
	@printf "$(blue)(Building lib) $(sgr0)"
	$(CXX) $(CXXFLAGS) -dynamiclib -shared $(OTHER_LDFLAGS) -install_name $(OUTDIR)/$(OUT) -o $(OUT) $(OBJS) $(LIBS)

# linux
else
$(OUT): $(OBJS)
	@printf "$(blue)(Building lib) $(sgr0)"
	$(CXX) $(CXXFLAGS) -shared -o $(OUT) $(OBJS) $(LIBS)

endif


# print docs msg
msg_docs:
	@echo "$(blue)------------------------------------------------------------$(sgr0)"
	@echo "$(blue)Making GeoTessCPP documentation...$(sgr0)"
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


# remove .o files
clean:
	@printf "$(blue)(Clean *.o) $(sgr0)"
	$(RM) src/*.o src/*.d

cleanall: clean
	@printf "$(blue)(Clean docs) $(sgr0)"
	$(RM) doc/html
	@printf "$(blue)(Clean libs) $(sgr0)"
	$(RM) $(OUTDIR)/$(OUT)

# include dependency files, which forces `make` to search for changes in headers
# and not just the source files. this coincides with '-MD' in the CXXFLAGS. this
# is primarily for ease in development. it doesn't help or hurt the end-user
# beyond just making *.d files in addition to *.o files.
-include $(OBJS:.o=.d)
