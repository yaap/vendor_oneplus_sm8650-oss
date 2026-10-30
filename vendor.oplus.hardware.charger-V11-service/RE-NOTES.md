# Reverse-engineering notes

Reference binary:

```text
vendor/oneplus/sm8650-common/proprietary/odm/bin/hw/vendor.oplus.hardware.charger-V11-service
```

The binary is a stripped AArch64 PIE service built against
`vendor.oplus.hardware.charger-V11-ndk`.  Ghidra recovered the complete
`Charger` vtable, the `ChgFuncFlag` callback map and the supporting sysfs/proc
helpers.

## AIDL surface

The interface in this tree contains 130 methods and the stock `Charger`
vtable has exactly 130 entries in the same order.  The only naming quirk is
slot 53: the stock vtable reuses the `getPsyQGVbatDeviation` implementation
for the `getQgVbatDeviation` AIDL method (both slots point at `0x29784`).
This source service implements both methods against
`/proc/charger/qg_vbat_deviation`.

The stock binary additionally contains five non-virtual helpers:
`getIsSingleHyperPathSupport`, `getSingleHyperSupportValue`,
`chgDualCellsProtInit`, `getTimeZone` and `chgFuncMapInit`.  They are not
reachable through binder; the source service reproduces the parts that
influence AIDL behavior.

## Common node pattern

Most getters follow:

```text
open("/sys/class/power_supply/<class>/<attr>", O_RDONLY)
if that fails:
    open("/sys/class/oplus_chg/<class>/<attr>", O_RDONLY)
read up to 16 bytes, atoi(), close
```

Failures return `0x80000000` (`INT_MIN`).  A read error returns `0x80000002`.
Setters normally return 0 on success, `0x80000000` when the node cannot be
opened and `0x80000003` when the write fails.

`ChargerInfo`-backed getters (`usbStatus`, `batteryRm`, `batteryFcc`,
`fastCharge`, `notifyCode`, `otgOnline`, `ppsCharging`, `ppsChargePower`,
`wlsAdapterPower`, `bmsStatus`) read the same physical nodes through a cached
`IntfInfo` table.  The source implementation reads the nodes directly and
exposes the same JSON names.

## ChgFuncFlag map

`chgFuncMapInit()` builds two `std::map<ChgFuncFlag, std::function<...>>`
instances:

Setters:

```text
5   SET_CHG_UP_LIMIT_VALUE
11  SET_BATT_FIRST_USAGE_DATE
12  CLEAR_BATT_ECO_DATA
13  SET_RECHG_SOC_EN
14  SET_CHG_UP_LIMIT_STATE
19  SET_ANTI_EXPANSION_INFO
22  WIRELESS_PENCIL_QCALI
25  SET_CHG_BYPASS
27  SET_RX_DISABLE
32  SET_CHG_PATH
34  SET_BT_INFO
36  SET_REVERSE_CHG_INFO
38  SET_WLSPEN_INFO
39  SET_WLSPEN_DISCHG_SOC
41  SET_PENCIL_TX_CHARGE_DISABLE
```

Getters: every flag from 0 through 40 except flag 7.  Flag 7
(`GET_BATT_EXPANSION_STATE`) is not present in the stock map, so
`getChgConfig(flag=7)` returns an empty string.

The `extra` argument is `'+'`-separated.  The first token is a string payload
and the following tokens are integers (up to four).  Setter flags 25/27/32/38/
39/41 append `|callname=<n>` or `callname=<n>` before writing, exactly as the
stock code does.  The source implementation preserves those suffixes.

`setChgUpLimitValue` writes the composite five-integer form to
`/sys/class/oplus_chg/common/chg_up_limit`:

```text
"%d,%d,%d,%d,%d\n"
```

`setChgUpLimitState` additionally writes the same record to the reserve
partition when `callname == 10`.

## queryChargeInfo

The output is a sequence of `name=value` lines.  The source implementation
reproduces the full stock key set, including the special mappings:

- `/proc/wireless/enable_tx`: `charging`→1, `enable`→2, `disable`→3, else 4.
- battery status: `Charging`→2, `Discharging`→3, `Not charging`→4,
  `Full`→5, else 1.
- USB type mapping for `wireless_charger_type`: `5`/`6`→5,
  `USB_PD`/`USB_PD_DRP`/`USB_HVDCP`/`USB_HVDCP_3`/`9`/`10`→6, else -1.
