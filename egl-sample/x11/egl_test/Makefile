TARGET = render-tex

all: $(TARGET)

$(TARGET): *.c
	gcc -o $@ $^ -L/usr/lib/x86_64-linux-gnu/  -lX11 -lGL -lEGL -lva -lva-x11 -lva-egl -lm

clean:
	rm -f $(TARGET)
