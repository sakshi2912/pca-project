OUTPUTDIR := bin/
SRCDIR := src/
CFLAGS := -std=c++14 -fvisibility=hidden -lpthread -Wall -msse4.2 -O2 -fopenmp

# Define specific source files with their path
SOURCES := $(SRCDIR)traditional_approach_1.cpp $(SRCDIR)traditional_approach_2.cpp $(SRCDIR)traditional_approach_3.cpp $(SRCDIR)traditional_approach_4.cpp $(SRCDIR)seq_baseline.cpp $(SRCDIR)main.cpp
HEADERS := $(SRCDIR)*.h

# Set the target binary name
TARGETBIN := traditional_graph_coloring

.SUFFIXES:
.PHONY: all clean

all: $(TARGETBIN)

$(TARGETBIN): $(SOURCES) $(HEADERS)
	@mkdir -p $(OUTPUTDIR)
	$(CXX) -o $@ $(CFLAGS) $(SOURCES)

format:
	clang-format -i $(SOURCES) $(HEADERS)

clean:
	rm -rf ./$(TARGETBIN)
	rm -rf $(OUTPUTDIR)