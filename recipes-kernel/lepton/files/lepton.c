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
#define BITS_PER_WORD 16
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
    struct spi_device *s_dev;
    struct spi_transfer xfers[1];
    struct spi_message message;
    int result;

    if (len != BUF_SIZE) {
        printk(KERN_INFO "Refusing to read length of %d, only accepting %d\n", (int)len, (int)BUF_SIZE);
        return -1;
    }

    s_dev = spi_dev;
    if (s_dev == NULL) {
        printk(KERN_INFO "Tried to read from a closed device\n");
        return -1;
    }
    memset(&xfers[0], 0, sizeof(struct spi_transfer));

    xfers[0].len = BUF_SIZE;
    // TODO: dma_pool instead of full allocation
    //xfers[0].rx_buf = kmalloc(BUF_SIZE, GFP_DMA | GFP_KERNEL);
    xfers[0].rx_buf = dma_zalloc_coherent(&s_dev->dev, BUF_SIZE, &(xfers[0].rx_dma), GFP_DMA | GFP_KERNEL);
    xfers[0].speed_hz = s_dev->max_speed_hz;
    xfers[0].bits_per_word = s_dev->bits_per_word;
    if(xfers[0].rx_buf == NULL) {
        printk(KERN_ALERT "Failed to allocated %d DMA bytes for lepton\n", (int)BUF_SIZE);
        return -1;
    }

    spi_message_init_with_transfers(
            &message,
            xfers,
            1
    );
    message.spi = s_dev;

    spi_bus_lock(s_dev->master);
    result = spi_sync_locked(s_dev, &message);
    spi_bus_unlock(s_dev->master);
    if (message.status) {
        printk(KERN_ERR "Failed to read from Lepton SPI: %d\n", (int)message.status);
        result = -1;
    } else {
        result = message.actual_length - copy_to_user(out, xfers[0].rx_buf, BUF_SIZE);
    }

    dma_free_coherent(&s_dev->dev, BUF_SIZE, xfers[0].rx_buf, xfers[0].rx_dma);
    //kfree(xfers[0].rx_buf);
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
    /*
  printk(KERN_INFO "Probing lepton SPI\n");
  spi_bus_lock(spi->master);
  if (spi_dev != NULL) {
    printk(KERN_INFO "Already have an SPI device, failing probe\n");
    spi_bus_unlock(spi->master);
    return -1;
  }
  spi->max_speed_hz = LEPTON_SPEED_HZ;
  spi->bits_per_word = BITS_PER_WORD;
  spi_setup(spi);
  spi_dev = spi;
  lepton_module_running = 1;
  spi_bus_unlock(spi->master);
  printk(KERN_INFO "Lepton SPI protocol probed.\n");
  return 0;
  */
}

static int lepton_spi_remove(struct spi_device *spi) {
    printk(KERN_ALERT "Shouldn't remove!\n");
    return -1;
    /*
  if (spi != spi_dev) {
    printk(KERN_INFO "Asked to remove an SPI device we aren't using\n");
    return -1;
  }
  spi_bus_lock(spi->master);
  spi_dev = NULL;
  spi_bus_unlock(spi->master);
  return 0;
  */
}

// See https://github.com/primiano/edison-kernel/blob/master/arch/x86/platform/intel-mid/board.c
// and https://github.com/primiano/edison-kernel/blob/master/drivers/spi/spi-dw-pci.c
// and https://github.com/primiano/edison-kernel/blob/master/drivers/spi/spi-dw.h
// and https://github.com/primiano/edison-kernel/blob/master/drivers/spi/spi.c#L71
static struct spi_device_id idtable[] = {
  // Other devices on edison
  //{"ads7955", 0},
  //{"spi_max3111", 0},
  {"spidev", 0},
  /* Intel Medfield platform SPI controller 1 */
  //{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x0800), .driver_data = 0 },
  //{"dw_spi_pci", 0},
  {}
};
static struct spi_driver lepton_spi_driver = {
  .driver = {
    .name = "lepton",
    /** Auto populated
    .owner = THIS_MODULE,
    .bus = &spi_bus_type,
    .probe = lepton_spi_probe,
    .remove = lepton_spi_remove,
    */
  },
  .probe = lepton_spi_probe,
  .remove = lepton_spi_remove,
  .id_table = idtable,
};

static int __lepton_driver_list(struct device *dev, void *n)
{
    const struct spi_device *spi = to_spi_device(dev);
    printk(KERN_INFO "Found spi device [%s]\n", spi->modalias);
    return 0;
}

static int __init lepton_spi_init(void)
{
    struct spi_master *master = NULL;
    int res;
    u16 i;

    for(i = 0; i < 10; ++i) {
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
        device_find_child(&master->dev, NULL, __lepton_driver_list);
    }

    master = spi_busnum_to_master(5);
    spi_dev = spi_alloc_device(master);
    spi_dev->max_speed_hz = LEPTON_SPEED_HZ;
    spi_dev->mode = SPI_MODE_3;
    spi_dev->bits_per_word = BITS_PER_WORD;
    spi_dev->chip_select = 3;// we don't use it
    spi_dev->cs_gpio = -ENOENT;
    res = spi_add_device(spi_dev);
    printk(KERN_INFO "Added lepton device with%s dma\n", spi_dev->dev.dma_mask ? "" : "out");
    //res = master->setup(&lepton_spi_driver);
    //res = spi_register_driver(&lepton_spi_driver);
    if (res)
      return res;
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if(major_number < 0) {
        //spi_unregister_driver(&lepton_spi_driver);
        spi_unregister_device(spi_dev);
        printk(KERN_ALERT "Failed to register lepton major number!\n");
        return major_number;
    }
    leptonClass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(leptonClass)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        //spi_unregister_driver(&lepton_spi_driver);
        spi_unregister_device(spi_dev);
        printk(KERN_ALERT "Failed to register lepton device class!\n");
        return PTR_ERR(leptonClass);
    }
    leptonDevice = device_create(leptonClass, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(leptonDevice)) {
        class_unregister(leptonClass);
        class_destroy(leptonClass);
        unregister_chrdev(major_number, DEVICE_NAME);
        //spi_unregister_driver(&lepton_spi_driver);
        spi_unregister_device(spi_dev);
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
    spi_unregister_device(spi_dev);
    printk(KERN_INFO "Lepton unregistered\n");
}


module_init(lepton_spi_init);
module_exit(lepton_spi_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Allen");
MODULE_DESCRIPTION("SPI module for the Lepton");
MODULE_VERSION("0.1");
MODULE_ALIAS("spi:lepton");
MODULE_DEVICE_TABLE(spi, idtable);

