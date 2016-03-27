CFLAGS=-c -Wall -O2
LIBS = -lpthread

all: imageinfo

imageinfo: main.o pil_io.o
	$(CC) main.o pil_io.o $(LIBS) -o imageinfo

main.o: main.c
	$(CC) $(CFLAGS) main.c

pil_io.o: pil_io.c
	$(CC) $(CFLAGS) pil_io.c

clean:
	rm -rf *o imageinfo

