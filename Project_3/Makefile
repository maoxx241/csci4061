CC = gcc
CFLAGS = -D_REENTRANT
LDFLAGS = -lpthread -pthread

web_server: server.c cache.c logger.c request.c
	${CC} -o web_server -g server.c util.o cache.c logger.c request.c ${LDFLAGS}

clean:
	rm web_server
