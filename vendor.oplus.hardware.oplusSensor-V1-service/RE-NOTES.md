# Reverse-engineering notes

Reference binary:

```text
vendor/oneplus/sm8650-common/proprietary/odm/bin/hw/
    vendor-oplus-hardware-oplusSensor-V1-service
```

The binary is stripped but its AIDL implementation class and the calibration
helper names are present in RTTI/exported metadata. Ghidra recovered the
`SensorFeature` class, the AIDL dispatch and the calibration helpers.

## Service registration

`main()` allocates `SensorFeature`, then chooses:

- `AServiceManager_addService()` when
  `ro.vendor.oplus.sensor.lowmem` is false;
- `AServiceManager_registerLazyService()` when it is true.

The interface descriptor is
`vendor.oplus.hardware.oplusSensor.ISensorFeature/default`.

## OOS storage mode

At construction the stock service calls `getScreenLightSensorCaliData()`. That
helper sets a process-wide `gIsOosStorageMode` flag when any of these files can
be read:

```text
/mnt/vendor/persist/engineermode/red_max_lux
/mnt/vendor/persist/engineermode/green_max_lux
/mnt/vendor/persist/engineermode/blue_max_lux
/mnt/vendor/persist/engineermode/white_max_lux
```

The flag is suppressed when `ro.soc.model` is `SM4350`. It changes the RGB,
proximity and screen-light schemas, so the implementation evaluates it once and
keeps it for the process lifetime. The constructor also publishes the current
`R_MAX/G_MAX/B_MAX/W_MAX` values to `/proc/sensor/als_cali/*_max_lux`. In OOS
storage mode the screen-light scale helper also backs up the previous
`persist/engineermode/calicoe` value to `calicoe_bak` when it changes.

## Node allowlist

`sensorReadNodeFile` and `sensorWriteNodeFile` use a 23-entry table of
`{name, base directory, read size}` records:

```text
red_max_lux             /proc/sensor/als_cali                 16
green_max_lux           /proc/sensor/als_cali                 16
blue_max_lux            /proc/sensor/als_cali                 16
white_max_lux           /proc/sensor/als_cali                 16
row_coe                 /proc/sensor/als_cali                 16
cali_coe                /proc/sensor/als_cali                 16
offset                  /proc/sensor/pressure_cali            16
prjName                 /proc/oplusVersion                    16
ps                      /sys/bus/platform/drivers/als_ps      16
ps_raw                  /sys/bus/platform/drivers/als_ps      16
ps_state                /sys/bus/platform/drivers/als_ps      16
cali                    /sys/bus/platform/drivers/als_ps      16
gain_als                /sys/bus/platform/drivers/als_ps      16
als                     /sys/bus/platform/drivers/als_ps      16
test_id                 /sys/bus/platform/drivers/msensor     16
magnet_close            /sys/bus/platform/drivers/msensor     16
magnet_leave            /sys/bus/platform/drivers/msensor     16
get_ic_modle            /sys/bus/platform/drivers/msensor     16
factory_step_debounce   /sys/bus/platform/drivers/gsensor     16
hw_region_id            /proc/devinfo                         16
lcd                     /proc/devinfo                        100
lcd_s                   /proc/devinfo                        100
ssc_screenshot_info     /dev                                 128
```

Reads use the table's size, not the AIDL `length` argument. Unsupported names
return `EX_ILLEGAL_ARGUMENT`; writes larger than 4 KiB are rejected. Reads and
writes return the raw byte count from the node.

## Calibration backend

The stock helpers resolve a calibration file through the sensor registry:

1. parse `/mnt/vendor/persist/sensors/registry/registry/<devinfo key>`;
2. read `<node>.cal_path.data` (the calibration file name);
3. parse `/mnt/vendor/persist/sensors/registry/registry/<file name>`.

Observed keys include:

