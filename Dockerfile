FROM ubuntu:16.04

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get install -y dirmngr
RUN echo "deb http://pkg.mxe.cc/repos/apt/debian wheezy main" > /etc/apt/sources.list.d/mxeapt.list && apt-key adv --keyserver keyserver.ubuntu.com --recv-keys D43A795B73B16ABE9643FE1AFD8FFF16DB45C6AB

RUN apt-get update && apt-get install -y \
	astyle \
	ca-certificates \
	colordiff \
	cppcheck \
	doxygen \
	gcc \
	graphviz \
	make \
	mxe-i686-w64-mingw32.static-gcc \
	pdftk \
	poppler-utils \
	software-properties-common \
	valgrind \
	wget

RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key|apt-key add - \
 && apt-add-repository "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial main" \
 && apt-get update \
 && apt-get install -y clang-8 clang-format-8 clang-tools-8
