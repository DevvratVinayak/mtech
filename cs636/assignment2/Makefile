CXX=g++
CPPFLAGS=
CXXFLAGS=-O3 -std=c++17 -fopenmp
TESTFLAGS=-lgtest -pthread 
TBBFLAGS=-I/usr/local/tbb/include -L/usr/include/tbb -ltbb

OUTDIR=${PWD}

# First target is the default target
all:  $(OUTDIR)/problem1.out  $(OUTDIR)/problem2.out  $(OUTDIR)/problem3.out
#Use this instead of the above line if Google test framework is available
#all:  $(OUTDIR)/problem1.out $(OUTDIR)/problem1_test.out $(OUTDIR)/problem2.out $(OUTDIR)/problem2_test.out $(OUTDIR)/problem3.out

# Ensure output directory exists
$(shell mkdir -p $(OUTDIR))

# Rule for problem1
$(OUTDIR)/problem1.out: problem1/241110079.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@ $(TBBFLAGS)
#Uncomment these lines if Google test framework is available
#$(OUTDIR)/problem1_test.out: problem1/241110079_test.cpp 
#	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@ $(TESTFLAGS) 
# Rule for problem2
$(OUTDIR)/problem2.out: problem2/241110079.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@ $(TBBFLAGS)
#Uncomment these lines if Google test framework is available
#$(OUTDIR)/problem2_test.out: problem2/241110079_test.cpp
#	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@ $(TESTFLAGS) 
# Rule for problem3
$(OUTDIR)/problem3.out: problem3/241110079.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@
# Clean rule
clean:
	rm -rf $(OUTDIR)/problem1.out compile_commands.json
	rm -rf $(OUTDIR)/problem1_test.out compile_commands.json
	rm -rf $(OUTDIR)/problem2.out compile_commands.json
	rm -rf $(OUTDIR)/problem2_test.out compile_commands.json
	rm -rf $(OUTDIR)/problem3.out compile_commands.json

.PHONY: all clean