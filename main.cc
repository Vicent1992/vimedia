#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#include "camera/rkisp.h"
#include "camera/v4l2_capture.h"
#include "display/drm_display.h"


typedef struct vi_media_info {
 	vi_v4l2_info_t		v4l2_info;
	vi_display_info_t   disp_info;
	RockchipRga         rkRga;
} vi_media_info_t;

vi_media_info_t media_info;

static void mainloop(void);
static void parse_args(int argc, char **argv);
static void dump_media_info();

int main(int argc, char **argv)
{
 	vi_v4l2_info_t *v4l2_info = &media_info.v4l2_info;
	vi_display_info_t *disp_info = &media_info.disp_info;

	parse_args(argc, argv);
	dump_media_info();

	if (rkisp_get_media_info(v4l2_info->rkisp_dev_name))
    	errno_debug("Bad media topology\n");
	init_engine();
    init_device(v4l2_info);
	start_engine();
    start_capturing(v4l2_info);
	init_dispaly(disp_info);
    mainloop();
	deinit_dispaly(disp_info);
    stop_capturing(v4l2_info);
    stop_engine();
    uninit_device(v4l2_info);
    deinit_engine();
	return 0;
}

void v4l2info_to_dispinfo(vi_display_info_t * disp_info,
                                vi_v4l2_info_t * v4l2_info,
                                int index)
{
	void *srcPtr;
	size_t srcSize;
	struct v4l2_buffer * buf = &v4l2_info->buffers[index];

	switch (v4l2_info->io) {
		case IO_METHOD_READ:
			srcPtr	= (void *)buf->m.userptr;
			srcSize = buf->length;
		break;
		case IO_METHOD_MMAP:
		case IO_METHOD_USERPTR:
#ifdef USE_ROCKCHIP_DRM
		case IO_METHOD_DRM:
		if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == v4l2_info->buf_type) {
			srcSize = buf->m.planes[0].bytesused;
			srcPtr = (void *)buf->m.planes[0].m.userptr;
		} else {
			srcSize = buf->bytesused;
			srcPtr = (void *)buf->m.userptr;
		}
		break;
#endif
	}

	disp_info->src_width = v4l2_info->width;
	disp_info->src_height = v4l2_info->height;
	disp_info->src_format = RK_FORMAT_YCrCb_420_SP;
	memset(&disp_info->src_rga_info, 0, sizeof(rga_info_t));
	disp_info->src_rga_info.mmuFlag = 1;
    disp_info->src_rga_info.virAddr = srcPtr;
}

void rga_to_dispaly(vi_display_info_t * disp_info, RockchipRga *rkRga)
{
    int ret, index;
    unsigned int bytesused;

	rga_set_rect(&disp_info->src_rga_info.rect, 0, 0,
	             disp_info->src_width, disp_info->src_height,
		         disp_info->src_width, disp_info->src_height,
		         disp_info->src_format);
    rga_set_rect(&disp_info->dst_rga_info.rect, 0, 0, 
				 disp_info->dst_width, disp_info->dst_height,
		         disp_info->dst_width, disp_info->dst_height,
		         disp_info->dst_format);
    ret = rkRga->RkRgaBlit(&disp_info->src_rga_info,
		         &disp_info->dst_rga_info, NULL);
	drmModeSetCrtc(disp_info->dev_fd, disp_info->crtc_id,
		         disp_info->dst_buf.fb_id, 0, 0, 
		         &disp_info->conn_id, 1,
		         &disp_info->conn->modes[0]);
}

