server: const.h connector.h
	gcc -Wall -D_FORTIFY_SOURCE=2 -Wl,-z,relro,-z,now -fPIE -fstack-protector -o server server.c
clean:
	rm server
