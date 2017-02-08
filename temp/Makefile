#scp tmp root@192.168.1.1:/data

#gcc -Wall enables all compiler's warning messages. This option should always be used, in order to generate better code.
#gcc -c compiles source files without linking.
#$ gcc -fPIC [options] [source files] [object files] -o output file
#Use -fpic instead of -fPIC to generate more efficient code, if supported by the platform compiler.

#-L/path/to/lib/folder -llibshortname
CC=mips-openwrt-linux-gcc
PLATFORM=
#linux
#mips
DEBUG_SHOW=1
NAME=tmp
OBJS=						   \
	io_log.o				   \
	utils_custom.o			   \
	utils_time.o			   \
	network.o				   \
	conf_options.o			   \
	main.o	
				  
LIBS=-lm -lrt -L/usr/local/lib/uwifi_mips/ -luwifi -L/home/colt/tplinkfirmware_custom/openwrt/build_dir/target-mips_34kc_uClibc-0.9.33.2/libnl-3.2.21/ipkg-install/usr/lib/ -lnl-3 -lnl-genl-3

INCLUDES=-I/usr/local/include/uwifi
CFLAGS+=-std=gnu99 -Wall -Wextra $(INCLUDES) -DLOG_LEVEL=7


ifeq ($(DEBUG_SHOW),1)
  CFLAGS+=-DDO_DEBUG
endif

.PHONY: all check clean force

all: $(NAME)

.objdeps.mk: $(OBJS:%.o=%.c)
	$(CC) -MM $(INCLUDES) $^ >$@

-include .objdeps.mk

$(NAME): $(OBJS)
	$(CC) $(LDFLAGS) $(LIBS)  -o $@   $(OBJS) 

$(OBJS): .buildflags

check: $(OBJS:%.o=%.c)
	sparse $(CFLAGS) $^

clean:
	-rm -f *.o  *~
	-rm -f .buildflags
	-rm -f .objdeps.mk

.buildflags: force
	echo '$(CFLAGS)' | cmp -s - $@ || echo '$(CFLAGS)' > $@
