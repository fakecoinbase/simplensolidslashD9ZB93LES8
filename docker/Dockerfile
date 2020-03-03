FROM ubuntu:18.04

ARG USER=ubuntu

RUN apt-get update
RUN apt-get upgrade -y
RUN apt-get install -y make nano sudo wget

RUN apt-get install -y libcurl3-gnutls libtinfo5 libusb-1.0-0
RUN wget http://security.ubuntu.com/ubuntu/pool/main/i/icu/libicu60_60.2-3ubuntu3_amd64.deb
RUN wget https://github.com/EOSIO/eos/releases/download/v2.0.3/eosio_2.0.3-1-ubuntu-18.04_amd64.deb
RUN wget https://github.com/EOSIO/eosio.cdt/releases/download/v1.7.0/eosio.cdt_1.7.0-1-ubuntu-18.04_amd64.deb
RUN dpkg -i libicu60_60.2-3ubuntu3_amd64.deb
RUN apt-get install -y 
RUN dpkg -i eosio_2.0.3-1-ubuntu-18.04_amd64.deb
RUN dpkg -i eosio.cdt_1.7.0-1-ubuntu-18.04_amd64.deb

COPY entrypoint.sh /

RUN useradd -U -G sudo -m -s /bin/bash ${USER}
RUN echo "${USER} ALL=(ALL:ALL) NOPASSWD: ALL" > /etc/sudoers.d/${USER}
USER ${USER}
ENV USER=${USER}
WORKDIR /home/${USER}/
ENTRYPOINT /entrypoint.sh && /bin/bash

