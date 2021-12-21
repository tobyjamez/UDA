#!/bin/bash

module ()
{
    eval `/usr/bin/modulecmd bash $*`
}

module purge
module load cineca
module unload gnu
module load gnu/7.3.0
module load itm-openmpi/4.0.4--gnu--7.3.0
module load itm-boost/1.61.0--gnu--7.3.0
module load cmake/3.5.2

export CC=gcc
export CXX=g++

cmake -Bbuild-gcc-7 -H. -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX=$UDA_INSTALL \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_PLUGINS=help\;uda $*
