TARGET=egl-test
CFLAGS=-std=gnu99 -lwayland-client -lwayland-egl -lEGL -lGL

CC=gcc

all:
	$(CC) -o $(TARGET) *.c  $(CFLAGS)

clean:
	rm -f $(TARGET)
