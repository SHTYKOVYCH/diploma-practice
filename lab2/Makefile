obj-m = lab2.o
KVERSION=$(shell uname -r)
all:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
