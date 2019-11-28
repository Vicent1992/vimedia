#include "v4l2_memory.h"

void errno_debug(const char *s)
{
	fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
}

int xioctl(int fh, int request, void *arg)
{
	int ret;
	do {
		ret = ioctl(fh, request, arg);
	} while (-1 == ret && EINTR == errno);
	return ret;
}

void init_read(vi_v4l2_info_t * cap_info, unsigned int buffer_size)
{
	cap_info->buffers[0].bytesused = buffer_size;
	cap_info->buffers[0].m.userptr = (unsigned long)malloc(buffer_size);
    if (!cap_info->buffers[0].m.userptr) {
        fprintf(stderr, "Out of memory\\n");
        exit(EXIT_FAILURE);
    }
}

void deinit_read(vi_v4l2_info_t * cap_info)
{
	free((void *)cap_info->buffers[0].m.userptr);
}

void init_mmap(vi_v4l2_info_t * cap_info)
{
    for (int i = 0; i < cap_info->buffers_num; ++i) {
        cap_info->buffers[i].type   = cap_info->buf_type;
        cap_info->buffers[i].memory = V4L2_MEMORY_MMAP;
        cap_info->buffers[i].index  = i;
	    if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == cap_info->buf_type) {
            cap_info->buffers[i].m.planes = cap_info->planes[i];
            cap_info->buffers[i].length = FMT_NUM_PLANES;
        }

		device_querybuf(cap_info->video_dev_fd, &cap_info->buffers[i]);
        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == cap_info->buf_type) {
            cap_info->buffers[i].length = cap_info->buffers[i].m.planes[0].length;
            cap_info->buffers[i].m.planes[0].m.userptr = (unsigned long)mmap(NULL,
                                 cap_info->buffers[i].m.planes[0].length,
                                 PROT_READ | PROT_WRITE, MAP_SHARED,
                                 cap_info->video_dev_fd,
                                 cap_info->buffers[i].m.planes[0].m.mem_offset);
	        if (MAP_FAILED == (void *)cap_info->buffers[i].m.planes[0].m.userptr)
	            errno_debug("mplane mmap");
        } else {
            cap_info->buffers[i].length = cap_info->buffers[i].length;
            cap_info->buffers[i].m.userptr = (unsigned long)mmap(NULL,
                                 cap_info->buffers[i].length,
                                 PROT_READ | PROT_WRITE, MAP_SHARED,
                                 cap_info->video_dev_fd,
                                 cap_info->buffers[i].m.offset);
	        if (MAP_FAILED == (void *)cap_info->buffers[i].m.userptr)
	            errno_debug("mmap");
        }
    }
}

void deinit_mmap(vi_v4l2_info_t * cap_info)
{
	for (int i = 0; i < cap_info->buffers_num; ++i) {
        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == cap_info->buf_type) {
			if (-1 == munmap((void *)cap_info->buffers[i].m.planes[0].m.userptr,
				cap_info->buffers[i].length))
				errno_debug("mplane munmap");
        } else {
			if (-1 == munmap((void *)cap_info->buffers[i].m.userptr,
				cap_info->buffers[i].length))
				errno_debug("munmap");
		}
	}
}

void init_userp(vi_v4l2_info_t * cap_info, unsigned int buffer_size)
{
    for (int i = 0; i < cap_info->buffers_num; ++i) {
		cap_info->buffers[i].type = cap_info->buf_type;
		cap_info->buffers[i].memory = V4L2_MEMORY_USERPTR;
		cap_info->buffers[i].index = i;
		if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == cap_info->buf_type) {
			cap_info->buffers[i].length = FMT_NUM_PLANES;
			cap_info->buffers[i].m.planes = cap_info->planes[i];
			cap_info->buffers[i].m.planes[0].m.userptr = (unsigned long)malloc(buffer_size);
			cap_info->buffers[i].m.planes[0].length = buffer_size;
	        if (!cap_info->buffers[i].m.planes[0].m.userptr) {
	            fprintf(stderr, "Out of memory\\n");
	            exit(EXIT_FAILURE);
	        }
		} else {
			cap_info->buffers[i].m.userptr = (unsigned long)malloc(buffer_size);
			cap_info->buffers[i].length = buffer_size;
			if (!cap_info->buffers[i].m.userptr) {
				fprintf(stderr, "Out of memory\\n");
				exit(EXIT_FAILURE);
			}
		}
    }
}

