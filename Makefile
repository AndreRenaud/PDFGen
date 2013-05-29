CFLAGS=-g -Wall -pipe
default: testprog

testprog: pdfgen.o main.o
	$(CC) -o testprog pdfgen.o main.o

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm *.o testprog
