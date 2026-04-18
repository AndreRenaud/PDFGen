FROM ubuntu:24.04

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get install -y dirmngr

RUN apt-get update && apt-get install --no-install-recommends -y \
	ca-certificates \
	clang \
	clang-format \
	clang-tools \
	cloc \
	colordiff \
	cppcheck \
	curl \
	doxygen \
	gcc \
	gcovr \
	g++ \
	ghostscript \
	git \
	graphviz \
	libclang-rt-dev \
	libtinfo6 \
	llvm \
	locales-all \
	make \
	mingw-w64 \
	pdftk-java \
	poppler-utils \
	python3 \
	python3-pip \
	python3-setuptools \
	software-properties-common \
	tzdata \
	valgrind \
	vim \
	xxd \
	xz-utils \
	zbar-tools

RUN python3 -m pip install --break-system-packages cpp-coveralls

# Install Infer
RUN mkdir -p /opt && curl -L https://github.com/facebook/infer/releases/download/v1.2.0/infer-linux-x86_64-v1.2.0.tar.xz | tar -C /opt -x -J
ENV PATH $PATH:/opt/infer-linux-x86_64-v1.2.0/bin/

RUN apt-get clean
