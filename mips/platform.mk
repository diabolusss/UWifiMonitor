#scp tmp root@192.168.1.1:/data

#gcc -Wall enables all compiler's warning messages. This option should always be used, in order to generate better code.
#gcc -c compiles source files without linking.
#$ gcc -fPIC [options] [source files] [object files] -o output file
#Use -fpic instead of -fPIC to generate more efficient code, if supported by the platform compiler.

#-L/path/to/lib/folder -llibshortname
CC=mips-openwrt-linux-gcc
				  
LIBS+=-L/usr/local/lib/uwifi_mips/ -luwifi -L/home/colt/tplinkfirmware_custom/openwrt/build_dir/target-mips_34kc_uClibc-0.9.33.2/libnl-3.2.21/ipkg-install/usr/lib/ -lnl-3 -lnl-genl-3

INCLUDES+=-I/home/colt/tplinkfirmware_custom/openwrt/build_dir/target-mips_34kc_uClibc-0.9.33.2/libnl-3.2.21/include/

CFLAGS+=$(INCLUDES)

