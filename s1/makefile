S1_SOURCES := $(wildcard *.c)
S1_OBJECTS := $(patsubst %.c,%.o, $(S1_SOURCES))

S1_VERSION_SCRIPT = s1_version_script
S1_VERSION = 0.1.0
S1_TARGET = libs1.so.$(S1_VERSION)

CFLAGS := -fPIC -c -O2 -g
LDFLAGS := -shared --version-script $(S1_VERSION_SCRIPT) -o $(S1_TARGET) -ldl

all: $(S1_OBJECTS)
	ld $(LDFLAGS) $^ -o $(S1_TARGET)

.c.o:
	gcc $(CFLAGS) $<

.PHONY: clean

clean:
	rm $(S1_OBJECTS) $(S1_TARGET)
