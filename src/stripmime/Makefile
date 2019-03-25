include Makefile.inc

TARGET := stripmime
SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)
rm       = rm -rf


all:  comp link

comp:$(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES)
	@echo "Stripmime Compilation complete."

link:$(OBJECTS)
	$(LINKER) $(LFLAGS) $(OBJECTS) ./../Utils/*.o -o $(TARGET).out
	@echo "Stripmime Linking complete."

%.o : %.c
	$(CC) $(CFLAGS) -I./include -c $< -o $@

%.out : %.o
	$(LINKER) $(LFLAGS) $< -o $@

clean:
	@$(rm) $(OBJECTS)
	@$(rm) $(TARGET).out
	@echo "Stripmime Cleanup complete."



