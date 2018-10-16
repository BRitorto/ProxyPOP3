include Makefile.inc

TARGET := main
SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)
rm       = rm -rf


all:  parsers comp link

parsers:
	cd Parsers; make all


comp:$(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES)
	@echo "Proxy Compilation complete."

link:$(OBJECTS)
	$(LINKER) $(LFLAGS) $(OBJECTS) ./../Utils/*.o ./Parsers/*.o ./../Admin/*.o -o $(TARGET).out
	@echo "Proxy Linking complete."

%.o : %.c
	$(CC) $(CFLAGS) -I./include -c $< -o $@

%.out : %.o
	$(LINKER) $(LFLAGS) $< -o $@

clean:
	@$(rm) $(OBJECTS)
	@$(rm) $(TARGET).out
	@echo "Proxy Cleanup complete."



