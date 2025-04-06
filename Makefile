
# all: 
# 	cc -std=c99 -O3 -Wall -Wextra -Wpedantic $(wildcard src/*.c)

all:
	clang -std=c99 -O3 -Wall -Wextra -Wpedantic src/*.c `llvm-config --cflags --libs core analysis`

test:
	./a.out project/foo.txt
	clang -Wall -Wextra -Wpedantic project/main.c project/foo.o -o project/main

