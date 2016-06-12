SUMMARY = "ST7735 DMA test module"
LICENSE = "GPLv2"

LIC_FILES_CHKSUM="file://license-destdir/st7735/generic_GPLv2;md5=801f80980d171dd6425610833a22dbe6"

inherit module

PR = "r0"
PV = "0.1"

SRC_URI = "file://Makefile \
           file://st7735.c \
          "

S = "${WORKDIR}"

# The inherit of module.bbclass will automatically name module packages with
# "kernel-module-" prefix as required by the oe-core build environment.
