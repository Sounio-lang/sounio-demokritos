CXX ?= g++
CXXFLAGS ?= -std=c++23 -O2 -Wall -Wextra
TOOLS = tools/csv2sio tools/phreeqc2csv tools/parity
all: $(TOOLS)
tools/%: tools/%.cpp tools/csv.hpp
	$(CXX) $(CXXFLAGS) -o $@ $<
clean:
	rm -f $(TOOLS)
.PHONY: all clean