void deinit_userp(vi_v4l2_info_t * cap_info)
{
	for (int i = 0; i < cap_info->buffers_num; ++i) {
		if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == cap_info->buf_type) {
			if (cap_info->buffers[i].m.planes[0].m.userptr)
				free((void *)cap_info->buffers[i].m.planes[0].m.userptr);
		} else {
			if (cap_info->buffers[i].m.userptr)
				free((void *)cap_info->buffers[i].m.userptr);
		}
	}
}

#ifdef USE_ROCKCHIP_DRM

void init_drm(vi_v4l2_info_t * cap_info)
{
    int ret = 0;
	RockchipRga rkRga;
	rkRga.RkRgaInit();
	for (int i = 0; i < cap_info->buffers_num; ++i) {
		ret = rkRga.RkRgaGetAllocBuffer(&cap_info->bos[i],
			                            cap_info->width,
			                            cap_info->height, 32);
		ret = rkRga.RkRgaGetMmap(&cap_info->bos[i]);
	}
    rkRga.RkRgaDeInit();

    for (int i = 0; i < cap_info->buffers_num; ++i) {
		cap_info->buffers[i].type = cap_info->buf_type;
		cap_info->buffers[i].memory = V4L2_MEMORY_USERPTR;
		cap_info->buffers[i].index = i;
		if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == cap_info->buf_type) {
			cap_info->buffers[i].length = FMT_NUM_PLANES;
			cap_info->buffers[i].m.planes = cap_info->planes[i];
			cap_info->buffers[i].m.planes[0].m.userptr = (unsigned long)cap_info->bos[i].ptr;
			cap_info->buffers[i].m.planes[0].length = cap_info->bos[i].size;
	        if (!cap_info->buffers[i].m.planes[0].m.userptr) {
	            fprintf(stderr, "Out of memory\\n");
	            exit(EXIT_FAILURE);
	        }
		} else {
			cap_info->buffers[i].m.userptr = (unsigned long)cap_info->bos[i].ptr;
			cap_info->buffers[i].length = cap_info->bos[i].size;
			if (!cap_info->buffers[i].m.userptr) {
				fprintf(stderr, "Out of memory\\n");
				exit(EXIT_FAILURE);
			}
		}
    }
}

void deinit_drm(vi_v4l2_info_t * cap_info)
{
    int ret;
	RockchipRga rkRga;
	rkRga.RkRgaInit();
    for (int i = 0; i < cap_info->buffers_num; ++i) {
		bo_t bo;
		ret = rkRga.RkRgaUnmap(&cap_info->bos[i]);
		ret = rkRga.RkRgaFree(&cap_info->bos[i]);
    }
    rkRga.RkRgaDeInit();
}

#endif

int check_io_method(enum io_method io, unsigned int capabilities)
{
	switch (io) {
	case IO_METHOD_READ:
		if (!(capabilities & V4L2_CAP_READWRITE)) {
		    fprintf(stderr, "Not support read i/o\n");
			return -1;
		}
	break;

	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
#ifdef USE_ROCKCHIP_DRM
	case IO_METHOD_DRM:
#endif
		if (!(capabilities & V4L2_CAP_STREAMING)) {
			fprintf(stderr, "Not support streaming i/o\n");
			return -1;
		}
	break;
	}
	return 0;
}

int init_io_method(vi_v4l2_info_t * cap_info, unsigned int size)
{
	switch (cap_info->io) {
	case IO_METHOD_READ:
		init_read(cap_info, size);
	break;
	case IO_METHOD_MMAP:
		init_mmap(cap_info);
	break;
	case IO_METHOD_USERPTR:
		init_userp(cap_info, size);
	break;
#ifdef USE_ROCKCHIP_DRM
	case IO_METHOD_DRM:
		init_drm(cap_info);
	break;
#endif
	}
}

int deinit_io_method(vi_v4l2_info_t * cap_info)
{
	switch (cap_info->io) {
	case IO_METHOD_READ:
		deinit_read(cap_info);
	break;
	case IO_METHOD_MMAP:
		deinit_mmap(cap_info);
	break;
	case IO_METHOD_USERPTR:
		deinit_userp(cap_info);
	break;
#ifdef USE_ROCKCHIP_DRM
	case IO_METHOD_DRM:
		deinit_drm(cap_info);
	break;
#endif
	}
}

int get_memory_type_by_io(enum io_method io)
{
	switch (io) {
	case IO_METHOD_MMAP:
		return V4L2_MEMORY_MMAP;
	break;
	case IO_METHOD_USERPTR:
		return V4L2_MEMORY_USERPTR;
	break;
#ifdef USE_ROCKCHIP_DRM
	case IO_METHOD_DRM:
		return V4L2_MEMORY_USERPTR;
	break;
#endif
	}
	return 0;
}

