#ifndef _CAMERA_CAPTURE_H_
#define _CAMERA_CAPTURE_H_

#include "v4l2_memory.h"
#include "v4l2_device.h"

int wait_capture(vi_v4l2_info_t * cap_info);
int dequeue_capture(vi_v4l2_info_t * cap_info, int index);
int queue_capture(vi_v4l2_info_t * cap_info, int index);
void stop_capturing(vi_v4l2_info_t * cap_info);
void start_capturing(vi_v4l2_info_t * cap_info);
void uninit_device(vi_v4l2_info_t * cap_info);
int init_device(vi_v4l2_info_t * cap_info);

#endif