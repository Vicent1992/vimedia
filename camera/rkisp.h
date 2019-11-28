#ifndef _RKISP_H_
#define _RKISP_H_

int rkisp_get_media_info(const char *mdev_path);
void init_engine(void);
void start_engine(void);
void stop_engine(void);
void deinit_engine(void);

#endif