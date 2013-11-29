LDLIBS=-lxqilla -lxerces-c
CXXFLAGS?=-O2

all: xqillac
clean:
	$(RM) xqillac

deb-build-dep:
	apt-get install libxqilla-dev

test: all
	./test.sh

