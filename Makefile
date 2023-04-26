SERVER_OBJ=server.o
CLIENT_OBJ=client.o
UTILS_OBJ=utils.o


INCLUDE_DIR=include/
SRC_DIR=src

CFLAGS=-Wall -g -std=gnu11
LIBS=-lm

server:$(SERVER_OBJ) $(UTILS_OBJ)
	$(CC) $(CFLAGS) $(SERVER_OBJ) $(UTILS_OBJ) -o $@ $(LIBS)

client:$(CLIENT_OBJ) $(UTILS_OBJ)
	$(CC) $(CFLAGS) $(CLIENT_OBJ) $(UTILS_OBJ) -o $@ $(LIBS)

server.o: $(SRC_DIR)/server.c
	$(CC) $(CLFAGS) $(INCLUDE_DIR:%=-I %) -c $<

client.o: $(SRC_DIR)/client.c
	$(CC) $(CLFAGS) $(INCLUDE_DIR:%=-I %) -c $<

utils.o:$(SRC_DIR)/utils.c
	$(CC) $(CLFAGS) $(INCLUDE_DIR:%=-I %) -c $<

clean:
	rm -f *.o *.gch *.out server client
