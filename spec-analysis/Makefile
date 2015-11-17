# This make file is to be added by the Makefile at the main directory

SPEC_OBJS := $(SPEC_DIR)/specanalysis.o \
	$(SPEC_DIR)/cpgraph.o \
	$(SPEC_DIR)/spec_lib.o

OBJECTS += $(SPEC_OBJS)

$(SPEC_DIR)/specanalysis.o: $(SPEC_DIR)/specanalysis.cc $(SPEC_DIR)/specannotation.h
	$(CXX) -c -fPIC -o $@ $< $(CPPFLAGS)

$(SPEC_DIR)/cpgraph.o: $(SPEC_DIR)/cpgraph.cc $(SPEC_DIR)/cpgraph.h
	$(CXX) -c -fPIC -o $@ $< $(CPPFLAGS)

# Non-model rules
$(SPEC_DIR)/spec_lib.o: $(SPEC_DIR)/spec_lib.cc $(SPEC_DIR)/spec_lib.h $(SPEC_DIR)/specannotation.h
	$(CXX) -fPIC -c -o $@ $< $(CPPFLAGS)



