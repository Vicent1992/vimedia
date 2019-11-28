#!/bin/sh


CPP=/home/cl/workplace/1109/buildroot/output/rockchip_puma/host/bin/arm-linux-g++
SYSROOT=/home/cl/workplace/1109/buildroot/output/rockchip_puma/host/arm-buildroot-linux-gnueabihf/sysroot

${CPP} \
main.cc \
camera/rkisp.c \
camera/v4l2_capture.c \
camera/v4l2_memory.c \
camera/v4l2_device.c \
display/drm_display.c \
-o vimedia \
-I./camera/ \
-I${SYSROOT}/usr/include/ \
-I${SYSROOT}/usr/include/libdrm/ \
-L${SYSROOT}/usr/lib/ \
-ldl -lrkisp -lrga -ldrm \
-std=c++11 -DLINUX -DUSE_ROCKCHIP_DRM 

cp vimedia ../../download/
