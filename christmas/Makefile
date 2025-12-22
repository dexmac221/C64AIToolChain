PROGRAM = christmas.prg
SOURCES = main.c
CC = cl65
CFLAGS = -O -t c64

all: $(PROGRAM)

$(PROGRAM): $(SOURCES)
	$(CC) $(CFLAGS) -o $(PROGRAM) $(SOURCES)

clean:
	rm -f $(PROGRAM) *.o
