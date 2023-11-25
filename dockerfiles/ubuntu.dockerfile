FROM ubuntu

RUN \
  apt-get update && \
  apt-get install -y \
    git \
    gcc \
    g++ \
    cmake \
    make \
    ruby-bundler \
    ruby-dev \
    zlib1g-dev \
    libssl-dev \
    sudo

WORKDIR /opt/ilios
