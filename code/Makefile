OUTPUTDIR := bin/

CFLAGS := -std=c++17 -fvisibility=hidden -lpthread -Wall

ifeq (,$(CONFIGURATION))
	CONFIGURATION := release
endif

# Common sources and headers
SOURCES := src/*.cpp
HEADERS := src/*.h
TARGETBIN := color-$(CONFIGURATION)

# Configuration-specific settings
ifeq (debug,$(CONFIGURATION))
	CFLAGS += -g
else ifeq (MPI, $(CONFIGURATION))
	CFLAGS += -O2
	CXX = mpic++
	SOURCES = openmpi-coloringv2.cpp
else ifeq (MPI2, $(CONFIGURATION))
	CFLAGS += -O2
	CXX = mpic++
	SOURCES = synch-openmpi-coloring.cpp
else ifeq (STM, $(CONFIGURATION))
    CXX = g++
    CFLAGS += -O2 -fopenmp -fgnu-tm -DUSE_LIBITM_STM=1
    LDFLAGS += -litm
else ifeq (release,$(CONFIGURATION))
    # Default release build WITH STM support
    CFLAGS += -O2 -fopenmp -fgnu-tm -DUSE_LIBITM_STM=1 -litm -mavx2 -pg  -mrtm -mavx -march=native
    LDFLAGS += -litm
else ifeq (release-no-stm,$(CONFIGURATION))
    # Special case: release build WITHOUT STM
    CFLAGS += -O2 -fopenmp
endif

.SUFFIXES:
.PHONY: all clean stm release release-no-stm

all: $(TARGETBIN)

# Additional targets
stm:
	$(MAKE) CONFIGURATION=STM

release:
	$(MAKE) CONFIGURATION=release

release-no-stm:
	$(MAKE) CONFIGURATION=release-no-stm

$(TARGETBIN): $(SOURCES) $(HEADERS)
	$(CXX) -o $@ $(CFLAGS) $(SOURCES) $(LDFLAGS)

format:
	clang-format -i src/*.cpp src/*.h

clean:
	rm -rf ./color-*
