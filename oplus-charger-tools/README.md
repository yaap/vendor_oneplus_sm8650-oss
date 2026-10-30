# OPLUS charger helper daemons

Open-source replacements for the two small OPLUS charger helpers:

```text
/odm/bin/hw/gauged
/odm/bin/hw/wlschgd
```

Both are started by the stock
`vendor.oplus.hardware.charger-V11-service.rc` (and the V9/V10 rc files) as
one-shot `main`-class services. They are not AIDL services and expose no binder
interface.

## gauged

`gauged` opens `/dev/sec_dev` and waits for a 0x88-byte command record:

```text
offset 0   int32 cmd      0 = exit, 1 = gauge authentication
offset 4   int32 size     must be 4 for cmd 1
offset 8   int32 index    must be 0
```

For command 1 it copies the stock 32-byte gauge verification record and sends
it to the charger driver with:

```text
ioctl(fd, 0x40017301, verify_data)
```

The stock binary has exactly one verification record (index 0) embedded in
`.data`; it is reproduced byte-for-byte.

## wlschgd

`wlschgd` opens `/dev/wls_dev` and waits for a 0x88-byte command record. Only
command 0 is supported:

1. read eight bytes from `/dev/random`;
2. run the OPLUS two-stage DES challenge;
3. send the 16-byte `{random[8], encoded[8]}` record with:

```text
ioctl(fd, 0x4010f101, auth)
```

The challenge uses the stock fixed-key DES implementation. The two key
schedules are built from the exact 64-byte bit arrays found in the binary, using
the stock PC1/shift/PC2 tables. The encoded value is:

```text
encoded = DES_K2(DES_K1(random))
```

## Build

```text
gauged
wlschgd
```

Both install to `/odm/bin/hw` with stems `gauged` and `wlschgd`, matching the
existing rc files and file contexts. They are selected from
`device/oneplus/sm8650-common/common.mk`.

The prebuilt binaries remain in the vendor extraction list as an unused
fallback, but the prebuilt packages are no longer selected in
`sm8650-common-vendor.mk`.
