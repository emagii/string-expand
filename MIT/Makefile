CC = gcc


all:	string-expand

string-expand:	string-expand.c
	$(CC) -D__MAIN__ -g -o $@ $<

clean:
	rm -fr string-expand *.o
