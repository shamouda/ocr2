#!/bin/bash

if [ $1 == "output" ]; then
    echo ""
    exit 0
fi

if [ $1 == "resources" ]; then
    echo "nodes=4"
    exit 0
fi
