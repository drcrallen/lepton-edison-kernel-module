#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/pci.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/pci_ids.h>

#define DEVICE_NAME "st7735"
#define CLASS_NAME "st7735"

#define BITS_PER_WORD 8
#define ST7735_SPEED_HZ 12500000

static struct spi_device* spi_dev = NULL;
static int major_number;
static struct class *st7735Class = NULL;
static struct device *st7735Device = NULL;

static int dev_open(struct inode *ignore1, struct file *ignore2) {
    return 0;
}
static int dev_release(struct inode *ignore1, struct file *ignore2) {
    return 0;
}
static ssize_t dev_read(struct file *f, char *in, size_t len, loff_t *off) {
    struct spi_device *s_dev = spi_dev;
    struct spi_transfer xfers[1];
    struct spi_message message;
    u16 prior_mode;
    int result;

    if (s_dev == NULL) {
        printk(KERN_INFO "Tried to read from a closed device\n");
        return -EINVAL;
    }
    memset(&xfers[0], 0, sizeof(struct spi_transfer));

    xfers[0].len = len;
    xfers[0].tx_buf = kmalloc(len, GFP_DMA | GFP_KERNEL);
    if(!xfers[0].tx_buf) {
        printk(KERN_ALERT "Failed to allocate %d bytes for st7735\n", (int)BUF_SIZE);
        return -EBUSY;
    }
    memset(xfers[0].tx_buf, 0, len);
    result = copy_from_user(xfers[0].tx_buf, in, len);
    if(!result) {
        printk(KERN_ALERT "Faile to copy %zd bytes from user space\n", len);
        kfree(xfers[0].tx_buf);
        return -EBUSY;
    }
    xfers[0].speed_hz = ST7735_SPEED_HZ;
    xfers[0].bits_per_word = BITS_PER_WORD;
    xfers[0].cs_change = 0;

    spi_message_init_with_transfers(
            &message,
            xfers,
            1
    );
    message.spi = s_dev;
    message.is_dma_mapped = 1;

    spi_bus_lock(s_dev->master);
    // Temporarily force mode
    prior_mode = s_dev->mode;
    s_dev->mode = SPI_MODE_0;
    result = spi_sync_locked(s_dev, &message);
    s_dev->mode = prior_mode;
    spi_bus_unlock(s_dev->master);

    if (message.status) {
        printk(KERN_ERR "Failed to write to st7735 SPI: %d\n", (int)message.status);
        result = -EAGAIN;
    }

    kfree(xfers[0].tx_buf);
    return result;
}

static struct file_operations fops = {
    .open = dev_open,
    .write = dev_write,
    .release = dev_release,
};

static int st7735_spi_probe(struct spi_device *spi) {
    printk(KERN_ALERT "Shouldn't probe!\n");
    return -1;
}

static int st7735_spi_remove(struct spi_device *spi) {
    printk(KERN_ALERT "Shouldn't remove!\n");
    return -1;
}


static int __st7735_driver_list(struct device *dev, void *n)
{
    struct list_head *dma_head = NULL;
    struct dma_pool *pool;
    const struct spi_device *spi = to_spi_device(dev);
    printk(KERN_INFO "Found spi device [%s]\n", spi->modalias);
    return !strcmp(spi->modalias, "spidev");
}

static int __init st7735_spi_init(void)
{
    struct spi_master *master = NULL;
    u16 i;

    for(i = 0; i < 10 && !spi_dev; ++i) {
        master = spi_busnum_to_master(i);
        if(master == NULL) {
            continue;
        }
        printk(KERN_INFO "Found master on bus %d\n", (int)i);
        spi_dev = device_find_child(&master->dev, NULL, __st7735_driver_list);
    }

    if(!spi_dev) {
        printk(KERN_ALERT "No spidev found for st7735 module\n");
        return -ENODEV;
    }

    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if(major_number < 0) {
        printk(KERN_ALERT "Failed to register st7735 major number!\n");
        return major_number;
    }
    st7735Class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(st7735Class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Failed to register st7735 device class!\n");
        return PTR_ERR(st7735Class);
    }
    st7735Device = device_create(st7735Class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(st7735Device)) {
        class_unregister(st7735Class);
        class_destroy(st7735Class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create st7735 device!\n");
        return PTR_ERR(st7735Device);
    }
    printk(KERN_INFO "st7735 intialized\n");
    return 0;
}

static void __exit st7735_spi_cleanup(void)
{
    printk(KERN_INFO "Cleaning up st7735 module.\n");
    device_destroy(st7735Class, MKDEV(major_number, 0));
    class_unregister(st7735Class);
    class_destroy(st7735Class);
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "st7735 unregistered\n");
}


module_init(st7735_spi_init);
module_exit(st7735_spi_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Allen");
MODULE_DESCRIPTION("SPI module for the st7735");
MODULE_VERSION("0.1");
MODULE_ALIAS("spi:st7735");

