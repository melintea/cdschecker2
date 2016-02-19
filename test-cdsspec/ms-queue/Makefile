BENCH := queue

TESTS := testcase1 testcase2 testcase3

all: $(TESTS)

# Should include it after "all"; otherwise it will only make cdsspec-generated.o
include ../benchmarks.mk

%.o : %.c 
	$(CC) -fPIC -c -MMD -MF .$@.d -o $@ $< $(CPPFLAGS) $(LDFLAGS)

$(TESTS): % : %.o $(BENCH).o $(CDSSPEC)
	$(CXX) -MMD -MF .$@.d -o $@ $< $(BENCH).o $(CDSSPEC) $(CXXFLAGS) $(LDFLAGS)

-include .*.d

clean:
	rm -f *.o *.d $(TESTS) main
