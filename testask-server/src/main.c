/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * main.c
 * Copyright (C) 2019 ?????????????????? ???????????? ???????????????????? <abrakadabra21099@gmail.com>
 * 
 * testask-server is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * testask-server is distributed in the hope that it will be useful, but
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
#include <stdbool.h>
#include <arpa/inet.h>

FILE *get_recvfile(const char *file_to_sent) {
	FILE *sentfile = fopen(file_to_sent, "a");;  	
	if (sentfile == NULL) {
//		fprintf(stderr, "failed to open file_to_sent: %s\n", file_to_sent);
		perror("failed to open file for append :(.");
		exit(1);
	};
	return sentfile;
}
//******* is ten packets consistency?
bool is_ten_consist( struct SINGLE_MSG msg[TEN_SERIES_MSG_COUNT]) {

	bool res = true;
	for (short int i = 0; (i < TEN_SERIES_MSG_COUNT) && res; i++) 
		res = msg[i].head.MSG_ID == i;
	return res;

}

int main() {
    int sock;
	FILE *recvfile;
	struct addrinfo hint, *res;
	memset(&hint, 0, sizeof hint);
	hint.ai_family = AF_INET;
	hint.ai_socktype = SOCK_DGRAM;
	hint.ai_flags = AI_PASSIVE;
	char port[6];
	sprintf(port, "%u", SERVER_LISTEN_PORT);
	getaddrinfo(NULL, port, &hint, &res);
    sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(sock < 0)
    {
        perror("socket");
        exit(1);
    }
    
//    sock = socket(AF_INET, SOCK_DGRAM, 0);
//    addr.sin_family = AF_INET;
//    addr.sin_port = htons(SERVER_LISTEN_PORT);
//    addr.sin_addr.s_addr = htonl(INADDR_ANY);
//    if(bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	
    if(bind(sock, res->ai_addr, res->ai_addrlen) < 0)
    {
        perror("bind");
        exit(2);
    }
    struct SINGLE_MSG buf, msg[TEN_SERIES_MSG_COUNT];
    ssize_t bytes_read;

	struct sockaddr_in from;
	socklen_t fromlen = sizeof from;
	char s[INET_ADDRSTRLEN], filename[5 + INET_ADDRSTRLEN + 25];
	//							/tmp/XXX.XXX.XXX.XXX:XXXXX.testask-server
	unsigned long packet_count = 0,
				skip_packet_count = 0, 
				ten_packet_count = 0;
	
    while(1) {

		unsigned short int lost = 0;
		int clientsock = get_sock();

		// **** initialize ten series array ****
		memset(&msg, 0, sizeof msg);	
		for (short int i = 0; i < TEN_SERIES_MSG_COUNT; i++) {
			msg[i].head.MSG_ID = -1;
		}
		memset(&buf, 0, sizeof buf);
		buf.head.MSG_ID = -1;

		bool request = false;
		do {			
			do {
				
//				*****wait incoming packets*****			
				struct timeval timeout = {(buf.head.MSG_ID == -1)*30, 200000};
				fd_set set;
				FD_ZERO( &set );
				FD_SET( sock, &set );

				int selret = select(sock+1, &set, NULL, NULL, &timeout); 
				if (selret == -1) {
					perror("select");
					exit(1);
				};
//				*****wait timeout event*****
				if (selret == 0) { 
					if (buf.head.MSG_ID != -1) {
						printf("Got receive timeout...\n");
						break;
					}else continue; 
				};
//				*******************************
			
//				*********** receive ***********
	        	bytes_read = recvfrom(sock, (void *)&buf, sizeof buf, 
                              0, (struct sockaddr *)&from, &fromlen);
				if (bytes_read > 0) 
					printf("> Recv msg id: %d, operation: %d, size: %d, from: %s:%d.\n",
		                           buf.head.MSG_ID, 
		                           buf.head.MSG_OPERATION, 
		                           buf.head.MSG_SIZE, 
					       		   inet_ntop(AF_INET, 
							                    (void *)&from.sin_addr.s_addr, 
							                    s, sizeof s),
				       			   ntohs( from.sin_port )
					       );
				if (buf.head.MSG_ID < TEN_SERIES_MSG_COUNT) {
//					*********** copy received msg into array ***********
					memcpy(
				       &msg[buf.head.MSG_ID],
				       &buf,
				       bytes_read
					);
					packet_count++;					
//					*********** file finish detect *************
					if (request && is_ten_consist( msg )) {
						request = false;
						break;
					}
				} else { 
					printf("* Incorrect packet recived, drop it.\n");
				}
			} while (buf.head.MSG_ID != (TEN_SERIES_MSG_COUNT-1));
			//**********Ten packets partialy received.		
			//**********calculate lost packets and request it.			
			lost = 0;
			for (short int i = 0; i < TEN_SERIES_MSG_COUNT; i++) {
				
				if (msg[i].head.MSG_ID != -1) continue;
				request = true;
				lost++;
				struct SINGLE_MSG_HEAD request = {1, i, 0};
				sent_one_packet(clientsock, from, &request, sizeof request);
				printf("* < Sent request for lost packet with id: %d.\n", i);

			}
			skip_packet_count += lost;
		} while (lost != 0);
		
//		sent confirmation about ten packets received		

		if ( buf.head.MSG_ID >=0 ) {
			ten_packet_count++;
			for (short int i = 0; i<TEN_SERIES_MSG_COUNT; i++) {
				if (msg[i].head.MSG_SIZE > 0) {
					sprintf(filename, "/tmp/%s:%u.testask-server", 
					        inet_ntop(AF_INET, (void *)&from.sin_addr.s_addr, 
					                  s, sizeof s), ntohs( from.sin_port )
					        );
					recvfile = get_recvfile(filename);
					fwrite( &msg[i].MSG_BODY, msg[i].head.MSG_SIZE, 1, 
					       recvfile);
					fclose(recvfile);
				}
			}			
			struct SINGLE_MSG_HEAD confirm = {2, buf.head.MSG_ID, 0};
			printf("* Ten packets full recived, last msg id: %d.\n", 
			       buf.head.MSG_ID);
			int sop = sent_one_packet(clientsock, from, &confirm, 
			                          sizeof confirm);
			printf("* < Sent ten packets confirmation (%d of %ld bytes).\n", sop, 
			       sizeof confirm);
//			*********** finish msg detect ***********
			if ( (msg[TEN_SERIES_MSG_COUNT - 1].head.MSG_ID == 
			    (TEN_SERIES_MSG_COUNT - 1)) && 
			    (msg[TEN_SERIES_MSG_COUNT - 1].head.MSG_SIZE == 0)
			    ) {
				printf("*** Recv FINISH! Ten packets count: %ld, \
skipped and requested msg count: %ld, \
total msg count: %ld.\n", ten_packet_count, skip_packet_count, packet_count);
				ten_packet_count = 0;
				skip_packet_count = 0;
				packet_count = 0;
			}
		}
		close(clientsock);
    }
	return (0);
}
