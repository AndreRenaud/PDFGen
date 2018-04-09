CFLAGS=-g -Wall -pipe --std=c1x -O3 -pedantic -Wsuggest-attribute=const -Wsuggest-attribute=format -Wclobbered -Wempty-body -Wignored-qualifiers -Wmissing-field-initializers -Wold-style-declaration -Wmissing-parameter-type -Woverride-init -Wtype-limits -Wuninitialized -Wunused-but-set-parameter -fprofile-arcs -ftest-coverage
LFLAGS=-fprofile-arcs -ftest-coverage
CLANG=clang


default: testprog

testprog: pdfgen.o main.o
	$(CC) -o testprog pdfgen.o main.o $(LFLAGS)

fuzz-%: fuzz-%.c pdfgen.c
	$(CLANG) -g -o $@ $^ -fsanitize=fuzzer,address

%.o: %.c Makefile
	$(CC) -c -o $@ $< $(CFLAGS)

check: testprog pdfgen.c pdfgen.h
	cppcheck --std=c99 --enable=style,warning,performance,portability,unusedFunction --quiet pdfgen.c pdfgen.h main.c
	./tests.sh
	astyle -s4 < pdfgen.c | colordiff -u pdfgen.c -
	astyle -s4 < pdfgen.h | colordiff -u pdfgen.h -
	astyle -s4 < main.c | colordiff -u main.c -
	gcov -r pdfgen.c

fuzz-check: fuzz-ppm fuzz-jpg fuzz-header
	./fuzz-ppm -verbosity=0 -max_total_time=60 -max_len=4096 -rss_limit_mb=1024
	./fuzz-jpg -verbosity=0 -max_total_time=60 -max_len=4096 -rss_limit_mb=1024
	./fuzz-header -verbosity=0 -max_total_time=60 -max_len=4096 -rss_limit_mb=1024

format: pdfgen.c pdfgen.h main.c
	astyle -q -n -s4 pdfgen.c
	astyle -q -n -s4 pdfgen.h
	astyle -q -n -s4 main.c

docs: FORCE
	doxygen pdfgen.dox

FORCE:

clean:
	rm -f *.o testprog *.gcda *.gcno *.gcov output.pdf output.txt fuzz-ppm fuzz-jpg output.pdftk fuzz.jpg fuzz.ppm
	rm -rf docs
