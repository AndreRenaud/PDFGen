CFLAGS=-g -Wall -pipe --std=c1x -O3 -pedantic -Wsuggest-attribute=const -Wsuggest-attribute=format -Wclobbered -Wempty-body -Wignored-qualifiers -Wmissing-field-initializers -Wold-style-declaration -Wmissing-parameter-type -Woverride-init -Wtype-limits -Wuninitialized -Wunused-but-set-parameter -fprofile-arcs -ftest-coverage
LFLAGS=-fprofile-arcs -ftest-coverage


default: testprog

testprog: pdfgen.o main.o
	$(CC) -o testprog pdfgen.o main.o $(LFLAGS)

fuzz-ppm: fuzz-ppm.c pdfgen.c
	$(CLANG) -o $@ fuzz-ppm.c pdfgen.c -fsanitize=fuzzer,address

fuzz-jpg: fuzz-jpg.c pdfgen.c
	$(CLANG) -o $@ fuzz-jpg.c pdfgen.c -fsanitize=fuzzer,address

%.o: %.c Makefile
	$(CC) -c -o $@ $< $(CFLAGS)

check: testprog pdfgen.c pdfgen.h
	cppcheck --std=c99 --enable=style,warning,performance,portability,unusedFunction --quiet pdfgen.c pdfgen.h main.c
	./tests.sh
	astyle -s4 < pdfgen.c | colordiff -u pdfgen.c -
	astyle -s4 < pdfgen.h | colordiff -u pdfgen.h -
	astyle -s4 < main.c | colordiff -u main.c -
	gcov -r pdfgen.c

format: pdfgen.c pdfgen.h main.c
	astyle -q -n -s4 pdfgen.c
	astyle -q -n -s4 pdfgen.h
	astyle -q -n -s4 main.c

docs: FORCE
	doxygen pdfgen.dox

FORCE:

clean:
	rm -f *.o testprog *.gcda *.gcno *.gcov output.pdf output.txt fuzz-ppm fuzz-jpg output.pdftk
	rm -rf docs
