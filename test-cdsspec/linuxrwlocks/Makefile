BENCH := linuxrwlocks

TESTS := testcase1 testcase2 testcase_full

all: $(TESTS) 

# Should include it after "all"; otherwise it will only make cdsspec-generated.o 
include ../benchmarks.mk

%.o : %.c 
	$(CC) -fPIC -c -MMD -MF .$@.d -o $@ $< $(CPPFLAGS) $(LDFLAGS)

$(TESTS): % : %.o $(BENCH).o $(CDSSPEC)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS)

-include .*.d

clean:
	rm -f $(TESTS) *.o .*.d *.dSYM
