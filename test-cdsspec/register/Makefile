BENCH := register

TESTS = testcase

all: $(TESTS)

# Should include it after "all"; otherwise it will only make cdsspec-generated.o
include ../benchmarks.mk

%.o : %.cc 
	$(CXX) -fPIC -c -MMD -MF .$@.d -o $@ $< $(CXXFLAGS) $(LDFLAGS)

$(TESTS) : % : %.o $(BENCH).o $(CDSSPEC)
	$(CXX) -MMD -MF .$@.d -o $@ $< $(BENCH).o $(CDSSPEC) $(CXXFLAGS) $(LDFLAGS)

-include .*.d 

clean:
	rm -f $(TESTNAME) *.o .*.d

.PHONY: clean all
