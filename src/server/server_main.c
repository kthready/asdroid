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
#include <crypto.h>
#include <user_verify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <openssl/md5.h>

#define WAITBUF		10
#define RDAS_PORT	ASWR_PORT
#define WRAS_PORT	ASRD_PORT
#define RDCT_PORT	CTWR_PORT
#define WRCT_PORT	CTRD_PORT

#define VIDEO_FILE_NAME		"video"

static struct rwfd asfd;
static struct rwfd cltfd;
//static struct user_list online_user_list;

static int save_video_to_file(struct message *vd_msg)
{
	int fd;

	fd = open(VIDEO_FILE_NAME, O_RDWR | O_APPEND | O_CREAT, 0666);
	if (fd < 0) {
		printf("Failed to open %s, %s\n", VIDEO_FILE_NAME, strerror(errno));
		return -1;
	}

	write(fd, vd_msg->priv_data, vd_msg->header.msg_length);
	close(fd);

	return 0;
}

static void *msg_from_asdroid_to_client(void *arg)
{
	int fd = *(int *)arg;
	struct message *pre_msg = NULL;
	struct message *vd_msg = NULL;
	struct message *kick_msg = NULL;
	struct as_message *as_msg;
	size_t pre_msg_len, vd_msg_len, kick_msg_len;
	static u32 length = 0;
	int err;

	kick_msg_len = AES_PADDING_LEN(sizeof(struct message) + sizeof(struct as_message));
	pre_msg_len = kick_msg_len;

	pre_msg = (struct message *)malloc(pre_msg_len);
	if (!pre_msg)
		goto out;
	kick_msg = (struct message *)malloc(kick_msg_len);
	if (!kick_msg)
		goto out;

	memset(pre_msg, 0, pre_msg_len);
	memset(kick_msg, 0, kick_msg_len);
	kick_msg->header.magic_num = MAGIC;
	kick_msg->header.msg_length = sizeof(struct as_message);
	as_msg = (struct as_message *)kick_msg->priv_data;
	as_msg->flag = CMD_KICK;

	while(1) {
		printf("%s\n", __func__);
		err = msg_send(fd, kick_msg, kick_msg_len, 0);
		if (err < 0) {
			printf("%s: failed to send kick message to asdroid\n", __func__);
			goto out;
		}

		err = msg_recv(fd, &pre_msg, pre_msg_len, 0);
		if (err < 0) {
			printf("%s: failed to read message from asdroid\n", __func__);
			goto out;
		}

		as_msg = (struct as_message *)pre_msg->priv_data;
		vd_msg_len = as_msg->length;

		if (!vd_msg) {
			vd_msg = (struct message *)malloc(vd_msg_len);
			if (!vd_msg)
				break;
			memset(vd_msg, 0, vd_msg_len);
			length = vd_msg_len;
		} else {
			if (length < vd_msg_len) {
				if (vd_msg)
					free(vd_msg);
				vd_msg = (struct message *)malloc(vd_msg_len);
				if (!vd_msg)
					break;
				memset(vd_msg, 0, vd_msg_len);
				length = vd_msg_len;
			}
		}

		err = msg_recv(fd, &vd_msg, vd_msg_len, 0);
		if (err < 0) {
			printf("%s: failed to read video data from asdroid\n", __func__);
			goto out;
		}

		save_video_to_file(vd_msg);
		if (cltfd.wrfd == 0)
			continue;

		err = msg_send(cltfd.wrfd, pre_msg, pre_msg_len, 0);
		if (err < 0) {
			printf("%s: failed to send prev data to client\n", __func__);
			goto out;
		}

		err = msg_send(cltfd.wrfd, vd_msg, vd_msg_len, 0);
		if (err < 0) {
			printf("%s: failed to send video data to client\n", __func__);
			goto out;
		}
	}

out:
	if (kick_msg)
		free(kick_msg);
	if (vd_msg)
		free(vd_msg);

	pthread_exit(NULL);
}

static void *msg_from_client_to_asdroid(void *arg)
{
	int fd = *(int *)arg;
	struct message *msg = NULL;
	struct clt_message *clt_msg = NULL;
	size_t msg_len;
	int err;

	msg_len = AES_PADDING_LEN(sizeof(struct message) + sizeof(struct clt_message));
	msg = (struct message *)malloc(msg_len);
	if (!msg)
		pthread_exit(NULL);

	while(1) {
		printf("%s\n", __func__);
		if (cltfd.rdfd == 0) {
			sleep(1);
			continue;
		}
		err = msg_recv(cltfd.rdfd, &msg, msg_len, 0);
		if (err < 0) {
			printf("%s: Failed to read message from client\n", __func__);
			continue;
		}
		clt_msg = msg->priv_data;
		if (clt_msg->user.logon_status == 0) {
			cltfd.wrfd = 0;
			cltfd.rdfd = 0;
			continue;
		}

		err = msg_send(fd, msg, msg_len, 0);
		if (err < 0)
			printf("%s: Failed to send message to asdroid\n", __func__);
	}

	if (msg)
		free(msg);

	pthread_exit(NULL);
}

