CC = /opt/gbdk-linux/bin/lcc
PNG2MTSPR = /opt/gbdk-linux/bin/png2mtspr

CFLAGS = -Wa-l -Wl-m -Wl-j

TARGET = gbjam9.gb

ASSETS = $(wildcard assets/*.png)
ASSETS_SRC = $(ASSETS:.png=.c)
ASSETS_HEADERS = $(ASSETS:.png=.h)
ASSETS_OBJ = $(ASSETS:.png=.o)

SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

all: $(TARGET)

%.c: %.png
	$(PNG2MTSPR) $< -c $@ -sh 16 -spr8x8 -sp 0x10

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(TARGET): $(ASSETS_SRC) $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

run: $(TARGET)
	higan $(TARGET)

clean:
	rm -rf *.o *.lst *.map *.gb *~ *.rel *.cdb *.ihx *.lnk *.sym *.asm *.noi $(ASSETS_SRC) $(ASSETS_HEADERS) $(ASSETS_OBJ)

