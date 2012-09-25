#!/bin/bash
#
# Copyright (C) 2011-2012 Pasquale Convertini    aka psquare ([email="psquare.dev@gmail.com"]psquare.dev@gmail.com[/email])
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# [url="http://www.gnu.org/licenses/gpl-2.0.txt"]http://www.gnu.org/licenses/gpl-2.0.txt[/url]
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# Fix for tablet ZT280 Zenithink 
# Giacomo Giacalone aka morfes ([url="http://pointnext.blogspot.it/"]http://pointnext.blogspot.it/[/url])
#
# 

UIMAGE=ZT280.recovery

echo '>>>>> Remove header from ZT280.recovery'
IMAGEOLDLZMA='Image.lzma'
dd if=${UIMAGE} bs=1 skip=128 of=${IMAGEOLDLZMA}

echo '>>>>> Extracting kernel from recovery'
IMAGE='Image'
unlzma < ${IMAGEOLDLZMA} > ${IMAGE}

#echo "Extracting config from recovery"
#PRECONFIG=`grep -a -b -m 1 -o -P '\x1F\x8B\x08' ${IMAGE} | cut -f 1 -d :`
#dd if=${IMAGE} bs=1 skip=$PRECONFIG | gunzip > config

##////////////////////////////////////////////////////////////
#Extracting initramfs
# [url="http://www.garykessler.net/library/file_sigs.html"]www.garykessler.net/library/file_sigs.html[/url]
# The end of the cpio archive is recognized with an empty file named 'TRAILER!!!' = '54 52 41 49 4C 45 52 21 21 21' (hexadecimal)
##////////////////////////////////////////////////////////////
#
echo '>>>>> Extracting initramfs from recovery'
CPIOSTART=`grep -a -b -m 1 -o '070701' ${IMAGE} | head -1 | cut -f 1 -d :`
CPIOEND=`grep -a -b -m 1 -o -P '\x54\x52\x41\x49\x4C\x45\x52\x21\x21\x21\x00\x00\x00\x00\x00\x00\x00' ${IMAGE} | head -1 | cut -f 1 -d :`
CPIOEND=$((CPIOEND + 11 + 3))
CPIOSIZE=$((CPIOEND - CPIOSTART))
if [ "$CPIOSIZE" -le '0' ]
 then
  echo 'initramfs.cpio not found'
  exit
fi
dd if=${IMAGE} bs=1 skip=$CPIOSTART count=$CPIOSIZE > initramfs.cpio
OLDINITRAMFSDIR='initramfs'
mkdir -p $OLDINITRAMFSDIR
cd $OLDINITRAMFSDIR
cpio -v -i --no-absolute-filenames < ../initramfs.cpio
