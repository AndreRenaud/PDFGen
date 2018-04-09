FROM ubuntu:16.04

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get install -y \
	astyle \
	ca-certificates \
	colordiff \
	cppcheck \
        doxygen \
	gcc \
	make \
	pdftk \
	poppler-utils \
	valgrind \
