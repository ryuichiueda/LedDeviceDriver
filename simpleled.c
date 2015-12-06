/* simpleled.c
 *
As shown in the code, this code can be distributed under GNU GPL.
*/
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/time.h>

MODULE_AUTHOR("Ryuichi Ueda");
MODULE_DESCRIPTION("Enjoy led");
MODULE_LICENSE("GPL");

static int devmajor = 0;
static int devminor = 0;
static char* msg = "module [simpleled.o]";
static char* devname = "simpleled";

static struct cdev cdv;
static int access_num = 0;
static spinlock_t spn_lock;

/* step2: GPIOの初期化 */
const int led = 18;
static volatile uint32_t *gpio_base = NULL;
const uint32_t rpi_gpio_base = 0x3f000000 + 0x200000;//レジスタのベース+ GPIOのオフセット
const uint32_t rpi_block_size = 4096;
void *gpio_map;
const char* gpio_map_name = "simpleled";

static struct class *cls = NULL;

static int led_open(struct inode* inode, struct file* filp);
static ssize_t led_write(struct file* filp, const char* buf, size_t count, loff_t* pos);
static int led_release(struct inode* inode, struct file* filp);

static struct file_operations led_fops = 
{
	owner   : THIS_MODULE,
	//read    : led_read,
	write    : led_write,
	open    : led_open,
	release : led_release,
};

static int led_open(struct inode* inode, struct file* filp)
{
	printk(KERN_INFO "%s : open() called\n", msg);

	spin_lock(&spn_lock);

	if(access_num){
		spin_unlock(&spn_lock);
		return -EBUSY;
	}

	access_num++;
	spin_unlock(&spn_lock);

	return 0;
}

static ssize_t led_write(struct file* filp, const char* buf, size_t count, loff_t* pos)
{
	char c;
	uint32_t index;
	unsigned char shift;
	uint32_t mask;

	if(copy_from_user(&c,buf,sizeof(char)))
		return -EFAULT;

	//GPIO Function Setレジスタに機能を設定 
	index = 0 + led/10;// 0: index of GPFSEL0
	shift = (led%10)*3;
	mask = ~(0x7 << shift);
	gpio_base[index] = (gpio_base[index] & mask) | (1 << shift);//1: OUTPUTのフラグ

	/* step3: GPIOに値を送る */
	printk(KERN_INFO "input: %c\n", c);
	if(c == '0'){
		gpio_base[10] = 1<<led;//10: index of GPCLR0
	}else if(c != '\n'){
		gpio_base[7] = 1<<led;//7: index of GPSET0
	}//改行の場合スルー

	return 1;
}

static int led_release(struct inode* inode, struct file* filp)
{

	printk(KERN_INFO "%s : close() called\n", msg);

	spin_lock(&spn_lock);
	access_num--;
	spin_unlock(&spn_lock);

	return 0;
}

static int __init dev_init_module(void)
{
	int retval;
	dev_t dev, devno;
	
	retval =  alloc_chrdev_region(&dev, 0, 1, "simpleled");
	if(retval < 0){
		printk(KERN_ERR "alloc_chrdev_region failed.\n");
		return retval;
	}
	
	cls = class_create(THIS_MODULE,"simpleled");
	if(IS_ERR(cls))
		return PTR_ERR(cls);

	devmajor = MAJOR(dev);
	devno = MKDEV(devmajor, devminor);
	cdev_init(&cdv, &led_fops);
	cdv.owner = THIS_MODULE;
	if(cdev_add(&cdv, devno, 1) < 0)
		printk(KERN_ERR "cdev_add failed minor = %d\n", devminor);
	else
		device_create(cls, NULL, devno, NULL, "simpleled%u",devminor);

	if(!request_mem_region(rpi_gpio_base, rpi_block_size, gpio_map_name)){
		printk(KERN_ERR "request_mem_region failed.\n");
		return -EBUSY;
	}

	gpio_map = ioremap_nocache(rpi_gpio_base,rpi_block_size);
	gpio_base = (volatile uint32_t *)gpio_map;


	return 0;
}

static void __exit dev_cleanup_module(void)
{
	dev_t devno;

	iounmap(gpio_map);
	release_mem_region(rpi_gpio_base, rpi_block_size);

	gpio_map = NULL;
	gpio_base = NULL;

	cdev_del(&cdv);
	devno = MKDEV(devmajor, devminor);
	device_destroy(cls, devno);
	class_destroy(cls);
	unregister_chrdev(devmajor, devname);
	printk(KERN_INFO "%s : removed from kernel\n", msg);
}

module_init(dev_init_module);
module_exit(dev_cleanup_module);
