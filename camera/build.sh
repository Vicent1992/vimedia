#../../buildroot/output/rockchip_puma/host/bin/arm-linux-gcc camera_memory.c camera_device.c camera_capture.c main.c -o camera_capture && cp camera_capture ../../download/


SYSROOT=/home/cl/workplace/1109/buildroot/output/rockchip_puma/host/arm-buildroot-linux-gnueabihf/sysroot

../../buildroot/output/rockchip_puma/host/bin/arm-linux-g++ \
rkisp.c \
camera_memory.c \
camera_device.c \
camera_capture.c \
main.c \
-o camera_capture \
-I${SYSROOT}/usr/include/ \
-L${SYSROOT}/usr/lib/ \
-ldl -lrkisp \
-std=c++11 -DLINUX 

cp camera_capture ../../download/
