FROM ubuntu:18.10

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get install -y dirmngr
RUN apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 86B72ED9 \
 && echo "deb http://mirror.mxe.cc/repos/apt xenial main" > /etc/apt/sources.list.d/mxeapt.list

RUN apt-get update && apt-get install --no-install-recommends -y \
	ca-certificates \
	cloc \
	colordiff \
	cppcheck \
	curl \
	doxygen \
	gcc \
	graphviz \
	libtinfo5 \
	make \
	mxe-i686-w64-mingw32.static-gcc \
	pdftk-java \
	poppler-utils \
	python3-pip \
	python3-setuptools \
	software-properties-common \
	tzdata \
	valgrind \
	vim \
	xz-utils

RUN curl -L https://apt.llvm.org/llvm-snapshot.gpg.key|apt-key add - \
 && apt-add-repository "deb http://apt.llvm.org/cosmic/ llvm-toolchain-cosmic-8 main" \
 && apt-get update \
 && apt-get install -y clang-8 clang-format-8 clang-tools-8

RUN pip3 install cpp-coveralls

RUN mkdir -p /opt && curl -L https://github.com/facebook/infer/releases/download/v0.16.0/infer-linux64-v0.16.0.tar.xz | tar -C /opt -x -J
ENV PATH $PATH:/opt/infer-linux64-v0.16.0/bin/

RUN apt-get clean
