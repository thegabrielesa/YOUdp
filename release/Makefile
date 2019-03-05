CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -O3
LIB = -pthread

do:
	$(CC) $(CFLAGS) $(LIB) ./CLIENT/client.c -o ./CLIENT/client
	$(CC) $(CFLAGS) $(LIB) ./SERVER/server.c -o ./SERVER/server
	$(CC) $(CFLAGS) file_compare.c -o file_compare

	@echo " "
	@echo "Successful compiling!"

clean:
	rm ./CLIENT/client
	rm ./SERVER/server
	rm file_compare

	@echo " "
	@echo "Executables deleted!"
