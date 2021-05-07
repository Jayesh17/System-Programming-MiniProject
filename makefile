CC=gcc

main.out: main.c
	$(CC) -o main.out -pthread main.c
	
.PHONY:clean
clean:
	rm main.out 
