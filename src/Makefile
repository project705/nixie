CC=gcc
CFLAGS=-c -Wall -Werror
LDFLAGS=-lpthread -lwiringPi
SOURCES=main.c ec1117.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=nixie

all:
	$(MAKE) $(EXECUTABLE) CFLAGS="$(CFLAGS) -O3"

debug:
	$(MAKE) $(EXECUTABLE) CFLAGS="$(CFLAGS) -O0 -ggdb -DDEBUG"

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

-include $(OBJECTS:.o=.d)

%.o:   %.c
	$(CC) -c $(CFLAGS) -MMD -o $@ $<

clean:
	rm -f $(EXECUTABLE) *.o *.d
