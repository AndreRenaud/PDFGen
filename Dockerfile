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
	software-properties-common \
	valgrind \
	wget

RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key|apt-key add - \
 && apt-add-repository "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial main" \
 && apt-get update \
 && apt-get install -y clang-7
