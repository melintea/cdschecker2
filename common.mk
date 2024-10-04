# A few common Makefile items

CC := gcc
CXX := g++

UNAME := $(shell uname)

LIB_NAME := model
LIB_SO := lib$(LIB_NAME).so


CDSDIR               := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
LIBBACKTRACE_INCLUDE := -I $(CDSDIR)/libbacktrace/libbacktrace
LIBBACKTRACE_LIB     := -L $(CDSDIR)/libbacktrace/libbacktrace/.libs -lbacktrace

# -DCONFIG_DEBUG 
CPPFLAGS += -Wall -Wno-volatile -O0 -g -fno-omit-frame-pointer -std=c++20

CFLAGS := -Wall -O0 -g -fno-omit-frame-pointer 

# Mac OSX options
ifeq ($(UNAME), Darwin)
CPPFLAGS += -D_XOPEN_SOURCE -DMAC
endif

