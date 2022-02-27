FROM debian:stable

RUN apt-get update && apt-get install -y \
    build-essential \
    clang \
    clang-format \
    cmake \
    cmake-curses-gui \
    default-jdk \
    git \
    htop \
    libunwind-dev \
    procps \
    python3 \
    python3-pip \
    sed \
    slurm-wlm \
    tar \
    unzip \
    vim \
    wget \
    && rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir \
    click \
    jupyter \
    matplotlib \
    more-itertools \
    numpy \
    pandas \
    pymongo \
    seaborn \
    sh \
    tabulate \
    tqdm

WORKDIR /opt
RUN git clone https://github.com/ogdf/ogdf.git ogdf \
    && cd /opt/ogdf \
    && git checkout catalpa-202002

WORKDIR /root
RUN git clone https://github.com/N-Coder/pc-tree.git pc-tree \
    && cd pc-tree/ \
    && git submodule update --init --recursive

RUN dd if=/dev/urandom bs=1 count=1024 > /etc/munge/munge.key \
    && chown munge:munge /etc/munge/munge.key \
    && chmod 400 /etc/munge/munge.key \
    && mkdir /run/munge \
    && chown munge:munge /run/munge

CMD /bin/bash
