# prometheus-nano-exporter

TODO: one-paragraph description.

## Word of Warning

There are no particularly compelling reasons to use this software over
the alternatives. On a Real Computer™, the standard [Prometheus node
exporter](https://github.com/prometheus/node_exporter) is perfectly
adequate, and has a much larger feature set, not to mention adoption
rate. On OpenWRT in particular, the standard community-supported
package repository already has a lightweight Lua rewrite as
[prometheus-node-exporter-lua](https://openwrt.org/packages/pkgdata/prometheus-node-exporter-lua),
which is even smaller in disk footprint, if you don't count the Lua
interpreter.

The only reason why you would *need* this version is if you were
building for a particularly constrained system, and did not want to
include the Lua interpreter, or *any* foreign dependencies (other than
a C runtime library).

Other than that, you might choose to use it for æsthetic reasons.

## Installation

TODO: instructions.

The metrics collection is highly Linux-specific. You probably won't
have much luck on other operating systems.

## Collectors

For the most part, the produced metrics try to loosely (but not
slavishly) adhere to the format used by the standard Prometheus node
exporter (as of version v0.16).

This is an overview of the available collectors. See the [Collector
Reference](#collector-reference) below for detailed documentation,
including generated metrics, labels and configuration options.

| Name | Description |
| ---- | ----------- |
| [`cpu`](#cpu) | CPU usage from `/proc/stat` and CPU frequency scaling data from sysfs. |
| [`hwmon`](#hwmon) | Temperature, fan and voltage sensors from `/sys/class/hwmon`. |
| [`network`](#network) | Network device transmit/receive statistics from `/proc/net/dev`. |
| [`textfile`](#textfile) | Custom metrics from `.prom` text files dropped in a directory. |
| [`uname`](#uname) | Node information returned by the `uname` system call. |

## Usage

By default, all collectors are enabled. Use a command line argument of
the form `--{collector}-off` (e.g., `--cpu-off`) to selectively
disable specific collectors. Alternatively, if you explicitly enable
any collector with `--{collector}-on`, only such explicitly enabled
collectors are actually enabled. You probably don't want to mix both
`on` and `off` flags in the same invocation.

The general command line arguments are documented below. Specific
collectors may also accept further arguments, which will always have
the prefix `--{collector}-`. They are documented in the [Collector
Reference](#collector-reference) section.

| Flag | Description |
| ---- | ----------- |
| `--port=X` | Listen on port *X* instead of the default port (9100). |

## Collector Reference

### `cpu`

Metrics and labels:

* `node_cpu_seconds_total{cpu=N,mode=M}`: Number of CPU seconds spent
  in various different modes, as reported in `/proc/stat`. *N* is the
  CPU index, a number between 0 and one less than the number of
  (logical) CPUs in the system. The mode *M* is one of `user`, `nice`,
  `system`, `idle`, `iowait`, `irq`, `softirq` or `steal`. There will
  be one row for each CPU and each mode in the scrape.
* `node_cpu_frequency_hertz{cpu=N}`: The current CPU clock frequency
  in Hertz at the time of the scrape. This is the
  `cpufreq/scaling_cur_freq` value under the CPU-specific sysfs
  directory.

### `hwmon`

The `hwmon` collector pulls data from all the sysfs subdirectories
under `/sys/class/hwmon`. The supported entry types are temperature
(`temp*`), fan (`fan*` and voltage (`in*`) sensors.

Metrics and dimensions:

* `node_hwmon_temp_celsius`: Current temperature in degrees Celsius.
* `node_hwmon_fan_rpm`: Current fan speed in RPM.
* `node_hwmon_fan_min_rpm`: Threshold for minimum fan speed.
* `node_hwmon_fan_alarm`: Active fan alarm signal: `0`/`1`.
* `node_hwmon_in_volts`: Input voltage measurement.
* `node_hwmon_in_min_volts`: Lower threshold for a voltage alarm.
* `node_hwmon_in_max_volts`: Upper threshold for a voltage alarm.
* `node_hwmon_in_alarm`: Active voltage alarm signal: `0`/`1`.

All the metrics have the same two labels: `chip` and `sensor`. The
`chip` label is derived from the sysfs directory path, while the
`sensor` label designates a specific sensor on the same (logical)
chip.

The values are by default directly as reported in sysfs: there's no
built-in scaling. TODO: scaling options.

### `network`

Metrics and labels:

* `node_network_receive_X{device=D}`: Metrics related to receiving
  data on network interface *D*.
* `node_network_transmit_Y{device=D}`: Metrics related to sending data
  on network interface *D*.

The exact set of metrics (`X` and `Y` above) depends on the columns
included in your `/proc/net/dev` file. A normal set is:

| Receive | Transmit | Metric | Description |
| ------- | -------- | ------ | ----------- |
| X | X | `bytes` | Byte counter |
| X | X | `packets` | Packet counter |
| X | X | `errs` | Errors while receiving/transmitting |
| X | X | `drop` | Dropped frame count |
| X | X | `fifo` | ? |
| X |   | `frame` | ? |
| X |   | `compressed` | ? |
| X |   | `multicast` | Byte count |
|   | X | `colls` | Collisions while transmitting |
|   | X | `carrier` | ? |

### `textfile`

The `textfile` collector can be used to conveniently export custom
node-bound metrics. Metrics in any files ending in `.prom` in the
designated directory are included in the scrape. Generally you should
write to a file with a different suffix (say `.prom.tmp`) and then
atomically rename the file, to prevent the server for sending data
from incomplete metrics files.

The implementation in this program copies the file contents directly
to the outgoing HTTP response. It is your responsibility to make sure
the files conform to the Prometheus exposition format. The only
modification done is to add a terminating newline to the file, is one
is not already present.

### `uname`

The `uname` collector exports data from the eponymous system call as
labels attached to the metric `node_uname_info`, which always has the
value 1. The attached labels are:

* `machine`
* `nodename`
* `release`
* `sysname`
* `version`

See your `uname(2)` man page for details of the values.