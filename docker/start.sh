#/bin/sh

SCRIPT=`realpath -s $0`
FOLDER=`dirname $SCRIPT`

docker build -t $USER/eosio --build-arg USER=$USER $FOLDER
#docker run -it --rm --name eosio --hostname eosio -v $FOLDER/../:$HOME/eosio/ -w $HOME/eosio/ -e TERM=$TERM $USER/eosio bash
docker run -it --rm --hostname eosio -v $FOLDER/../:$HOME/eosio/ -w $HOME/eosio/ -e TERM=$TERM $USER/eosio bash

