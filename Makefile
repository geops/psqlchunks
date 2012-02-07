CC?=gcc
CXX?=g++
# clang needs some extra include directories to find its stuff
#CC=clang++ -I/usr/include
PG_CONFIG?=pg_config

PREFIX?=/usr/local

# remove the comment from the following line to enable debug statements
#DEBUG=-DDEBUG=1

PQ_LIBS=-lpq
PQ_FLAGS=
PQ_INCLUDES=-I $(shell $(PG_CONFIG) --includedir)

CFLAGS=-Wall -Wextra \
	-Wno-format-extra-args \
	-Wformat-nonliteral \
	-Wformat-security \
	-Wformat=2 \
	-D_XOPEN_SOURCE=600 $(DEBUG)
ifeq ($(strip $(DEBUG)), )
	# production build
	CFLAGS+=-O2
   ifeq ($(CXX), g++)
      CFLAGS+=-s
   endif
else
	# debug build - add debug info to the binaries
	CFLAGS+=-g -O0
   ifeq ($(CXX), g++)
      CFLAGS+=-Weffc++
   endif
endif

CXXFLAGS=$(CFLAGS) $(PQ_FLAGS)
ifeq ($(CXX), g++)
	CXXFLAGS+=-std=gnu++98
endif
INCLUDES=-I src/ $(PQ_INCLUDES)
LIBS=$(PQ_LIBS)
CXX_SOURCES := $(wildcard src/*.cc)
HEADERS := $(wildcard src/*.h)
SOURCES := $(CXX_SOURCES)
OBJECTS := $(patsubst %.cc,%.o,$(CXX_SOURCES))
BIN_PSQLCHUNKS=psqlchunks

all: $(BIN_PSQLCHUNKS)

dist: all strip

strip: $(BIN_PSQLCHUNKS)
	strip $(BIN_PSQLCHUNKS)

install: dist
	install $(BIN_PSQLCHUNKS) $(PREFIX)/bin/
	
$(BIN_PSQLCHUNKS): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(BIN_PSQLCHUNKS) $(OBJECTS) $(LIBS)

clean:
	find ./src/ -name '*.o' -delete
	rm -f $(BIN_PSQLCHUNKS)

# trigger a complete rebuild if a header changed
$(OBJECTS): $(HEADERS)

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $*.o -c $*.cc

cppcheck:
	cppcheck -q --enable=all src
