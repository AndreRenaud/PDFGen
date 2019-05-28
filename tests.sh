#!/bin/sh

fail() {
	echo $*
	exit 1
}

run() {
	name=$1
	shift
	printf "Running test $name ..."
	"$@" || fail "Failed to run '$name'"
	printf "\n"
}

run_fail() {
	name=$1
	shift
	"$@"
	if [ $? -eq 0 ] ; then
		fail "Successfully ran '$name' (expected failure)"
	fi
}

# Run the test program
run "valgrind" valgrind --log-file=valgrind.log --leak-check=full --error-exitcode=1 ./testprog
run "pdftotext" pdftotext -layout output.pdf
run "pdftk" pdftk output.pdf dump_data output output.pdftk

# Check for various output strings
run "check utf8 characters" grep -q "Special characters: €ÜŽžŠšÁáüöäÄÜÖß" output.txt
run "check special charactesr" grep -q "( ) < > \[ \] { } / %" output.txt
run_fail "check for line wrapping" grep -q "This is a great big long string that I hope will wrap properly around several lines." output.txt

# Check for pdftk meta data
run "check page count" grep -q "NumberOfPages: 4$" output.pdftk
run "check bookmarks" grep -q "BookmarkTitle: First page$" output.pdftk
run "check for subject" grep -q "InfoValue: My subject$" output.pdftk

echo "Tests completed successfully"
