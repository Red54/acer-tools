Acer MergedOSFile unpacker
========================

```
Usage: ./unpack_acer_mergedosfile [-s] acer_MergedOSFile.bin
    -s  Skip 20-byte header for some files (enabled for A4)
    -d dir  Use this directory to extract (default ".")
```

**Important**: If you want to flash the .img file with fastboot, you will
need to skip first 20 bytes of the image. Use **-s** option.

```
$ ./unpack_acer_mergedosfile -s /home/rye/Projects/VFIT/acer_MergedOSFile.bin
AMSS Version: 05.01.06
Extracting partition.mbn to ./partition.mbn (464 bytes)
Extracting dbl.mbn to ./dbl.mbn (33972 bytes)
Extracting fsbl.mbn to ./fsbl.mbn (176164 bytes)
Extracting osbl.mbn to ./osbl.mbn (354464 bytes)
Extracting amss.mbn to ./amss.mbn (15327232 bytes)
Extracting appsboot.mbn to ./appsboot.mbn (128208 bytes)
Extracting dsp1.mbn to ./dsp1.mbn (9373756 bytes)
Extracting NPRG8650.hex to ./NPRG8650.hex (385944 bytes)
Extracting boot.img to ./boot.img (2536512 bytes)
Extracting system.img to ./system.img (157318656 bytes)
Extracting userdata.img to ./userdata.img (2112 bytes)
Extracting recovery.img to ./recovery.img (2766720 bytes)
Extracting initlogo.rle to ./initlogo.rle (8448 bytes)
Extracting touch.bin to ./touch.bin (4224 bytes)
MergedFile is missing adsp.mbn, skipping.
MergedFile is missing flex.img, skipping.
MergedFile is missing logodinfo.img, skipping.
```
