#!/bin/bash


if [ ! -d third_party/build ] ; then
	mkdir third_party/build
fi

cd third_party
MYPWD=${PWD}/build

export CFLAGS=-I${MYPWD}/include
export CPPFLAGS=-I${MYPWD}/include
export LDFLAGS=-L${MYPWD}/lib

function get_from_git()
{
	gitpath="";

	echo "--- "${1}" ---";
	case ${1} in
		czmq)  gitpath="https://github.com/zeromq/czmq.git"
			;;
		zeromq)  gitpath="https://github.com/zeromq/zeromq4-x.git"
			;;
		libsodium)  gitpath="https://github.com/jedisct1/libsodium.git"
			;;
		nanomsg)  gitpath="https://github.com/nanomsg/nanomsg.git"
			;;
		msgpack)  gitpath="https://github.com/msgpack/msgpack-c.git"
			;;
		*) echo "Not Valid path"
			exit 1
			;;
	esac

	if [ ! -d ${1} ] ; then
		git clone ${gitpath} ${1}
	else
		cd ${1} && \
		git pull && \
		cd ..
	fi

	if [ $? -ne 0 ] ; then
		echo "git error"
		exit 1
	fi
}

function compile()
{
	echo "--- "${1}" ---";
	cd ${1} && \
	./autogen.sh || ./bootstrap && \
	./configure --prefix=${MYPWD} && \
	make -j5 && \
	make install && \
	cd ..

	if [ $? -ne 0 ] ; then
		echo "compile error"
		exit 1
	fi
}

echo "------------------ GIT ------------------";

if [ -z ${1} ] ; then #OffLine mode
	get_from_git libsodium
	get_from_git zeromq
	get_from_git czmq
fi

echo "------------------ Compile ------------------";

compile libsodium
compile zeromq
compile czmq

echo "------------------ Mariadb Client ------------------";

mkdir build/mariadb_cmake
cd build/mariadb_cmake
cmake -DCMAKE_INSTALL_PREFIX=${MYPWD} ../../mariadb-native-client/
make -j5
make install

cd ../../..

