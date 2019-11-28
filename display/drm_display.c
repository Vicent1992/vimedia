#include <errno.h>
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

#include "drm_display.h"

static void modeset_destroy_fb(int fd, struct buffer_object *bo)
{
	struct drm_mode_destroy_dumb destroy = {};

	drmModeRmFB(fd, bo->fb_id);

	munmap(bo->vaddr, bo->size);

	destroy.handle = bo->handle;
	drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

static int modeset_create_fb(int fd, struct buffer_object *bo)
{
	struct drm_mode_create_dumb create = {};
 	struct drm_mode_map_dumb map = {};

	/* create a dumb-buffer, the pixel format is XRGB888 */
	create.width = bo->width;
	create.height = bo->height;
	create.bpp = 32;
	drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);

	/* bind the dumb-buffer to an FB object */
	bo->pitch = create.pitch;
	bo->size = create.size;
	bo->handle = create.handle;
	drmModeAddFB(fd, bo->width, bo->height, 24, 32, bo->pitch,
			   bo->handle, &bo->fb_id);

	/* map the dumb-buffer to userspace */
	map.handle = create.handle;
	drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map);

	bo->vaddr = mmap64(0, create.size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, map.offset);
	return 0;
}

void init_dispaly(vi_display_info_t *disp_info)
{
	int ret;

	disp_info->dst_width = 720;
	disp_info->dst_height = 1280;
	disp_info->dst_bpp = 32;
	disp_info->dev_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);

	disp_info->res = drmModeGetResources(disp_info->dev_fd);
	disp_info->crtc_id = disp_info->res->crtcs[0];
	disp_info->conn_id = disp_info->res->connectors[0];

	disp_info->conn = drmModeGetConnector(disp_info->dev_fd,
	                                     disp_info->conn_id);
	disp_info->dst_buf.width = disp_info->conn->modes[0].hdisplay;
	disp_info->dst_buf.height = disp_info->conn->modes[0].vdisplay;

	modeset_create_fb(disp_info->dev_fd, &disp_info->dst_buf);

	disp_info->dst_format = RK_FORMAT_RGBA_8888;
	memset(&disp_info->dst_rga_info, 0, sizeof(rga_info_t));
	disp_info->dst_rga_info.mmuFlag = 1;
	disp_info->dst_rga_info.virAddr = disp_info->dst_buf.vaddr;
}

void deinit_dispaly(vi_display_info_t *disp_info)
{
	int ret;
	modeset_destroy_fb(disp_info->dev_fd, &disp_info->dst_buf);
	drmModeFreeConnector(disp_info->conn);
	drmModeFreeResources(disp_info->res);
	close(disp_info->dev_fd);
}

