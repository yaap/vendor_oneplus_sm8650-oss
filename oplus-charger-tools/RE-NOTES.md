# Reverse-engineering notes

Reference binaries:

```text
vendor/oneplus/sm8650-common/proprietary/odm/bin/hw/gauged
vendor/oneplus/sm8650-common/proprietary/odm/bin/hw/wlschgd
```

Both are stripped AArch64 PIE executables with a small `.text` section and a
few imported libc/libcutils/liblog functions. Ghidra recovered the complete
`main`, monitor thread and authentication helpers.

## gauged

`main` creates `Charger::Gauged::MonitorThread` and joins it.

`MonitorThread`:

- sets a process-wide "started" flag;
- opens `/dev/sec_dev` with `O_RDWR`;
- retries 50 times with a one-second sleep;
- reads 0x88 bytes per command;
- exits on command 0;
- handles command 1 only;
- rejects any other command.

The command record layout is:

```text
+0x00  int32 cmd
+0x04  int32 data_size
+0x08  int32 index
+0x0c  ... 0x88 bytes total
```

For command 1 the stock code requires `data_size == 4`. It then calls
`GaugeGetVerifyDataByIndex(index, buffer, 0x20)`, which only accepts index 0 and
copies a 32-byte record from `.data`. The record is:

```text
d50e1f5bba5ec9aa7d2f4ac98419539c
8d068026923f84b2a72888a563bf8673
```

The result is passed to `ioctl(fd, 0x40017301, buffer)`. The ioctl constant is
also encoded as `0x40017301` in the stock binary even though its `_IOC` size
field is 1; the driver presumably uses the command value directly.

On a read error the stock code closes the device and keeps reading the closed
descriptor. The source replacement logs the error, closes the descriptor and
reopens it with the same retry policy so the daemon remains usable.

## wlschgd

`main` logs `wlschgd enterd`, creates `monitor_thread` and joins it.

`monitor_thread`:

- opens `/dev/wls_dev` with `O_RDWR`;
- retries 15 times with a one-second sleep;
- reads 0x88 bytes per command;
- supports command 0 only;
- calls `wls_chg_authenticate` for command 0.

The command record is at least an `int32 cmd`; the rest is unused.

`wls_chg_authenticate`:

- opens `/dev/random` with `O_RDONLY` and retries 15 times;
- reads eight random bytes one byte at a time;
- closes `/dev/random`;
- runs `wls_authenticate(random, encoded)`;
- sends the 16-byte `{random, encoded}` record with
  `ioctl(fd, 0x4010f101, record)`.

`wls_authenticate` builds two fixed DES key schedules and applies:

```text
middle  = DES_K1(random)
encoded = DES_K2(middle)
```

The stock implementation uses a 64-bit DES key represented as 64 one-byte
0/1 values, plus the standard PC1, shift and PC2 tables. The two key bit arrays
were recovered from the stack initialization in `wls_authenticate`; the first
array packs to `2143882affe177cc`, the second to `24a2760810b34800`.

The key schedule rotation is the stock in-place rotation, which is not the
usual per-half DES rotation; it is reproduced literally, including the
`+0x1c`/`+0x39` sliding-buffer behavior. The DES round function itself uses the
standard IP, expansion, S-box, P and final-permutation tables (all extracted
from the binary).

On a read error the stock code also closes the device and keeps reading. The
source replacement reopens the device with the same retry policy.

## Logging

`wlschgd` writes kernel-log records through `klog_write()` from libcutils:
level 3 for errors and level 6 for the random/encoded dumps. `gauged` uses
`__android_log_print()` with the `gauged` tag.

## SELinux

- `/dev/wls_dev` and `wlschgd` already had a domain and device type. The source
  build adds read access to `random_device` and write access to `kmsg_device`.
- `gauged` and `/dev/sec_dev` are added, with `write_logd()` for the Android
  log.
