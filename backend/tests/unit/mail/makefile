CXXFLAGS ?= -O2 -Wall -Wextra
CURL_CFLAGS := $(shell pkg-config --cflags libcurl)
CURL_LIBS := $(shell pkg-config --libs libcurl)

.PHONY: all clean

all: mail_test.o

mail_test.o: mail_test.cc
	$(CXX) $(CPPFLAGS) $(CURL_CFLAGS) $(CXXFLAGS) -std=c++20 $(LDFLAGS) -o $@ $< $(CURL_LIBS) $(LDLIBS)

clean:
	$(RM) mail_test.o
