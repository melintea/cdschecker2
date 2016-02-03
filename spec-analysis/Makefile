# This make file is to be added by the Makefile at the main directory

SPEC_OBJS := $(SPEC_DIR)/specanalysis.o \
	$(SPEC_DIR)/executiongraph.o \
	$(SPEC_DIR)/methodcall.o

OBJECTS += $(SPEC_OBJS)

$(SPEC_DIR)/specanalysis.o: $(SPEC_DIR)/specanalysis.cc $(SPEC_DIR)/specannotation.h
	$(CXX) -c -fPIC -o $@ $< $(CPPFLAGS)

$(SPEC_DIR)/executiongraph.o: $(SPEC_DIR)/executiongraph.cc $(SPEC_DIR)/executiongraph.h
	$(CXX) -c -fPIC -o $@ $< $(CPPFLAGS)

$(SPEC_DIR)/methodcall.o: $(SPEC_DIR)/methodcall.cc $(SPEC_DIR)/methodcall.h
	$(CXX) -c -fPIC -o $@ $< $(CPPFLAGS)
