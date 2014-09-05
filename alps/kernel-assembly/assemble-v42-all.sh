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

cp ../kernel/out/kernel_s7511.bin ./boot.img-kernel.img
cp ../kernel/out/mediatek/platform/mt6577/kernel/drivers/m4u/m4u.ko ./system/lib/modules/m4u.ko
cp ../kernel/out/drivers/staging/zram/zram.ko ./system/lib/modules/zram.ko
# cp ../kernel/out/lib/lzo/lzo_compress.ko ./system/lib/modules/lzo_compress.ko
# cp ../kernel/out/lib/lzo/lzo_decompress.ko ./system/lib/modules/lzo_decompress.ko

# additional modules
cp ../kernel/out/mediatek/kernel/drivers/ccci/ccci.ko ./system/lib/modules/
cp ../kernel/out/mediatek/platform/mt6577/kernel/drivers/ccci/ccci_plat.ko ./system/lib/modules/
cp ../kernel/out/mediatek/kernel/drivers/ccmni/ccmni.ko ./system/lib/modules/
cp ../kernel/out/mediatek/platform/mt6577/kernel/drivers/devapc/devapc.ko ./system/lib/modules/
cp ../kernel/out/mediatek/platform/mt6577/kernel/drivers/devinfo/devinfo.ko ./system/lib/modules/
cp ../kernel/out/drivers/misc/eeprom/eeprom_93cx6.ko ./system/lib/modules/
cp ../kernel/out/drivers/hid/hid-logitech-dj.ko ./system/lib/modules/
cp ../kernel/out/mediatek/kernel/drivers/fmradio/mtk_fm_drv.ko ./system/lib/modules/
cp ../kernel/out/mediatek/kernel/drivers/fmradio/private/mtk_fm_priv.ko ./system/lib/modules/
cp ../kernel/out/mediatek/kernel/drivers/combo/common_mt6628/mtk_hif_sdio.ko ./system/lib/modules/
cp ../kernel/out/mediatek/kernel/drivers/combo/common_mt6628/mtk_stp_bt.ko ./system/lib/modules/
cp ../kernel/out/mediatek/kernel/drivers/combo/common_mt6628/mtk_stp_gps.ko ./system/lib/modules/
cp ../kernel/out/mediatek/kernel/drivers/combo/common_mt6628/mtk_stp_uart.ko ./system/lib/modules/
cp ../kernel/out/mediatek/kernel/drivers/combo/common_mt6628/mtk_stp_wmt.ko ./system/lib/modules/
cp ../kernel/out/mediatek/kernel/drivers/combo/common_mt6628/mtk_wmt_wifi.ko ./system/lib/modules/
cp ../kernel/out/mediatek/platform/mt6577/kernel/drivers/gpu/pvr/mtklfb.ko ./system/lib/modules/
cp ../kernel/out/mediatek/platform/mt6577/kernel/drivers/gpu/pvr/pvrsrvkm.ko ./system/lib/modules/
cp ../kernel/out/mediatek/platform/mt6577/kernel/drivers/masp/sec.ko ./system/lib/modules/
cp ../kernel/out/drivers/scsi/scsi_wait_scan.ko ./system/lib/modules/
cp ../kernel/out/drivers/scsi/scsi_tgt.ko ./system/lib/modules/
cp ../kernel/out/mediatek/platform/mt6577/kernel/drivers/videocodec/vcodec_kernel_driver.ko ./system/lib/modules/
cp ../kernel/out/mediatek/kernel/drivers/combo/drv_wlan/mt6628/wlan/wlan_mt6628.ko ./system/lib/modules/

# unknown in source: p2p_mt6628.ko

# strip modules
echo "**** Patching all built modules (.ko) in /build_result/modules/ ****"
find ./system/lib/modules/ -type f -name '*.ko' | xargs -n 1 $TOOLCHAIN/arm-eabi-strip --strip-unneeded

# fix permissions
chmod 755 ./boot.img-ramdisk/ -R

~/projects/tools/mtk-tools/repack-MT65xx.pl -boot boot.img-kernel.img boot.img-ramdisk.v42 boot.img
cp ./f3_testkernel_template.zip ./f3_testkernel.zip
zip -u -r ./f3_testkernel.zip ./boot.img system
