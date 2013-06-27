# Simple makefile:
# To build for linux, just run make
# For Win32 version via mingw32msvc - make unpack_acer_mergedosfile.exe
CFLAGS=-Wall -Werror -Wunused
SOURCES=unpack_acer_mergedosfile.c md5.c
PROGRAM_NAME=unpack_acer_mergedosfile

all: unpack_acer_mergedosfile

windres.o: unpack_acer_mergedosfile.rc
	i586-mingw32msvc-windres -o $@ $<

$(PROGRAM_NAME).exe: $(SOURCES) windres.o
	i586-mingw32msvc-gcc $(CFLAGS) $^ -o $@
	i586-mingw32msvc-strip $@

$(PROGRAM_NAME): $(SOURCES)
	$(CC) $(CFLAGS) $^ -o $@
	strip $@

clean:
	rm *.o
