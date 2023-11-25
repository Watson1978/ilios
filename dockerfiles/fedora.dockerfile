FROM fedora

RUN \
  dnf install -y \
    git \
    gcc \
    g++ \
    cmake \
    make \
    redhat-rpm-config \
    ruby-devel \
    openssl-devel \
    zlib-devel \
    sudo \
    which && \
  dnf clean all

WORKDIR /opt/ilios
