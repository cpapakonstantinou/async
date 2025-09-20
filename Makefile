TARGET ?= async_test
CXX = g++-13
CXXFLAGS = -std=c++23 -Wall -O3 -I ./inc -march=native
LDFLAGS ?= 
LDLIBS ?= -ltbb
HEADERS = inc/async.h
PREFIX ?= /usr
INSTALLDIR ?= $(PREFIX)/include/async
THREADS = 4
TEST_SIZE = 2048
SHELL := /bin/bash

$(TARGET).o:test/$(TARGET).cc
	$(CXX) $(CXXFLAGS) -DASYNC_NUM_THREADS=${THREADS} -c -o $@ $^

$(TARGET):$(TARGET).o
	${CXX} -o $(TARGET) $^ ${LDFLAGS} ${LDLIBS}

clean:
	$(RM) *.o $(TARGET).ii $(TARGET).s

cleanall: clean
	$(RM) $(TARGET)

install: $(HEADERS)
	install -d $(INSTALLDIR)
	install -m 644 $(HEADERS) $(INSTALLDIR)

uninstall:
	$(RM) -r $(INSTALLDIR)

.DEFAULT_GOAL=install