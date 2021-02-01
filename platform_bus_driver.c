#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include "platform.h"
#include <linux/slab.h>

#define MAX_DEVICES 10

/* private_driver_data */
struct private_driver_data
{
  struct class *class_pcd;
  dev_t device_number_base;
};

struct private_driver_data private_driver_data_struct;

/*
platform_driver_remove() -
*/
int pcd_platform_driver_remove(struct platform_device *pdev)
{
  pr_info("Platform device removed\n");
  return 0;
}

/*
platform_driver_probe() - gets called when the platform device driver's name value matches
with that of the platform bus driver i.e when the matched device has been loaded
*/
int pcd_platform_driver_probe(struct platform_device *pdev)
{
  pr_info("Platform device matched with the driver!\n");
  return 0;
}

/*
struct platform_driver {
	int (*probe)(struct platform_device *);
	int (*remove)(struct platform_device *);
	void (*shutdown)(struct platform_device *);
	int (*suspend)(struct platform_device *, pm_message_t state);
	int (*resume)(struct platform_device *);
	struct device_driver driver;
	const struct platform_device_id *id_table;
	bool prevent_deferred_probe;
};
*/
struct platform_driver platform_driver_struct =
{
  .probe = pcd_platform_driver_probe,
  .remove = pcd_platform_driver_remove,
  .driver = {
    .name = "pseudo-char-device"
  }
};

/* @platform_driver_init - called upon loading the platform driver */
static int __init platform_driver_init(void)
{
  int ret;

  /* allocate a range of char device numbers */
  ret = alloc_chrdev_region(&private_driver_data_struct.device_number_base, 0, MAX_DEVICES, "pcd_driver");
  if (ret < 0)
  {
    pr_err("Char devices weren't allocated\n");
    return ret;
  }

  /* Create device class under /sys/class */
  private_driver_data_struct.class_pcd = class_create(THIS_MODULE, "pcd_class");
  if (IS_ERR(private_driver_data_struct.class_pcd))
  {
    pr_err("Class creation failed\n");
    return ret;
  }

  /* Register platform driver */
  platform_driver_register(&platform_driver_struct);
  pr_info("Platform driver is loaded...\n");
  return 0;
}

/* platform_driver_cleanup() - called upon unloading the platform driver */
static void __exit platform_driver_cleanup(void)
{
  platform_driver_unregister(&platform_driver_struct);

  class_destroy(private_driver_data_struct.class_pcd);

  unregister_chrdev_region(private_driver_data_struct.device_number_base, MAX_DEVICES);

  pr_info("Platform driver is unloaded...\n");
}

module_init(platform_driver_init);
module_exit(platform_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SPNX");
MODULE_DESCRIPTION("Platform driver");
