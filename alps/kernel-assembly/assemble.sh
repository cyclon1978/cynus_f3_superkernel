TOOLCHAIN=$(pwd)/../../toolchain/arm-cortex_a9-linux-gnueabihf-linaro_4.9.1-2014.05/bin

export PATH=$TOOLCHAIN:$PATH
export ARCH=arm
export CROSS_COMPILE=arm-eabi-

mkdir system
mkdir system/lib
mkdir system/lib/modules

# cleanup old files
rm ./boot.img-kernel.img
rm ./system/lib/modules/*.ko
rm ./f3_testkernel.zip
rm ./f3_testkernel_v41.zip
rm ./f3_testkernel_v42.zip

cp ../kernel/out/kernel_s7511.bin ./boot.img-kernel.img
cp ../kernel/out/mediatek/platform/mt6577/kernel/drivers/m4u/m4u.ko ./system/lib/modules/m4u.ko
cp ../kernel/out/drivers/staging/zram/zram.ko ./system/lib/modules/zram.ko
# cp ../kernel/out/lib/lzo/lzo_compress.ko ./system/lib/modules/lzo_compress.ko
# cp ../kernel/out/lib/lzo/lzo_decompress.ko ./system/lib/modules/lzo_decompress.ko

# strip modules
echo "**** Patching all built modules (.ko) in /build_result/modules/ ****"
find ./system/lib/modules/ -type f -name '*.ko' | xargs -n 1 $TOOLCHAIN/arm-eabi-strip --strip-unneeded

# fix permissions
chmod 755 ./boot.img-ramdisk.v41/ -R
chmod 755 ./boot.img-ramdisk.v42/ -R

../../../../tools/mtk-tools/repack-MT65xx.pl -boot boot.img-kernel.img boot.img-ramdisk.v41 boot.img
cp ./f3_testkernel_template.zip ./f3_testkernel_v41.zip
zip -u -r ./f3_testkernel_v41.zip ./boot.img system

../../../../tools/mtk-tools/repack-MT65xx.pl -boot boot.img-kernel.img boot.img-ramdisk.v42 boot.img
cp ./f3_testkernel_template.zip ./f3_testkernel_v42.zip
zip -u -r ./f3_testkernel_v42.zip ./boot.img system
