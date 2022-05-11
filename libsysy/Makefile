
all:
	gcc -c -m32 sylib.c
	ar rcs libsysy.a sylib.o

arm:
	arm-linux-gnueabi-gcc -mthumb sylib.c -c -o sylib-arm.o
	arm-linux-gnueabi-ar rcs libsysy-arm.a sylib-arm.o

clean:
	- rm *.o
	- rm *.a
