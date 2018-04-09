FROM ubuntu:16.04

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get install -y \
	astyle \
	ca-certificates \
	colordiff \
	cppcheck \
        doxygen \
	gcc \
	gcj-jdk \
	g++ \
	make \
	poppler-utils \
	unzip \
	valgrind \
	wget

RUN wget https://www.pdflabs.com/tools/pdftk-the-pdf-toolkit/pdftk-2.02-src.zip && unzip pdftk-2.02-src.zip && cd pdftk-2.02-dist/pdftk && make -f Makefile.Redhat

ENV PATH="/pdftk-2.02-dist/pdftk:${PATH}"
