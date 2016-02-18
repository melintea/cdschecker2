BENCH := queue

TESTS := testcase1 testcase2 testcase3

all: $(TESTS)

# Should include it after "all"; otherwise it will only make cdsspec-generated.o
include ../benchmarks.mk

$(BENCH).o : $(BENCH).c
	$(CXX) -c -fPIC -MMD -MF .$@.d -o $@ $< $(CXXFLAGS) $(LDFLAGS)

$(TESTS): % : %.c $(BENCH).o $(CDSSPEC)
	$(CXX) -MMD -MF .$@.d -o $@ $< $(BENCH).o $(CDSSPEC) $(CXXFLAGS) $(LDFLAGS)

-include .*.d

clean:
	rm -f *.o *.d $(TESTS) main
