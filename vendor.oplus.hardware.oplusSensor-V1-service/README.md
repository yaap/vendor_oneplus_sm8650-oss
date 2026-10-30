# vendor-oplus-hardware-oplusSensor-V1-service

Open-source replacement for the OPLUS engineering sensor feature HAL
(`vendor.oplus.hardware.oplusSensor.ISensorFeature/default`).

The stock service is a factory/engineering helper. It is not part of the
normal sensor data path; Android's sensor framework uses the QTI/OPLUS sensor
multihal. This service is used by engineering-mode tools, factory calibration
and the `fusionlight` sensor subhal for display-leakage calibration.

## Interface

The AIDL is the public interface from `hardware/oplus/interfaces`:

```text
vendor.oplus.hardware.oplusSensor.ISensorFeature/default
```

It exposes nine methods:

- `calibrateSensor`
- `getSensorCalibrationData`
- `getSensorConfig`
- `getSensorRawData`
- `saveCalibrationData`
- `sensorRawDataEnableDisable`
- `sensorReadNodeFile`
- `sensorWriteNodeFile`
- `setPhoneMode`

## Backend

The implementation uses the same backends as the stock binary:

- sensor calibration registry:
  `/mnt/vendor/persist/sensors/registry/registry`
- camera calibration data: `/mnt/vendor/persist/camera`
- spectrum calibration data:
  `/mnt/vendor/persist/engineermode/sensor/spectrum`
- OPLUS reserve partition: `/dev/block/by-name/oplusreserve1`
- sensor configuration: `/odm/etc/sensor/sensor_config.json`
- ALS/pressure/magnetometer proc nodes
- the stock 23-entry proc/sysfs node allowlist

The reserve-partition offsets select the UFS or eMMC layout from
`ro.boot.bootdevice`.

At startup the service reproduces the stock "OOS storage mode" probe: when the
four `persist/engineermode/*_max_lux` files exist (and the SoC is not SM4350),
RGB, proximity and screen-light calibration use the legacy vendor schemas.
Otherwise the newer registry/reserve schemas are used. The constructor also
publishes the current screen-light maxima to `/proc/sensor/als_cali`.

The stock filtered schemas are preserved rather than returning raw registry
objects: light scale fields, RGB calibration/gain/leak fields, spectral
gain/SN/wavelength fields, proximity/fake-proximity/hall/angle-hall/hinge
fields, pad/vice-pad light, screen SN, pressure, consumer IR and free-fall.
The full key lists and reserve offsets are in `RE-NOTES.md`.

## Build

```text
vendor-oplus-hardware-oplusSensor-V1-service
```

The binary is installed as
`/odm/bin/hw/vendor-oplus-hardware-oplusSensor-V1-service`, matching the stock
init service and file context. Add the module to the device `PRODUCT_PACKAGES`
and remove the prebuilt from `sm8650-common-vendor.mk`; the source module
installs its own rc and VINTF fragment.

## Runtime

The stock rc starts the service when `ro.vendor.oplus.sensor.lowmem` is empty
or `false`. When the property is `true`, the service is registered lazily if
started manually:

```text
getprop ro.vendor.oplus.sensor.lowmem
service list | grep oplusSensor
```

No prebuilt daemon is required.

## Future reverse-engineering targets

The remaining OPLUS-specific blobs in `sm8650-common` are, in priority order:

1. `vendor.oplus.hardware.charger-V11-service` — the only charger HAL
   installed by the vendor makefile; the AIDL v11 interface is already open.
   Most methods map to `/sys/class/oplus_chg`, `/proc/charger` and the
   `oplus_chg` kernel driver. Large but high value.
2. `gauged` + `wlschgd` — tiny one-shot `/dev/sec_dev` and `/dev/wls_dev`
   helpers; low value, but a small standalone module.
3. `horae` — thermal/performance daemon; high value, but its shipped configs
   are binary/obfuscated and the policy surface is large.

QTI daemons (`tcmd`, `engine-service`, `sensors.qti`,
`sensorscalibrate`) and display support libraries such as
`libclstc_fod_color.so` should be skipped; they are not OPLUS-specific or
require the closed display/composer pipeline.
