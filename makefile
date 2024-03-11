C_FLAGS 	:= -std=c2x -g -masm=intel -msse3 -mavx -mavx2
CPP_FLAGS 	:= -std=c++11 -g -masm=intel -fno-threadsafe-statics -fno-exceptions -msse3 -mavx -mavx2

C_SRC := $(wildcard *.c) $(wildcard */*.c)
ASM_SRC := $(wildcard *.asm)
INC := $(wildcard *.s) $(wildcard *.h) $(wildcard */*.h)

LIB_PATH := $(wildcard libs/*)
LIB_SRC := $(wildcard libs/*/src/*.cpp) $(wildcard libs/*/src/*/*.cpp)
LIB_INC := $(wildcard libs/*/include)

OBJ := $(C_SRC:.c=.o) $(ASM_SRC:.asm=.o) $(LIB_SRC:.cpp=.o)

%.o: %.c
	gcc -c $< $(foreach path, $(LIB_PATH), -I$(path)/include) $(C_FLAGS) -o $@

%.o: %.cpp
	gcc -c $< $(foreach path, $(LIB_PATH), -I$(path)/include) $(CPP_FLAGS) -o $@

%.o: %.asm
	as $< -o $@

all: $(OBJ) $(INC)
	gcc $(OBJ) -Wl,--copy-dt-needed-entries -g $(foreach path, $(LIB_PATH),-L$(path)/lib/linux -Wl,-rpath=$(path)/lib/linux) -lSDL2main -lSDL2 -lGLEW -lstdc++ -o snes.out

test:
	echo $(OBJ)

clean:
	rm $(OBJ)