BASE := ..

include $(BASE)/common.mk
OBJECTS := specanalysis.o spec_lib.o

CPPFLAGS += -I$(BASE) -I$(BASE)/include -I. -O3

all: $(OBJECTS)

PHONY += all 

specanalysis.o: specanalysis.cc specannotation.h
	$(CXX) -c -fPIC -o $@ $< $(CPPFLAGS)

# Non-model rules
spec_lib.o: spec_lib.c spec_lib.h specannotation.h
	$(CC) -c -o $@ $< $(CPPFLAGS) -std=c99

PHONY += clean
clean::
	rm -f *.o .*.d

.PHONY: $(PHONY)
