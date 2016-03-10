lepton-edison-kernel-module
--------------------------
Kernel module for the lepton on the edison

# Installing

1. Checkout this repository to your development machine.
1. From your Edison SDK directory enter your build environment with the normal `source poky/oe-init-build-env` commnad.
1. Add the path to this repository to the list of `BBLAYERS` in `conf/bblayers.conf`
1. Build the module with the command `bitbake lepton`
1. When the build completes, the module will be installable by copying `tmp/deploy/ipk/edison/kernel-module-lepton_0.1-r0_edison.ipk` and `tmp/deploy/ipk/edison/lepton_0.1-r0_edison.ipk` to your edison via `scp`
1. On the **EDISON** issue the command `opkg install kernel-module-lepton_0.1-r0_edison.ipk lepton_0.1-r0_edison.ipk` to install the module.
1. On the **EDISON** issue the comand `modprobe lepton` to load up the module.
1. You will now have a device at `/dev/lepton` which can be used to read from 

# Expected result of opkg install

```
root@edison:~# opkg install kernel-module-lepton_0.1-r0_edison.ipk lepton_0.1-r0_edison.ipk
Installing kernel-module-lepton (0.1-r0) to root...
Installing lepton (0.1-r0) to root...
Configuring kernel-module-lepton.
Configuring lepton.
```

# Expected result of loading the module via modprobe

```
root@edison:~# dmesg | tail -n 10
<<other stuff redacted>>
[  310.696871] Found master on bus 3
[  310.696901] Found master on bus 5
[  310.696917] Found spi device [ads7955]
[  310.696931] Found spi device [spidev]
[  310.704988] Lepton intialized
root@edison:~# ls -al /dev/lepton
crw-------    1 root     root      243,   0 Mar 10 04:53 /dev/lepton
```

# Reading from the Lepton
Every read from `/dev/lepton` *MUST* be *EXACTLY* 164 bytes. Any other size read will fail.
