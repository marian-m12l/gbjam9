CC = /opt/gbdk-linux/bin/lcc -Wa-l -Wl-m -Wl-j

BINS = gbjam9.gb

all: $(BINS)

%.gb: %.c
	$(CC) -o $@ $<

clean:
	rm -f *.o *.lst *.map *.gb *~ *.rel *.cdb *.ihx *.lnk *.sym *.asm *.noi

