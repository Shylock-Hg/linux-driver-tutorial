/*! \brief a simple linux driver file abstraction based on linux HAL
 *         test this on raspberry pi 3 B+
 *  \author Shylock Hg
 *  \date 2018-06-26
 *  \email tcath2s@gmail.com
 * */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <uapi/asm-generic/errno-base.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/time.h>

/******************** gpio device config********************/
#define MAX_GPIO_PIN_NUM  21
#define MAX_GPIO_NUM      32
#define MAX_BUF_SIZE      512

#define STR_DEVICE_NAME     "rasp_gpio_driver"
#define STR_INT_DEVICE_NAME "rasp_gpio_int_driver"

/******************** user define data type ********************/
enum rasp_gpio_value {low, high};
enum rasp_gpio_direction {input, output};

struct rasp_gpio_dev {
	struct cdev cdev;

	spinlock_t lock;

	//< custom field
	int irq_counter;
	bool irq_is_enabled;  //!< set by ioctl
};

/******************** entry of file ********************/
static int rasp_gpio_open(struct inode * inode, struct file * filp);
static int rasp_gpio_release(struct inode * inode, struct file * filp);
static ssize_t rasp_gpio_read(struct file * filp,
		char * __user buf,
		size_t count,
		loff_t * f_pos);
static ssize_t rasp_gpio_write(struct file * filp,
		const char * __user buf,
		size_t count,
		loff_t * f_pos);
static long rasp_unlocked_ioctl(struct file *, unsigned int, unsigned long);

static struct file_operations rasp_gpio_fops = {
	.owner          = THIS_MODULE,
	.open           = rasp_gpio_open,
	.release        = rasp_gpio_release,
	.read           = rasp_gpio_read,
	.write          = rasp_gpio_write,
	.unlocked_ioctl = rasp_unlocked_ioctl
};

/******************** interrupt device interface ********************/
static irqreturn_t rasp_gpio_irq_handler(int irq, void * args);

/******************** non file function ********************/
static __init int rasp_gpio_init(void);
static __exit void rasp_gpio_exit(void);
static uint32_t millis(void);

/******************** inner global variable ********************/
static struct rasp_gpio_dev * p_rasp_gpio_dev[MAX_GPIO_PIN_NUM];
static dev_t first;
static struct class * rasp_gpio_class;
static uint64_t epochMilli;

/******************** implement ********************/
/*! \brief get current time as millisecond
 *  \retval current time as millisecond
 * */
static uint32_t millis(void){
	struct timeval timeval;
	uint64_t time;

	
	do_gettimeofday(&timeval);

	time = (uint64_t)(timeval.tv_sec*1000) +
		(uint64_t)(timeval.tv_usec/1000);

	return (uint32_t)(time - epochMilli);
}

/*! \brief top half handler of raspberry gpio ISR
 *  \param irq irq number
 *  \param args 
 *  \retval irq return status
 * */
static irqreturn_t rasp_gpio_irq_handler(int irq, void * args){
	unsigned long flag;
	static uint32_t last_int_time;
	uint32_t time = millis();

	//< ignore
	if(time - last_int_time < 200){
		printk(KERN_NOTICE "Ignore rasp gpio irq [%d] for too often!\n", irq);
		return IRQ_HANDLED;
	}

	//< real handle
	last_int_time = time;
	
	local_irq_save(flag);
	printk(KERN_NOTICE "Rasp gpio irq [%d] triggered!\n", irq);
	local_irq_restore(flag);

	return IRQ_HANDLED;
}

/*! \brief open the gpio file
 *  \param inode inode of file
 *  \param filp file pointer
 *  \retval return status
 * */
static int rasp_gpio_open(struct inode * inode, struct file * filp){
	unsigned int gpio = iminor(inode);
	int err = 0;
	struct rasp_gpio_dev * p_rasp_gpio_dev = container_of(inode->i_cdev,
			struct rasp_gpio_dev,
			cdev);

	printk(KERN_NOTICE "Open rasp gpio [%d]!\n", gpio);

	if(! gpio_is_valid(gpio)){
		printk(KERN_ALERT "Invalid gpio [%d]!\n", gpio);
		return -ENODEV;
	}

	/*  //< request gpio when module initialize
	err = gpio_request(gpio, GPIOF_OUT_INIT_LOW, NULL);
	if(err){
		printk(KERN_ALERT, "Request gpio [%d] failed!\n", gpio);
		return err;
	}
	*/

	//< gpio direction
	if(FMODE_READ & filp->f_mode){
		err = gpio_direction_input(gpio);
	}else if(FMODE_WRITE | filp->f_mode){
		err = gpio_direction_output(gpio, 0);
	}else{
		printk(KERN_ALERT "Undefined w/r access of rasp gpio [%d]!\n", gpio);
		return -1;
	}

	if(err){
		printk(KERN_ALERT "Set gpio [%d] direction failed!\n", gpio);
		return err;
	}

	filp->private_data = p_rasp_gpio_dev;
	
	return 0;
}

