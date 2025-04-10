CXX = g++
CXXFLAGS = -Wall -Wextra -O3 -fopenmp -std=c++17
LDFLAGS = -lm -fopenmp

# Target executables
SEQUENTIAL = graphcolor_seq
OPENMP = graphcolor_omp

all: $(SEQUENTIAL) $(OPENMP)

$(SEQUENTIAL): graphcolor_seq.cpp cycleTimer.h
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

$(OPENMP): graphcolor_omp.cpp cycleTimer.h
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

# Generate test graphs
GRAPHS_DIR = graphs
graphs: | $(GRAPHS_DIR)
	python3 graph_generator.py random 100 0.1 $(GRAPHS_DIR)/random_100_01.graph
	python3 graph_generator.py random 1000 0.01 $(GRAPHS_DIR)/random_1000_001.graph
	python3 graph_generator.py grid 10 10 $(GRAPHS_DIR)/grid_10x10.graph
	python3 graph_generator.py grid 30 30 $(GRAPHS_DIR)/grid_30x30.graph
	python3 graph_generator.py scale-free 100 3 $(GRAPHS_DIR)/scalefree_100_3.graph
	python3 graph_generator.py scale-free 1000 5 $(GRAPHS_DIR)/scalefree_1000_5.graph
	python3 graph_generator.py complete 50 $(GRAPHS_DIR)/complete_50.graph
	python3 graph_generator.py bipartite 50 50 0.3 $(GRAPHS_DIR)/bipartite_50_50_03.graph
	python3 graph_generator.py small-world 100 4 0.1 $(GRAPHS_DIR)/smallworld_100_4_01.graph

$(GRAPHS_DIR):
	mkdir -p $(GRAPHS_DIR)

# Run tests
RESULTS_DIR = results
test: all graphs | $(RESULTS_DIR)
	@echo "Running tests..."
	@for graph in $(GRAPHS_DIR)/*.graph; do \
		base=$$(basename $$graph .graph); \
		echo "Testing $$base with sequential algorithm"; \
		./$(SEQUENTIAL) $$graph $(RESULTS_DIR)/$$base.seq.result; \
		echo "Testing $$base with OpenMP (4 threads)"; \
		./$(OPENMP) $$graph $(RESULTS_DIR)/$$base.omp4.result 4; \
		echo "Testing $$base with OpenMP (8 threads)"; \
		./$(OPENMP) $$graph $(RESULTS_DIR)/$$base.omp8.result 8; \
	done

$(RESULTS_DIR):
	mkdir -p $(RESULTS_DIR)

# Make comparison script executable
compare:
	chmod +x compare_results.sh
	./compare_results.sh

clean:
	rm -f $(SEQUENTIAL) $(OPENMP)
	rm -rf $(GRAPHS_DIR) $(RESULTS_DIR)

.PHONY: all graphs test clean compare