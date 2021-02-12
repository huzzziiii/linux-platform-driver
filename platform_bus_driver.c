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
#define DEV_MEM_SIZE 512

static char device_buffer[DEV_MEM_SIZE];   // pseudo device's memory

/* private_driver_data */
struct private_driver_data
{
  struct class *class_pcd;
  struct device *device_pcd;
  dev_t device_number_base;
  int total_devices;
};

struct private_device_data
{
  struct pcd_dev_platform_data p_data;
  char *buffer;                         // stores class name
  struct cdev cdev;
  dev_t device_num;
};

struct private_driver_data private_driver_data_struct;


// --------------------------------------------------------------------------------------------
/* File operation functions */
loff_t pcd_llseek (struct file *filep, loff_t off, int whence)
{
  pr_info("lseek requested");
  return 0;
}

ssize_t pcd_write (struct file *filp, const char __user *buff, size_t count, loff_t *f_pos)
{
  pr_info("write request for %zu bytes\n", count);
  pr_info("current file pos: %lld\n", *f_pos);

  if ( (*f_pos + count) > DEV_MEM_SIZE)
  {
    pr_warning("Requested writes bytes %zu are greater than the device memory size of %d\n",
              count, DEV_MEM_SIZE);
    count = DEV_MEM_SIZE - *f_pos;
  }

  if (!count)
  {
    pr_err("No more space available to write\n");
    return -ENOMEM;
  }

  if (copy_from_user(&device_buffer[*f_pos], buff, count))
  {
    return -EFAULT;
  }

  *f_pos += count;

  pr_info("Number of successful write transfer: %zu bytes\n", count);
  pr_info("Updated file position: %lld\n", *f_pos);

  return count;
}

ssize_t pcd_read (struct file *filp, char __user *buff, size_t count, loff_t *f_pos)
{
  pr_info("read requested for %zu bytes\n", count);
  pr_info ("current file pos: %lld\n", *f_pos);

  /* Adjust the count */
  if ( (*f_pos + count) > DEV_MEM_SIZE)
  {
    pr_warning("Requested read bytes %zu are greater than the device memory size of %zu\n",
              count, DEV_MEM_SIZE);

    count = DEV_MEM_SIZE - *f_pos;

    pr_warning("After calculation, count is now = %zu\n", count);
  }

  /* copy to user */
  if (copy_to_user(buff, &device_buffer[*f_pos], count))
  {
    return -EFAULT;
  }

  /* update the current file position */
  *f_pos += count;

  pr_info("Number of successful read transfer: %zu bytes\n", count);
  pr_info("Updated file position: %lld\n", *f_pos);

  return count;
}

int pcd_open(struct inode *inode, struct file  *filp)
{
  pr_info("open requested\n");
  return 0;
}

int pcd_release(struct inode *inode, struct file *filp)
{
  pr_info("release requested\n");
  return 0;
}

/* file operations of the driver */
struct file_operations pcd_fops=
{
	.open = pcd_open,
	.release = pcd_release,
	.read = pcd_read,
	.write = pcd_write,
	.llseek = pcd_llseek,
	.owner = THIS_MODULE
};


/*
platform_driver_remove() -
*/
int pcd_platform_driver_remove(struct platform_device *pdev)
{
  struct private_device_data *device_data = dev_get_drvdata(&pdev->dev);

  /* remove a cdev entry */
  cdev_del(&device_data->cdev);

  /* Free the memory allocated by the device --> using devm_kzalloc() == automatic deallocation */
  // kfree(device_data->buffer);
  // kfree(device_data);

  private_driver_data_struct.total_devices--;

  pr_info("Platform device removed\n");
  return 0;
}

/*
platform_driver_probe() - gets called when the platform device driver's name value matches
with that of the platform bus driver i.e when the matched device has been loaded
*/
int pcd_platform_driver_probe(struct platform_device *pdev)
{
  struct pcd_dev_platform_data *pdata;      // platform data
  struct private_device_data *device_data;
  int ret;

  /* Get the platform data */
  pdata = (struct pcd_dev_platform_data*) dev_get_platdata(&pdev->dev);
  if (!pdata)
  {
    pr_info("No platform data is available...\n");
    return -EINVAL;
  }

  /* Now there is platform data, dynamically allocate memory for the device private data */
  device_data =  devm_kzalloc(&pdev->dev, sizeof(*device_data), GFP_KERNEL);
  if (!device_data)
  {
    pr_info("Dynamic allocation for platform device data failed!\n");
    return -ENOMEM;
  }

  /* Store the device's private data pointer in platform device struct */
  dev_set_drvdata(&pdev->dev, device_data);

  /* Store the platform device data */
  device_data->p_data.size = pdata->size;
  device_data->p_data.perm = pdata->perm;
  device_data->p_data.serial_number = pdata->serial_number;

  pr_info("Device serial number: %s\n", device_data->p_data.serial_number);
  pr_info("Device permission: %d\n", device_data->p_data.perm);
  pr_info("Device size: %d\n", device_data->p_data.size);

  /* Dynamically allocate memory for the device buffer */
  device_data->buffer = kzalloc(device_data->p_data.size, GFP_KERNEL);
  if (!device_data->buffer)
  {
    pr_info("Dynamic memory allocation failed for the device's buffer!\n");
    return -ENOMEM;
  }

  /* Get the device number */
  device_data->device_num = private_driver_data_struct.device_number_base + pdev->id;

  /* Init cdev */
  cdev_init(&device_data->cdev, &pcd_fops);

  /* Add cdev */
  ret = cdev_add(&device_data->cdev, device_data->device_num, 1);
  if (ret < 0)
  {
    pr_err("cdev_add() failed\n");
    return ret;
  }

  device_data->cdev.owner = THIS_MODULE;
  pr_info("Platform device matched with the driver!\n");

  /* create device */
  private_driver_data_struct.device_pcd = device_create(private_driver_data_struct.class_pcd, NULL, device_data->device_num, NULL,
                                          "pcd-dev-%d", pdev->id);

  if (IS_ERR(private_driver_data_struct.device_pcd))
  {
    pr_err("Device creation failed!\n");
    return -ENOMEM;
  }

  private_driver_data_struct.total_devices++;

  pr_info("A device is successfully probed!\n");
  return 0;
}
//
// /*
// struct platform_driver {
// 	int (*probe)(struct platform_device *);
// 	int (*remove)(struct platform_device *);
// 	void (*shutdown)(struct platform_device *);
// 	int (*suspend)(struct platform_device *, pm_message_t state);
// 	int (*resume)(struct platform_device *);
// 	struct device_driver driver;
// 	const struct platform_device_id *id_table;
// 	bool prevent_deferred_probe;
// };
// */
struct platform_driver platform_driver_struct =
{
  .probe = pcd_platform_driver_probe,
  .remove = pcd_platform_driver_remove,
  .driver = {
    .name = "pseudo-char-device"
  }
};

//
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

  pr_debug("Creating a device class of platform driver under /sys/class");
  private_driver_data_struct.class_pcd = class_create(THIS_MODULE, "platform_driver");
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
