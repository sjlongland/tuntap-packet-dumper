all: packetdumper
clean:
	-rm -f *.o packetdumper

packetdumper: linuxtun.o main.o
	$(CC) -o $@ $^

linuxtun.o: linuxtun.h
main.o: linuxtun.h
