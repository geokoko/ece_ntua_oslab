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

	if (state->buf_timestamp < sensor->msr_data[state->type]->last_update) 
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
	int ret;
	struct lunix_sensor_struct *sensor;
	uint32_t raw_data;
	/*
	 * Grab the raw data quickly, hold the
	 * spinlock for as little as possible.
	 */

	WARN_ON(!(sensor = state->sensor));

	/* Use spinlocks due to interrupt context */
	spin_lock(&sensor->lock);
	debug("Locked critical section");
	
	/*
	 * Checking for raw data
	 */

	if(!lunix_chrdev_state_needs_refresh(state)) {
		ret = -EAGAIN;
		spin_unlock(&sensor->lock);
		goto out;
	}

	/* Are there any new data? If yes update the values */
	raw_data = sensor->msr_data[state->type]->values[0];
	state->buf_timestamp = sensor->msr_data[state->type]->last_update;

	spin_unlock(&sensor->lock);
	debug("Unlocked critical section, Saved new data");

	debug("CAPTURE RAW DATA {%ud, %ud}, CONVERT THEM TO READABLE FORMAT AND POPULATE THE STATE BUFFER", raw_data, state->buf_timestamp);

	/* Add data here to the character device buffer, and update buffer limiter */
	switch (state->type) {
		case BATT: // Battery level
			state->buf_lim = snprintf(state->buf_data, LUNIX_CHRDEV_BUFSZ, "%ld.%03ld\n",
					lookup_voltage[raw_data]/1000, lookup_voltage[raw_data]%1000);
			break;
		case TEMP: // Temperature
			state->buf_lim = snprintf(state->buf_data, LUNIX_CHRDEV_BUFSZ, "%ld.%03ld\n",
					lookup_temperature[raw_data]/1000, lookup_temperature[raw_data]%1000);
			break;
		case LIGHT: // Light intensity
			state->buf_lim = snprintf(state->buf_data, LUNIX_CHRDEV_BUFSZ, "%ld.%03ld\n",
					lookup_light[raw_data]/1000, lookup_light[raw_data]%1000);
			break;
		case N_LUNIX_MSR:
			ret = -EFAULT;
			goto out;
		default:
			ret = -EINVAL;
			goto out;
	}

	debug("DATA ADDED SUCCESSFULLY");
	ret = 0;
	goto out;

out:
	debug("leaving\n");
	return ret;
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
	int ret = 0;

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
	struct lunix_chrdev_state_struct *state = filp->private_data;

    if (!state) {
        pr_err("lunix_chrdev_release: private_data is NULL\n");
        return -EINVAL; // Invalid state
    }

    /* Free the allocated memory for the device state */
    kfree(state);
    filp->private_data = NULL; // Clear the pointer to avoid use-after-free

    pr_debug("lunix_chrdev_release: released resources successfully\n");
    return 0;
}

static long lunix_chrdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return -EINVAL;
}

static ssize_t lunix_chrdev_read(struct file *filp, char __user *usrbuf, size_t cnt, loff_t *f_pos)
{
	int ret = 0;
	size_t bytes_to_copy;
	struct lunix_sensor_struct *sensor;
	struct lunix_chrdev_state_struct *state;

	state = filp->private_data;
	WARN_ON(!state);

	sensor = state->sensor;
	WARN_ON(!sensor);

	debug("Entering read\n");

	/* Acquire the lock */
	if (down_interruptible(&state->lock)) {
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
			/* The process needs to sleep 
			 * as new data is unavailable
			 */

			/* Release the semaphore before sleeping */
			up(&state->lock);

			/* If the file is opened in a non-blocking mode, return -EAGAIN */
			if (filp->f_flags & O_NONBLOCK) {
				ret = -EAGAIN;
				goto out;
			}

			/* Sleeping... 
			 * Inserting processes into queue to avoid race conditions when waking up
			 */

			/* sensor->wq is a wait queue associated with the sensor structure. 
			 * It's used to suspend the process in the read function until new sensor data becomes available.
			 */
			if (wait_event_interruptible(sensor->wq, lunix_chrdev_state_needs_refresh(state))) {
                ret = -ERESTARTSYS; /* restart process if interrupt comes */
                goto out;
            }

			/* re-acquire the semaphore lock, and continue with read */
			if (down_interruptible(&state->lock)) {
				ret = -ERESTARTSYS;
				goto out;			
			}
		}	
	}

	debug("Updated with new data, printing...");
	/* Determine the number of cached bytes to copy to userspace */
	bytes_to_copy = state->buf_lim - *f_pos; 

	/* Handle EOF mode */
	if (bytes_to_copy < 0) {
		ret = 0;
		goto out;
	}

	cnt = min(cnt, (size_t)bytes_to_copy);

	if (copy_to_user(usrbuf, state->buf_data + *f_pos, cnt)) {
		ret = -EFAULT;
		goto out;
	}

	*f_pos += cnt;
	ret = cnt;
	bytes_to_copy = state->buf_lim - *f_pos;

	if (bytes_to_copy == 0)
		*f_pos = 0;

out:
	debug("Leaving read..., with ret = %d", ret);
	up(&state->lock);
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
