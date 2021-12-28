FROM ubuntu:18.04

WORKDIR /plugin
ADD . /plugin
COPY . /plugin

RUN apt-get -qq update && \
    apt-get install -y software-properties-common && \
    add-apt-repository -y ppa:obsproject/obs-studio && \
    apt-get -qq update && \
    apt-get install -y \
    libc-dev-bin  \
    libc6-dev git \
    build-essential \
    checkinstall \
    cmake \
    obs-studio \
    qtbase5-dev \
    libpulse-dev