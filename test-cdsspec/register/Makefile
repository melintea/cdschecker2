TESTNAME = register

all: $(TESTNAME)

include ../benchmarks.mk

$(TESTNAME): $(TESTNAME).cc $(CDSSPEC)
	$(CXX) -MMD -MF $(dir $@).$(notdir $@).d -o $@ $< $(CDSSPEC) $(CXXFLAGS) $(LDFLAGS)

-include .*.d 

clean:
	rm -f $(TESTNAME) *.o .*.d

.PHONY: clean all
