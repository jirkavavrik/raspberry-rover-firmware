# Project:	Raspberry Pi rover firmware
# Autor:	  Jiří Vavřík
# Date:		  10. 8. 2019

CC=g++
FLAGS=`sdl2-config --cflags --libs`
EXE=robot

main: robot.cpp
	$(CC) robotl.cpp $(FLAGS) -o $(EXE)

run:
	./$(EXE)
