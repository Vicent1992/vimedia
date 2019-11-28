#if 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h> /* getopt_long() */
#include <fcntl.h> /* low-level i/o */
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#include "camera_engine_rkisp/interface/rkisp_control_loop.h"
#include "camera_engine_rkisp/interface/mediactl.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define DBG(...) do { if(!silent) printf("DBG: " __VA_ARGS__);} while(0)
#define ERR(...) do { fprintf(stderr, "ERR: " __VA_ARGS__); } while (0)

#define FILE_PATH_LEN                       64
#define CAMS_NUM_MAX                        2
#define FLASH_NUM_MAX                       2

struct rkisp_media_info {
    char sd_isp_path[FILE_PATH_LEN];
    char vd_params_path[FILE_PATH_LEN];
    char vd_stats_path[FILE_PATH_LEN];

    struct {
            char sd_sensor_path[FILE_PATH_LEN];
            char sd_lens_path[FILE_PATH_LEN];
            char sd_flash_path[FLASH_NUM_MAX][FILE_PATH_LEN];
            bool link_enabled;
    } cams[CAMS_NUM_MAX];
};

static struct rkisp_media_info media_info;
static void* rkisp_engine;
static int silent = 0;
static const char *mdev_path;

static int
rkisp_get_devname(struct media_device *device, const char *name, char *dev_name)
{
    const char *devname;

    media_entity *entity =  NULL;

    entity = media_get_entity_by_name(device, name, strlen(name));
    if (!entity)
        return -1;

    devname = media_entity_get_devname(entity);
    if (!devname) {
        ERR("can't find %s device path!", name);
        return -1;
    }

    strncpy(dev_name, devname, FILE_PATH_LEN);

    DBG("get %s devname: %s\n", name, dev_name);

    return 0;
}

static int
rkisp_enumrate_modules (struct media_device *device)
{
    uint32_t nents, i;
    const char* dev_name = NULL;
    int active_sensor = -1;

    nents = media_get_entities_count (device);
    for (i = 0; i < nents; ++i) {
        int module_idx = -1;
        struct media_entity *e;
        const struct media_entity_desc *ef;
        const struct media_link *link;

        e = media_get_entity(device, i);
        ef = media_entity_get_info(e);
        if (ef->type != MEDIA_ENT_T_V4L2_SUBDEV_SENSOR &&
            ef->type != MEDIA_ENT_T_V4L2_SUBDEV_FLASH &&
            ef->type != MEDIA_ENT_T_V4L2_SUBDEV_LENS)
            continue;

        if (ef->name[0] != 'm' && ef->name[3] != '_') {
            ERR("sensor/lens/flash entity name format is incorrect,"
                "pls check driver version !\n");
            return -1;
        }

        /* Retrive the sensor index from sensor name,
         * which is indicated by two characters after 'm',
         *   e.g.  m00_b_ov13850 1-0010
         *          ^^, 00 is the module index
         */
        module_idx = atoi(ef->name + 1);
        if (module_idx >= CAMS_NUM_MAX) {
            ERR("multiple sensors more than two not supported, %s\n", ef->name);
            continue;
        }

        dev_name = media_entity_get_devname (e);

        switch (ef->type) {
        case MEDIA_ENT_T_V4L2_SUBDEV_SENSOR:
            strncpy(media_info.cams[module_idx].sd_sensor_path,
                    dev_name, FILE_PATH_LEN);
            link = media_entity_get_link(e, 0);
            if (link && (link->flags & MEDIA_LNK_FL_ENABLED)) {
                media_info.cams[module_idx].link_enabled = true;
                active_sensor = module_idx;
            }
            break;
        case MEDIA_ENT_T_V4L2_SUBDEV_FLASH:
            // TODO, support multiple flashes attached to one module
            strncpy(media_info.cams[module_idx].sd_flash_path[0],
                    dev_name, FILE_PATH_LEN);
            break;
        case MEDIA_ENT_T_V4L2_SUBDEV_LENS:
            strncpy(media_info.cams[module_idx].sd_lens_path,
                    dev_name, FILE_PATH_LEN);
            break;
        default:
            break;
        }
    }

    if (active_sensor < 0) {
        ERR("Not sensor link is enabled, does sensor probe correctly?\n");
        return -1;
    }

    return 0;
}

int rkisp_get_media_info(const char *mdev_path) {
    struct media_device *device = NULL;
    int ret;

    CLEAR(media_info);

    device = media_device_new (mdev_path);

    media_device_enumerate (device);

    ret = rkisp_get_devname(device, "rkisp1-isp-subdev",
                            media_info.sd_isp_path);
    ret |= rkisp_get_devname(device, "rkisp1-input-params",
                             media_info.vd_params_path);
    ret |= rkisp_get_devname(device, "rkisp1-statistics",
                             media_info.vd_stats_path);
    if (ret) {
        media_device_unref (device);
        return ret;
    }

    ret = rkisp_enumrate_modules(device);
    media_device_unref (device);

    return ret;
}

void init_engine(void)
{
    struct rkisp_cl_prepare_params_s params = {0};
    int ret = 0;

    ret = rkisp_cl_init(&rkisp_engine, NULL, NULL);
    if (ret) {
        ERR("rkisp engine init failed !\n");
        exit(-1);
    }

    params.isp_sd_node_path = media_info.sd_isp_path;
    params.isp_vd_params_path = media_info.vd_params_path;
    params.isp_vd_stats_path = media_info.vd_stats_path;
    params.staticMeta = NULL;

    for (int i = 0; i < CAMS_NUM_MAX; i++) {
        if (!media_info.cams[i].link_enabled) {
            DBG("Link disabled, skipped\n");
            continue;
        }

        DBG("%s: link enabled %d\n", media_info.cams[i].sd_sensor_path,
            media_info.cams[i].link_enabled);

        params.sensor_sd_node_path = media_info.cams[i].sd_sensor_path;

        if (strlen(media_info.cams[i].sd_lens_path))
            params.lens_sd_node_path = media_info.cams[i].sd_lens_path;
        if (strlen(media_info.cams[i].sd_flash_path[0]))
            params.flashlight_sd_node_path[0] = media_info.cams[i].sd_flash_path[0];

        break;
    }

    if (rkisp_cl_prepare(rkisp_engine, &params)) {
        ERR("rkisp engine prepare failed !\n");
        exit(-1);
    }
}

void start_engine(void)
{
    DBG("device manager start\n");
    rkisp_cl_start(rkisp_engine);
    if (rkisp_engine == NULL) {
        ERR("rkisp_init engine failed\n");
        exit(-1);
    } else {
        DBG("rkisp_init engine succeed\n");
    }
}

void stop_engine(void)
{
    rkisp_cl_stop(rkisp_engine);
}

void deinit_engine(void)
{
    rkisp_cl_deinit(rkisp_engine);
}

#endif
