#include "v4l2_capture.h"

int request_capture(vi_v4l2_info_t * cap_info)
{
    struct v4l2_requestbuffers req;
	req.count = 4;
    req.type = cap_info->buf_type;
    req.memory = get_memory_type_by_io(cap_info->io);

	device_request(cap_info->video_dev_fd, &req);

	cap_info->buffers = (struct v4l2_buffer *)calloc(req.count, 
                         sizeof(struct v4l2_buffer));
    if (!cap_info->buffers) {
        fprintf(stderr, "Out of memory\\n");
        exit(EXIT_FAILURE);
    }
	cap_info->buffers_num = req.count;
	cap_info->bos = (struct bo *)calloc(cap_info->buffers_num, sizeof(struct bo));

	if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == cap_info->buf_type) {
		cap_info->planes = (struct v4l2_plane **)calloc(
							cap_info->buffers_num, 
							sizeof(struct v4l2_plane));
		for (int i = 0; i < cap_info->buffers_num; ++i) {
	    	cap_info->planes[i] = (struct v4l2_plane *)calloc(
				                   FMT_NUM_PLANES,
				                   sizeof(struct v4l2_plane));
		    cap_info->buffers[i].length = FMT_NUM_PLANES;
		}
	}
	return 0;
}

int free_request_capture(vi_v4l2_info_t * cap_info)
{
	for (int i = 0; i < cap_info->buffers_num; ++i) {
		if (cap_info->planes[i])
			free(cap_info->planes[i]);
	}
	if (cap_info->planes) {
    	free(cap_info->planes);
	}
	if (cap_info->bos) {
		free(cap_info->bos);
	}
	if (cap_info->buffers) {
		free(cap_info->buffers);
	}
	return 0;
}

int wait_capture(vi_v4l2_info_t * cap_info)
{
	fd_set fds;
	struct timeval tv;
	int r;
	FD_ZERO(&fds);
	FD_SET(cap_info->video_dev_fd, &fds);
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	r = select(cap_info->video_dev_fd + 1, &fds, NULL, NULL, &tv);
	if (-1 == r) {
		if (EINTR == errno)
			return -1;
		errno_debug("select");
	}
	if (0 == r) {
		fprintf(stderr, "select timeout\\n");
		exit(EXIT_FAILURE);
	}
	return 0;
}

int dequeue_capture(vi_v4l2_info_t * cap_info, int index)
{
	switch (cap_info->io) {
		case IO_METHOD_READ:
			if (-1 == read(cap_info->video_dev_fd,
			    (void*)cap_info->buffers[0].m.userptr,
			    cap_info->buffers[0].length)) {
				switch (errno) {
					case EAGAIN:
						return 0;
					case EIO:
					default:
						errno_debug("read");
				}
			}
		break;
		case IO_METHOD_MMAP:
		case IO_METHOD_USERPTR:
#ifdef USE_ROCKCHIP_DRM
		case IO_METHOD_DRM:
#endif
		device_dqbuf(cap_info->video_dev_fd, &cap_info->buffers[index]);
		break;

	}
	return 0;
}

int queue_capture(vi_v4l2_info_t * cap_info, int index)
{
	switch (cap_info->io) {
		case IO_METHOD_READ:
		break;
		case IO_METHOD_MMAP:
#ifdef USE_ROCKCHIP_DRM
		case IO_METHOD_DRM:
#endif
		if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == cap_info->buf_type) {
			cap_info->buffers[index].length = FMT_NUM_PLANES;
		}
		case IO_METHOD_USERPTR:
		device_qbuf(cap_info->video_dev_fd, &cap_info->buffers[index]);
		break;
	}
	return 0;
}


void stop_capturing(vi_v4l2_info_t * cap_info)
{
	enum v4l2_buf_type type;

	switch (cap_info->io) {
		case IO_METHOD_READ:
			/* Nothing to do. */
		break;
		case IO_METHOD_MMAP:
		case IO_METHOD_USERPTR:
#ifdef USE_ROCKCHIP_DRM
		case IO_METHOD_DRM:
#endif
			type = cap_info->buf_type;
			device_streamoff(cap_info->video_dev_fd, &type);
		break;
	}
}

void start_capturing(vi_v4l2_info_t * cap_info)
{
	unsigned int i;
	enum v4l2_buf_type type;

	switch (cap_info->io) {
		case IO_METHOD_READ:
			/* Nothing to do. */
		break;
		case IO_METHOD_MMAP:
		case IO_METHOD_USERPTR:
#ifdef USE_ROCKCHIP_DRM
		case IO_METHOD_DRM:
#endif
			printf("buffers_num    %d\n", cap_info->buffers_num);
			for (i = 0; i < cap_info->buffers_num; ++i) {
				printf("type    %d\n", cap_info->buffers[i].type);
				printf("index   %d\n", cap_info->buffers[i].index);
				printf("1userptr %x\n", cap_info->buffers[i].m.userptr);
				printf("2userptr %x\n", cap_info->buffers[i].m.planes[0].m.userptr);
				printf("length  %d\n", cap_info->buffers[i].length);
				device_qbuf(cap_info->video_dev_fd, &cap_info->buffers[i]);
			}
			type = cap_info->buf_type;
			device_streamon(cap_info->video_dev_fd, &type);
		break;
	}
}

void uninit_device(vi_v4l2_info_t * cap_info)
{
    deinit_io_method(cap_info);
	free_request_capture(cap_info);
	device_close(cap_info->video_dev_fd);
}

int init_device(vi_v4l2_info_t * cap_info)
{
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	unsigned int min;

	cap_info->video_dev_fd = device_open(cap_info->video_dev_name);

	if (-1 != device_querycap(cap_info->video_dev_fd, &cap)) {
		if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
		    !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
			fprintf(stderr, "%s is no video capture device\\n",
			cap_info->video_dev_name);
			return -1;
		}
	}

	if (-1 == check_io_method(cap_info->io, cap.capabilities)) {
		return -1;
	}

	CLEAR(cropcap);
	cropcap.type = cap_info->buf_type;
	crop.type = cap_info->buf_type;
	crop.c = cropcap.defrect; /* reset to default */
	device_cropcap(cap_info->video_dev_fd, &cropcap, &crop);

	CLEAR(fmt);
	if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
		cap_info->buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	else if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
		cap_info->buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	if (cap_info->format) {
		fmt.type = cap_info->buf_type;
		fmt.fmt.pix.width       = cap_info->width;
		fmt.fmt.pix.height      = cap_info->height;
		fmt.fmt.pix.pixelformat = cap_info->format;
		fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
		device_setformat(cap_info->video_dev_fd, &fmt);
	} else {
		device_getformat(cap_info->video_dev_fd, &fmt);
	}

	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	request_capture(cap_info);
	init_io_method(cap_info, fmt.fmt.pix.sizeimage);
}

