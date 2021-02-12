#include <linux/module.h>
#include <linux/platform_device.h>
#include "platform.h"
#include <linux/cdev.h>

#undef pr_fmt
#define pr_fmt(fmt) "%s : " fmt,__func__

/* platform device's private data structure */
struct pcd_dev_platform_data pcd_dev_data[2] =
{
  [0] = {.size = 512, .perm = RDWR, .serial_number = "AXZ"},
  [1] = {.size = 1024, .perm = RDWR, .serial_number = "CXZ"}
};

/* release any dynamically allocated memory */
void pcd_dev_release(struct device *dev)
{
  pr_info("Device released [incomplete!!]\n");
}

/* Create 2 platform devices */
struct platform_device platform_pcd_dev_1 = {
    .name = "pseudo-char-device",
    .id = 0,
    .dev = {
      .platform_data = &pcd_dev_data[0],
      .release = pcd_dev_release
    }
};

struct platform_device platform_pcd_dev_2 = {
    .name = "pseudo-char-device",
    .id = 1,
    .dev = {
      .platform_data = &pcd_dev_data[1],
      .release = pcd_dev_release
    }
};

struct platform_device *platform_pcdevs[] =
{
  &platform_pcd_dev_1, &platform_pcd_dev_2
};

static int __init pcd_dev_platform_init(void)
{
  /*
    register the platform devices
    add platform devices to device hierarchy
  */
  // platform_device_register(&platform_pcd_dev_1);
  // platform_device_register(&platform_pcd_dev_2);

  platform_add_devices(platform_pcdevs, ARRAY_SIZE(platform_pcdevs));

  pr_info("Platform device loaded...\n");

  return 0;
}

static void __exit pcd_dev_platform_exit(void)
{
  platform_device_unregister(&platform_pcd_dev_1);
  platform_device_unregister(&platform_pcd_dev_2);

  pr_info("Platform device module unloaded...\n");
}

module_init(pcd_dev_platform_init);
module_exit(pcd_dev_platform_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Module that registers platform devices");
