TARGET ?= async_test
CXX = g++-13
CXXFLAGS = -std=c++23 -Wall -O3 -I ./inc -march=native
LDFLAGS ?=
LDLIBS ?= 
HEADERS = inc/async.h
PREFIX ?= /usr
INSTALLDIR ?= $(PREFIX)/include/async

$(TARGET).o:test/$(TARGET).cc
	$(CXX) $(CXXFLAGS) -c -o $@ $^

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