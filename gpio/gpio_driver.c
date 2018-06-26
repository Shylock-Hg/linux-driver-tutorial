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
};

/******************** entry of file ********************/
static int rasp_gpio_open(struct inode * inode, struct file * filp);
static int rasp_gpio_release(struct inode * inode, sturct file * filp);
static ssize_t rasp_gpio_read(struct file * filp,
		char * buf,
		size_t count,
		loff_t * f_pos);
static ssize_t rasp_gpio_write(struct file * filp,
		char * buf,
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
static int rasp_gpio_init(void);
static void rasp_gpio_exit(void);
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

	return (uint32_t)(time - epochMilli)
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
		printk(KERN_NOTICE "Ignore rasp gpio intrrupt for too often!\n");
		return IRQ_HANDLED;
	}

	//< real handle
	last_int_time = time;
	
	local_irq_save(flag);
	printk(KERN_NOTICE "Rasp gpio irq [%d] triggered!\n", irq);
	local_irq_restore(flag);

	return IRQ_HANDLED;
}

static int rasp_gpio_open(struct inode * inode, struct file * filp){
	
}



