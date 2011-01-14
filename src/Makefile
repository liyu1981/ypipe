
CC=gcc
CFLAGS=-c -Wall -g
LDFLAGS=

SOURCES=ypiped.c
OBJECTS=$(SOURCES:.c=.o)

EXECUTABLE=ypiped

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o $(EXECUTABLE)
