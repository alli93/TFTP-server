
/* A UDP echo server with timeouts.
 *
 * Note that you will not need to use select and the timeout for a
 * tftp server. However, select is also useful if you want to receive
 * from multiple sockets at the same time. Read the documentation for
 * select on how to do this (Hint: Iterate with FD_ISSET()).
 */
#include <stdbool.h>
#include <assert.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

#define TRUE 1
#define FALSE 0

/*
 * union for conveniently writing to and from 2-byte memories
 */
union {unsigned short n;char byte[2];} tmp;

//writes 2 bytes to a buffer
void numbertobuffer(char * buffer, int index, unsigned short n) {
	tmp.n = htons(n);			//convert to network endian
	buffer[index] = tmp.byte[0];
	buffer[index + 1] = tmp.byte[1];
}
/*
 *reads 2 bytes from buffer
 */
unsigned short numberfrombuffer(char * buffer, int index){
	tmp.byte[0] = buffer[index];
	tmp.byte[1] = buffer[index + 1];
	return ntohs(tmp.n);
}

char * make_datapacket(unsigned short block, char data[], size_t size) {
	unsigned int i;
	char * packet = (char *) malloc(size + 4);	//allocate memory for data and header
	memset(packet, 0, size + 4);
	numbertobuffer(packet, 0, 3); 			//opcode for data packet
	numbertobuffer(packet, 2, block) ;		//block #
	for(i = 0; i < size; i++) {
		packet[i + 4] = data[i];
	}
	return packet;
};

/*
 *Returns a relative filepath to a requested file from our server
 */
FILE * getfile(char * buffer_in, char * dir) {
	FILE * file;
	char * relativefilepath = malloc(strlen(buffer_in + 1) + strlen(dir) + 1);
	strcpy(relativefilepath, dir);
	strcat(relativefilepath, "/");
	strcat(relativefilepath, buffer_in + 2);
	printf("file: %s\n", relativefilepath);
	file = fopen(relativefilepath, "r");
	free(relativefilepath);
	return file;
}
/*
 * Returns false file being requested is anything more than a filename
 */
bool validate(char filepath[]) {
	unsigned int i;
	for(i = 0; i < strlen(filepath); i++) {
		if(filepath[i] == '/') {
			return FALSE;
		}
	}
	return TRUE;
}
char * make_errpacket(unsigned short errorcode, char errmsg[]) {
	int i, len = strlen(errmsg);
	char * packet = (char *) malloc(len + 5);
	memset(packet, 0, len + 5);
	numbertobuffer(packet, 0, 5);			//opcode
	numbertobuffer(packet, 2, errorcode);		//errorcode
	for(i = 0; i < len; i++) {
		packet[i + 4] = errmsg[i];		//message
	}
	packet[len + 4] = '\0';
	return packet;
}

/*
 * Sends a packet using the C socket programing interface
 */
void send_packet(int socket, char * packet, int length, struct sockaddr_in * client) {
	sendto(socket, packet, length, 0, (struct sockaddr *) client, (socklen_t) sizeof(*client));
	free(packet);
}


