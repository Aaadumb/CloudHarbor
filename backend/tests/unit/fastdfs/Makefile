CXXFLAGS ?= -O2 -Wall -Wextra

.PHONY: all clean

all: fastdfs_test.o

fastdfs_test.o: fastdfs_test.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -std=c++20 $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	$(RM) fastdfs_test.o
