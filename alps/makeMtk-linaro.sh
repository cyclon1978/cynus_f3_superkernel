export PATH=~/projects/tools/Arm_A9_linaro_4.8.3/bin:~/projects/tools/lz4:$PATH
export ARCH=arm
export CROSS_COMPILE=arm-gnueabi-

./makeMtk -t s7511 r drvgen k

