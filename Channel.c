#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "winsock2.h"
#include <ws2tcpip.h>
#include <math.h>
#include <windows.h>
#include <process.h>

#pragma comment(lib, "Ws2_32.lib")


#define BUFF_1 64
#define WB_BUFF 4

void Init_Winsock();
void flip_bits(char chnl_buff_1[], double err_prob);
void send_frame(char buff[], int fd, struct sockaddr_in to_addr, int bytes_to_write);
void receive_frame(char buff[], int fd, int bytes_to_read);
DWORD WINAPI thread_end_listen(void *param);
struct sockaddr_in get_sender_ip(int sockfd);




int END_FLAG = 0;
int write_back_buff[WB_BUFF];

int main(int argc, char** argv) {
	Init_Winsock();
	struct sockaddr_in peer_addr;
	int connfd = -1;


	if (argc != 6) {
		printf("Error: not enough arguments were provided!\n");
		exit(1);
	}

	int local_port = (unsigned int)strtoul(argv[1], NULL, 10);
	int rec_ip_add = (unsigned int)strtoul(argv[2], NULL, 10);
	int recv_port = (unsigned int)strtoul(argv[3], NULL, 10);
	double err_prob = (unsigned int)strtoul(argv[4], NULL, 10)*pow(2, -16);
	unsigned long rand_seed = (unsigned int)strtoul(argv[5], NULL, 10);
	int c_s_fd = -1, c_r_fd = -1;
	char chnl_buff[BUFF_1];
	struct sockaddr_in chnl_addr;
	struct sockaddr_in recv_addr;

	srand(rand_seed);
	socklen_t addrsize = sizeof(struct sockaddr_in);
	//channel - sender socket
	if ((c_s_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		exit(1);
	}
	memset(&chnl_addr, 0, sizeof(struct sockaddr_in));
	chnl_addr.sin_family = AF_INET;
	chnl_addr.sin_addr.s_addr = htonl(INADDR_ANY);	// INADDR_ANY = any local machine address
	chnl_addr.sin_port = htons(local_port);

	if (0 != bind(c_s_fd, (struct sockaddr*) &chnl_addr, addrsize)) {
		printf("Error : Bind Failed. %s \n", strerror(errno));
		return 1;
	}

	if (0 != listen(c_s_fd, 10)) {
		fprintf(stderr, "%s\n", strerror(errno));
		return 1;
	}
	connfd = accept(c_s_fd, (SOCKADDR*)&peer_addr, addrsize);
	if (connfd < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		if (shutdown(c_s_fd, SD_BOTH) != 0) {
			fprintf(stderr, "%s\n", strerror(errno));
		}
		/* die here? loop? */
	}

	/*only for template, change it accordingly - this is for receiving msg from sender and sending back to him */
	char buffer[1332]; int len;
	int temp_bytes_read = recv(connfd, buffer, len, 0);

	/* sending back to SENDER !!!! (need to actually to be receiver) */
	char * transmitted_data; int length; int val;
	if ((val = send(connfd, transmitted_data, length, 0)) < 1) {
		free(transmitted_data);
		return -1;
	}
	/*only for template, change it accordingly - this is for receiving msg from sender and sending back to him */




/*
	//channel - receiver socket
	if ((c_r_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		exit(1);
	}
	struct sockaddr_in cnl_addr;
	memset(&cnl_addr, 0, sizeof(cnl_addr));
	cnl_addr.sin_family = AF_INET;
	cnl_addr.sin_port = htons(recv_port); // htons for endiannes
	cnl_addr.sin_addr.s_addr = inet_addr(rec_ip_add);
	HANDLE thread = CreateThread(NULL, 0, thread_end_listen, &c_r_fd, 0, NULL); */

	while (END_FLAG == 0) {
		receive_frame(chnl_buff, c_s_fd, BUFF_1);
		flip_bits(chnl_buff, err_prob);//manipulate flipping on received bits, change in place in chnl_buff
		send_frame(chnl_buff, c_r_fd, recv_addr, BUFF_1);//send to receiver
	}

	send_frame((char*)write_back_buff, c_s_fd, get_sender_ip(connfd), WB_BUFF * sizeof(int)); //write back to sender


	if (closesocket(c_s_fd) != 0) {
		fprintf(stderr, "%s\n", strerror(errno));
	}

	return 0;
}

struct sockaddr_in get_sender_ip(int sockfd) {
	struct sockaddr_in addr;
	socklen_t addr_size = sizeof(struct sockaddr_in);
	int res = getpeername(sockfd, (struct sockaddr *)&addr, &addr_size);
	return addr;
}

void Init_Winsock() {
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR)
		printf("Error at WSAStartup()\n");
	exit(1);
}


void flip_bits(char chnl_buff_1[], double err_prob) {

	int i, j, flip;
	double r;
	char mask, tmp;

	for (i = 0; i < BUFF_1; i++) {
		tmp = 1;
		mask = 0;
		for (j = 0; j < 8; j++) {
			r = (rand() % 101) / 100; // rand num [0,1]
			flip = r < err_prob; // 0 -not flip, 1- flip
			if (flip) {
				mask = mask | tmp;
			}
			tmp <<= 1; //left shift by 1 position
		}
		chnl_buff_1[i] ^= mask;
	}

	return;
}


DWORD WINAPI thread_end_listen(void *param) {

	int status;
	int r_c_fd = *(int*)(param);

	while (END_FLAG == 0) {
		receive_frame((char*)write_back_buff, r_c_fd, WB_BUFF * sizeof(int));
		status = shutdown(r_c_fd, SD_BOTH);
		if (status) {
			printf("Error while closing socket. \n");
			return 1;
		}
		END_FLAG = 1;
	}
	return 0;
}


void send_frame(char buff[], int fd, struct sockaddr_in to_addr, int bytes_to_write) {
	int totalsent = 0, num_sent = 0;

	while (bytes_to_write > 0 && END_FLAG == 0) {
		num_sent = sendto(fd, buff + totalsent, bytes_to_write, 0, (SOCKADDR*)&to_addr, sizeof(to_addr));
		if (num_sent == -1) {
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		totalsent += num_sent;
		bytes_to_write -= num_sent;
	}
}


void receive_frame(char buff[], int fd, int bytes_to_read) {
	int totalread = 0, bytes_been_read = 0;

	totalread = 0;
	while (bytes_to_read > 0) {
		bytes_been_read = recvfrom(fd, (char*)buff + totalread, bytes_to_read, 0, 0, 0);
		if (bytes_been_read == -1) {
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		totalread -= bytes_been_read;
	}
}