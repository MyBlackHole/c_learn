# module

ramdisk_driver.c
```shell
make

insmod ramdisk_driver.ko
sudo mkfs.ext4 /dev/ram0
mkdir /tmp/ramdisk
mount /dev/myramdisk ./ramdisk/
cd /tmp/ramdisk

root@ubuntu:ramdisk# ls
lost+found
root@ubuntu:ramdisk# 
```
