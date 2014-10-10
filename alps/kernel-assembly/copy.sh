while read line; do
     INCREMENT=$line
done < ./current.ver

read -p "Increment version numer or use current version: $INCREMENT (y for increment)? " 
if [[ $REPLY =~ ^[Yy]$ ]]
then
  INCREMENT=$((INCREMENT + 1)) 
  echo $INCREMENT > current.ver
fi

echo $INCREMENT > current.ver

echo "Using version $INCREMENT ..."

adb push ./f3_testkernel_v41.zip /sdcard/f3_kernel_6.$INCREMENT-4.1.zip
adb push ./f3_testkernel_v42.zip /sdcard/f3_kernel_6.$INCREMENT-4.2.zip

echo "Done."
