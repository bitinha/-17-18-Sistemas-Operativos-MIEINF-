CC = gcc
OBJS = processador.o readline.o stringlist.o

program: $(OBJS) naotermina
	$(CC) -o notebook $(OBJS) -g -Wall

processador.o readline.o : readline.h 

processador.o stringlist.o : stringlist.h

naotermina :
	$(CC) naotermina.c -o naotermina

clean:
	rm notebook *.o naotermina
