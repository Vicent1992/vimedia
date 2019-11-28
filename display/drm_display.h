#ifndef _DRM_DISPLAY_H_
#define _DRM_DISPLAY_H_

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#ifdef USE_ROCKCHIP_DRM
#include "rga/drmrga.h"
#endif

typedef struct buffer_object {
	uint32_t fb_id;
	uint32_t width;
	uint32_t height;
	uint32_t size;
	uint32_t pitch;
	uint32_t handle;
	void *vaddr;
} buffer_object_t;

typedef struct vi_display_info {
	char * dev_name;
	int dev_fd;
	drmModeConnector *conn;
	drmModeRes *res;
	uint32_t conn_id;
	uint32_t crtc_id;

	int src_width;
	int src_height;
	int src_format;
	int src_bpp;
	rga_info_t src_rga_info;

	int dst_width;
	int dst_height;
	int dst_format;
	int dst_bpp;
	rga_info_t dst_rga_info;
	buffer_object_t dst_buf;
} vi_display_info_t;

void init_dispaly(vi_display_info_t *disp_info);
void deinit_dispaly(vi_display_info_t *disp_info);

#endif
