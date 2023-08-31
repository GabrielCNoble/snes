SRC := $(wildcard *.c) $(wildcard */*.c)
INC := $(wildcard *.s) $(wildcard *.h) $(wildcard */*.h)

LIB_PATH := $(wildcard libs/*)
LIB_SRC := $(wildcard libs/*/src/*.cpp) $(wildcard libs/*/src/*/*.cpp)
LIB_INC := $(wildcard libs/*/include)

OBJ := $(SRC:.c=.o) $(LIB_SRC:.cpp=.o)

%.o: %.c
	gcc -c $< $(foreach path, $(LIB_PATH), -I$(path)/include) -g -o $@

%.o: %.cpp
	gcc -c $< $(foreach path, $(LIB_PATH), -I$(path)/include) -g -o $@

all: $(OBJ) $(INC)
	gcc $(OBJ) -Wl,--copy-dt-needed-entries -g $(foreach path, $(LIB_PATH),-L$(path)/lib/linux) -lSDL2main -lSDL2 -lGLEW -lstdc++ -o snes.out

test:
	echo $(OBJ)

clean:
	rm $(OBJ)