CC=gcc
CFLAGS=-Wall `pkg-config fuse --cflags --libs`
DEPS = tosfs.h
OBJ = futofs.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: $(OBJ)
	$(CC) -o futofs $^ $(CFLAGS)
clean:
	rm -f $(BIN)
	rm -f *.o
	rm -f *~

indent:
	indent -linux -i4 -nut -ts2 *.c

.PHONY: all clean indent
