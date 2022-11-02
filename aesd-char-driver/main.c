/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include "aesd_ioctl.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

static long aesd_adjust_file_offset(struct file *filp, unsigned int write_cmd, unsigned int write_cmd_offset);
MODULE_AUTHOR("Ganesh KM"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    struct aesd_dev *dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
		
	struct aesd_dev* device = filp->private_data;
	struct aesd_buffer_entry * read_entry = NULL;
	size_t bytes_to_read = 0;
	size_t entry_offset_byte = 0;
	size_t available_bytes = 0;
	ssize_t retval = 0;	
	
	PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
	/**
	 * TODO: handle read
	 */
	 
	if(mutex_lock_interruptible(&device->char_device_lock))
		return -ERESTARTSYS; 
	
	read_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&device->aesd_buffer, *f_pos, &entry_offset_byte);	
		
	if(read_entry == NULL) 
	{
	   retval = 0;
	   goto out;
	} 
	
	available_bytes = read_entry->size - entry_offset_byte;
	
	if(available_bytes > count)
	{
	   bytes_to_read = count;
	}
	else
	{ 
	   bytes_to_read = available_bytes;
	}
	
	int ret = copy_to_user(buf, read_entry->buffptr + entry_offset_byte, bytes_to_read);

	if(ret)
	{
	   retval = -EFAULT;
	   goto out;
	}
	
	retval= bytes_to_read;
	
	*f_pos += bytes_to_read;
	
out: 
	mutex_unlock(&device->char_device_lock);	
	return retval;	
	 
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	ssize_t retval = -ENOMEM;
	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	/**
	 * TODO: handle write
	 */
	 
	const char* ret_entry = NULL;
	struct aesd_dev* device = filp->private_data;
	
	if(mutex_lock_interruptible(&device->char_device_lock))
		return -ERESTARTSYS;
		
	if(device->add_entry.size == 0)
	{
	   device->add_entry.buffptr = kzalloc(count, GFP_KERNEL);
	}	
	else 
	{
	   device->add_entry.buffptr = krealloc(device->add_entry.buffptr, device->add_entry.size + count, GFP_KERNEL);
	}
	 
	if(device->add_entry.buffptr == NULL)
	{
	   retval = -ENOMEM;
	   goto out;
	}  
	 
	retval = count;
	
	size_t error_bytes_num = copy_from_user((void*)(&device->add_entry.buffptr[device->add_entry.size]), buf, count);
	
	if(error_bytes_num)
	{
	   retval = retval - error_bytes_num;
	}
	
	device->add_entry.size += retval;
	
	
	if(strchr((char*)(device->add_entry.buffptr), '\n'))
	{
	   ret_entry = aesd_circular_buffer_add_entry(&device->aesd_buffer, &device->add_entry);
	  
	   if(ret_entry)
	   {
	      kfree(ret_entry);
	   }
	
	   device->add_entry.buffptr = NULL;
	   device->add_entry.size = 0; 
	}
out: 
	mutex_unlock(&device->char_device_lock);	
	return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{
	loff_t ret_val;
	struct aesd_dev *dev = filp->private_data;
	loff_t size = dev->aesd_buffer.filled_buffer_size;
	//include lock
	if(mutex_lock_interruptible(&dev->char_device_lock))
		return -ERESTARTSYS; 
	ret_val = fixed_size_llseek(filp, offset, whence, size);
	mutex_unlock(&dev->char_device_lock);
	//remove lock
	return ret_val;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret_val = 0;
	struct aesd_seekto seekto;

	switch(cmd)
	{
		case AESDCHAR_IOCSEEKTO:
			{
				if(copy_from_user(&seekto,(const void __user *)arg, sizeof(seekto))!=0)
				{
					ret_val = -EFAULT;
				}
				else
				{
					ret_val = aesd_adjust_file_offset(filp, seekto.write_cmd, seekto.write_cmd_offset);
				}
				break;
			}
		default:
		{
			ret_val = -EINVAL;
		}
	}
	return ret_val;
}

static long aesd_adjust_file_offset(struct file *filp, unsigned int write_cmd, unsigned int write_cmd_offset)
{
	struct aesd_dev *dev = filp->private_data;
	loff_t offset = 0;
	int i = 0;
	int ret_val =0;
	if(!dev)
	{
		return -EINVAL;
	}
	if(mutex_lock_interruptible(&dev->char_device_lock))
		return -ERESTARTSYS;
	
	if(write_cmd > (AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED-1))
	{
		printk(" Invalid write cmd");
		return -1;
	}
	if(write_cmd_offset > (dev->aesd_buffer.entry[write_cmd].size - 1))
	{
		printk("Invalid offset");
		return -1;
	}
	for(i=0;i< (write_cmd);i++)
	{
		if(dev->aesd_buffer.entry[i].size == 0)
		{
			return -1;
		}
		offset += dev->aesd_buffer.entry[i].size;//iterating through all the entries
	}
	offset = offset + write_cmd_offset;

	filp->f_pos = offset;
	mutex_unlock(&dev->char_device_lock);

	return ret_val;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
	.llseek = aesd_llseek,
	.unlocked_ioctl = aesd_ioctl,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    mutex_init(&aesd_device.char_device_lock);
    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    uint8_t index;
	
	struct aesd_buffer_entry *entry;

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    kfree(aesd_device.add_entry.buffptr);
	
	AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.aesd_buffer, index){
		if(entry->buffptr != NULL)
			kfree(entry->buffptr);
			}
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
