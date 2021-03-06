#!/usr/bin/env bash

RELEASE=4.0.0
RELEASE_VERSION_NAME=40000

VERSION=$(date +%Y%m%d_%H%M)
TOOLCHAIN_NAME="toolchain_${RELEASE_VERSION_NAME}_${VERSION}"
mkdir ${TOOLCHAIN_NAME}
cd ${TOOLCHAIN_NAME}

mkdir bin
cp ../build/bin/clang bin/
cp ../build/bin/llc bin/
cp ../build/bin/llvm-dis bin/
cp ../build/bin/llvm-as bin/
cp ../build/bin/llvm-spirv bin/
cp ../SPIRV-Tools/build/tools/spirv-as bin/
cp ../SPIRV-Tools/build/tools/spirv-dis bin/
cp ../SPIRV-Tools/build/tools/spirv-val bin/

cp -R ../build/lib/clang/${RELEASE}/include clang

cp -R ../libcxx/include libcxx
cp ../libcxx/{LICENSE.TXT,CREDITS.TXT} libcxx/

BUILD_PLATFORM=$(uname | tr [:upper:] [:lower:])
case ${BUILD_PLATFORM} in
	"mingw"*)
		# also copy all necessary .dll files
		cp "/mingw64/bin/libgcc_s_seh-1.dll" bin/
		cp "/mingw64/bin/libstdc++-6.dll" bin/
		cp "/mingw64/bin/libwinpthread-1.dll" bin/
		cp "/mingw64/bin/zlib1.dll" bin/
		;;
	*)
		;;
esac

cd ..


zip -qr ${TOOLCHAIN_NAME}.zip ${TOOLCHAIN_NAME}
zip_ret_code=$?
if [ ${zip_ret_code} -ne 0 ]; then
	echo "failed to create toolchain archive!"
	exit 1
else
	echo "sucessfully created toolchain archive: ${TOOLCHAIN_NAME}.zip"
fi
