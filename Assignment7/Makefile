all:
	gcc -c rsocket.c -pthread
	ar -rcs librsocket.a rsocket.o
	gcc user1.c -o user1 -L. -lrsocket 
	gcc user2.c -o user2 -L. -lrsocket
clean:
	rm rsocket.o librsocket.a user1 user2