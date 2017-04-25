ic: ic0.o
	cc ic0.o -o ic0

ic0.o: ic0.c
	cc -Wall -Werror -c ic0.c -o ic0.o
