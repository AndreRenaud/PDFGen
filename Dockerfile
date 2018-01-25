FROM ubuntu:16.04

RUN apt-get update && apt-get install -y \
	astyle \
	ca-certificates \
	colordiff \
	cppcheck \
	gcc \
	make \
	poppler-utils \
	valgrind

