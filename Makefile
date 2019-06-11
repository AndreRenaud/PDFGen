CFLAGS=-g -Wall -pipe --std=c1x -O3 -pedantic -Wsuggest-attribute=const -Wsuggest-attribute=format -Wclobbered -Wempty-body -Wignored-qualifiers -Wmissing-field-initializers -Wold-style-declaration -Wmissing-parameter-type -Woverride-init -Wtype-limits -Wuninitialized -Wunused-but-set-parameter -fprofile-arcs -ftest-coverage
LFLAGS=-fprofile-arcs -ftest-coverage
CLANG=clang
CLANG_FORMAT=clang-format
XXD=xxd


default: testprog

testprog: pdfgen.o tests/main.o tests/penguin.o
	$(CC) -o $@ pdfgen.o tests/main.o tests/penguin.o $(LFLAGS)

tests/fuzz-%: tests/fuzz-%.c pdfgen.c
	$(CLANG) -I. -g -o $@ $^ -fsanitize=fuzzer,address

tests/penguin.c: data/penguin.jpg
	# Convert data/penguin.jpg to a C source file with binary data in a variable
	$(XXD) -i $< > $@ || ( rm -f $@ ; false )

%.o: %.c Makefile
	$(CC) -I. -c -o $@ $< $(CFLAGS)

check: testprog pdfgen.c pdfgen.h example-check
	cppcheck --std=c99 --enable=style,warning,performance,portability,unusedFunction --quiet pdfgen.c pdfgen.h tests/main.c
	./tests.sh
	$(CLANG_FORMAT) pdfgen.c | colordiff -u pdfgen.c -
	$(CLANG_FORMAT) pdfgen.h | colordiff -u pdfgen.h -
	$(CLANG_FORMAT) tests/main.c | colordiff -u tests/main.c -
	gcov -r pdfgen.c

example-check: FORCE
	# Extract the code block from the README & make sure it compiles
	sed -n '/^```/,/^```/ p' < README.md | sed '/^```/ d' > example-check.c
	$(CC) $(CFLAGS) -o example-check example-check.c pdfgen.c $(LFLAGS)
	rm example-check example-check.c

check-fuzz-%: tests/fuzz-% FORCE
	mkdir -p fuzz-artifacts
	./$< -verbosity=0 -max_total_time=60 -max_len=4096 -rss_limit_mb=1024 -artifact_prefix="./fuzz-artifacts/"

fuzz-check: check-fuzz-ppm check-fuzz-jpg check-fuzz-header check-fuzz-text check-fuzz-dstr

format: FORCE
	$(CLANG_FORMAT) -i pdfgen.c pdfgen.h tests/main.c tests/fuzz-ppm.c tests/fuzz-jpg.c tests/fuzz-header.c tests/fuzz-text.c

docs: FORCE
	doxygen pdfgen.dox 2>&1 | tee doxygen.log
	cat doxygen.log | test `wc -c` -le 0
	rm doxygen.log

FORCE:

clean:
	rm -f *.o tests/*.o testprog *.gcda *.gcno *.gcov tests/*.gcda tests/*.gcno output.pdf output.txt tests/fuzz-ppm tests/fuzz-jpg tests/fuzz-header tests/fuzz-text output.pdftk fuzz.jpg fuzz.ppm fuzz.pdf doxygen.log tests/penguin.c valgrind.log
	rm -rf docs fuzz-artifacts
