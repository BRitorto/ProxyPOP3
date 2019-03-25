include Makefile.inc

TARGET  := filterWrapper
SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)
rm       = rm -rf


all:  comp link

comp:$(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES)
	@echo "FilterWrapper Compilation complete."

link:$(OBJECTS)
	$(LINKER) $(LFLAGS) $(OBJECTS) ./../multiplexor.o ./../Parsers/*.o ./../../Utils/*.o -o $(TARGET).out
	@echo "FilterWrapper Linking complete."

%.o : %.c
	$(CC) $(CFLAGS) -I./include -c $< -o $@

%.out : %.o
	$(LINKER) $(LFLAGS) $< -o $@

clean:
	@$(rm) $(OBJECTS)
	@$(rm) $(TARGET).out
	@echo "FilterWrapper Cleanup complete."
