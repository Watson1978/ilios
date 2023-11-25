FROM archlinux

RUN \
  pacman --sync --noconfirm --refresh --sysupgrade && \
  pacman --sync --noconfirm \
    git \
    gcc \
    cmake \
    make \
    ruby \
    openssl \
    zlib \
    sudo \
    which

WORKDIR /opt/ilios
