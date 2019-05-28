#!/bin/sh
# Grabs information from valgrind logs and PDF output file and uploads
# them to corlysis for graphing

set -e

if [ ! -e valgrind.log ] ; then
	echo "valgrind.log not found - please run tests" >&2
	exit 1
fi

if [ ! -e output.pdf ] ; then
	echo "output.pdf not found - please run tests" >&2
	exit 1
fi

if [ -z "$CORLYSIS_TOKEN" ] ; then
	echo "No API token for Corlysis available" >&2
	exit 1
fi

ALLOCS=`sed -rn 's/^.* ([0-9,]+) allocs.*$/\1/gp' < valgrind.log | sed 's/,//g'`
FREES=`sed -rn 's/^.* ([0-9,]+) frees.*$/\1/gp' < valgrind.log | sed 's/,//g'`
ALLOCATED=`sed -rn 's/^.* ([0-9,]+) bytes allocated.*$/\1/gp' < valgrind.log | sed 's/,//g'`
XREFS=`grep -a "/Size" output.pdf | awk '{print $2}' | tr -d '[:space:]'`
FILESIZE=`du -b output.pdf | awk '{print $1}'`
LOC=`cloc --quiet --csv pdfgen.c | grep "^1," | cut -d , -f 5`

DATA="pdfgen_usage allocs=$ALLOCS,frees=$FREES,allocated=$ALLOCATED,xref_count=$XREFS,file_size=$FILESIZE,loc=$LOC"
echo data="$DATA"
curl -i -XPOST 'https://corlysis.com:8086/write?db=TestStats' \
	 -u token:$CORLYSIS_TOKEN --data-binary "$DATA"
