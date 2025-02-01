CC=g++
CXX=g++
RANLIB=ranlib

LIBSRC= VirtualMemory.cpp VirtualMemory.h
LIBOBJ=$(LIBSRC:.cpp=.o)

INCS=-I.
CFLAGS = -Wall -std=c++11 -g -pthread -lrt $(INCS)
CXXFLAGS = -Wall -std=c++11 -g -pthread -lrt $(INCS)

LIB = libVirtualMemory.a
TARGETS = $(LIB)

TAR=tar
TARFLAGS=-cvf
TARNAME=ex3.tar
TARSRCS=$(LIBSRC) Makefile README

all: $(TARGETS)

$(TARGETS): $(LIBOBJ)
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@


depend:
	makedepend -- $(CFLAGS) -- $(SRC) $(LIBSRC)

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)