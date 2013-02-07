LDLIBS=-lxqilla

all: xqillac
clean:
	$(RM) xqillac

deb-build-dep:
	apt-get install libxqilla-dev

test: all
	./test.sh

