include Makefile.inc

TARGET := pop3filter
SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)
rm       = rm -rf


all:  parsers comp wrapper link

parsers:
	cd Parsers; make all

wrapper:
	cd FilterWrapper; make all

comp:$(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES)
	@echo "pop3filter Compilation complete."

link:$(OBJECTS)
	$(LINKER) $(LFLAGS) $(OBJECTS) ./../Utils/*.o ./Parsers/*.o ./../Admin/*.o -o $(TARGET).out
	@echo "pop3filter Linking complete."

%.o : %.c
	$(CC) $(CFLAGS) -I./include -c $< -o $@

%.out : %.o
	$(LINKER) $(LFLAGS) $< -o $@

clean:
	@$(rm) $(OBJECTS)
	@$(rm) $(TARGET).out
	@echo "pop3filter Cleanup complete."



