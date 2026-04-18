FROM ubuntu:24.04

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get install -y dirmngr

RUN dpkg --add-architecture i386
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

# Install acroread
RUN apt-get install -y --no-install-recommends \
	libgtk2.0-0:i386 \
	libxml2:i386 \
	libcanberra-gtk-module:i386 \
	gtk2-engines-murrine:i386 \
	libatk-adaptor:i386 \
	axel

RUN axel -q -n 16 -o /tmp/adobe.deb http://ardownload.adobe.com/pub/adobe/reader/unix/9.x/9.5.5/enu/AdbeRdr9.5.5-1_i386linux_enu.deb && \
	dpkg -i /tmp/adobe.deb || apt-get install -fy && \
	rm /tmp/adobe.deb

RUN apt-get clean
