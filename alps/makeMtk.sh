export PATH=~/projects/tools/arm-linux-androideabi-4.6/bin:~/projects/tools/lz4:$PATH
export ARCH=arm
export SUBARCH=arm
export CROSS_COMPILE=arm-linux-androideabi-

./makeMtk -t s7511 r drvgen k

