P7_SOURCES := $(wildcard *.c)
P7_OBJECTS := $(patsubst %.c,%.o, $(P7_SOURCES))

P7_VERSION_SCRIPT = p7_version_script
P7_VERSION = 0.2.0
P7_TARGET = libp7.so.$(P7_VERSION)

CFLAGS := -fPIC -c -O2 -g
LDFLAGS := -shared --version-script $(P7_VERSION_SCRIPT) -o $(P7_TARGET) -lpthread

all: $(P7_OBJECTS)
	ld $(LDFLAGS) $^ -o $(P7_TARGET)

.c.o:
	gcc $(CFLAGS) $<

.PHONY: clean

clean:
	rm $(P7_OBJECTS) $(P7_TARGET)
