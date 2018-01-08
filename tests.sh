#!/bin/sh

run() {
	name=$1
	shift
	"$@" || ( echo "Failed to run $name" ; exit 1 )
}

run_fail() {
	name=$1
	shift
	"$@"
	if [ $? -eq 0 ] ; then
		echo "Successfully ran $name (expected failure)"
		exit 1
	fi
}

# Run the test program
valgrind --quiet --leak-check=full --error-exitcode=1 ./testprog
pdftotext -layout output.pdf

# Check for various output strings
run "check utf8 characters" grep -q "Special characters: €ÜŽžŠšÁáüöäÄÜÖß" output.txt
run "check special charactesr" grep -q "( ) < > \[ \] { } / %" output.txt
run_fail "check for line wrapping" grep -q "This is a great big long string that I hope will wrap properly around several lines." output.txt
