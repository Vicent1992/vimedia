#ifndef _CAMERA_DEVICE_H_
#define _CAMERA_DEVICE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#include "v4l2_memory.h"

void device_close(int dev_fd);
int device_open(const char* dev_name);
int device_querycap(int dev_fd, struct v4l2_capability *cap);
int device_cropcap(int dev_fd,
                       struct v4l2_cropcap *cropcap,
                       struct v4l2_crop *crop);
int device_setformat(int dev_fd, struct v4l2_format *fmt);
int device_getformat(int dev_fd, struct v4l2_format *fmt);
int device_streamon(int dev_fd, enum v4l2_buf_type *type);
int device_streamoff(int dev_fd, enum v4l2_buf_type *type);
int device_qbuf(int dev_fd, struct v4l2_buffer *buf);
int device_dqbuf(int dev_fd, struct v4l2_buffer *buf);
int device_request(int dev_fd, struct v4l2_requestbuffers *req);
int device_querybuf(int dev_fd, struct v4l2_buffer *buffers);

#endif