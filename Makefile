CC       ?= cc
PREFIX   ?= /usr/local
DATADIR  ?= $(PREFIX)/share/hanote

CFLAGS   += -std=c99 -Wall -Wextra
CFLAGS   += $(shell pkg-config --cflags gtk4 json-glib-1.0)
CFLAGS   += -DSOURCE_DATA_DIR=\"$(CURDIR)/data\"
CFLAGS   += -DINSTALL_DATA_DIR=\"$(DATADIR)\"

LDFLAGS  += $(shell pkg-config --libs gtk4 json-glib-1.0)

SRCS     = src/main.c src/note.c src/emoji.c src/format.c src/image.c \
           src/store.c src/config.c src/scaled_paintable.c \
           src/monitor.c src/compositor.c
OBJS     = $(SRCS:.c=.o)
TARGET   = hanote

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) -s

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	install -Dm644 data/emojis.dic $(DESTDIR)$(DATADIR)/emojis.dic
	install -Dm644 data/com.suhokang.hanote.desktop $(DESTDIR)$(PREFIX)/share/applications/com.suhokang.hanote.desktop
	install -Dm644 data/hanote.svg $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/com.suhokang.hanote.svg

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	rm -f $(DESTDIR)$(DATADIR)/emojis.dic
	rm -f $(DESTDIR)$(PREFIX)/share/applications/com.suhokang.hanote.desktop
	rm -f $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/com.suhokang.hanote.svg

.PHONY: all clean install uninstall
