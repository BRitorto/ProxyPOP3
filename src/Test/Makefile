include Makefile.inc

TARGET := AllTests
SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)
rm       = rm -rf


all: clean comp link

comp:$(SOURCES)
	$(CC) $(CFLAGS) -I./.. $(SOURCES)
	@echo "Tests Compilation complete."

link:$(OBJECTS)
	$(LINKER) $(LFLAGS) $(OBJECTS) -o $(TARGET).out
	@echo "Tests Linking complete."

%.o : %.c
	$(CC) $(CFLAGS) $< -o $@

%.out : %.o
	$(LINKER) $(LFLAGS) $< -o $@

clean:
	@$(rm) $(OBJECTS)
	@echo "Tests Cleanup complete."

.PHONY: clean  all  
