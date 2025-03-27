CXX := g++
CXXFLAGS := -O2 -std=c++17

SRC_DIR := src
SRC := $(wildcard $(SRC_DIR)/*.cpp)

OBJ := $(SRC:.cpp=.o)

all: noforward forward

noforward: $(SRC_DIR)/noforward.o
	$(CXX) $(CXXFLAGS) -o $@ $^

forward: $(SRC_DIR)/forward.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(SRC_DIR)/*.o noforward forward

