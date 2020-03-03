#/bin/sh

SCRIPT=`realpath $0`
FOLDER=`dirname $SCRIPT`

docker build -t $USER/eosio --build-arg USER=$USER $FOLDER
docker run -it --rm --name eosio -v $FOLDER/../:$HOME/eosio/ -w $HOME/eosio/ -e TERM=$TERM $USER/eosio bash

