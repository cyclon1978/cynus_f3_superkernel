rm ./boot.img-kernel.img
rm ./system/lib/modules/*.ko
cp ../kernel/out/kernel_s7511.bin ./boot.img-kernel.img
cp ../kernel/out/mediatek/platform/mt6577/kernel/drivers/m4u/m4u.ko ./system/lib/modules/m4u.ko
cp ../kernel/out/drivers/staging/zram/zram.ko ./system/lib/modules/zram.ko
# cp ../kernel/out/lib/lzo/lzo_compress.ko ./system/lib/modules/lzo_compress.ko
# cp ../kernel/out/lib/lzo/lzo_decompress.ko ./system/lib/modules/lzo_decompress.ko
~/projects/tools/mtk-tools/repack-MT65xx.pl -boot boot.img-kernel.img boot.img-ramdisk boot.img
zip -u -r ./f3_testkernel.zip ./boot.img system
