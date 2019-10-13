#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int get_sock() {
	int sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    };
	return sock;
};

int sent_one_packet(const int sock, const struct sockaddr_in addr, 
                    const void *msg, const int msglen) {
	int ret;
	if ((ret = sendto(sock, msg, msglen, 0, 
	                 (struct sockaddr*)&addr, sizeof(struct sockaddr_in) 
	    		)) == -1) {
		perror("sendto");
		exit(1);
	}
	return ret;
};

ssize_t recv_one_packet(const int sock, struct sockaddr_in *addr, 
                    void *msg, const ssize_t msglen) {
	ssize_t ret;
	socklen_t addrlen = sizeof addr;
	ret = recvfrom(sock, msg, msglen, 0, (struct sockaddr*)&addr, 
	               &addrlen); 
	return ret;
}
