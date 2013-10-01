CFLAGS=-g -Wall -pipe
default: testprog

testprog: pdfgen.o main.o
	$(CC) -o testprog pdfgen.o main.o

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

docs: FORCE
	doxygen pdfgen.dox
	cd docs/latex ; make

FORCE:

clean:
	rm *.o testprog
