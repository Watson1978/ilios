FROM ubuntu:22.04

RUN apt update && \
    apt install -y tzdata sudo && \
    apt install -y curl cmake make gcc g++ git bzip2 zlib1g-dev libgdbm-dev libreadline-dev libffi-dev libssl-dev libyaml-dev && \
    git clone --depth 1 https://github.com/rbenv/ruby-build.git && \
    cd ruby-build/bin && ./ruby-build 3.0.6 /usr/local

WORKDIR /opt/ilios
