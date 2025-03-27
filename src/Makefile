CXX = g++
CXXFLAGS = -O2 -std=c++17

SRC = $(wildcard *.cpp)
OBJ = $(SRC:.cpp=.o)

# Executables
TARGETS = noforward forward

all: $(TARGETS)

noforward: noforward.o
	$(CXX) $(CXXFLAGS) -o $@ $^

forward: forward.o
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGETS)

.PHONY: all clean

