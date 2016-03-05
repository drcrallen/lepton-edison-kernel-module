SUMMARY = "Lepton DMA test module"
LICENSE = "GPLv2"

LIC_FILES_CHKSUM="file://license-destdir/lepton/generic_GPLv2;md5=801f80980d171dd6425610833a22dbe6"

inherit module

PR = "r0"
PV = "0.1"

SRC_URI = "file://Makefile \
           file://lepton.c \
          "

S = "${WORKDIR}"

# The inherit of module.bbclass will automatically name module packages with
# "kernel-module-" prefix as required by the oe-core build environment.
