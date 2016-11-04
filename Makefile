CF = -Wall --std=c++14 -g $(shell pkg-config --cflags bullet)
LF = -Wall --std=c++14 -lSDL2 -lSDL2_image -lGLEW -lGL -lBulletDynamics -lBulletCollision -lLinearMath
CXX = g++
SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:src/%.cpp=obj/%.o)
TRG = ladus


all: $(TRG)


obj/%.o: src/%.cpp Makefile
	@mkdir -p obj/
	$(CXX) $(CF) $< -c -o $@


$(TRG): $(OBJ) Makefile
	$(CXX) $(OBJ) $(LF) -o $@


clean:
	rm -rf $(TRG) obj/