```text
devinfo.gsensor          x/y/z offset + version
devinfo.gsensor_sub
devinfo.gyro             x/y/z offset + version
devinfo.gyro_sub
devinfo.als              scale{data,ver}
devinfo.als_rear
devinfo.hall_effect      20/0/180 degree data + version
devinfo.angle_hall       60/150/180 degree data + version
devinfo.hinge_detect     cali_para0..2
devinfo.fold_state       cali_para0..2
devinfo.structure_ps     offset + version
devinfo.ps
devinfo.rgb / devinfo.pad_rgb / devinfo.vice_rgb / devinfo.rgb_rear
devinfo.rgb_rear1
devinfo.cct_rear_name
cct_rear_platform.spectral_serial.serial_num
cct_rear_platform.sync_scale.{512,2048}_{3000k,6000k}
cct_rear_platform.ch_scale.{fac_cal,fac1_cal}
cct_rear_platform.cali_gold_data.{gold_3000k,gold_6000k}
pad_als_platform.fac_cal
vice_pad_als_platform.fac_cal
stk2232_0_platform.ps.fac_cal
wise_rgb_comp_scale
pad_wise_rgb_comp_scale
```

`getSensorConfig()` parses `/odm/etc/sensor/sensor_config.json` and returns
the styled JSON object.

## Filtered getters

The stock getters do not return raw registry objects. They emit these schemas:

- Light/back-light: `cali_lux = 1000000 / atoi(scale.data)`,
  `cali_para = atoi(scale.data)`, `cali_para_extra = scale.data`, and
  `cali_para_ver = scale.ver`. The getter fails if `scale.data` is zero.
- GSensor/gyro: `offset_x/y/z` plus `x_ver/y_ver/z_ver`.
- RGB: `R_cali/G_cali/B_cali/C_cali/W_cali/F_cali` as numeric strings, plus
  `scale`, `clock`, `bias`, `scale_l`, `scale_h`, `cali_para_ver`, and the
  `Gain_1024_*`/`Gain_2048_*` fields. Missing source keys become `"-1"`.
- Spectral (`0x1fa2673`): `Z/Y/XL/NIR/VIS/F/F2/F3/F4/F6/F1/F7/F8/F5_cali_gain1`
  from `devinfo.rgb_rear`, the same `_gain2` fields from
  `devinfo.rgb_rear1`, `sn_low_32bit`/`sn_high_8bit`, and
  `wavelength_cali` (`"0"`/`"1"`).
- Spectral 6000K (`999999`): `_cali_gain1` from
  `cct_rear_platform.sync_scale.512_6000k`, `_cali_gain2` from
  `...2048_6000k`. This getter also writes the derived
  `/mnt/vendor/persist/camera/spectrum_cali_data` file and fails if that write
  fails.
- Proximity (new mode): `ps0_offset`, `ps0_distance_value`,
  `ps0_distance_delta`, `ps0_value`, `ps0_offset_lp`,
  `ps0_distance_1cm_value`, `ps0_distance_1cm_delta`, `ps0_cali_para`,
  `ps0_cali_flag`, and the matching `*_ver` fields.
- Proximity (OOS storage mode): only `ps0_offset`, `offset_ver`,
  `ps0_distance_delta`, `delta_ver`, `ps0_value = "0"` and
  `cali_goal_ver = "0"` from `stk2232_0_platform.ps.fac_cal`.
- Fake proximity: `offset_1/offset_2/threshold_1/threshold_2` plus versions.
- Hall effect: `20deg_data/0deg_data/180deg_data` plus versions.
- Angle hall: `60deg_data/150deg_data/180deg_data` plus versions.
- Hinge: `cali_para1/2/3` plus versions. The stock source keys are
  `cali_para0/1/2`, so the output index is shifted by one.
- Screen light: `R_MAX/G_MAX/B_MAX/W_MAX`, `row_coe`, and the four light-scale
  fields. New mode reads the reserve partition; OOS storage mode reads the
  `persist/engineermode/*_max_lux` files.
- Pad/vice-pad light: `R_MAX/G_MAX/B_MAX/W_MAX`, the four light-scale fields
  from `<pad>_als_platform.fac_cal`, and a numeric `row_coe`. The node exposes
  two integers; pad uses the first and vice-pad uses the second.
