# remove the # in the following line to enable reorg compilation (and running)
all: cruncher reorg

cruncher: cruncher.c utils.h 
	gcc -I. -O3 -o cruncher cruncher.c # bloom.c MurmurHash2.c -lbz2

loader: loader.c utils.h
	gcc -I. -O3 -o loader loader.c 

reorg: reorg.c utils.h
	gcc -I. -O3 -o reorg reorg.c #coding_policy.c pfordelta.c pack.c unpack.c

test: test.c utils.h
	gcc -I. -g -o test test.c #coding_policy.c pfordelta.c pack.c unpack.c


clean:
	rm -f loader cruncher reorg

