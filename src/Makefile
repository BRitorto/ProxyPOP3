
all:  admin utils proxy test run

admin:
	cd Admin; make all
utils:
	cd Utils; make all

proxy:
	cd Proxy; make all

test:
	cd Test; make all
	./Test/AllTests.out
run:
	./run.sh

clean:
	cd Admin; make clean	
	cd Utils; make clean
	cd Proxy; make clean
	cd Test; make clean

.PHONY: all clean
