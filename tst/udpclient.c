#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

static void help(void) 
{
	printf("UDP client for test purpose\n");
	printf("client <ip_to_use> <port_to_use>\n");
}


int main(int argc, char *argv[])
{
	int sock;
	struct sockaddr_in server_addr;
	struct hostent *host = NULL;
	char *send_data = NULL;
	size_t size = 0;
	ssize_t len = 0;

	if (argc < 3) {
		help();
		exit(1);
	}

	host= (struct hostent *) gethostbyname((char *)argv[1]);

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		perror("socket");
		exit(1);
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[2]));
	server_addr.sin_addr = *((struct in_addr *)host->h_addr);
	bzero(&(server_addr.sin_zero),8);

	printf("Sending to %s:%d\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));

	while (1)
	{
		printf("Type Something (q or Q to quit):");
		if ((len = getline(&send_data, &size, stdin)) > 0) {
			len--;
			send_data[len] = '\0';
			if (strncasecmp(send_data , "q", len) == 0) {
				break;
			} else {
				fprintf(stdout, "sending %s size %u", send_data, (unsigned int)len);
				sendto(sock, send_data, len, 0,
						(struct sockaddr *)&server_addr, sizeof(struct sockaddr));
			}
			free(send_data);
			send_data = NULL;
			size = 0;
			len = 0;
		} else {
			break;
		}
	}
	return 0;
}
