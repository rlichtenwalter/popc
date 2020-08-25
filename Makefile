CC := g++
COMMON_FLAGS := -std=c++14 -Wall -Wextra -Werror -Wpedantic -Wno-unused-local-typedefs -fopenmp -fuse-ld=gold -larmadillo -lmlpack

DEBUG_FLAGS := -Og -g -fsanitize=address -fno-omit-frame-pointer
RELEASE_FLAGS := -O3 -march=native -flto -fomit-frame-pointer -D NDEBUG

MLPACK_VERSION := 3.3.2

ifeq ($(DEBUG),1)
	CFLAGS := $(COMMON_FLAGS) $(DEBUG_FLAGS)
else
	CFLAGS := $(COMMON_FLAGS) $(RELEASE_FLAGS)
endif

all: bin/popc

bin/popc: src/popc.o | bin/
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

bin/:
	mkdir -p $@

.PHONY: clean

clean:
	rm -f src/*.o
