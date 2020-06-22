# Project:	Raspberry Pi rover firmware
# Autor:	Jiří Vavřík
# Date:		22. 6. 2020

CC=g++
FLAGS=-lwiringPi
EXE=rover

main: rover.cpp
	$(CC) rover.cpp $(FLAGS) -o $(EXE)

run:
	./$(EXE)
