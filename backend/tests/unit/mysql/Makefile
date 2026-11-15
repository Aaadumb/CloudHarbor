CXXFLAGS ?= -O2 -Wall -Wextra
MYSQL_PKG := $(shell pkg-config --exists libmariadb && printf libmariadb || printf mysqlclient)
MYSQL_CFLAGS := $(shell pkg-config --cflags $(MYSQL_PKG) jsoncpp)
MYSQL_LIBS := $(shell pkg-config --libs $(MYSQL_PKG) jsoncpp)

.PHONY: all clean

all: mysql_test.o

mysql_test.o: mysql_test.cc ../test_config.h
	$(CXX) $(CPPFLAGS) $(MYSQL_CFLAGS) $(CXXFLAGS) -std=c++20 $(LDFLAGS) -o $@ $< $(MYSQL_LIBS) $(LDLIBS)

clean:
	$(RM) mysql_test.o
