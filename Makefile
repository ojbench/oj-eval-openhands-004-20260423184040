CXX = g++
CXXFLAGS = -std=c++14 -O2

code: main.cpp
	$(CXX) $(CXXFLAGS) -o code main.cpp

clean:
	rm -f code *.dat *.log

.PHONY: clean
