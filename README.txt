qemu-rpi (c) 2012 Gregory Estrade, licensed under the GNU GPLv2 and later.

================================================================================
About
================================================================================

This project aims at providing a basic chipset emulation of the Raspberry Pi,
to help low-level programmers or operating system developers in their tasks.

If you just want to test your favorite Linux distribution, there are easier 
ways, such as http://xecdesign.com/qemu-emulating-raspberry-pi-the-easy-way/

Emulated chipset parts are at the time of this writing:
- System Timer.
- UART.
- Mailbox system.
- Framebuffer interface.
- DMA.
- eMMC SD host controller.

The emulation is quite incomplete for many parts, however it is advanced enough
to boot a Pi-targetted Linux kernel, along with a SD image of a compatible
Linux distribution.
I've successfully tested it with the latest official Raspbian wheezy image.

As it is at its current stage some very "alpha" software, which probably doesn't
meet QEMU project's requirements, I haven't submitted it as a QEMU patch yet.

================================================================================
Installation instructions
================================================================================

Preparing QEMU:
- Make sure you can compile and run QEMU according to the instructions provided
  by http://xecdesign.com/compiling-qemu/
- Copy the *.c and *.h files of this project into the qemu/hw/ subfolder.
- Edit the qemu/hw/arm/Makefile.objs file and add the following line:

obj-y += raspi.o bcm2835_ic.o bcm2835_st.o bcm2835_sbm.o bcm2835_power.o \
                bcm2835_fb.o bcm2835_property.o bcm2835_vchiq.o \
                bcm2835_emmc.o bcm2835_dma.o bcm2835_todo.o

  near the end of the file.
- Recompile and reinstall QEMU.

Preparing Linux:
- From a working SD image, extract the kernel image from the FAT32 partition.
  On Raspbian wheezy SD image, it is the "kernel.img" file.

Now run QEMU using the following command (warning, long line) :

/path/to/qemu-system-arm -kernel kernel.img -cpu arm1176 -m 512 -M raspi -serial stdio -append "rw dma.dmachans=0x7f35 bcm2708_fb.fbwidth=1024 bcm2708_fb.fbheight=768 bcm2708.boardrev=0xf bcm2708.serial=0xcad0eedf smsc95xx.macaddr=B8:27:EB:D0:EE:DF sdhci-bcm2708.emmc_clock_freq=100000000 vc_mem.mem_base=0x1c000000 vc_mem.mem_size=0x20000000 dwc_otg.lpm_enable=0 console=tty1 root=/dev/mmcblk0p2 rootfstype=ext4 elevator=deadline rootwait" -snapshot -sd 2012-10-28-wheezy-raspbian.img -d guest_errors

It should boot up normally, with a few additional error messages.
After booting it up, you can use the stdio console, but not the framebuffer's 
one, as there is no USB emulation yet (for keyboard and mouse support). 

Here are some explanations about the parameters provided to QEMU :
- "-m 512"
  defines the memory size, which corresponds in this case to a Model B.
- "-d guest_errors"
  logs the (considered) incorrect memory-mapped I/O access to /tmp/qemu.log  
- "-snapshot"
  commits write operations to temporary files instead of the SD image, which
  is probably wise, considering the current status of the emulation. :) 
  
Here are some explanations about the parameters provided to the Linux kernel :
- Most of them correspond to what is passed to the Linux kernel by the
  bootloader. You can retreive them with the "dmesg" command.
- "vc_mem.mem_base=0x1c000000 vc_mem.mem_size=0x20000000"
  defines the "memory split" between the ARM and the VideoCore.
  If you want to change it, you will have to change the VCRAM_SIZE value
  defined in bcm2835_common.h accordingly, otherwise you will encounter
  kernel memory corruption issues.
- "rw"
  forces the kernel to mount the root filesystem in read-write mode. 
  I have yet to find out why a "prepared" kernel mounts the root filesystem as
  read-write when booting on the hardware, whereas the same "unprepared" kernel 
  booting in QEMU mounts the root filesystem as read-only.
  I though it was due to different ARM boot tags settings, but it doesn't seem
  to be the reason why. If someone has an explanation, I'd be glad to hear it. :)

================================================================================
Gregory Estrade, 12/22/2012
