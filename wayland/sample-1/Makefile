SCAN = wayland-scanner
TARGET= simple-egl
PROTOCOL_DIR = ./protocol
PROTOCOL_SRC = $(wildcard $(PROTOCOL_DIR)/*.xml)
PROTOCOL_CODE = $(patsubst $(PROTOCOL_DIR)/%.xml, $(PROTOCOL_DIR)/%-protocol.c, $(PROTOCOL_SRC))
PROTOCOL_HEADER = $(patsubst $(PROTOCOL_DIR)/%.xml, $(PROTOCOL_DIR)/%-client-protocol.h, $(PROTOCOL_SRC))

AM_GEN = @echo "  GEN     "

CFLAGS = -lwayland-client -lwayland-egl -lwayland-cursor -lEGL -lGL -lm
CC = gcc

all : $(PROTOCOL_CODE) $(PROTOCOL_HEADER) $(TARGET)

$(PROTOCOL_DIR)/%-protocol.c : $(PROTOCOL_DIR)/%.xml
	$(AM_GEN)$@ && $(SCAN) code $< $@

$(PROTOCOL_DIR)/%-client-protocol.h : $(PROTOCOL_DIR)/%.xml
	$(AM_GEN)$@ && $(SCAN) client-header $< $@

$(TARGET) : $(PROTOCOL_CODE) *.c
	$(AM_GEN)$@ && $(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f $(PROTOCOL_DIR)/*.c
	rm -f $(PROTOCOL_DIR)/*.h
	rm -f $(TARGET)
