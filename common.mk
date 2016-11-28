# A few common Makefile items

CC := gcc
CXX := g++

UNAME := $(shell uname)

LIB_NAME := model
LIB_SO := lib$(LIB_NAME).so

CPPFLAGS += -Wall -O3 -g

CFLAGS := $(CPPFLAGS)

# Mac OSX options
ifeq ($(UNAME), Darwin)
CPPFLAGS += -D_XOPEN_SOURCE -DMAC
endif
