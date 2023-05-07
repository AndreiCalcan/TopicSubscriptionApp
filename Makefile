CFLAGS = -Wall -g -Werror -Wno-error=unused-variable

# Portul pe care asculta serverul
PORT = 12345

# Adresa IP a serverului
IP_SERVER = 127.0.0.1

all: server subscriber

vector.o: vector.c vector.h

common.o: common.c common.h vector.h

# Compileaza server.c
server: server.c common.o vector.o

# Compileaza client.c
subscriber: subscriber.c common.o vector.o

.PHONY: clean run_server run_client

# Ruleaza serverul
run_server:
	./server ${PORT}

# Ruleaza clientul 	
run_client:
	./subscriber 1 127.0.0.1 12345

clean:
	rm -rf server subscriber *.o