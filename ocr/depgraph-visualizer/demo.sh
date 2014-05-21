#!/bin/bash

DEMOS_PATH=./demos
PROG=test1.py

while [ true ] ; do
    for demo in ${DEMOS_PATH}/*.xml ;
    do
        echo "******** running $demo *********"
        python $PROG $demo

        sleep 3
    done
done

