BENCH := mcs-lock

TEST = testcase

all: $(TEST)

# Should include it after "all"; otherwise it will only make cdsspec-generated.o
include ../benchmarks.mk

$(BENCH).o: $(BENCH).cc
	$(CXX) -fPIC -c -MMD -MF .$@.d -o $@ $< $(CXXFLAGS) $(LDFLAGS)

$(TEST) : % : %.cc $(BENCH).o $(CDSSPEC)
	$(CXX) -MMD -MF .$@.d -o $@ $< $(BENCH).o $(CDSSPEC) $(CXXFLAGS) $(LDFLAGS)

-include .*.d

clean:
	rm -rf $(BENCH) $(TEST) *.o *.dSYM
