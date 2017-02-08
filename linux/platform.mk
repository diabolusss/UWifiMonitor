#gcc -Wall enables all compiler's warning messages. This option should always be used, in order to generate better code.
#gcc -c compiles source files without linking.
#$ gcc -fPIC [options] [source files] [object files] -o output file
#Use -fpic instead of -fPIC to generate more efficient code, if supported by the platform compiler.

#-L/path/to/lib/folder -llibshortname
CC=gcc

CFLAGS += $(shell pkg-config --cflags libnl-$(LIBNL))
    	  
LIBS+=-L/usr/local/lib -luwifi -lnl-3 -lnl-genl-3

CFLAGS+=$(INCLUDES)

