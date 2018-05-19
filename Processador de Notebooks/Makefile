CC = gcc
OBJS = processador.o readline.o stringlist.o

program: $(OBJS)
	$(CC) -o processador $(OBJS) -g -Wall

processador.o readline.o : readline.h 

processador.o stringlist.o : stringlist.h

clean:
	rm processador
	rm *.o
