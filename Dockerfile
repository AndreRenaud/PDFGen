FROM ubuntu:24.04

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get install -y dirmngr

RUN apt-get update && apt-get install --no-install-recommends -y \
	ca-certificates \
	clang \
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

# clang-format is pinned so that CI and developer machines format identically;
# the Ubuntu package (v18) formats compound literals differently to v19+
RUN python3 -m pip install --break-system-packages cpp-coveralls clang-format>=22.1

# Install Infer
RUN mkdir -p /opt && curl -L https://github.com/facebook/infer/releases/download/v1.2.0/infer-linux-x86_64-v1.2.0.tar.xz | tar -C /opt -x -J
ENV PATH $PATH:/opt/infer-linux-x86_64-v1.2.0/bin/

RUN apt-get clean
