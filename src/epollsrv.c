#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#define MAX_EVENTS 100
#define DEFAULT_PORT 22629
#define LISTENQ 10
#define MAX_CONN 100

static void setnonblocking(int fd) {
	int flag = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

int main() {
	int epfd;
	int lport = DEFAULT_PORT;
	char *rawaddr = "133.11.206.167";
	
	struct epoll_event event;
	
	epfd = epoll_create(1);
	if (epfd < 0) {
		perror("epoll_create");
		exit(EXIT_FAILURE);
	}

	memset(&event, 0, sizeof(event));

	int lsock = socket(AF_INET, SOCK_STREAM, 0);
	if (lsock < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}
	struct sockaddr_in server;
	server.sin_port = htons(lport);
	server.sin_family = AF_INET;
	if (inet_pton(AF_INET, rawaddr, &server.sin_addr) != 1) {
		perror("inet_pton");
		close(lsock);
		exit(EXIT_FAILURE);
	}
	if (bind(lsock, (struct sockaddr *)&server, sizeof(server)) < 0) {
		perror("bind");
		close(lsock);
		exit(EXIT_FAILURE);
	}

	if (listen(lsock, LISTENQ) < 0 ) {
		perror("listen");
		close(lsock);
		exit(EXIT_FAILURE);
	}

	event.events = EPOLLIN;
	event.data.fd = lsock;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, lsock, &event) < 0) {
		perror("epoll_ctl");
		close(lsock);
		exit(EXIT_FAILURE);
	}

	int csocks[MAX_CONN];
	int h;
	for (h = 0; h < MAX_CONN; h++) csocks[h] = -1;

	int nfd;
	while(1) {
		struct epoll_event e[MAX_EVENTS];
		nfd = epoll_wait(epfd, e, MAX_EVENTS, -1);
		int i = 0;
		char *test = "Connected. ";

		for (i = 0; i < nfd; i++) {
			if (e[i].data.fd == lsock) {
				struct sockaddr_in client;
				socklen_t len;
				//int csock = accept(lsock,
				//		(struct sockaddr *)&client,
				//		&len);
				int idx = 0;
				for (idx = 0; idx < MAX_CONN; idx++) {
					if (csocks[idx] < 0) {
						csocks[idx] = accept(lsock,
								(struct sockaddr *)&client,
								&len);
				//		printf("%d,%d\n",idx,csocks[idx]);
						printf("%d connected.\n",csocks[idx]);
						break;
					}
				}
				if (csocks[idx] < 0) {
					perror("accept");
					close(lsock);
					exit(EXIT_FAILURE);
				}
				setnonblocking(csocks[idx]);
				event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
				event.data.fd = csocks[idx];
				if (epoll_ctl(epfd, EPOLL_CTL_ADD, csocks[idx], &event) < 0) {
					perror("epoll_ctl");
					close(lsock);
					close(epfd);
					exit(EXIT_FAILURE);
				}
				write(csocks[idx], test, strlen(test));

				char greet[256];
				sprintf(greet,"Your ID: %d\n",csocks[idx]);
				write(csocks[idx], greet, strlen(greet));
				sprintf(greet,"Member(s) online: ");

				char notify[256];
				sprintf(notify,"%d joined.\n",csocks[idx]);
				int j;
				for(j = 0; j < MAX_CONN; j++) {
					if (csocks[j] > 0) {
						sprintf(greet,"%s, %d",greet,csocks[j]);
						write(csocks[j],notify,strlen(notify));
					}
				}
				sprintf(greet,"%s\n",greet);
				write(csocks[idx],greet,strlen(greet));
			} else {
//				char out[2048];
				if (e[i].events & EPOLLRDHUP) {
					printf("Client %d disconnected.\n",e[i].data.fd);
					if (epoll_ctl(epfd, EPOLL_CTL_DEL, e[i].data.fd, 0) < 0) {
						perror("epoll_ctl");
					}
					int j;
					char notify[256];
					sprintf(notify, "%d left.\n",e[i].data.fd);
					for (j = 0; j < MAX_CONN; j++) {
						if (csocks[j] == e[i].data.fd) {
							csocks[j] = -1;
							if (close(e[i].data.fd) < 0) {
								perror("close");
							}
						}
						else if (csocks[j] > 0) {
							write(csocks[j],notify,strlen(notify));
						}	
					}
				}
				else if (e[i].events & EPOLLIN) {
					printf("%d: ",e[i].data.fd);
					char inbuf[2048];
					memset(&inbuf, 0, sizeof(inbuf));
					if (read(e[i].data.fd, inbuf, sizeof(inbuf)) < 0) {
						perror("read");
						close(lsock);
						close(epfd);
						exit(EXIT_FAILURE);
					}
					printf("%s",inbuf);
					/*
					int j = 0;
					for (j = 0; j < nfd; j++) {
						if (j != i && e[j].data.fd != lsock) {
							if (write(epfd, inbuf, sizeof(inbuf)) < 0) {
								perror("write");
							}
						}
					}*/
					char message[4096];
					sprintf(message, "%d: %s",e[i].data.fd,inbuf);

					int j;
					for (j = 0; j < MAX_CONN; j++) {
				//		printf("%d,%d\t",j,csocks[j]);
						if (csocks[j] == e[i].data.fd) {
							continue;
						}
						else if (csocks[j] < 0) {
							continue;
						}
						else {
						//	printf("trying push..\n");
							//if (write(csocks[j], inbuf, sizeof(inbuf)) < 0) {
							if (write(csocks[j], message, strlen(message)) < 0) {
								perror("write");
							}
						}
					}
				}
				
//				write(e[i].data.fd, out, sizeof(out));
				
			}
		}
	}
}
