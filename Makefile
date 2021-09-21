GBDK = /opt/gbdk-linux
CC = $(GBDK)/bin/lcc
PNG2MTSPR = $(GBDK)/bin/png2mtspr
PNG2GBTILES = ~/gimp-tilemap-gb/console/bin/linux/png2gbtiles

CFLAGS = -Wa-l -Wl-m -Wl-j

TARGET = gbjam9.gb

METASPRITES = $(wildcard metasprites/*.png)
METASPRITES_SRC = $(METASPRITES:.png=.c)
METASPRITES_HEADERS = $(METASPRITES:.png=.h)
METASPRITES_OBJ = $(METASPRITES_SRC:.c=.o)

TILESETS = $(wildcard tilesets/*.png)
TILESETS_SRC = $(TILESETS:.png=_tiles.c) $(TILESETS:.png=_map.c)
TILESETS_HEADERS = $(TILESETS:.png=_tiles.h) $(TILESETS:.png=_map.h)
TILESETS_OBJ = $(TILESETS_SRC:.c=.o)

SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

all: $(TARGET)

metasprites/%.c: metasprites/%.png
	$(PNG2MTSPR) $< -c $@ -sh 16 -spr8x8 -sp 0x10

tilesets/%_map.c: tilesets/%_tiles.c ;

tilesets/%_tiles.c: tilesets/%.png
	$(PNG2GBTILES) $< -csource -g -tilesz8x8 tilesets/$*.c

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(TARGET): $(METASPRITES_SRC) $(TILESETS_SRC) $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

run: $(TARGET)
	higan $(TARGET)

clean:
	rm -rf *.o *.lst *.map *.gb *~ *.rel *.cdb *.ihx *.lnk *.sym *.asm *.noi $(METASPRITES_SRC) $(METASPRITES_HEADERS) $(METASPRITES_OBJ) $(TILESETS_SRC) $(TILESETS_HEADERS) $(TILESETS_OBJ)

