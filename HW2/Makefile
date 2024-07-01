all: service
service.o: service.c
	gcc -c service.c
service: service.o
	gcc -o service service.o
	# compile `service.c` into an executable named `service`
clean:
	rm service.o service
	# remove `service`
