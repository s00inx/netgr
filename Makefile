CFLAGS = -O2 -Wall -Wextra -Wpedantic -Wshadow -Wno-unused-parameter -Wpointer-arith -Wframe-larger-than=4096 -Wformat=2 -Wnull-dereference -Wswitch-enum -Wswitch-default -Wtrivial-auto-var-init -Wuseless-cast -Wstrict-flex-arrays -Wattribute-alias=2 -Wduplicated-branches -Wduplicated-cond -Wtrampolines -Wundef -Wwrite-strings -Wconversion -Wsign-conversion -Wflex-array-member-not-at-end -Wlogical-op -Wdeprecated-non-prototype -Wmissing-prototypes -Wmissing-variable-declarations -Wmissing-declarations -Wnested-externs -Winvalid-pch -Wvla-larger-than=1024 -Wuse-after-free=3 -Walloc-zero -fstrict-flex-arrays=3 -fzero-init-padding-bits=all -ftrivial-auto-var-init=zero -fno-delete-null-pointer-checks -fno-finite-loops

all: 
	gcc $(CFLAGS) -g -Iinclude -lmnl $(wildcard src/*.c) -o ng 
