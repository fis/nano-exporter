# nano-exporter (OpenWrt packaging)

A minimalistic exporter of node metrics for the Prometheus monitoring
system.

See the [master branch](https://github.com/fis/nano-exporter) for the
actual program. This entirely unrelated branch contains the OpenWrt
packaging.

## Disclaimer

This is not an officially supported Google product.

There are more disclaimers in the [master
README](https://github.com/fis/nano-exporter/blob/master/README.md) as
well.

## Installation

To include the package in your OpenWrt build, add the following line
to your `feeds.conf` file:

    src-git nanoexporter https://github.com/fis/nano-exporter.git;openwrt

Then update and install it:

    ./scripts/feeds update nanoexporter
    ./scripts/feeds install -p nanoexporter -a

The package `nano-exporter` should now be available via `make
menuconfig`, in the "Utilities" category.
