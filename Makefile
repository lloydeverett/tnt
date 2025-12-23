
.PHONY: all clean

all: tmt

tmt: tmt.o main.o
	$(CC) $(CFLAGS) $^ -o $@

tmt.o: tmt.c
	$(CC) $(CFLAGS) -c $< -o $@

main.o: main.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o tmt

