#!/bin/bash


#sudo insmod  comedi/drivers/accesisa.ko 

COMEDI_DEV_ROOT=$(pwd)
COMEDI_GIT=comedi_git



function load_comedi_test() { 
    #sudo modprobe comedi comedi_num_legacy_minors=8         
    #sudo modprobe comedi_test
    sudo insmod ${COMEDI_DEV_ROOT}/${COMEDI_GIT}/comedi/comedi.ko comedi_num_legacy_minors=8        
    sudo insmod ${COMEDI_DEV_ROOT}/${COMEDI_GIT}/comedi/drivers/comedi_fc.ko 
    sudo insmod ${COMEDI_DEV_ROOT}/${COMEDI_GIT}/comedi/drivers/comedi_test.ko 
    sudo /usr/sbin/comedi_config -v /dev/comedi0 comedi_test --read-buffer 100 --write-buffer 100 20000,20000
}

function unload_comedi_test() { 
    sudo rmmod comedi comedi_test comedi_fc
}

#
# Load the temp accesisa
#

function build_acces() { 
    dir=$(pwd)
    cd COMEDI_GIT && make
    cd ${dir}
}

function load_acces() { 
    sudo insmod ${COMEDI_DEV_ROOT}/${COMEDI_GIT}/comedi/comedi.ko comedi_num_legacy_minors=8        
    sudo insmod ${COMEDI_DEV_ROOT}/${COMEDI_GIT}/comedi/drivers/accesisa.ko 
    sudo /usr/sbin/comedi_config -v /dev/comedi0 accesio_idio16 0x300,0,0
    sudo chmod a+rwx /dev/comedi0
}

function unload_acces() { 
    sudo rmmod accesisa comedi comedi_test
}

function try_build() { 
    make -I/home/jdamon/Projects/OnGoing/ACCES/first_comedi_driver/${COMEDI_GIT}/comedi -C /lib/modules/3.5.0-52-generic/build M=/home/jdamon/Projects/OnGoing/ACCES/first_comedi_driver/${COMEDI_GIT}/comedi CC="gcc -I/home/jdamon/Projects/OnGoing/ACCES/first_comedi_driver/${COMEDI_GIT}/ -I/home/jdamon/Projects/OnGoing/ACCES/first_comedi_driver/${COMEDI_GIT}/include  " modules
}

function retry_acces() { 
    unload_acces
    try_build
    load_acces
}
