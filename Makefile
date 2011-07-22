CC=gcc
CFLAGS=-g3 -Wall -fPIC

.PHONY: all clean

all: pam_iccard.so initcard registercard

pam_iccard.so: pam_iccard.o card.o common.o settings.o
	$(CC) $(CFLAGS) -shared -o $@ $^ -lssl -lpam -lpcsclite

initcard: initcard.o card.o common.o settings.o
	$(CC) $(CFLAGS) -o $@ $^ -lssl -lpcsclite

registercard: registercard.o card.o common.o settings.o
	$(CC) $(CFLAGS) -o $@ $^ -lssl -lpcsclite
	sudo chown root $@
	sudo chmod +s $@

card.o: card.c
	$(CC) $(CFLAGS) `pkg-config --libs --cflags libpcsclite` -c -o $@ $^ 

clean:    
	-rm -f module pam_iccard.so initcard registercard
	-rm -f *.o

# vim: set noet:
