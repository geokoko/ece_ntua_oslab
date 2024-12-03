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
static int lunix_chrdev_state_needs_refresh(struct lunix_chrdev_state_struct *);
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
	struct lunix_sensor_struct  *sensor;
	uint32_t raw_data;
	uint32_t time_stamp;
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

	if(!lunix_chrdev_state_needs_refresh(&state)) return -EAGAIN;
	/* ? */
	
	/*
	 * Now we can take our time to format them,
	 * holding only the private state semaphore
	 */
	if (down_interruptible(&state->lock))
		return -ERESTARTSYS;

	state->buf_timestamp = time_stamp;

	switch (state->type) {
    case BATT: // Battery level
        snprintf(state->buf_data, LUNIX_CHRDEV_BUFSZ, "%.2ln\n",
                 lookup_voltage[raw_data]);
        break;
    case TEMP: // Temperature
        snprintf(state->buf_data, LUNIX_CHRDEV_BUFSZ, "%.2ln\n",
                 lookup_temperature[raw_data]);
        break;
    case LIGHT: // Light intensity
        snprintf(state->buf_data, LUNIX_CHRDEV_BUFSZ, "%.2ln\n",
                 lookup_light[raw_data]);
        break;
    default:
		up(&state->lock);
        return -EINVAL; // Invalid measurement type
    }
	/* ? */
	up(&state->lock);
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
	
	/* Allocate a new Lunix character device private state structure (including its buffer) */
	struct lunix_chrdev_state_struct* state;

	//struct lunix_sensor_struct *sensor;
	int ret;

	debug("entering file with {major, minor}: {%d, %d}\n", major, minor);
	debug("sensor_id = %d and Measurement Type = %d", sensor_id, measurement_type);
	
	if (!(state = kmalloc(sizeof(struct lunix_chrdev_state_struct), GFP_KERNEL))) {
		ret = -ENOMEM; // error no memory
		goto out;
	}

	/* Initializing character device state struct*/ 
	if (measurement_type >= N_LUNIX_MSR) {
		debug("Invalid measurement type: %u\n", state->type);
		kfree(state);
		ret = -EINVAL;
		goto out;
	}

	if (sensor_id >= lunix_sensor_cnt) {
		debug("Invalid sensor initialization");
		kfree(state);
		ret = -ENODEV;
		goto out;
	}

	state->sensor = lunix_sensors + sensor_id; // Store which sensor we want to open
	state->type = measurement_type;   // Store measurement type
	state->buf_lim = 0;               // No data initially in the buffer
	state->buf_timestamp = 0;         // No timestamp yet
	memset(state->buf_data, 0, LUNIX_CHRDEV_BUFSZ);  // Clear the buffer
	sema_init(&state->lock, 1);  // Set initial semaphore count to 1

	filp->private_data = state; // (fd) private data now points to the state buffer
	
	debug("open successful for sensor %d, type %d\n", sensor_id, measurement_type);

	/* Register as non-seekable */
    if ((ret = nonseekable_open(inode, filp)) < 0) {
        kfree(state);
        goto out;
    }

	ret = 0;

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
	struct lunix_sensor_struct *sensor;
	struct lunix_chrdev_state_struct *state;

	state = filp->private_data;
	WARN_ON(!state);

	sensor = state->sensor;
	WARN_ON(!sensor);

	debug("Entering read\n");

	/* Acquire the lock */
	if (down_interruptible(&dev->sem)) {
		ret = -ERESTARTSYS;
		goto out;
	}
	/*
	 * If the cached character device state needs to be
	 * updated by actual sensor data (i.e. we need to report
	 * on a "fresh" measurement, do so
	 */
	if (*f_pos == 0) {
		while (lunix_chrdev_state_update(state) == -EAGAIN) {
			/* ? */
			/* The process needs to sleep */
			up(&state->lock);

			if (down_interruptible(&dev->sem)) {
				ret = -ERESTARTSYS;
				goto out;
			}
		}
	}

	/* Determine the number of cached bytes to copy to userspace */
	bytes_to_copy = buf_lim - *f_pos; 

	/* Handle EOF mode */
	if (bytes_to_copy <= 0) {
		*f_pos = 0;
		bytes_to_copy = buf_lim;
		ret = 0;
		goto out;
	}

	if (copy_to_user(usrbuf, buf_data + *f_pos, bytes_to_copy)) {
		ret = -EFAULT;
	}

	*f_pos += bytes_to_copy;

	ret = 0;
	goto out;
out:
	down(&state->lock);
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
