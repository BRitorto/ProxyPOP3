include Makefile.inc

TARGET := pop3ctl
SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)
rm       = rm -rf


all:	comp link


comp:$(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES)
	@echo "Pop3ctl Compilation complete."

link:$(OBJECTS)
	$(LINKER) $(LFLAGS) $(OBJECTS) ./../Utils/*.o ./../pop3filter/multiplexor.o ./../pop3filter/stateMachine.o ./../Admin/*.o -o $(TARGET).out
	@echo "Pop3ctl Linking complete."

%.o : %.c
	$(CC) $(CFLAGS) -I./include -c $< -o $@

%.out : %.o
	$(LINKER) $(LFLAGS) $< -o $@

clean:
	@$(rm) $(OBJECTS)
	@$(rm) $(TARGET).out
	@echo "Pop3ctl Cleanup complete."



