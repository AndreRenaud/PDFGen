CFLAGS=-g -Wall -pipe --std=c1x -O3 -pedantic -Wsuggest-attribute=const -Wsuggest-attribute=format -Wclobbered -Wempty-body -Wignored-qualifiers -Wmissing-field-initializers -Wold-style-declaration -Wmissing-parameter-type -Woverride-init -Wtype-limits -Wuninitialized -Wunused-but-set-parameter
default: testprog

testprog: pdfgen.o main.o
	$(CC) -o testprog pdfgen.o main.o

%.o: %.c Makefile
	$(CC) -c -o $@ $< $(CFLAGS)

check: testprog pdfgen.c pdfgen.h
	cppcheck --std=c99 --enable=style,warning,performance,portability --quiet pdfgen.c pdfgen.h main.c
	valgrind --quiet --leak-check=full --error-exitcode=1 ./testprog
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