- The four power supplies (`ac`, `battery`, `usb`, `wireless`) are enumerated
  and emitted as `chargerAcOnline`, `chargerUSBOnline` or
  `chargerWirelessOnline`, followed by `charger_type`.

The stock code also appends charge-pump, BCC/BSL/EIS debug blobs.  The
source service emits the stable fields and empty `bsl_data`/`eis_data`
payloads when the corresponding kernel/config support is absent; the BCC CSV
path is populated from the live sysfs telemetry when `bcc_parms` exists.

## queryWlsPencilInfo

Keys and fallbacks:

```text
ble_pencil_addr  /sys/class/oplus_wireless/pencil/ble_mac_addr
                 /proc/wireless/mac_addr
ping_time        /sys/class/oplus_wireless/pencil/ping_time
                 /sys/class/oplus_chg/wireless/ping_time
tx_status        /sys/class/oplus_wireless/pencil/tx_status
                 /proc/wireless/enable_tx
tx_current       /sys/class/oplus_wireless/pencil/tx_current
                 /sys/class/power_supply/wireless/current_now
tx_voltage       /sys/class/oplus_wireless/pencil/tx_voltage
                 /sys/class/power_supply/wireless/voltage_now
fm_version       /sys/class/oplus_wireless/pencil/fw_version
wireless_ic_id   /sys/class/oplus_wireless/pencil/wireless_ic_id
```

## ChargerInfo JSON

`ChargerInfo::getInfo(type, update)` accepts a `'+'`-separated list of
`Update`, `Common`, `Battery`, `Wired` and `Wireless`.  `Update` selects the
whole table.  The result is a `Json::FastWriter` object keyed by the IntfInfo
names.  The source implementation uses `libjsoncpp` and the same names.

## Test kit

`TestKit::KernelTest` opens `/dev/chg_test_kit` and issues:

```text
0x4004f601  set index
0x9000f603  get name (4096-byte buffer)
0x9000f604  get feature list (4096-byte buffer)
0x8004f605  get feature number (uint*)
0x9000f606  run test (4096-byte buffer)
```

`runTest` maps the ioctl return value 0/1 to the returned string and 2 to an
empty string, exactly like the stock wrapper.

## Eye diagrams

`getCurrentPlatform()` reads `ro.soc.model`:

```text
QTI      -> qcom
Mediatek -> mtk
other    -> unknown
```

QCOM writes `/proc/usb/repeater_parameters`.  MTK writes
`/proc/oplus_usb_eusb2/eusb2` (PHY 1) and `/proc/oplus_usb_xsphy/xsphy`
(PHY 2).  The MTK setter stores the full input string so the getter can return
the current parameters when the hardware value differs, matching the stock
`DAT_0018d3f0` behavior.

## Battery SOH

The stock service keeps the UI SOH in a checksummed record in
`oplusreserve1` at offset `0x48a11a` (UFS) or `0x467d1a` (eMMC).  The source
implementation reads the live `battery_ui_soh`/`battery_soh` sysfs value,
parses the stock `ui_soh debug data is good` blob, and writes a bounded record
at the same offset for `updateUiSohToPartion()`.  The full BCC/BSL/EIS
partition format is not required for the AIDL surface.

## Exchange thread

`chgExchangeMesgInit()` starts the stock mutual-command thread.  The thread
reads 0x108-byte `{cmd, data_size, data[0x100]}` records from
`/sys/class/oplus_chg/common/mutual_cmd` and writes the response back to the
same descriptor.  Supported commands:

```text
0  WLS third-party AES authentication
1  gauge calibration data obtain
2  gauge calibration data update
3  decay-CV data obtain
4  decay-CV data update
5  precharge battery resistance obtain
6  discharge boost error obtain
```

The WLS command uses the stock fallback AES-128 key
`000102030405060708090a0b0c0d0e0f` and the same 36-byte response layout
(`key_index + random[16] + encoded[16]`).  Partition offsets and record sizes
for commands 1-6 match the stock service.

## SELinux

The charger domain already had access to the battery/wireless/usb power-supply
classes, `/proc/charger`, `/proc/wireless`, `/proc/oplusVersion`, the OPLUS
charger device and the reserve partition.  This change adds:

- `chg_test_kit_device` and `/dev/chg_test_kit` (rw).
- genfs labels for the root-level charger proc files, the PMIC FTM
  device-tree nodes and the wireless pencil class.
