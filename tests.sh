#!/bin/sh

fail() {
	echo $*
	exit 1
}

run() {
	name=$1
	shift
	"$@" || fail "Failed to run '$name'"
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
run "valgrind" valgrind --quiet --leak-check=full --error-exitcode=1 ./testprog
run "pdftotext" pdftotext -layout output.pdf

# Check for various output strings
run "check utf8 characters" grep -q "Special characters: €ÜŽžŠšÁáüöäÄÜÖß" output.txt
run "check special charactesr" grep -q "( ) < > \[ \] { } / %" output.txt
run_fail "check for line wrapping" grep -q "This is a great big long string that I hope will wrap properly around several lines." output.txt

echo "Tests completed successfully"
