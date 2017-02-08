# libuwifi - Userspace Wifi Library
#
# Copyright (C) 2005-2016 Bruno Randolf (br1@einfach.org)
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

NAME=wifisniffer

# build options
DEBUG=0

# see log levels in util/util.h
LOG_LEVEL=7 

PLATFORM=linux
#linux
#mips

LIBNL=3.0

OBJS=						   \
	io_log.o				   \
	utils_custom.o			   \
	utils_time.o			   \
	utils_net.o				   \
	network.o				   \
	conf_options.o			   \
	main.o		

INCLUDES=-I. -I/usr/local/include/uwifi
#-I./$(PLATFORM)
				  
LIBS=-lm -lrt

CFLAGS+=-std=gnu99 -Wall -Wextra

ifeq ($(DEBUG),1)
	# full debug  
	LOG_LEVEL=7 
else
	# print only critical, error, warning, info messages
	LOG_LEVEL=6
endif

CFLAGS += -DDEBUG=$(DEBUG) -DLOG_LEVEL=$(LOG_LEVEL)

#ifeq ($(DEBUG_SHOW),1)
#  CFLAGS+=-DDO_DEBUG
#endif

include $(PLATFORM)/platform.mk

.PHONY: all check clean force

all: $(NAME)

.objdeps.mk: $(OBJS:%.o=%.c)
	$(CC) -MM $(INCLUDES) $^ >$@

-include .objdeps.mk

$(NAME): $(OBJS)
	$(CC) $(LDFLAGS) -o $@  $(OBJS)  $(LIBS)

$(OBJS): .buildflags

check: $(OBJS:%.o=%.c)
	sparse $(CFLAGS) $^

clean:
	-rm -f $(NAME)
	-rm -f *.o  *~
	-rm -f .buildflags
	-rm -f .objdeps.mk

.buildflags: force
	echo '$(CFLAGS)' | cmp -s - $@ || echo '$(CFLAGS)' > $@

transfer:
	scp $(NAME) root@192.168.1.1:/data

full-transfer:
	scp $(NAME) $(NAME).conf $(NAME).sh root@192.168.1.1:/data
