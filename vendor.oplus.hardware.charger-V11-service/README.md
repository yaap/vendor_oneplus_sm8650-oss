# OPLUS charger V11 HAL

Open-source replacement for:

```text
/odm/bin/hw/vendor.oplus.hardware.charger-V11-service
```

The service implements the complete
`vendor.oplus.hardware.charger.ICharger/default` AIDL interface from
`hardware/oplus/interfaces/aidl/vendor/oplus/hardware/charger`.

## Layout

```text
Charger.cpp        AIDL implementation and all 130 overrides
Charger.h          BnCharger subclass
ChargerInfo.cpp    ChargerInfo JSON reader used by ChgFuncFlag
NodeControl.cpp    sysfs/proc read/write helpers
TestKit.cpp        /dev/chg_test_kit ioctl wrapper
main.cpp           binder service registration
```

## Build

The module is named `vendor.oplus.hardware.charger-V11-service`, matching the
stock prebuilt module name and the installed binary name, so the existing ODM
init file and VINTF manifest keep referencing the stock path.

It is selected from `device/oneplus/sm8650-common/common.mk`. The prebuilt
package entry is no longer selected in `sm8650-common-vendor.mk`; the prebuilt
file remains in the extraction list only as an unused fallback.

## Behavior

- Getters use `/sys/class/power_supply/*` first and fall back to
  `/sys/class/oplus_chg/*` where the stock service does.
- Setters use the matching `oplus_chg`/`power_supply` nodes.
- `/dev/oplus_chg`, `/proc/charger/*`, `/proc/wireless/*`, `/proc/ui_soc_decimal`
  and the `oplus_wireless/pencil` nodes are used directly.
- `queryChargeInfo()` emits the same `key=value` report (one field per line) as
  the stock binary.
- `queryWlsPencilInfo()` emits the stock wireless-pencil report.
- `GET_CHARGER_INFO` / `GET_CHARGER_INFO_UPDATE` return a JSON object built from
  the `ChargerInfo` type groups (`Update`, `Common`, `Battery`, `Wired`,
  `Wireless`).
- The `ChgFuncFlag` getter/setter map covers flags 0-41.  The flag-7 getter is
  absent in the stock map and therefore returns an empty value, matching the
  original behavior.
- `chgExchangeMesgInit()` starts the kernel mutual-command thread.  It handles
  WLS third-party AES authentication plus the gauge-calibration, decay-CV,
  precharge-resistance and discharge-boost-error commands over
  `/sys/class/oplus_chg/common/mutual_cmd`.
- `chgExchangeSohMesgInit()` starts the UI-SOH polling thread.
- RUS configuration and WLS third-party records are persisted to
  `oplusreserve1` at the stock offsets.
- BMS heating status is derived from the stock BMS config and the
  `bms_heat_temp_compensation` node; the running-status report exposes the
  configured CPU mask.
- `TestKit` reproduces the five `/dev/chg_test_kit` ioctls
  (`0x4004f601`, `0x9000f603`, `0x9000f604`, `0x8004f605`, `0x9000f606`).
- USB eye-diagram handling reproduces the QCOM
  `/proc/usb/repeater_parameters` and MTK
  `/proc/oplus_usb_eusb2` / `/proc/oplus_usb_xsphy` paths.
- Battery SOH debug parsing accepts the stock
  `ui_soh debug data is good` blob and extracts the twelve debug values.
  The `oplusreserve1` write uses the stock offsets (UFS `0x48a11a`, eMMC
  `0x467d1a`).

## SELinux

`hardware/oplus/sepolicy/qti/vendor` gains the charger test-kit device type,
the `/dev/chg_test_kit` context, and proc/sysfs labels for the root-level
charger proc nodes, the PMIC FTM device-tree nodes and the wireless pencil
class.
