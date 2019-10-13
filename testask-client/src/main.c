/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * main.c
 * Copyright (C) 2019 ?????????????????? ???????????? ???????????????????? <abrakadabra21099@gmail.com>
 * 
 * testask-client is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * testask-client is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <common.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <time.h>
/*
int broadcast_send(const struct SINGLE_MSG msg){
	
	int sock = get_sock();
	int broadcast = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, 
	               sizeof broadcast) == -1) {
		perror("broadcast");
		exit(1);
	}
	struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_LISTEN_PORT);
    addr.sin_addr.s_addr = htonl(SERVER_ADDRESS);
    int res = sendto(sock, (void *) &msg, sizeof(msg.head) + msg.head.MSG_SIZE, 
                     0, (struct sockaddr *)&addr, sizeof(addr));
	if (res < 0) printf("Error: %d", errno); else printf("Send %d bytes.", res);
	close(sock);

	return res;
};*/

FILE *get_sentfile(const char *file_to_sent) {
	FILE *sentfile = fopen(file_to_sent, "r");;  	
	if (sentfile == NULL) {
//		fprintf(stderr, "failed to open file_to_sent: %s\n", file_to_sent);
		perror("failed to open file_to_sent param");
		exit(1);
	};
	return sentfile;
}

ssize_t recv_one_packet(const int sock, struct sockaddr_in addr, 
                    void *msg, const ssize_t msglen) {
	socklen_t addrlen = sizeof ( addr );
	return recvfrom(sock, msg, msglen, 0, (struct sockaddr *)&addr, 
	               &addrlen); 
}

bool sent_ten_packets(FILE *sentfile, const int sock, 
                      const char *server_ip_addr,
                      const bool need_skip, long unsigned int * skipped, 
                      long unsigned int * packet_count) {

	struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_LISTEN_PORT);
//    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (inet_pton(AF_INET, server_ip_addr, (void *)&addr.sin_addr) != 1) {
        perror("server_ip_addr");
        exit(1);
    };

	int ret, r_size = SINGLE_MSG_BODY_SIZE;
	// random skip random packet if need
	short int skip_id; //= rand() % TEN_SERIES_MSG_COUNT;
	struct SINGLE_MSG msg[TEN_SERIES_MSG_COUNT];
	memset(&msg, 0, sizeof msg);
	for (short int i=0; i < TEN_SERIES_MSG_COUNT; i++){
		msg[i].head.MSG_ID = i;
		if (r_size > 0) {
//			read data from file and assign into ten msg array
			r_size = fread(msg[i].MSG_BODY, sizeof(unsigned char), 
	               SINGLE_MSG_BODY_SIZE, sentfile);
			msg[i].head.MSG_SIZE = r_size;
			msg[i].head.MSG_OPERATION = 0;
		};
		if (need_skip) skip_id = rand() % TEN_SERIES_MSG_COUNT;
		if ( need_skip && (i == skip_id) ) {
			(*skipped)++;
			printf("\t - Skip msg id: %d.\n", i);			
		} else {
			ret = sent_one_packet (sock, addr, &msg[i], 
		                       sizeof(struct SINGLE_MSG_HEAD) + r_size);
			printf("< Send msg id: %d, bytes read\\sent: %d\\%d.\n", i, r_size, 
		       ret);
		}
		(*packet_count)++;
		r_size = (r_size != SINGLE_MSG_BODY_SIZE) ? 0 : SINGLE_MSG_BODY_SIZE;
	};
	
//	Recive request for skiped or lost packets.
	struct SINGLE_MSG confirm;
	do { 
		if (recv_one_packet(sock, addr, &confirm, sizeof confirm) < 
		    sizeof(struct SINGLE_MSG_HEAD)) {
			printf("* > Received msg from server id: %d, op: %d, size: %d. Too small.\n", 
		       confirm.head.MSG_ID, confirm.head.MSG_OPERATION, 
		       confirm.head.MSG_SIZE);
			continue;
		}
		if (confirm.head.MSG_OPERATION == 1) { 
			 if (confirm.head.MSG_ID < TEN_SERIES_MSG_COUNT) {

				printf("* > Request for packed with msg id %d recived.\n", 
				       confirm.head.MSG_ID);
				sent_one_packet(sock, addr, &msg[confirm.head.MSG_ID], 
			                       sizeof(struct SINGLE_MSG_HEAD) + 
			                msg[confirm.head.MSG_ID].head.MSG_SIZE);
				printf("< Re-sent requested packed with msg id %d.\n", 
				       confirm.head.MSG_ID);
			
			} else { 
				printf("* Incorrect request recived, drop it.\n");
			}
		}	
	} while (confirm.head.MSG_OPERATION != 2);
	printf("* > Ten packets confirmation received with id: %d, oper: %d, size: %d.\n",
		       confirm.head.MSG_ID, confirm.head.MSG_OPERATION, 
		       confirm.head.MSG_SIZE);
	return (r_size != 0);
}

int main(int argc, char *argv[]) {
	if (argc != 4) {
		perror("Usage: testask-client server_ip_addr \
file_to_sent max_skip_packet_count.");
		/*fprintf(stderr, "Usage: testask-client server_ip_addr \
file_to_sent max_skip_packet_count.\n%d", argc);*/
		exit(1);
	};
	int sock = get_sock();
	FILE *sentfile = get_sentfile(argv[2]);	

	bool need_skip;
	unsigned long max_skip_packet_count = strtod(argv[3], NULL);
	unsigned long packet_count = 0,
				skip_packet_count = 0, 
				ten_packet_count = 0;
	srand( time(NULL) );
	do {
		ten_packet_count++;
		need_skip = ( skip_packet_count < (max_skip_packet_count - 1) ) && 
			( rand() % 2 ) ;
	} while (sent_ten_packets(sentfile, sock, argv[1], need_skip, 
	                          &skip_packet_count, &packet_count));

	printf("*** Sent FINISH! Ten packet count: %ld, artificially skipped \
msg count: %ld, total msg count: %ld.\n", 
	       ten_packet_count, skip_packet_count, packet_count);
	fclose(sentfile);
	close(sock);
	return (0);
}
