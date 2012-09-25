#!/bin/sh


export MK_ROOT=out

if [ ! -d $MK_ROOT ];then
mkdir $MK_ROOT
else
rm -rf $MK_ROOT/*
fi

# H1 3n
export MK_VERSION=H1
export MK_SUBVERSION=3n
export MK_LOGO=1024_600
export MK_CONFIG=${MK_SUBVERSION}
./makeone.sh

# G0 2n
export MK_VERSION=G0
export MK_SUBVERSION=2n
export MK_LOGO=1024_600
export MK_CONFIG=${MK_SUBVERSION}
./makeone.sh

# H1 2n
export MK_VERSION=H1
export MK_SUBVERSION=2n
export MK_LOGO=1024_600
export MK_CONFIG=${MK_SUBVERSION}
./makeone.sh

# H1 2n
export MK_VERSION=H1
export MK_SUBVERSION=2n
export MK_CUSTOMER=avatek
export MK_LOGO=1024_600_avatek
export MK_CONFIG=${MK_SUBVERSION}
./makeone.sh
export MK_CUSTOMER=

# H1 2f
export MK_VERSION=H1
export MK_SUBVERSION=2f
export MK_LOGO=1024_600
export MK_CONFIG=${MK_SUBVERSION}
./makeone.sh

# H2
export MK_VERSION=H2
export MK_SUBVERSION=1n
export MK_LOGO=984_600
export MK_CONFIG=${MK_SUBVERSION}
./makeone.sh

# K0 2n
export MK_VERSION=K0
export MK_SUBVERSION=2n
export MK_LOGO=1024_600
export MK_CONFIG=${MK_SUBVERSION}
./makeone.sh

chmod 777 -R ./$MK_ROOT
