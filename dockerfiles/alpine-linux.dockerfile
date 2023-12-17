FROM alpine

RUN \
  apk update && apk upgrade && \
  apk add \
    bash \
    build-base \
    git \
    gcc \
    cmake \
    make \
    ruby \
    ruby-dev \
    openssl \
    openssl-dev \
    zlib \
    zlib-dev \
    sudo \
    which

WORKDIR /opt/ilios
