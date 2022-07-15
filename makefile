SRC := $(wildcard *.c)
INC := $(wildcard *.s)
OBJ := $(SRC:.c=.o)

%.o: %.c
	gcc -c $< -I"SDL/include" -o $@

all: $(OBJ) $(INC)
	gcc $(OBJ) -L"SDL/lib" -lSDL2main -lSDL2 -o snes.exe

clean:
	del $(OBJ)