SRC := $(wildcard *.c) $(wildcard */*.c)
INC := $(wildcard *.s) $(wildcard */*.h)
OBJ := $(SRC:.c=.o)

%.o: %.c
	gcc -c $< -I"SDL/include" -g -o $@

all: $(OBJ) $(INC)
	gcc $(OBJ) -g -L"SDL/lib" -lSDL2main -lSDL2 -o snes.exe
	cv2pdb -s. snes.exe

clean:
	rm $(OBJ)