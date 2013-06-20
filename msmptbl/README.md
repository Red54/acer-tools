Parser for partition.mbn
========================

Please note that it is not clear how exactly partition entries are stored in
partition.mbn so this code is just based on the information available in msm
linux kernel sources.

Usage:
```
$ ./msmptbl-parser ../unpack_acer_mergedosfile/partition.mbn 
Partition header:
  MAGIC1: aa7d1b9a
  MAGIC2: 1f7d48bc
  Version: 3
  Number of partitions: 13
Partition table:
  00: 0:MIBIB           offset=6        size=4        flags=feffffff
  01: 0:SIM_SECURE      offset=4        size=2        flags=feffffff
  02: 0:FSBL            offset=384      size=30       flags=ffffffff
  03: 0:OSBL            offset=384      size=30       flags=ffffffff
  04: 0:AMSS            offset=18000    size=1800     flags=ffffffff
  05: 0:APPSBL          offset=256      size=30       flags=ffffffff
  06: 0:DSP1            offset=16000    size=600      flags=ffffffff
  07: 0:FOTA            offset=128      size=100      flags=ffffffff
  08: 0:EFS2            offset=12000    size=200      flags=ffffff01
  09: 0:APPS            offset=30000    size=2200     flags=ffffffff
  10: 0:FTL             offset=16384    size=2048     flags=ffff0102
  11: 0:FTL2            offset=16384    size=2048     flags=ffff0102
  12: 0:EFS2APPS        offset=-1       size=65535    flags=ffffff01
```
