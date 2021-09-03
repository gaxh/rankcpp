all : zeeset.test

all : zeeset.bench

zeeset.test : zeeset.h zeeset.test.cpp
	g++ zeeset.test.cpp -o $@ -O2 -g -Wall

zeeset.bench : zeeset.h zeeset.bench.cpp
	g++ zeeset.bench.cpp -o $@ -O2 -g -Wall

clean:
	rm -f zeeset.test
	rm -f zeeset.bench
