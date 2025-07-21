CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g
LDFLAGS =

SRC = $(wildcard test_*.cpp)
BIN = $(SRC:.cpp=)

.PHONY: all clean run

all: $(BIN)

%: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< ../common.cpp ../hashtable.cpp ../serialisation.cpp ../zset.cpp ../utils.cpp ../AVL.cpp ../timer.cpp ../DList.cpp ../heap.cpp

run: all
	@for t in $(BIN); do echo "Running $$t"; ./$$t || exit 1; done

clean:
	rm -f $(BIN) *.o 