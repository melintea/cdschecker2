BASE := ..

OBJECTS := specanalysis.o spec_lib.o testanalysis.o
include $(BASE)/common.mk

CPPFLAGS += -I$(BASE) -I$(BASE)/include -I.

all: $(OBJECTS)

specanalysis.o: specanalysis.cc
	$(CXX) -c -fPIC -o $@ $< $(CPPFLAGS)
testanalysis.o: testanalysis.cc
	$(CXX) -c -fPIC -o $@ $< $(CPPFLAGS)

# Non-model rules
%.o: %.c
	$(CC) -c -o $@ $< $(CPPFLAGS)

PHONY += clean
clean::
	rm -f *.o
