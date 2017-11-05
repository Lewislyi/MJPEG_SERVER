CC:=g++
CFLAGS:= -L /usr/local/lib
LDFLAGS:=-ljpeg -lpthread
EXE:= mjpeg_server
SRC:=$(shell pwd)/../src/*.cpp
OUT_PATH:= $(shell pwd)/../out/$(EXE)
OBJ_PATH:= $(shell pwd)/../objects
OBJECT:= $(OBJ_PATH)/*.o
all: exe
exe: object
	$(CC) $(OBJECT) $(CFLAGS) $(LDFLAGS) -o $(OUT_PATH)
object:
	$(CC) -c $(SRC)
	mv ./*.o $(OBJ_PATH)
clean:
	rm $(shell pwd)/../out/*