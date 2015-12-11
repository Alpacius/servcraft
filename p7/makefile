P7_SOURCES := $(wildcard *.c)
P7_OBJECTS := $(patsubst %.c,%.o, $(P7_SOURCES))
UTIL_SOURCES := ../util/scraft_hashtable.c ../util/scraft_rbt.c
UTIL_OBJECTS := $(patsubst %.c,%.o, $(UTIL_SOURCES))

P7_VERSION_SCRIPT = p7_version_script
P7_VERSION = 0.3.0
P7_TARGET = libp7.so.$(P7_VERSION)

CFLAGS := -fPIC -c -O2 -g
LDFLAGS := -shared --version-script $(P7_VERSION_SCRIPT) -o $(P7_TARGET) -lpthread

all: $(P7_OBJECTS) $(UTIL_OBJECTS)
	ld $(LDFLAGS) $^ -o $(P7_TARGET)

.c.o:
	gcc $(CFLAGS) $< -o $@

.PHONY: clean

clean:
	rm $(P7_OBJECTS) $(P7_TARGET) $(UTIL_OBJECTS)
