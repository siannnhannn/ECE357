all: test_spinlock shell_game

#creating the test_spinlock executable
test_spinlock: test_spinlock.o spinlock.o tas.o
	gcc -g -o test_spinlock.out test_spinlock.o spinlock.o tas.o

#compiling test_spinlock.o
test_spinlock.o: test_spinlock.c
	gcc -g -c test_spinlock.c

#compiling spinlock.o
spinlock.o: spinlock.c spinlock.h
	gcc -g -c spinlock.c

#compiling tas.o
tas.o: tas.S tas.h
	gcc -g -c tas.S -o tas.o

#compiling sem.o
sem.o: sem.c sem.h
	gcc -g -c sem.c

#creating the shell_game executable
shell_game: shellgame.o sem.o spinlock.o tas.o
	gcc -g -o shellgame.out shellgame.o sem.o spinlock.o tas.o

#compiling shell_game.o
shellgame.o: shellgame.c
	gcc -g -c shellgame.c

clean:
	rm -f *.out *.o
