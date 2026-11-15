CXXFLAGS ?= -O2 -Wall -Wextra
SODIUM_CFLAGS := $(shell pkg-config --cflags libsodium)
SODIUM_LIBS := $(shell pkg-config --libs libsodium)

.PHONY: all clean

all: password_test.o

password_test.o: password_test.cc
	$(CXX) $(CPPFLAGS) $(SODIUM_CFLAGS) $(CXXFLAGS) -std=c++20 $(LDFLAGS) -o $@ $< $(SODIUM_LIBS) $(LDLIBS)

clean:
	$(RM) password_test.o
