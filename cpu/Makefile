CFLAGS += -O2
LDFLAGS += -lpthread

.PHONY: all
all: server client

server: server.o ec-method.o ec-gf.o timer.o

client: client.o ec-method.o ec-gf.o timer.o

test: test.o ec-method.o ec-gf.o

timer.o: timer.h timer.c

server.o: server.c ec-method.h nodes.h

client.o: client.c ec-method.h nodes.h

test.o: test.c ec-method.h

ec-method.o: ec-method.c ec-method.h ec-gf.h

ec-gf.o: ec-gf.c ec-gf.h

.PHONY: clean
clean:
	$(RM) server client test server.o client.o test.o ec-method.o ec-gf.o timer.o

