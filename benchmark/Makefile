
all : clean test mtest
test:
	gcc -g -w -m32 -o test test.c -L../ -lmy_vm

mtest:
	gcc -g -w -m32 -o mtest multi_test.c -L../ -lmy_vm  -lpthread

clean:
	rm -rf test mtest *.o