/*! \brief release the gpio file
 *  \param inode inode of file
 *  \param filp file pointer
 *  \retval return status
 * */
static int rasp_gpio_release(struct inode * inode, struct file * filp){
	unsigned int gpio = iminor(inode);
	int irq = gpio_to_irq(gpio);
	struct rasp_gpio_dev * dev = container_of(inode->i_cdev,
			struct rasp_gpio_dev,
			cdev);

	printk(KERN_NOTICE "Close gpio [%d]!\n", gpio);

	//< release irq
	if(dev->irq_is_enabled){
		spin_lock(&dev->lock);
		dev->irq_counter = 0;

		free_irq(irq, dev);

		dev->irq_is_enabled = false;
		spin_unlock(&dev->lock);
	}

	//gpio_free(gpio); //free gpio when module exit

	return 0;
}

/*! \brief read value of gpio
 *  \param filp pointer to file
 *  \param buf the buffer of data in user space
 *  \param count length of buf
 *  \param f_pos pointer to file offset
 *  \retval length of data read
 * */
static ssize_t rasp_gpio_read(struct file * filp,
		char * __user buf,
		size_t count,
		loff_t * f_pos){

	unsigned int gpio = iminor(file_inode(filp));
	int value = gpio_get_value(gpio);
	if(! gpio_is_valid(gpio)){
		printk(KERN_ALERT "Invalid gpio [%d]!\n", gpio);
		return 0;
	}

	if(0 == count || NULL == buf){
		printk(KERN_ALERT "No available buffer for gpio [%d]!\n", gpio);
		return 0;
	}

	if(put_user('0'+value, buf+0)){
		printk(KERN_ALERT "Read gpio [%d] copy to user failed!\n", gpio);
		return 0;
	}

	return 1;
}

/*! \brief write value to gpio
 *  \param buf the buffer of data in user space
 *  \param count the length of buf 
 *  \param f_pos pointer to file offset
 *  \retval length of data write
 * */
static ssize_t rasp_gpio_write(struct file * filp,
		const char * __user buf,
		size_t count,
		loff_t * f_pos){
	unsigned int gpio = iminor(file_inode(filp));
	int value;
	if(! gpio_is_valid(gpio)){
		printk(KERN_ALERT "Invalid gpio [%d]!\n", gpio);
		return 0;
	}

	if(0 == count || NULL == buf){
		printk(KERN_ALERT "No available buffer for gpio [%d]!\n", gpio);
		return 0;
	}

	if(get_user(value, buf+0)){
		printk(KERN_ALERT "Write gpio [%d] copy from user failed!\n", gpio);
		return 0;
	}
	gpio_set_value(gpio, value-'0');

	return 1;
}

enum {
	I_SETDIR,  //!< command to set gpio output/input
	I_SETINT  //!< command to enable/diable gpio irq
};
/*! \brief ioctl of rasp gpio 
 *  \param filp pointer to file
 *  \param cmd 
 *  \param arg 
 *  \retval
 * */
static long rasp_unlocked_ioctl(struct file * filp, unsigned int cmd, unsigned long arg){

	unsigned long flag;

	struct inode * inode = file_inode(filp);
	unsigned int gpio = iminor(inode);
	struct rasp_gpio_dev * dev = container_of(inode->i_cdev,
			struct rasp_gpio_dev,
			cdev);
	int direction = -1;
	int irq = gpio_to_irq(gpio);
	int label = -1;
	int err = 0;
	switch(cmd){

	case I_SETDIR:
		get_user(direction, (int*)arg);
		if(output == direction){  //!< output
			gpio_direction_output(gpio, 0);
			printk(KERN_NOTICE "Set rasp gpio [%d] output!\n", gpio);
		}else if(input == direction){  //!< input
			gpio_direction_input(gpio);
			printk(KERN_NOTICE "Set rasp gpio [%d] input!\n", gpio);
		}else{
			printk(KERN_ALERT "Invalid I_SETDIR argument [%d]!\n", direction);
			return -EINVAL;
		}
		break;
	case I_SETINT:
		get_user(label, (int*)arg);
		if(-1 == label){
			printk(KERN_ALERT "Get I_SETDIR argument from user failed!\n");
			return -EINVAL;
		}
		if((label & 1) ){
			//< enable irq
			if((label & 2)){
				//< irq rising trigger
				spin_lock_irqsave(&dev->lock, flag);
				err = request_irq(irq,
						rasp_gpio_irq_handler,
						IRQF_SHARED | IRQF_TRIGGER_RISING,
						STR_INT_DEVICE_NAME,
						dev);
				spin_unlock_irqrestore(&dev->lock, flag);
			}else{
				//< irq falling trigger
				spin_lock_irqsave(&dev->lock, flag);
				err = request_irq(irq,
						rasp_gpio_irq_handler,
						IRQF_SHARED | IRQF_TRIGGER_FALLING,
						STR_INT_DEVICE_NAME,
						dev);
				spin_unlock_irqrestore(&dev->lock, flag);
			}
			if(err){
				printk(KERN_ALERT "Request rasp gpio [%d] irq failed!\n", gpio);
				return err;
			}
			dev->irq_is_enabled = true;
			enable_irq(irq);
		}else{
			//< diable irq
			if(dev->irq_is_enabled){
				spin_lock(&dev->lock);
				dev->irq_counter = 0;
				dev->irq_is_enabled = false;
				free_irq(irq, dev);
				disable_irq(irq);
				spin_unlock(&dev->lock);
			}
		}

		/*
		else{
			printk(KERN_ALERT, "Invalid I_SETINT argument [%d]!\n", enable);
			return -EINVAL;
		}
		*/
		break;
	default:
		printk(KERN_ALERT "Invalid ioctl request [%d]!\n", cmd);
		return -EINVAL;
	}

	return 0;
}

