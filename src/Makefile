# Project:	Raspberry Pi rover firmware
# Autor:	  Jiří Vavřík
# Date:		  10. 8. 2019

CC=g++
FLAGS=-lwiringPi -lpthread
EXE=robot

main: robot.cpp
	$(CC) robot.cpp $(FLAGS) -o $(EXE)

run:
	./$(EXE)
