# Makefile
# 

PREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64
OUT ?= ../../build/kernel
.DEFAULT_GOAL := all

CC=$(PREFIX)gcc
AS=$(PREFIX)as
LD=$(PREFIX)ld
OBJCOPY=$(PREFIX)objcopy
OBJDUMP=$(PREFIX)objdump

OBJS = \
	assert.o \
	console.o \
	cache.o \
	thread.o \
	device.o \
	elf.o \
	error.o \
	excp.o \
	heap0.o \
	intr.o \
	io.o \
	plic.o \
	see.o \
	start.o \
	string.o \
	timer.o \
	trap.o \
	ktfs.o \
	thrasm.o \
	dev/viorng.o \
	dev/virtio.o \
	dev/vioblk.o \
	dev/rtc.o \
	dev/uart.o \
	memory.o\
	process.o \
	syscall.o

CFLAGS = -Wall -Werror=implicit-function-declaration
CFLAGS += -fno-omit-frame-pointer -ggdb3 -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64imazicsr -mabi=lp64
CFLAGS += -fno-common -nostdlib -mno-relax -ffreestanding 
CFLAGS += -fno-asynchronous-unwind-tables -mno-riscv-attribute
CFLAGS += -I.
CFLAGS += -MMD -MP

#CFLAGS += -DDEBUG -DTRACE # Everything!
#CFLAGS += -DMEMORY_DEBUG -DMEMORY_TRACE
#CFLAGS += -DHEAP_DEBUG -DHEAP_TRACE
#CFLAGS += -DEZFS_DEBUG -DEZFS_TRACE
#CFLAGS += -DLOCK_DEBUG -DLOCK_TRACE
#CFLAGS += -DMAIN_DEBUG -DMAIN_TRACE
#CFLAGS += -DTIMER_DEBUG -DTIMER_TRACE
#CFLAGS += -DCACHE_DEBUG -DCACHE_TRACE
#CFLAGS += -DKTFS_DEBUG -DKTFS_TRACE

ASFLAGS = -march=rv64imazicsr

LDFLAGS = -melf64lriscv

OBJS := $(addprefix $(OUT)/,$(OBJS))

.PHONY: all clean run debug
all: $(OUT)/kernel.elf

$(OUT)/kernel.elf: $(OBJS) $(OUT)/main.o $(OUT)/blob.o kernel.ld
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $(filter %.o,$^)

$(OUT)/storage-test.elf: $(OBJS) $(OUT)/storage_test.o $(OUT)/blob.o kernel.ld
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $(filter %.o,$^)

$(OUT)/bench.elf: $(OBJS) $(OUT)/storage_bench.o $(OUT)/blob.o kernel.ld
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $(filter %.o,$^)

$(OUT)/storage_test.o: test/storage_test.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT)/storage_bench.o: test/storage_test.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -DSTORAGE_BENCH -c -o $@ $<

$(OUT)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(OUT)/%.o: %.s
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) -o $@ $<

-include $(OBJS:.o=.d) $(OUT)/main.d $(OUT)/storage_test.d $(OUT)/storage_bench.d

run:
	$(MAKE) -C ../.. run

debug:
	$(MAKE) -C ../.. debug

	
BLOB_OBJCOPY_FLAGS = \
	--add-section .rodata.blob=blob.raw \
	--set-section-flags .rodata.blob=alloc,contents,load,readonly

$(OUT)/blob.o:
	@mkdir -p $(@D)
	echo .end | $(AS) $(ASFLAGS) -o $@
	[ ! -f blob.raw ] || $(OBJCOPY) $(BLOB_OBJCOPY_FLAGS) $@

clean:
	rm -f $(OBJS) $(OBJS:.o=.d) $(OUT)/*.o $(OUT)/*.d $(OUT)/*.elf