- Screen SN: `sreen_sn` and `main_sreen_sn`.
- Pressure: `cali_offset`. Consumer IR: `is_tested`.
- Free-fall: `count`, `count1` (low byte), `count2` (high byte).
- Soft magnetometer: `M1_module = "First"`, `M1_param1..9`,
  `M2_module = "second"`, `M2_param1..9` from the packed 180-byte node.

## Reserve partition

UFS offsets, with the eMMC offsets selected when the boot device is not UFS:

```text
screen light max values   0x434400 / 0x41ae00   16 bytes (R,G,B,W)
pad light max values      0x434420 / 0x41ae20   16 bytes
vice pad max values       0x434440 / 0x41ae40   16 bytes
screen SN                 0x434480 / 0x41ae80   16 bytes
main screen SN            0x4344c0 / 0x41aec0   16 bytes
structure proximity       0x434200              4 bytes
pressure offset           0x434600 / 0x41b200    4 bytes
free-fall counter         0x434604 / 0x41b204    2 bytes
consumer IR test          0x434700 / 0x41b300    4 bytes
```

`saveScreenSnNum` requires 1..16 bytes and writes at the screen-SN base plus
`0x40` for the main display. `getFreeFallCalibrationData` returns the raw
16-bit value plus its low/high bytes. `getPressureCalibrationData` and
`getConsumerIrData` return the raw 32-bit values as strings.

## FusionLight

The `fusionlight` subhal calls `getSensorCalibrationData(33171070)`
(`0x1fa267e`). The stock helper reads `wise_rgb_comp_scale` (or the pad
variant for `0x1fa2662`) and produces:

- `W_VIEW_<R/G/B/C>_<level>` for brightness levels 0..8 from each `C_comp`;
- `<R/G/B/W>_VIEW_<R/G/B/C>_MAX` for `comp0`;
- `_MAX_L` for `comp1`;
- `_MAX_M` for `comp2`.

The RGB getter also emits the same leak fields for `0x1fa2654` and
`0x1fa2662`, so `FusionLightUtils` can apply device calibration instead of the
profile golden values.

## AIDL behavior

- `getSensorRawData` returns `0.0f`; `sensorRawDataEnableDisable` returns
  `false` (the stock service is a no-op for raw streaming).
- `calibrateSensor` returns `false` for the stock-supported factory types. It
  only acts on pressure (`0x6`: writes the proc node and 4 reserve bytes) and
  consumer IR (`0x1fa2a1f`: writes 4 reserve bytes). Unsupported types return
  `EX_UNSUPPORTED_OPERATION`.
- `saveCalibrationData` returns the stock helper value: screen/pad/vice light
  and wise-RGB/screen-test writes return the byte count (`16`, `64`), spectrum
  SN returns the input length, wavelength calibration returns `10`, screen SN
  returns `0/1`, normal-CM copy returns `0/-1`, and unsupported types return
  `-1` with an OK binder status. Empty input returns `-1` with
  `EX_NULL_POINTER`.
- Screen-light saving is storage-mode dependent: new mode writes the 16-byte
  reserve slot; OOS storage mode writes the four
  `persist/engineermode/*_max_lux` files and returns the last node write size.
- `saveCalibrationData(0x1fa2673)` writes
  `/mnt/vendor/persist/camera/spectrum_test_data` with
  `test_3000k/4500k/5000k/6000k/6500k_data`.
- `saveCalibrationData(0xf423d)` writes `spectrum_sn.json` to
  `persist/camera` (`type=0`) or the spectrum directory (`type=1`).
- `saveCalibrationData(0xf423e)` loads `libams.so`/`lib<cct>.so`, calls
  `wavelength_cal_impl`, and copies `cm_tcs3449_<sn>.txt` to the camera
  directory.
- `setPhoneMode(1, 1)` writes the stock dmesg marker and triggers the QCOM ADSP
  dump through `/proc/sysrq-trigger`, as the stock binary does.

## Coverage

The calibration registry/reserve formats are device/panel specific. On a
device without the stock persist files or the OPLUS reserve partition, the
getters return empty JSON and the setters report failure. The service is
lazy/engineering-only and is not on the normal sensor path.
