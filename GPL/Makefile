CC = gcc


all:	string-expand

string-expand:	string-expand.c
	$(CC) -D__MAIN__ -g -o $@ $< -liberty

clean:
	rm -fr string-expand *.o
