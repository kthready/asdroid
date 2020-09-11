#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <pthread.h>
#include <fcntl.h>
#include <common.h>
#include <message.h>
#include <thread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define WAITBUF		10
#define RDAS_PORT	ASWR_PORT
#define WRAS_PORT	ASRD_PORT
#define RDCT_PORT	CTWR_PORT
#define WRCT_PORT	CTRD_PORT

static int logon_status;
static struct rwfd asfd;

static void *msg_from_asdroid_to_client(void *arg)
{
	int fd = *(int *)arg;
	struct as_message msg;
	char *buf = NULL;
	static u32 length = 0;
	int err;

	memset(&msg, 0, sizeof(msg));

	while(1) {
		msg.magic = MAGIC;
		msg_send(asfd.rdfd, &msg, sizeof(msg), 0);
		msg.magic = 0;
		err = msg_recv(asfd.rdfd, &msg, sizeof(msg), 0);
		if (err < 0) {
			printf("failed to read message from asdroid\n");
			goto out;
		}
		if (msg.magic != MAGIC)
			goto out;

		if (!buf) {
			buf = malloc(msg.length);
			if (!buf)
				exit(0);
			memset(buf, 0, msg.length);
			length = msg.length;
		} else {
			if (length < msg.length) {
				if (buf)
					free(buf);
				buf = malloc(msg.length);
				if (!buf)
					exit(0);
				memset(buf, 0, msg.length);
				length = msg.length;
			}
		}

		err = msg_recv(asfd.rdfd, buf, msg.length, 0);
		if (err < 0) {
			printf("failed to read video data from asdroid\n");
			goto out;
		}

		if (logon_status == 0) {
			msg_send(fd, &msg, sizeof(msg), 0);
			msg_send(fd, buf, msg.length, 0);
		} else {
			msg.flag = 1;
			msg.length = 0;

			msg_send(fd, &msg, sizeof(msg), 0);
			logon_status = 0;
			break;
		}
	}

out:
	if (buf)
		free(buf);

	pthread_exit(NULL);
}

static void *msg_from_client_to_asdroid(void *arg)
{
	int fd = *(int *)arg;
	struct message *msg;
	struct clt_message *clt_msg;
	size_t msg_len;
	int err;

	msg_len = AES_PADDING_LEN(sizeof(struct message) + sizeof(struct clt_message));
	while(1) {
		err = msg_recv(fd, &clt_msg, sizeof(clt_msg), 0);
		if (err < 0) {
			printf("Failed to read message from client\n");
			exit(0);
		}

		if (asfd.wrfd > 0)
			msg_send(asfd.wrfd, &clt_msg.cmd, sizeof(clt_msg.cmd), 0);
		else
			printf("Cannot connect to asdroid\n");
	}
	pthread_exit(NULL);
}

static int logon_verification(int fd)
{
	struct user_info *user;
	struct message *msg;
	size_t msg_len;
	int err;

	msg_len = AES_PADDING_LEN(sizeof(struct message) + sizeof(struct user_info));

	err = msg_recv(fd, msg, msg_len, 0);
	if (err < 0) {
		printf("Failed to get user info\n");
		return err;
	}

	user = (struct user_info *)msg->priv_data;
	err = user_verification(user);
	free(user);

	return err;
}

static void *client_socket_thread(void *arg)
{
	struct thread_info *tinfo = (struct thread_info *)arg;
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	int sin_size, sockfd, fd;
	int port = tinfo->port;
	pthread_t tid;
	int err;

	if (port < 0) {
		printf("Invalid port number %d\n", port);
		pthread_exit(NULL);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		printf("Socket error:%s\n", strerror(errno));
		pthread_exit(NULL);
	}

	bzero(&server_addr, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);

	err = bind(sockfd, (struct sockaddr *)(&server_addr), sizeof(struct sockaddr));
	if (err < 0) {
		printf("Bind error:%s\n", strerror(errno));
		pthread_exit(NULL);
	}

	err = listen(sockfd, WAITBUF);
	if (err < 0) {
		printf("Listen error: %s\n", strerror(errno));
		pthread_exit(NULL);
	}

	sin_size = sizeof(struct sockaddr_in);

	while(1) {
		fd = accept(sockfd, (struct sockaddr *)(&client_addr), &sin_size);
		if(fd < 0) {
			printf("Accept error:%s\n", strerror(errno));
			continue;
		}

		err = logon_verification(fd);
		if (err < 0)
			continue;

		printf("Server get connection from %s, port %d, fd %d\n",
			inet_ntoa(client_addr.sin_addr), port, fd);

		if (tinfo->thread_work)
			pthread_create(&tid, NULL, tinfo->thread_work, &fd);
	}

	close(sockfd);
	pthread_exit(NULL);
}

static void *asdroid_socket_thread(void *arg)
{
	struct thread_info *tinfo = (struct thread_info *)arg;
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	int sin_size, sockfd, new_fd;
	int port = tinfo->port;
	int err;

	if (port < 0) {
		printf("Invalid port number %d\n", port);
		pthread_exit(NULL);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		printf("Socket error: %s\n", strerror(errno));
		pthread_exit(NULL);
	}

	bzero(&server_addr, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);

	err = bind(sockfd, (struct sockaddr *)(&server_addr), sizeof(struct sockaddr));
	if (err < 0) {
		printf("Bind error:%s\n", strerror(errno));
		pthread_exit(NULL);
	}

	err = listen(sockfd, 1);
	if (err < 0) {
		printf("Listen error: %s\n", strerror(errno));
		pthread_exit(NULL);
	}

	sin_size = sizeof(struct sockaddr_in);

	while(1) {
		new_fd = accept(sockfd, (struct sockaddr *)(&client_addr), &sin_size);
		if(new_fd < 0) {
			printf("Accept error:%s\n", strerror(errno));
			pthread_exit(NULL);
		}

		if (port == RDAS_PORT)
			asfd.rdfd = new_fd;
		else
			asfd.wrfd = new_fd;

		printf("Server get connection from %s, port %d, fd %d\n",
			inet_ntoa(client_addr.sin_addr), port, new_fd);
	}

	if (port == RDAS_PORT)
		asfd.rdfd = -1;
	else
		asfd.wrfd = -1;

	close(new_fd);
	close(sockfd);
	pthread_exit(NULL);
}

int main(void)
{
	int i;
	struct thread_info tinfo[] = {
		{
			.port = RDAS_PORT,
			.thread_work = NULL,
			.thread_socket = asdroid_socket_thread,
		},
		{
			.port = WRAS_PORT,
			.thread_work = NULL,
			.thread_socket = asdroid_socket_thread,
		},
		{
			.port = RDCT_PORT,
			.thread_work = msg_from_client_to_asdroid,
			.thread_socket = client_socket_thread,
		},
		{
			.port = WRCT_PORT,
			.thread_work = msg_from_asdroid_to_client,
			.thread_socket = client_socket_thread,
		},
	};

	for (i = 0; i < ARRAY_SIZE(tinfo); i++) {
		pthread_create(&tinfo[i].tid, NULL, tinfo[i].thread_socket, &tinfo[i]);
	}

	for (i = 0; i < ARRAY_SIZE(tinfo); i++) {
		pthread_join(tinfo[i].tid, NULL);
	}

	return 0;
}
