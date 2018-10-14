include Makefile.inc

TARGET := main
SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)
rm       = rm -rf


all: comp link test run

comp:$(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES)
	@echo "Compilation complete."

link:$(OBJECTS)
	$(LINKER) $(LFLAGS) $(OBJECTS)  -o $(TARGET).out
	@echo "Linking complete."

%.o : %.c
	$(CC) $(CFLAGS) $< -o $@

%.out : %.o
	$(LINKER) $(LFLAGS) $< -o $@

clean:
	@$(rm) $(OBJECTS)
	@$(rm) $(OUTPUTS)
	@echo "Cleanup complete."
test:
	cd Test; make all

run:
	./Test/AllTests.out

.PHONY: clean  all  test
