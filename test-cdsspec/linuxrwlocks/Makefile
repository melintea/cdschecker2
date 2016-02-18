BENCH := linuxrwlocks

TESTS := testcase1 testcase2 testcase_full

all: $(TESTS) 

# Should include it after "all"; otherwise it will only make cdsspec-generated.o 
include ../benchmarks.mk

$(BENCH).o: $(BENCH).c
	$(CXX) -fPIC -c -MMD -MF .$@.d -o $@ $< $(CXXFLAGS) $(LDFLAGS)

$(TESTS): % : %.c $(BENCH).o $(CDSSPEC)
	$(CXX) -MMD -MF .$@.d -o $@ $< $(BENCH).o $(CDSSPEC) $(CXXFLAGS) $(LDFLAGS)

-include .*.d

clean:
	rm -f $(TESTS) *.o .*.d *.dSYM
