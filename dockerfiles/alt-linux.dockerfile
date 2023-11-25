FROM alt

RUN \
  apt-get update && \
  apt-get install -y \
    gcc \
    libruby-devel \
    libpcre-devel \
    cmake \
    make \
    ruby \
    openssl \
    zlib \
    sudo \
    which

WORKDIR /opt/ilios
