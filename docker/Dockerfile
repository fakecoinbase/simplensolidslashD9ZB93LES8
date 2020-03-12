FROM ubuntu:18.04

# default account user
ARG USER=ubuntu

# installs software dependencies
RUN apt-get update
RUN apt-get upgrade -y
RUN apt-get install -y \
	autoconf automake autotools-dev clang cmake curl doxygen g++ git graphviz \
	libbz2-dev libcurl4-gnutls-dev libgmp3-dev libicu-dev libssl-dev libtool \
	libusb-1.0-0-dev llvm-7-dev make nano pkg-config python2.7 python2.7-dev \
	python3 python3-dev python3-pip ruby sudo wget zlib1g-dev

# installs the vanilla eosio stack
#RUN apt-get install -y libcurl3-gnutls libtinfo5 libusb-1.0-0
#RUN wget http://security.ubuntu.com/ubuntu/pool/main/i/icu/libicu60_60.2-3ubuntu3_amd64.deb
#RUN wget https://github.com/EOSIO/eos/releases/download/v2.0.3/eosio_2.0.3-1-ubuntu-18.04_amd64.deb
#RUN wget https://github.com/EOSIO/eosio.cdt/releases/download/v1.7.0/eosio.cdt_1.7.0-1-ubuntu-18.04_amd64.deb
#RUN dpkg -i libicu60_60.2-3ubuntu3_amd64.deb
#RUN dpkg -i eosio_2.0.3-1-ubuntu-18.04_amd64.deb
#RUN dpkg -i eosio.cdt_1.7.0-1-ubuntu-18.04_amd64.deb

# installs the customized eosio stack
RUN git clone --recursive https://github.com/simplensolid/eos-D9ZB93LES8/ /eos/
RUN git clone --recursive https://github.com/simplensolid/eosio.cdt-D9ZB93LES8/ /eosio.cdt/
RUN cd /eos/ && ./scripts/eosio_build.sh -y -i /usr/local/eos/ && cd build && make install && ln -s /usr/local/eos/bin/* /usr/local/bin/
RUN cd /eosio.cdt/ && mkdir build && cd build && cmake .. && make install -j4
RUN rm -rf /eos/ /eosio.cdt/

# install tester dependencies
RUN pip3 install ecdsa pyyaml

# install the solidity compiler
RUN apt-get install -y npm
RUN npm i -g solc@0.6.3

# sets up the user account
RUN useradd -U -G sudo -m -s /bin/bash ${USER}
RUN echo "${USER} ALL=(ALL:ALL) NOPASSWD: ALL" > /etc/sudoers.d/${USER}
USER ${USER}
ENV USER=${USER}
WORKDIR /home/${USER}/

