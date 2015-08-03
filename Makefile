# Daniel Cheng - dyc8av
# Written March 31, 2015
# makefile for HW3, includes main.cpp 
# creates an executable fat

all: main.cpp 
	g++ main.cpp -o fat

clean:
	-rm -f *.o *~
