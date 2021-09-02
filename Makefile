

zeeset.test : zeeset.h zeeset.test.cpp
	g++ zeeset.test.cpp -o $@ -O2 -g -Wall

clean:
	rm -f zeeset.test
