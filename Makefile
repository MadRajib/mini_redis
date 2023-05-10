SERVER_OBJ=server.o
CLIENT_OBJ=client.o
DB_OBJ=db.o
UTILS_OBJ=utils.o


INCLUDE_DIR=include/
SRC_DIR=src

CFLAGS=-Wall -g -std=gnu11
LIBS=-lm

all:server client

server:$(SERVER_OBJ) $(DB_OBJ) $(UTILS_OBJ)
	$(CC) $(CFLAGS) $(SERVER_OBJ) $(DB_OBJ) $(UTILS_OBJ) -o $@ $(LIBS)

client:$(CLIENT_OBJ) $(UTILS_OBJ)
	$(CC) $(CFLAGS) $(CLIENT_OBJ) $(UTILS_OBJ) -o $@ $(LIBS)

server.o: $(SRC_DIR)/server.c
	$(CC) $(CLFAGS) $(INCLUDE_DIR:%=-I %) -c $<

$(DB_OBJ):$(SRC_DIR)/db.c $(UTILS_OBJ)
	$(CC) $(CLFAGS) $(INCLUDE_DIR:%=-I %) -c $<

client.o: $(SRC_DIR)/client.c
	$(CC) $(CLFAGS) $(INCLUDE_DIR:%=-I %) -c $<

utils.o:$(SRC_DIR)/utils.c
	$(CC) $(CLFAGS) $(INCLUDE_DIR:%=-I %) -c $<

clean:
	rm -f *.o *.gch *.out server client
