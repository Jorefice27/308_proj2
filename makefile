make: server.c bank.c
	gcc -pthread -o appserver server.c bank.c -I.
	# change to -lpthread before you turn it in
