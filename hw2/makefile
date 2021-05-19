# 046267 Computer Architecture - Spring 2020 - HW #2

cacheSim: cache.o cacheSim.cpp
	g++ -std=c++11 -o cacheSim cacheSim.cpp

cache.o: cache.cpp
	g++ -c -std=c++11 -Wall -o cache.o cache.cpp


.PHONY: clean
clean:
	rm -f *.o
	rm -f cacheSim
