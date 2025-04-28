OUTPUTDIR := bin/

CFLAGS := -std=c++17 -fvisibility=hidden -lpthread -Wall

ifeq (,$(CONFIGURATION))
	CONFIGURATION := transactional
endif

# Common sources and headers
SOURCES := src/*.cpp
HEADERS := src/*.h
TARGETBIN := color-$(CONFIGURATION)

# Configuration-specific settings
ifeq (debug,$(CONFIGURATION))
	CFLAGS += -g
else ifeq (STM, $(CONFIGURATION))
    CXX = g++
    CFLAGS += -O2 -fopenmp -fgnu-tm -DUSE_LIBITM_STM=1
    LDFLAGS += -litm
else ifeq (transactional,$(CONFIGURATION))
    # Default transactional build WITH STM support
    CFLAGS += -O2 -fopenmp -fgnu-tm -DUSE_LIBITM_STM=1 -litm -mavx2 -pg  -mrtm -mavx -march=native
    LDFLAGS += -litm
else ifeq (transactional-no-stm,$(CONFIGURATION))
    # Special case: transactional build WITHOUT STM
    CFLAGS += -O2 -fopenmp
endif

.SUFFIXES:
.PHONY: all clean stm transactional transactional-no-stm

all: $(TARGETBIN)

# Additional targets
stm:
	$(MAKE) CONFIGURATION=STM

transactional:
	$(MAKE) CONFIGURATION=transactional

transactional-no-stm:
	$(MAKE) CONFIGURATION=transactional-no-stm

$(TARGETBIN): $(SOURCES) $(HEADERS)
	$(CXX) -o $@ $(CFLAGS) $(SOURCES) $(LDFLAGS)

format:
	clang-format -i src/*.cpp src/*.h

clean:
	rm -rf ./color-*
