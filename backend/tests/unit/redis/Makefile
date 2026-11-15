CXXFLAGS ?= -O2 -Wall -Wextra
REDIS_CFLAGS := $(shell pkg-config --cflags hiredis jsoncpp)
REDIS_LIBS := $(shell pkg-config --libs hiredis jsoncpp)

.PHONY: all clean

all: redis_test.o

redis_test.o: redis_test.cc ../test_config.h
	$(CXX) $(CPPFLAGS) $(REDIS_CFLAGS) $(CXXFLAGS) -std=c++20 $(LDFLAGS) -o $@ $< $(REDIS_LIBS) $(LDLIBS)

clean:
	$(RM) redis_test.o
