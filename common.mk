# A few common Makefile items

CC := gcc
CXX := g++

UNAME := $(shell uname)

LIB_NAME := model
LIB_SO := lib$(LIB_NAME).so

CPPFLAGS += -Wall -Wno-volatile -O3 -g -fno-omit-frame-pointer 

CFLAGS := -Wall -O3 -g -fno-omit-frame-pointer 

# Mac OSX options
ifeq ($(UNAME), Darwin)
CPPFLAGS += -D_XOPEN_SOURCE -DMAC
endif

CDSDIR               := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
LIBBACKTRACE_INCLUDE := -I $(CDSDIR)/libbacktrace/libbacktrace
LIBBACKTRACE_LIB     := -L $(CDSDIR)/libbacktrace/libbacktrace/.libs -lbacktrace
