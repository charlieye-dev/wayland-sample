TARGET=egl-test
CFLAGS=-fPIC -g -std=c++11 -lwayland-client -lwayland-egl -lEGL -lGL -L/usr/ye/lib -lcrvideotunnel

CXX=g++

all:
	$(CXX) -o $(TARGET) *.cc  $(CFLAGS)

clean:
	rm -f $(TARGET)
