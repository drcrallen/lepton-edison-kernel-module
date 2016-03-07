#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/pci.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/pci_ids.h>

#define DEVICE_NAME "lepton"
#define CLASS_NAME "lepton"

#define BUF_SIZE 164
#define BITS_PER_WORD 8
#define LEPTON_SPEED_HZ 12500000

static struct spi_device* spi_dev = NULL;
static int major_number;
static struct class *leptonClass = NULL;
static struct device *leptonDevice = NULL;
static volatile int lepton_module_running = 0;

static int dev_open(struct inode *ignore1, struct file *ignore2) {
    return 0;
}
static int dev_release(struct inode *ignore1, struct file *ignore2) {
    return 0;
}
static ssize_t dev_read(struct file *f, char *out, size_t len, loff_t *off) {
    struct spi_device *s_dev = spi_dev;
    struct spi_transfer xfers[1];
    struct spi_message message;
    int result;

    if (len != BUF_SIZE) {
        printk(KERN_INFO "Refusing to read length of %d, only accepting %d\n", (int)len, (int)BUF_SIZE);
        return -EINVAL;
    }

    if (s_dev == NULL) {
        printk(KERN_INFO "Tried to read from a closed device\n");
        return -EINVAL;
    }
    memset(&xfers[0], 0, sizeof(struct spi_transfer));

    xfers[0].len = BUF_SIZE;
    // TODO: dma_pool instead of full allocation
    xfers[0].rx_buf = kmalloc(BUF_SIZE, GFP_DMA | GFP_KERNEL);
    if(!xfers[0].rx_buf) {
        printk(KERN_ALERT "Failed to allocate %d bytes for lepton\n", (int)BUF_SIZE);
        return -EBUSY;
    }
    memset(xfers[0].rx_buf, 0, BUF_SIZE);
    //xfers[0].rx_buf = dma_zalloc_coherent(&s_dev->dev, BUF_SIZE, &(xfers[0].rx_dma), GFP_DMA | GFP_KERNEL);
    xfers[0].speed_hz = LEPTON_SPEED_HZ;
    xfers[0].bits_per_word = BITS_PER_WORD;
    xfers[0].cs_change = 0;
    xfers[0].delay_usecs = 10;
    //if(xfers[0].rx_buf == NULL || dma_mapping_error(&s_dev->dev, xfers[0].rx_dma)) {

    spi_message_init_with_transfers(
            &message,
            xfers,
            1
    );
    message.spi = s_dev;
    message.is_dma_mapped = 1;

    spi_bus_lock(s_dev->master);
    result = spi_sync_locked(s_dev, &message);
    spi_bus_unlock(s_dev->master);
    if (message.status) {
        printk(KERN_ERR "Failed to read from Lepton SPI: %d\n", (int)message.status);
        result = -EAGAIN;
    } else {
        result = message.actual_length - copy_to_user(out, xfers[0].rx_buf, BUF_SIZE);
    }

    //dma_free_coherent(&s_dev->dev, BUF_SIZE, xfers[0].rx_buf, xfers[0].rx_dma);
    kfree(xfers[0].rx_buf);
    return result;
}

static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .release = dev_release,
};

static int lepton_spi_probe(struct spi_device *spi) {
    printk(KERN_ALERT "Shouldn't probe!\n");
    return -1;
}

static int lepton_spi_remove(struct spi_device *spi) {
    printk(KERN_ALERT "Shouldn't remove!\n");
    return -1;
}


static int __lepton_driver_list(struct device *dev, void *n)
{
    struct list_head *dma_head = NULL;
    struct dma_pool *pool;
    const struct spi_device *spi = to_spi_device(dev);
    printk(KERN_INFO "Found spi device [%s]\n", spi->modalias);
    dma_head = &dev->dma_pools;
    printk(KERN_INFO "With%s DMA pools. With%s DMA mask\n", list_is_singular(dma_head) ? "out" : "", dev->coherent_dma_mask ? "" : "out");
    return !strcmp(spi->modalias, "spidev");
}

static int __init lepton_spi_init(void)
{
    struct spi_master *master = NULL;
    u16 i;

    for(i = 0; i < 10 && !spi_dev; ++i) {
        master = spi_busnum_to_master(i);
        if(master == NULL) {
            continue;
        }
        printk(KERN_INFO "Found master on bus %d\n", (int)i);
        if(master->dev.bus) {
            printk(KERN_INFO "Found master on bus %d [%s:%s]\n", (int)i, master->dev.bus->name, master->dev.bus->dev_name);
        } else {
            printk(KERN_INFO "No bus on %d\n", (int) i);
        }
        if(master->dma_alignment) {
            printk(KERN_INFO "DMA alignment: %d\n", (int) master->dma_alignment);
        }
        spi_dev = device_find_child(&master->dev, NULL, __lepton_driver_list);
    }

    printk(KERN_INFO "Added lepton device with%s dma\n", spi_dev->dev.dma_mask ? "" : "out");
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if(major_number < 0) {
        printk(KERN_ALERT "Failed to register lepton major number!\n");
        return major_number;
    }
    leptonClass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(leptonClass)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Failed to register lepton device class!\n");
        return PTR_ERR(leptonClass);
    }
    leptonDevice = device_create(leptonClass, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(leptonDevice)) {
        class_unregister(leptonClass);
        class_destroy(leptonClass);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create lepton device!\n");
        return PTR_ERR(leptonDevice);
    }
    printk(KERN_INFO "Lepton intialized\n");
    return 0;
}

static void __exit lepton_spi_cleanup(void)
{
    printk(KERN_INFO "Cleaning up lepton module.\n");
    device_destroy(leptonClass, MKDEV(major_number, 0));
    class_unregister(leptonClass);
    class_destroy(leptonClass);
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "Lepton unregistered\n");
}


module_init(lepton_spi_init);
module_exit(lepton_spi_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Allen");
MODULE_DESCRIPTION("SPI module for the Lepton");
MODULE_VERSION("0.1");
MODULE_ALIAS("spi:lepton");

