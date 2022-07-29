CC := gcc

MANAGER_SRC := src/manager.c
MANAGER_EXE := manager.out
IMG_TAR := alpine-shifted.tar.xz
IMG_URL := https://dl.stgraber.org/$(IMG_TAR)
FS := rootfs

default: config build run

config:
	mkdir $(FS) && cd $(FS) && wget $(IMG_URL) && tar -xvf $(IMG_TAR)

build:
	$(CC) $(MANAGER_SRC) -o $(MANAGER_EXE)

run: build
	sudo ./manager.out $(FS)

clean_exec:
	rm *.out

clean_fs:
	rm -rf $(FS)

cleanall: clean_fs clean_exec
