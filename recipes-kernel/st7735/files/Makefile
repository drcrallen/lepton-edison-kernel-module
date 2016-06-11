obj-m := st7735.o

SRC := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC)

modules_install:
	$(MAKE) INSTALL_MOD_DIR=kernel/drivers/misc/lepton -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	$(MAKE) INSTALL_MOD_DIR=kernel/drivers/misc/lepton -C $(KERNEL_SRC) M=$(SRC) clean

