CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g
LDFLAGS =

SRV_SRC = Server.cpp common.cpp hashtable.cpp serialisation.cpp zset.cpp utils.cpp AVL.cpp
SRV_OBJ = $(SRV_SRC:.cpp=.o)

CLI_SRC = client.cpp common.cpp hashtable.cpp serialisation.cpp zset.cpp utils.cpp AVL.cpp
CLI_OBJ = $(CLI_SRC:.cpp=.o)

BIN_SERVER = server
BIN_CLIENT = client

.PHONY: all clean test

all: $(BIN_SERVER) $(BIN_CLIENT)

$(BIN_SERVER): $(SRV_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $(SRV_SRC) $(LDFLAGS)

$(BIN_CLIENT): $(CLI_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $(CLI_SRC) $(LDFLAGS)

test: all
	$(MAKE) -C test

clean:
	rm -f $(BIN_SERVER) $(BIN_CLIENT) *.o
	$(MAKE) -C test clean || true 