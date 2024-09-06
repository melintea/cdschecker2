BASE := ..

OBJECTS := $(patsubst %.c, %.o, $(wildcard *.c))
OBJECTS += $(patsubst %.cc, %.o, $(wildcard *.cc))

include $(BASE)/common.mk

DIR := litmus
include $(DIR)/Makefile

DEPS := $(join $(addsuffix ., $(dir $(OBJECTS))), $(addsuffix .d, $(notdir $(OBJECTS))))

CPPFLAGS += -I$(BASE) -I$(BASE)/include -fPIC 
CFLAGS += -I$(BASE) -I$(BASE)/include

all: $(OBJECTS)

-include $(DEPS)

%.o: %.c
	$(CC) -MMD -MF $(@D)/.$(@F).d -o $@ $< $(CFLAGS) $(LIBBACKTRACE_INCLUDE) -L$(BASE) -l$(LIB_NAME) $(LIBBACKTRACE_LIB) -rdynamic

%.o: %.cc
	$(CXX) -MMD -MF $(@D)/.$(@F).d -o $@ $< $(CPPFLAGS) $(LIBBACKTRACE_INCLUDE) -L$(BASE) -l$(LIB_NAME) $(LIBBACKTRACE_LIB) -rdynamic

clean::
	rm -f $(OBJECTS) $(DEPS)
