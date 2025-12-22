TARGET = c64
PROGRAM = matrix.prg
SOURCES = main.c
CC = cl65
CFLAGS = -O -t $(TARGET)

all: $(PROGRAM)

$(PROGRAM): $(SOURCES)
	$(CC) $(CFLAGS) -o $(PROGRAM) $(SOURCES)

clean:
	rm -f $(PROGRAM) *.o *.s
