#
# Makefile for ip20mon, the standalone bring-up monitor for the IP20
# (Indigo R4000).  It needs a MIPS cross toolchain on a modern host, not
# the 68k tools the rest of the IRIS 3.6 tree builds with, so it is not
# part of the default stand build.  See README.
#
#	make				# ELF, using mips-linux-gnu- tools
#	make ecoff			# ECOFF for the PROM (uses python3)
#	make CROSS=mips-elf-		# any MIPS gcc that can do -EB
#	make ecoff MKECOFF=elf2ecoff	# NetBSD/Linux elf2ecoff instead
#

CROSS	= mips-linux-gnu-
CC	= $(CROSS)gcc
LD	= $(CROSS)ld
MKECOFF	= python3 mkecoff.py

CPUFLAGS = -EB -march=r4000 -mabi=32 -msoft-float -mno-abicalls -fno-pic -G 0
CFLAGS	= $(CPUFLAGS) -O2 -Wall -ffreestanding -fno-builtin \
	  -fno-stack-protector -fno-asynchronous-unwind-tables \
	  -fno-tree-loop-distribute-patterns
ASFLAGS	= $(CPUFLAGS)
LDFLAGS	= -EB -n -T ip20.ld

OBJS	= start.o exc.o tlb.o main.o arcs.o cons.o zs.o cpu.o mem.o subr.o \
	  trap.o vm.o

all: ip20mon.elf

ecoff: ip20mon.ecoff

ip20mon.elf: $(OBJS) ip20.ld
	$(LD) $(LDFLAGS) -Map ip20mon.map -o ip20mon.elf $(OBJS)

ip20mon.ecoff: ip20mon.elf mkecoff.py
	$(MKECOFF) ip20mon.elf ip20mon.ecoff

start.o: start.S
	$(CC) $(ASFLAGS) -c start.S

exc.o: exc.S ip20.h
	$(CC) $(ASFLAGS) -c exc.S

tlb.o: tlb.S ip20.h
	$(CC) $(ASFLAGS) -c tlb.S

.c.o:
	$(CC) $(CFLAGS) -c $<

main.o arcs.o cons.o cpu.o mem.o trap.o vm.o: ip20.h arcs.h
zs.o subr.o: ip20.h

clean:
	rm -f *.o ip20mon.map

clobber: clean
	rm -f ip20mon.elf ip20mon.ecoff
