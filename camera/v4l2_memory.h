#ifndef _CAMERA_MEMORY_H_
#define _CAMERA_MEMORY_H_

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

#ifdef USE_ROCKCHIP_DRM
#include <rga/RockchipRga.h>
#endif

#include "v4l2_device.h"

#define FMT_NUM_PLANES 1

#define CLEAR(x) memset(&(x), 0, sizeof(x))

enum io_method {
    IO_METHOD_READ,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
#ifdef USE_ROCKCHIP_DRM
    IO_METHOD_DRM,
#endif
};


typedef struct vi_v4l2_info {
 	const char			*video_dev_name;
 	const char			*rkisp_dev_name;
 	int             	video_dev_fd;
 	int             	format;
 	int					width;
 	int					height;
 	int					bpp;
 	enum io_method		io;
 	struct v4l2_buffer  *buffers;
 	struct v4l2_plane   **planes;
	bo_t                *bos;
 	unsigned int    	buffers_num;
	enum v4l2_buf_type	buf_type;
} vi_v4l2_info_t;

void errno_debug(const char *s);
int xioctl(int fh, int request, void *arg);
void init_read(vi_v4l2_info_t * cap_info, unsigned int buffer_size);
void init_mmap(vi_v4l2_info_t * cap_info);
void init_userp(vi_v4l2_info_t * cap_info, unsigned int buffer_size);
#ifdef USE_ROCKCHIP_DRM
//void bo_to_buffer(buffer * buf, bo_t * bo);
//void buffer_to_bo(buffer * buf, bo_t * bo);
#endif

int check_io_method(enum io_method io, unsigned int capabilities);
int init_io_method(vi_v4l2_info_t * cap_info, unsigned int size);
int deinit_io_method(vi_v4l2_info_t * cap_info);
int get_memory_type_by_io(enum io_method io);

#endif
