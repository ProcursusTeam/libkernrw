TARGET  = libkernrw.0.dylib
OUTDIR ?= bin

CC       = xcrun -sdk iphoneos clang -arch arm64 -fobjc-arc
CODESIGN = ldid -S

CFLAGS  = -Wall -I./apple_include

.PHONY: all clean

all: $(OUTDIR)/$(TARGET) $(OUTDIR)/libkernrw.dylib $(OUTDIR)/krwtest

DEBUG ?= 0
ifeq ($(DEBUG), 1)
    CFLAGS += -DDEBUG
else
    CFLAGS += -O2 -fvisibility=hidden
endif

$(OUTDIR):
	mkdir -p $(OUTDIR)


$(OUTDIR)/$(TARGET): libkernrw.c kernrw_daemonUser.c $(LIBHOOKER) | $(OUTDIR)
	$(CC) -dynamiclib -o $@ $^ $(CFLAGS)
	install_name_tool -id /usr/lib/libkernrw.0.dylib $(OUTDIR)/$(TARGET)
	strip -x $@
	$(CODESIGN) $@

$(OUTDIR)/krwtest: kernrwtest.c $(OUTDIR)/$(TARGET)
	$(CC) -o $@ $^ $(CFLAGS) $(OUTDIR)/libkernrw.0.dylib
	strip -x $@
	$(CODESIGN) $@

$(OUTDIR)/libkernrw.dylib: $(OUTDIR)/$(TARGET)
	ln -s libkernrw.0.dylib $(OUTDIR)/libkernrw.dylib

clean:
	rm -f $(OUTDIR)/$(TARGET) $(OUTDIR)/krwtest $(OUTDIR)/libkernrw.dylib