int main(int argc, char **argv) {
	if(argc < 2) {
		printf("port and directory required\n");
		return 0;
	}
	int master_socket, max_clients = 1, client_socket[max_clients], i, max_sd, sd, new_socket, retval, clilen, block, filesize;
        struct sockaddr_in server, client;
	char buffer_in[512];
	char buffer_out[512];
	char * dir = argv[2];
	int port = atoi(argv[1]);
	FILE * downloads[10];
	FILE * file;
	fd_set rfds;
	for(i = 0; i < max_clients; i++) {
		client_socket[i] = -1; //inactive sockets are marked '-1'
	}

	/* Clear buffers */
	memset(buffer_in, 0, 512);
	memset(buffer_out, 0, 512);

        /* Create and bind a UDP socket */
        master_socket = socket(AF_INET, SOCK_DGRAM, 0);

        memset(&server, 0, sizeof(server));
	client.sin_family = AF_INET;
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = htonl(INADDR_ANY);
        server.sin_port = htons(port);
        bind(master_socket, (struct sockaddr *) &server,  sizeof(server));

	/* Maximum of 2 pending connections */
	listen(master_socket, 2);
	clilen = sizeof(client);
	while(TRUE){
		/* clear socket set */
                FD_ZERO(&rfds);

		/* add master socket to socket set */
                FD_SET(master_socket, &rfds);
		max_sd = master_socket;

		/* Add child sockets to socket set */
		for(i = 0; i < max_clients; i++) {
			sd = client_socket[i];
			if(sd >= 0) {
				FD_SET(sd, &rfds);
			}
			if(sd > max_sd) {
				max_sd = sd;
			}
		}
                retval = select(max_sd + 1, &rfds, NULL, NULL, NULL);
                if(retval < 0) {
			printf("select error\n");
		}

 	        if(FD_ISSET(master_socket, &rfds)) {
			recvfrom(master_socket, buffer_in, sizeof(buffer_in)-1, 0,
                       	        (struct sockaddr *) &client, (socklen_t *) &clilen);
			if(!validate(buffer_in + 2) || numberfrombuffer(buffer_in, 0) == 2) {
				char * packet = make_errpacket(2, "Access violation");
				send_packet(master_socket, packet, 21, &client);
			}
			else {
				for(i = 0; i < max_clients; i++) {
					if(client_socket[i] < 0) {
						file = getfile(buffer_in, dir);
						if(file) {
							char ipaddr[INET_ADDRSTRLEN];
							int intaddr = client.sin_addr.s_addr;
						        new_socket = socket(AF_INET, SOCK_DGRAM, 0);
							connect(new_socket, (struct sockaddr *)&client, sizeof(client));
							downloads[i] = file;
							inet_ntop(AF_INET, &intaddr, ipaddr, INET_ADDRSTRLEN);
							printf("file \"%s\" requested from %s:%d\n", buffer_in + 2, ipaddr, ntohs(client.sin_port));
							client_socket[i] = new_socket;
							size_t datal = fread(buffer_out, 1, 512, downloads[i]);
							char * packet = make_datapacket(1, buffer_out, datal);
							send_packet(client_socket[i], packet, datal + 4,&client);
							memset(buffer_out, 0, 512);
							break;
						}
						else if(file == NULL){
							char * packet = make_errpacket(1, "File not found");
							send_packet(master_socket, packet, 19, &client);
							break;
						}
						else {
							char * packet = make_errpacket(2, "Access violation");
							send_packet(master_socket, packet, 21, &client);
							break;
						}
					}
					if(i == (max_clients - 1)) {
						char * packet = make_errpacket(3, "The number of concurrent users is at maximum");
						send_packet(master_socket, packet, 49, &client);
							break;
					}
				}
				memset(buffer_in, 0, 512);
			}
		}
		for(i = 0; i < max_clients; i++) {
			sd = client_socket[i];
			if(FD_ISSET(sd, &rfds)) {
				recvfrom(sd, buffer_in, sizeof(buffer_in) - 1, 0,
					(struct sockaddr *) &client, (socklen_t *) &clilen);
				block = numberfrombuffer(buffer_in, 2);
				fseeko(downloads[i], 0, SEEK_END);
				filesize = ftell(downloads[i]);
				//Free up space if we are done sending.
				if(block * 512 >= filesize) {
					fclose(downloads[i]);
					client_socket[i] = -1;
					FD_CLR(sd, &rfds);
				}
				else {
					fseeko(downloads[i], block * 512, SEEK_SET);
					size_t datal = fread(buffer_out, 1, 512, downloads[i]);
					char * packet = make_datapacket(block + 1, buffer_out, datal);
					send_packet(client_socket[i], packet, datal+4, &client);
				}
				memset(buffer_in, 0, 512);
				memset(buffer_out, 0, 512);
			}
		}
  }
}
