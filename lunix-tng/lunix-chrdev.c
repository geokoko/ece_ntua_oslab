/*
 * lunix-chrdev.c
 *
 * Implementation of character devices
 * for Lunix:TNG
 *
 * <geokoko, Konstantinos Fratzeskos >
 *
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mmzone.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>

#include "lunix.h"
#include "lunix-chrdev.h"
#include "lunix-lookup.h"

/*
 * Global data
 */
struct cdev lunix_chrdev_cdev;

/*
 * Just a quick [unlocked] check to see if the cached
 * chrdev state needs to be updated from sensor measurements.
 */
/*
 * Declare a prototype so we can define the "unused" attribute and keep
 * the compiler happy. This function is not yet used, because this helpcode
 * is a stub.
 */
static int __attribute__((unused)) lunix_chrdev_state_needs_refresh(struct lunix_chrdev_state_struct *);
static int lunix_chrdev_state_needs_refresh(struct lunix_chrdev_state_struct *state)
{
	struct lunix_sensor_struct *sensor;
	
	WARN_ON (!(sensor = state->sensor));

	if (!sensor->msr_data[state->type]) {  /* No measurements available, so return */
		WARN_ON(1);
		return 0;
	}

	if (state->buf_timestamp == 0 || state->buf_timestamp != sensor->msr_data[sensor->type]->last_update) 
		return 1;

	return 0; /* No refresh needed */
}

/*
 * Updates the cached state of a character device
 * based on sensor data. Must be called with the
 * character device state lock held.
 */
static int lunix_chrdev_state_update(struct lunix_chrdev_state_struct *state)
{
	struct lunix_sensor_struct __attribute__((unused)) *sensor;
	debug("leaving\n");

	/*
	 * Grab the raw data quickly, hold the
	 * spinlock for as little as possible.
	 */

	WARN_ON(!(sensor = state->sensor));

	spin_lock(&sensor->lock);
	debug("Locked critical section");

	uint32_t raw_data = sensor->msr_data[state->type]->values[0];
	uint32_t timestamp = sensor->msr_data[state->type]->last_update;

	spin_unlock(&sensor->lock);
	debug("Unlocked critical section");
	
	/* Why use spinlocks? Use spinlocks to handle interrupts */

	/*
	 * Check for any new data available?
	 */

	/*
	 * Now we can take our time to format them,
	 * holding only the private state semaphore
	 */

	/* ? */

	debug("leaving\n");
	return 0;
}

/*************************************
 * Implementation of file operations
 * for the Lunix character device
 *************************************/

static int lunix_chrdev_open(struct inode *inode, struct file *filp)
{
	unsigned int minor = iminor(inode); // extract minor number from passed inode
	unsigned int major = imajor(inode); // extract major number from passed inode
	unsigned int sensor_id = minor >> 3;       // Extract sensor ID (divide minor_no by 3 and see which sensor number it has)
	unsigned int measurement_type = minor & 7; // Extract measurement type (take the modulo of the division with 8, which will give the measurement type)
	
	//struct lunix_sensor_struct *sensor;
	int ret;

	debug("entering file with {major, minor}: {%d, %d}\n", major, minor);
	debug("sensor_id = %d and Measurement Type = %d", sensor_id, measurement_type);
	ret = -ENODEV;
	if ((ret = nonseekable_open(inode, filp)) < 0)
		goto out;
	
	/* Allocate a new Lunix character device private state structure (including its buffer) */
	struct lunix_chrdev_state_struct* state;
	state = kmalloc(sizeof(*state), GFP_KERNEL);
	if (!state) {
		ret = -ENOMEM; // error no memory
		goto out;
	}

	if ((state->type = measurement_type) >= N_LUNIX_MSR) {
		debug("Invalid measurement type: %u\n", state->type);
		kfree(state);
		return -EINVAL;
	}

	filp->private_data = state; // (fd) private data now points to the state buffer

out:
	debug("leaving, with ret = %d\n", ret);
	return ret;
}

static int lunix_chrdev_release(struct inode *inode, struct file *filp)
{
	/* ? */
	return 0;
}

static long lunix_chrdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return -EINVAL;
}

static ssize_t lunix_chrdev_read(struct file *filp, char __user *usrbuf, size_t cnt, loff_t *f_pos)
{
	ssize_t ret;
	char *dummy = "Hello World!\n";
	size_t len = strlen(dummy);
	struct lunix_sensor_struct *sensor;
	struct lunix_chrdev_state_struct *state;

	state = filp->private_data;
	WARN_ON(!state);

	sensor = state->sensor;
	WARN_ON(!sensor);

	debug("Entering read\n");

	/*
	Dummy read function
	*/

	if (*f_pos >= len)
		goto out;

	if (*f_pos + cnt > len)
		cnt = len - *f_pos; // count only until the end of the buffer
	
	if (copy_to_user(usrbuf, dummy, len)) {
		ret = -EFAULT;
		goto out;
	}

	*f_pos += cnt; 
	ret = cnt;	

	/* Lock? */
	/*
	 * If the cached character device state needs to be
	 * updated by actual sensor data (i.e. we need to report
	 * on a "fresh" measurement, do so
	 */
	if (*f_pos == 0) {
		while (lunix_chrdev_state_update(state) == -EAGAIN) {
			/* ? */
			/* The process needs to sleep */
			/* See LDD3, page 153 for a hint */
		}
	}

	/* End of file */
	/* ? */
	
	/* Determine the number of cached bytes to copy to userspace */
	/* ? */

	/* Auto-rewind on EOF mode? */
	/* ? */

	/*
	 * The next two lines  are just meant to suppress a compiler warning
	 * for the "unused" out: label, and for the uninitialized "ret" value.
	 * It's true, this helpcode is a stub, and doesn't use them properly.
	 * Remove them when you've started working on this code.
	 */
	//ret = -ENODEV;
	//goto out;
out:
	/* Unlock? */
	return ret;
}

static int lunix_chrdev_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return -EINVAL;
}

static struct file_operations lunix_chrdev_fops = 
{
	.owner          = THIS_MODULE,
	.open           = lunix_chrdev_open,
	.release        = lunix_chrdev_release,
	.read           = lunix_chrdev_read,
	.unlocked_ioctl = lunix_chrdev_ioctl,
	.mmap           = lunix_chrdev_mmap
};

int lunix_chrdev_init(void)
{
	/*
	 * Register the character device with the kernel, asking for
	 * a range of minor numbers (number of sensors * 8 measurements / sensor)
	 * beginning with LINUX_CHRDEV_MAJOR:0
	 */
	int ret;
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;

	debug("initializing character device\n");
	cdev_init(&lunix_chrdev_cdev, &lunix_chrdev_fops);
	lunix_chrdev_cdev.owner = THIS_MODULE;
	
	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	
	ret = register_chrdev_region(dev_no, lunix_minor_cnt, "sensor");

	if (ret < 0) {
		debug("failed to register region, ret = %d\n", ret);
		goto out;
	}
	
	ret = cdev_add(&lunix_chrdev_cdev, dev_no, lunix_minor_cnt);

	if (ret < 0) {
		debug("failed to add character device\n");
		goto out_with_chrdev_region;
	}
	debug("completed successfully\n");
	return 0;

out_with_chrdev_region:
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
out:
	return ret;
}

void lunix_chrdev_destroy(void)
{
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;

	debug("entering\n");
	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	cdev_del(&lunix_chrdev_cdev);
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
	debug("leaving\n");
}
