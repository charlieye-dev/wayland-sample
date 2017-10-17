TARGET=texture-test
CFLAGS=-lwayland-client -lwayland-egl -lEGL -lGL -lSOIL -lm

CC=gcc

all:
	$(CC) -o $(TARGET) *.c  $(CFLAGS)

clean:
	rm -f $(TARGET)
