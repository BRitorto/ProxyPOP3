
all:  admin utils filterApp proxy popctl test run

admin:
	cd Admin; make all
utils:
	cd Utils; make all

filterApp:
	cd stripmime; make all
	
proxy:
	cd pop3filter; make all
popctl:
	cd pop3ctl; make all

test:
	cd Test; make all
	./Test/AllTests.out
run:
	./run.sh

clean:
	cd Admin; make clean	
	cd Utils; make clean
	cd stripmime; make clean
	cd pop3filter; make clean
	cd pop3ctl; make clean
	cd Test; make clean

.PHONY: all clean
