LDLIBS=-lxqilla

all: xqillac
clean:
	$(RM) xqillac

test: all
	./test.sh

