CFLAGS += -O2
LDFLAGS += -lpthread

.PHONY: all
all: server client

server: server.o ec-method.o ec-gf.o

client: client.o ec-method.o ec-gf.o

test: test.o ec-method.o ec-gf.o

.PHONY: clean
clean:
	$(RM) server client test server.o client.o test.o ec-method.o ec-gf.o 

