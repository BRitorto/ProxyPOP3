include Makefile.inc

SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)
rm       = rm -rf


all:$(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES)
	@echo "Admin Compilation complete."


%.o : %.c
	$(CC) $(CFLAGS) -I./include -c $< -o $@

clean:
	@$(rm) $(OBJECTS)
	@echo "Admin Cleanup complete."