static void *logon_verification(void *arg)
{
	struct user_info *user;
	struct message *msg;
	size_t msg_len;
	int fd = *(int *)arg;
	int err = -1;

	msg_len = AES_PADDING_LEN(sizeof(struct message) + sizeof(struct user_info));
	msg = (struct message *)malloc(msg_len);
	if (!msg)
		goto exit;

	memset(msg, 0, msg_len);
	err = msg_recv(fd, &msg, msg_len, 0);
	if (err < 0) {
		printf("%s: Failed to get user info\n", __func__);
		goto exit;
	}

	user = (struct user_info *)msg->priv_data;
	err = user_verification(user);
	if (err < 0)
		printf("%s: user verification failed\n", __func__);
	free(msg);

	msg_len = AES_PADDING_LEN(sizeof(struct message) + sizeof(u32));
	msg = (struct message *)malloc(msg_len);
	if (!msg)
		goto exit;
	memset(msg, 0, msg_len);

	msg->header.magic_num = MAGIC;
	msg->header.msg_length = sizeof(u32);
	*(u32 *)msg->priv_data = err < 0 ? 0 : 1;
	err = msg_send(fd, msg, msg_len, 0);
	if (err < 0)
		printf("%s: Failed to send message to client\n", __func__);

exit:
	set_user_logon_status(user, err < 0 ? 0 : 1);
	pthread_exit(NULL);
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
		printf("%s: Invalid port number %d\n", __func__, port);
		pthread_exit(NULL);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		printf("%s: Socket error:%s\n", __func__, strerror(errno));
		pthread_exit(NULL);
	}

	bzero(&server_addr, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);

	err = bind(sockfd, (struct sockaddr *)(&server_addr), sizeof(struct sockaddr));
	if (err < 0) {
		printf("%s: Bind error:%s\n", __func__, strerror(errno));
		pthread_exit(NULL);
	}

	err = listen(sockfd, WAITBUF);
	if (err < 0) {
		printf("%s: Listen error: %s\n", __func__, strerror(errno));
		pthread_exit(NULL);
	}

	sin_size = sizeof(struct sockaddr_in);

	while(1) {
		fd = accept(sockfd, (struct sockaddr *)(&client_addr), &sin_size);
		if(fd < 0) {
			printf("%s: Accept error:%s\n", __func__, strerror(errno));
			continue;
		}

		printf("%s: Server get connection from %s, port %d, fd %d\n", __func__,
			inet_ntoa(client_addr.sin_addr), port, fd);

		if (port == RDCT_PORT)
			cltfd.rdfd = fd;
		else if (port == WRCT_PORT)
			cltfd.wrfd = fd;
		else if (port == LOGON_PORT)
			pthread_create(&tid, NULL, tinfo->thread_work, &fd);
		else
			close(fd);
	}

	close(sockfd);
	pthread_exit(NULL);
}

static void *asdroid_socket_thread(void *arg)
{
	struct thread_info *tinfo = (struct thread_info *)arg;
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	int sin_size, sockfd, fd;
	int port = tinfo->port;
	int err;

	if (port < 0) {
		printf("%s: Invalid port number %d\n", __func__, port);
		pthread_exit(NULL);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		printf("%s: Socket error: %s\n", __func__, strerror(errno));
		pthread_exit(NULL);
	}

	bzero(&server_addr, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);

	err = bind(sockfd, (struct sockaddr *)(&server_addr), sizeof(struct sockaddr));
	if (err < 0) {
		printf("%s: Bind error:%s\n", __func__, strerror(errno));
		pthread_exit(NULL);
	}

	err = listen(sockfd, 1);
	if (err < 0) {
		printf("%s: Listen error: %s\n", __func__, strerror(errno));
		pthread_exit(NULL);
	}

	sin_size = sizeof(struct sockaddr_in);

	fd = accept(sockfd, (struct sockaddr *)(&client_addr), &sin_size);
	if(fd < 0) {
		printf("%s: Accept error:%s\n", __func__, strerror(errno));
		pthread_exit(NULL);
	}

	printf("%s: Server get connection from %s, port %d, fd %d\n", __func__,
		inet_ntoa(client_addr.sin_addr), port, fd);

	if (tinfo->thread_work)
		tinfo->thread_work(&fd);

	close(sockfd);
	pthread_exit(NULL);
}

int main(void)
{
	int i;
	struct thread_info tinfo[] = {
		{
			.port = LOGON_PORT,
			.thread_work = logon_verification,
			.thread_socket = client_socket_thread,
		},
		{
			.port = RDAS_PORT,
			.thread_work = msg_from_asdroid_to_client,
			.thread_socket = asdroid_socket_thread,
		},
		{
			.port = WRAS_PORT,
			.thread_work = msg_from_client_to_asdroid,
			.thread_socket = asdroid_socket_thread,
		},
		{
			.port = RDCT_PORT,
			.thread_work = NULL,
			.thread_socket = client_socket_thread,
		},
		{
			.port = WRCT_PORT,
			.thread_work = NULL,
			.thread_socket = client_socket_thread,
		},
	};

	memset(&cltfd, 0, sizeof(cltfd));

	for (i = 0; i < ARRAY_SIZE(tinfo); i++) {
		pthread_create(&tinfo[i].tid, NULL, tinfo[i].thread_socket, &tinfo[i]);
	}

	for (i = 0; i < ARRAY_SIZE(tinfo); i++) {
		pthread_join(tinfo[i].tid, NULL);
	}

	return 0;
}