static void mainloop(void)
{
	unsigned int index = 0;
	struct timeval tpend1, tpend2;
	long usec1 = 0;
 	vi_v4l2_info_t *v4l2_info = &media_info.v4l2_info;
	vi_display_info_t *disp_info = &media_info.disp_info;
	RockchipRga *rkRga = &media_info.rkRga;
	for (;;) {
		if (wait_capture(v4l2_info))
			continue;
		gettimeofday(&tpend1, NULL);
		index = index % 4;
		dequeue_capture(v4l2_info, index);
#ifdef USE_ROCKCHIP_DRM
		v4l2info_to_dispinfo(disp_info, v4l2_info, index);
		rga_to_dispaly(disp_info, rkRga);
#endif
		queue_capture(v4l2_info, index);
		index >= 3 ? index = 0 : index++;
		gettimeofday(&tpend2, NULL);
		usec1 = 1000 * (tpend2.tv_sec - tpend1.tv_sec) + \
			    (tpend2.tv_usec - tpend1.tv_usec) / 1000;
		printf("cost_time=%ld ms\n", usec1);
	}
}

static void usage(FILE *fp, int argc, char **argv)
{
    fprintf(fp,
		"Usage: %s [options]\n"
		"Version 1.0\n"
		"V4l2 Options:\n"
		"-? | --help          Print this message\n"
		"-d | --device        Video device name [/dev/videox]\n"
		"-m | --memory        Video device buffers memory type"
		                      "[read:0 map:1 userptr:2 drm:3]\n"
		"-w | --width         Video device stream width\n"
		"-h | --height        Video device stream height\n"
		"-f | --format        Video device stream format[NV12]\n"
		"-r | --rkisp         Rkisp device [/dev/media0]\n"
		"such: %s -d /dev/video0 -m 3 -w 1920 -h 1080 -f NV12 -r /dev/media0"
		"\n", argv[0], argv[0]);
}

static const char short_options[] = "?d:m:w:h:f:r:";
static const struct option long_options[] = {
    { "help",   no_argument,       NULL, '?' },
    { "device", required_argument, NULL, 'd' },
    { "memory", required_argument, NULL, 'm' },
    { "width",  required_argument, NULL, 'w' },
    { "height", required_argument, NULL, 'h' },
    { "format", required_argument, NULL, 'f' },
    { "rkisp",  required_argument, NULL, 'r' },
    { 0, 0, 0, 0 }
};

static void parse_args(int argc, char **argv)
{
 	vi_v4l2_info_t *v4l2_info = &media_info.v4l2_info;
	vi_display_info_t *disp_info = &media_info.disp_info;
	for (;;) {
	int idx;
	int c;

	c = getopt_long(argc, argv, short_options, long_options, &idx);

	if (-1 == c)
		break;

	switch (c) {
		case 0:
		break;
		case 'd':
			v4l2_info->video_dev_name = optarg;
		break;
		case 'r':
			v4l2_info->rkisp_dev_name = optarg;
		break;
		case '?':
			usage(stdout, argc, argv);
			exit(EXIT_SUCCESS);
		case 'm':
			v4l2_info->io = (enum io_method)atoi(optarg);
		break;
		case 'w':
			v4l2_info->width = atoi(optarg);
		break;
		case 'h':
			v4l2_info->height = atoi(optarg);
		break;
		case 'f': {
			unsigned int format;
			format = v4l2_fourcc(optarg[0], optarg[1], 
			                     optarg[2], optarg[3]);
			v4l2_info->format = format;
			v4l2_info->bpp = 32;
		}
		break;
		default:
			usage(stderr, argc, argv);
			exit(EXIT_FAILURE);
	}
	}
}

static void dump_media_info()
{
	fprintf(stdout,
		"dump_media_info: \n"
		"V4l2 info:\n"
		"    video dev   :%s\n"
		"    rkisp dev   :%s\n"
		"    width       :%d\n"
		"    height      :%d\n"
		"    format      :%d\n"
		"    buffers_num :%d\n"
		"    buf_type    :%d\n"
		"    io          :%d\n"
		"\n",
		media_info.v4l2_info.video_dev_name,
		media_info.v4l2_info.rkisp_dev_name,
		media_info.v4l2_info.width,
		media_info.v4l2_info.height,
		media_info.v4l2_info.format,
		media_info.v4l2_info.buffers_num,
		media_info.v4l2_info.buf_type,
		media_info.v4l2_info.io);
}
