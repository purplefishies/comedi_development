#!/bin/bash


#sudo insmod  comedi/drivers/accesisa.ko 

COMEDI_DEV_ROOT=$(pwd)
COMEDI_GIT=comedi_git
COMEDI_DRIVERS=${COMEDI_DEV_ROOT}/${COMEDI_GIT}/comedi/drivers/



function load_comedi_test() { 
    sudo insmod ${COMEDI_DEV_ROOT}/${COMEDI_GIT}/comedi/comedi.ko comedi_num_legacy_minors=8        
    sudo insmod ${COMEDI_DEV_ROOT}/${COMEDI_GIT}/comedi/drivers/comedi_fc.ko 
    sudo insmod ${COMEDI_DEV_ROOT}/${COMEDI_GIT}/comedi/drivers/comedi_test.ko 
    sudo /usr/sbin/comedi_config -v /dev/comedi0 comedi_test --read-buffer 100 --write-buffer 100 20000,20000
}

function unload_comedi_test() { 
    sudo rmmod  comedi_test comedi_fc comedi 
}

#=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
# Load the temp accesisa
#=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
function build_acces() { 


    dir=$(pwd)
    cd ${COMEDI_DEV_ROOT}/${COMEDI_GIT}
    make -I/home/jdamon/Projects/comedi_development/comedi_git/comedi -C /lib/modules/3.13.0-46-generic/build M=/home/jdamon/Projects/comedi_development/comedi_git/comedi CC="gcc -I/home/jdamon/Projects/comedi_development/comedi_git/include -I/home/jdamon/Projects/comedi_development/comedi_git/include -I/home/jdamon/Projects/comedi_development/comedi_git/inc-wrap " modules

    cd ${dir}
}

function refresh_build() {
    cd ${COMEDI_DEV_ROOT}/${COMEDI_GIT}
    echo "Reconfiguring"
    configure > log.$(date '+%Y%m%d%H%M%S' )
    cd -
}

function clean_acces() {
    cd ${COMEDI_DRIVERS} 
    rm -f acces*.mod.*
    rm -f acces*.ko
    rm -f acces*.o
    cd -
}


function load_custom_module() { 
    module=$1
    sudo insmod ${COMEDI_DEV_ROOT}/${COMEDI_GIT}/comedi/comedi.ko comedi_num_legacy_minors=8        
    sudo insmod ${COMEDI_DEV_ROOT}/${COMEDI_GIT}/comedi/drivers/${module}.ko 
    sudo /usr/sbin/comedi_config -v /dev/comedi0 accesio_idio16 0x300,0,0
    sudo chmod a+rwx /dev/comedi0
}


function load_acces() { 
    sudo insmod ${COMEDI_DEV_ROOT}/${COMEDI_GIT}/comedi/comedi.ko comedi_num_legacy_minors=8        
    sudo insmod ${COMEDI_DEV_ROOT}/${COMEDI_GIT}/comedi/drivers/accesisa.ko 
    sudo /usr/sbin/comedi_config -v /dev/comedi0 accesio_idio16 0x300,0,0
    sudo chmod a+rwx /dev/comedi0
}

function unload_acces() { 
    sudo rmmod accesisa comedi
}

function try_build() { 
    make -I/home/jdamon/Projects/OnGoing/ACCES/first_comedi_driver/${COMEDI_GIT}/comedi -C /lib/modules/3.5.0-52-generic/build M=/home/jdamon/Projects/OnGoing/ACCES/first_comedi_driver/${COMEDI_GIT}/comedi CC="gcc -I/home/jdamon/Projects/OnGoing/ACCES/first_comedi_driver/${COMEDI_GIT}/ -I/home/jdamon/Projects/OnGoing/ACCES/first_comedi_driver/${COMEDI_GIT}/include  " modules
}

function retry_acces() { 
    unload_acces
    try_build
    load_acces
}

function demo_it() { 
    cd comedilib/demo
    cmd -v -p -n 2 -N 40
    cd -
}