/*! \brief check is gpio invalid
 *  \param gpio
 *  \retval is gpio invalid
 * */
static bool _gpio_is_in_blacklist(int gpio){
	static int _gpio_blacklist[] = {
		0,1,5,6,12,13,16,19,20,21,26
	};

	int i;
	for(i=0; i<sizeof(_gpio_blacklist)/sizeof(_gpio_blacklist[0]); i++){
		if(gpio == _gpio_blacklist[i])
			return true;
	}

	return false;
}

/*! \brief initialize rasp gpio module
 *  \retval return status
 * */
static __init int rasp_gpio_init(void){
	int err, i;
	int index = 0;
	struct timeval timeval;
	err = alloc_chrdev_region(&first,
				0,
				MAX_GPIO_PIN_NUM,
				STR_DEVICE_NAME);

	if(err){
		printk(KERN_ALERT "Can't alloc rasp gpio module!\n");
		return err;
	}

	rasp_gpio_class = class_create(THIS_MODULE, STR_DEVICE_NAME);
	if(NULL == rasp_gpio_class){
		printk(KERN_ALERT "Create rasp gpio class failed!\n");
		unregister_chrdev_region(first, MAX_GPIO_PIN_NUM);
		return -EINVAL;
	}

	for(i=0; i<MAX_GPIO_NUM; i++){
		if(! _gpio_is_in_blacklist(i)){
			//< valid gpio pin
			p_rasp_gpio_dev[index] = kmalloc(sizeof(struct rasp_gpio_dev),
					GFP_KERNEL);
			if(NULL == p_rasp_gpio_dev[index]){
				printk(KERN_ALERT "Alloc rasp gpio [%d] device failed!\n", i);
				return -ENOMEM;
			}

			//< request gpio
			if(gpio_request_one(i, GPIOF_OUT_INIT_LOW, NULL)){
				printk(KERN_ALERT "Request gpio [%d] failed!\n", i);
				return -ENODEV;
			}

			//< initailize device 
			p_rasp_gpio_dev[index]->irq_is_enabled = false;
			p_rasp_gpio_dev[index]->irq_counter = 0;
			p_rasp_gpio_dev[index]->cdev.owner = THIS_MODULE;

			spin_lock_init(&p_rasp_gpio_dev[index]->lock);

			//< register file_operations
			cdev_init(&p_rasp_gpio_dev[index]->cdev, &rasp_gpio_fops);

			err = cdev_add(&p_rasp_gpio_dev[index]->cdev,
					first+i,
					1);
			if(err){
				printk(KERN_ALERT "Register gpio [%d] device failed!\n", i);
				return err;
			}

			//< create device in VFS
			if(NULL == device_create(rasp_gpio_class,
					NULL,
					MKDEV(MAJOR(first), MINOR(first+i)),
					NULL,
					"rasp_gpio_%d",
					i
					)){

				return -1;
			}
		}  //!< !_gpio_is_blacklist
		index++;
	}  //!< for 

	//< initialize epoch 
	do_gettimeofday(&timeval);
	epochMilli = (uint64_t)(timeval.tv_sec*1000) +
		(uint64_t)(timeval.tv_usec/1000);

	return 0;
}  //!< function
module_init(rasp_gpio_init);

/*! \brief exit rasp gpio module
 * */
static __exit void rasp_gpio_exit(void){
	int i;
	unregister_chrdev_region(first, MAX_GPIO_PIN_NUM);
	
	for(i=0; i<sizeof(p_rasp_gpio_dev)/sizeof(p_rasp_gpio_dev[0]); i++){
		kfree(p_rasp_gpio_dev[i]);
	}

	for(i=0; i<MAX_GPIO_NUM; i++){
		if(! _gpio_is_in_blacklist(i)){
			gpio_direction_output(i,0);
		}
		device_destroy(rasp_gpio_class,
				MKDEV(MAJOR(first), MINOR(first+i)));
		gpio_free(i);
	}

	class_destroy(rasp_gpio_class);

	printk(KERN_NOTICE "Exit rasp gpio module!\n");
}
module_exit(rasp_gpio_exit);

/******************** information of this module ********************/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Shylock Hg <tcath2s@gmail.com>");
MODULE_DESCRIPTION("Raspberry pi 3 B+ gpio simple driver file abstraction.");


