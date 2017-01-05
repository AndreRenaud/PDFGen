CFLAGS=-g -Wall -pipe --std=c1x -O3 -pedantic
default: testprog

testprog: pdfgen.o main.o
	$(CC) -o testprog pdfgen.o main.o

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

check: testprog pdfgen.c pdfgen.h
	cppcheck --enable=style --quiet pdfgen.c pdfgen.h
	valgrind --quiet ./testprog
	astyle -s4 < pdfgen.c | colordiff -u pdfgen.c -
	astyle -s4 < pdfgen.h | colordiff -u pdfgen.h -

format: pdfgen.c pdfgen.h
	astyle -q -n -s4 pdfgen.c
	astyle -q -n -s4 pdfgen.h

docs: FORCE
	doxygen pdfgen.dox
	cd docs/latex ; make

FORCE:

clean:
	rm -f *.o testprog
