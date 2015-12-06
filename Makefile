#Reference: http://www.devdrv.co.jp/linux/kernel26-makefile.htm
TARGET:= simpleled.ko

all: ${TARGET}

simpleled.ko: simpleled.c
	make -C /usr/src/linux M=$(PWD) modules

clean:
	make -C /usr/src/linux M=$(PWD) clean

obj-m:= simpleled.o

clean-files := *.o *.ko *.mod.[co] *~
