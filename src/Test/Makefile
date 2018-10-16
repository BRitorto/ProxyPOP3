include Makefile.inc

TARGET := AllTests
SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)
rm       = rm -rf


all: clean comp link

comp:$(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES)
	@echo "Tests Compilation complete."

link:$(OBJECTS)
	$(LINKER) $(LFLAGS) ./../Proxy/stateMachine.o ./../Proxy/Parsers/*.o  ./../Utils/*.o $(OBJECTS) -o $(TARGET).out
	@echo "Tests Linking complete."

%.o : %.c
	$(CC) $(CFLAGS) $< -o $@

%.out : %.o
	$(LINKER) $(LFLAGS) $< -o $@

clean:
	@$(rm) $(OBJECTS)
	@$(rm) $(TARGET).out
	@echo "Tests Cleanup complete."

.PHONY: clean  all  
