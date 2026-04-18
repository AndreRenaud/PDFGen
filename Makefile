LFLAGS=-fprofile-arcs -ftest-coverage -lm
CLANG=clang
CLANG_FORMAT=clang-format
XXD=xxd

ifeq ($(OS),Windows_NT)
CFLAGS=-Wall
CFLAGS_OBJECT=/Fo:
CFLAGS_EXE=/Fe:
O_SUFFIX=.obj
EXE_SUFFIX=.exe
else
CFLAGS=-g -Wall -pipe --std=c1x -O3 -pedantic -Wsuggest-attribute=const -Wsuggest-attribute=format -Wclobbered -Wempty-body -Wignored-qualifiers -Wmissing-field-initializers -Wold-style-declaration -Wmissing-parameter-type -Woverride-init -Wtype-limits -Wuninitialized -Wunused-but-set-parameter -fprofile-arcs -ftest-coverage
CFLAGS_OBJECT=-o
CFLAGS_EXE=-o
O_SUFFIX=.o
EXE_SUFFIX=
endif

TESTPROG=testprog$(EXE_SUFFIX)

default: $(TESTPROG) tests/massive-file$(EXE_SUFFIX)

$(TESTPROG): pdfgen$(O_SUFFIX) tests/main$(O_SUFFIX) tests/penguin$(O_SUFFIX) tests/rgb$(O_SUFFIX)
	$(CC) $(CFLAGS_EXE) $@ pdfgen$(O_SUFFIX) tests/main$(O_SUFFIX) tests/penguin$(O_SUFFIX) tests/rgb$(O_SUFFIX) $(LFLAGS)

tests/massive-file$(EXE_SUFFIX): tests/massive-file.c pdfgen.c
	$(CC) -I. -g -o $@ tests/massive-file.c pdfgen.c $(LFLAGS)

tests/fuzz-dstr: tests/fuzz-dstr.c pdfgen.c
	$(CLANG) -I. -g -o $@ $< -fsanitize=fuzzer,address,undefined,integer

tests/fuzz-%: tests/fuzz-%.c pdfgen.c
	$(CLANG) -I. -g -o $@ $< pdfgen.c -fsanitize=fuzzer,address,undefined,integer

tests/penguin.c: data/penguin.jpg
	# Convert data/penguin.jpg to a C source file with binary data in a variable
	$(XXD) -i $< > $@ || ( rm -f $@ ; false )

%$(O_SUFFIX): %.c
	$(CC) -I. -c $< $(CFLAGS_OBJECT) $@ $(CFLAGS)

check: $(TESTPROG) pdfgen.c pdfgen.h example-check
	cppcheck --std=c99 --enable=style,warning,performance,portability,unusedFunction --quiet pdfgen.c pdfgen.h tests/main.c
	$(CXX) -c pdfgen.c $(CFLAGS_OBJECT) /dev/null -Werror -Wall -Wextra
	./tests/tests.sh
	./tests/tests.sh acroread
	$(CLANG_FORMAT) pdfgen.c | colordiff -u pdfgen.c -
	$(CLANG_FORMAT) pdfgen.h | colordiff -u pdfgen.h -
	$(CLANG_FORMAT) tests/main.c | colordiff -u tests/main.c -
	gcov -r pdfgen.c

coverage: $(TESTPROG)
	./testprog
	rm -rf coverage-html
	mkdir coverage-html
	gcovr -r . --html --html-details -o coverage-html/coverage.html

example-check:
	# Extract the code block from the README & make sure it compiles
	sed -n '/^```/,/^```/ p' < README.md | sed '/^```/ d' > example-check.c
	$(CC) $(CFLAGS) -o example-check example-check.c pdfgen.c $(LFLAGS)
	rm example-check example-check.c

check-fuzz-%: tests/fuzz-%
	mkdir -p fuzz-artifacts
	./$< -verbosity=0 -max_total_time=240 -max_len=8192 -rss_limit_mb=1024 -artifact_prefix="./fuzz-artifacts/"

fuzz-check: check-fuzz-image-data check-fuzz-image-file check-fuzz-header check-fuzz-text check-fuzz-dstr check-fuzz-barcode check-fuzz-ttf

format:
	$(CLANG_FORMAT) -i pdfgen.c pdfgen.h tests/main.c tests/fuzz-*.c tests/massive-file.c

docs:
	doxygen docs/pdfgen.dox 2>&1 | tee doxygen.log
	cat doxygen.log | test `wc -c` -le 0

podman-image:
	podman build -t pdfgen .

podman-build-win32: podman-image
	podman run --rm -v $(PWD):/src -w /src pdfgen bash -c 'make clean && make CC=x86_64-w64-mingw32-gcc && cp testprog testprog.exe'

podman-infer: podman-image
	podman run --rm -v $(PWD):/src -w /src pdfgen bash -c 'make clean && infer run --no-progress-bar -- make CFLAGS="-g -Wall -pipe" LFLAGS="-lm"'

podman-build: podman-image
	podman run --rm -v $(PWD):/src -w /src pdfgen bash -c 'make clean && make'

podman-test: podman-image
	podman run --rm -v $(PWD):/src -w /src -e CLANG_FORMAT=clang-format pdfgen bash -c 'make clean && scan-build --status-bugs make check CLANG_FORMAT=clang-format'

podman-check: podman-image
	podman run --rm -v $(PWD):/src -w /src pdfgen bash -c 'make clean && make check'

podman-fuzz-check: podman-image
	podman run --rm -v $(PWD):/src -w /src -e CLANG=clang -e CLANG_FORMAT=clang-format pdfgen bash -c 'make clean && make fuzz-check CLANG=clang CLANG_FORMAT=clang-format'

podman-docs: podman-image
	podman run --rm -v $(PWD):/src -w /src pdfgen make docs

podman-coverage: podman-image
	podman run --rm -v $(PWD):/src -w /src -e COVERALLS_REPO_TOKEN=$(COVERALLS_REPO_TOKEN) -e GITHUB_REF=$(GITHUB_REF) pdfgen bash -c 'make clean && make coverage && if [ "$${GITHUB_REF\#\#*/}" = "master" ]; then ./testprog && coveralls -i pdfgen.c; fi'

podman-shell: podman-image
	podman run -i -t --rm -v $(PWD):/src -w /src pdfgen /bin/bash

.PHONY: default check coverage example-check fuzz-check format docs podman-image podman-build-win32 podman-infer podman-build podman-test podman-check podman-fuzz-check podman-docs podman-coverage podman-shell clean

clean:
	rm -f *$(O_SUFFIX) tests/*$(O_SUFFIX) $(TESTPROG) *.gcda *.gcno *.gcov tests/*.gcda tests/*.gcno output.pdf output.txt tests/fuzz-header tests/fuzz-text tests/fuzz-image-data tests/fuzz-image-file test/massive-file output.pdftk fuzz-image-file.pdf fuzz-image-data.pdf fuzz-image.dat doxygen.log tests/penguin.c fuzz.pdf output.ps output.ppm output-barcodes.txt
	rm -rf docs/html docs/latex fuzz-artifacts infer-out coverage-html
