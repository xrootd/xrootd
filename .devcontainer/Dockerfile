FROM debian:13

RUN apt-get update -y

RUN apt-get install -y --no-install-recommends tzdata

ENV TZ=Europe/CET
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
RUN dpkg-reconfigure --frontend noninteractive tzdata

RUN <<EOF
        apt-get install -y --no-install-recommends ca-certificates apt-transport-https devscripts equivs sudo git ssh
        apt-get install -y --no-install-recommends git vim-tiny build-essential cmake make gdb valgrind cppcheck clang clang-format llvm libclang-rt-dev libgtest-dev
        apt-get clean
EOF

RUN <<EOF
        git clone https://github.com/xrootd/xrootd.git
        echo yes | mk-build-deps --install --remove xrootd/debian/control
        apt-get clean
        rm -rf xrootd
EOF

RUN useradd -m -s /bin/bash xrootd
RUN echo 'export PATH="$PATH:/workspaces/xrootd/build/bin"' >> /home/xrootd/.bashrc

USER xrootd
WORKDIR /home/xrootd
