TOOLCHAIN=$(pwd)/../toolchain/arm-cortex_a9-linux-gnueabihf-linaro_4.9.1-2014.05/bin

export PATH=$TOOLCHAIN:$PATH
export ARCH=arm
export CROSS_COMPILE=arm-eabi-

./makeMtk -t s7511 mrproper k
./makeMtk -t s7511 c k
