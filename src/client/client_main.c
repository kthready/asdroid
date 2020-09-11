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

#define USER_NAME "kthread"
#define USER_ID 0xFEADBACD
#define PASSWD "kthread2020"

static struct user_info user;

static void client_read(int fd)
{
	struct message *prev_msg = NULL;
	struct message *buf = NULL;
	strcut as_message *as_msg;
	static u32 length = 0;
	size_t prev_msg_len;
	size_t suf_msg_len;
	int err;

	prev_msg_len = AES_PADDING_LEN(sizeof(struct message) + sizeof(struct as_message));
	prev_msg = (struct message *)malloc(prev_msg_len);
	if (!prev_msg)
		pthread_exit(NULL);

	memset(prev_msg, 0, prev_msg_len);

	while(1) {
		err = msg_recv(fd, prev_msg, prev_msg_len, 0);
		if (err < 0) {
			printf("Failed to read message from asdroid\n");
			goto exit;
		}

		as_msg = (struct as_message *)prev_msg->priv_data;
		suf_msg_len = sizeof(struct message) + as_msg->length;
		if (length < suf_msg_len) {
			if (buf)
				free(buf);

			buf = malloc(suf_msg_len);
			if (!buf)
				goto exit;

			memset(buf, 0, suf_msg_len);
			length = suf_msg_len;
		}

		err = msg_recv(fd, buf, suf_msg_len, 0);
		if (err < 0) {
			printf("Failed to read video from asdroid\n");
			goto exit;
		}

		printf("%s\n", buf->priv_data);
	}

exit:
	if (buf)
		free(buf);
	if (prev_msg)
		free(prev_msg);

	pthread_exit(NULL);
}

static void client_write(int fd)
{
	struct message *msg;
	struct clt_message *clt_msg;
	size_t msg_len;
	int i = 0;

	msg_len = sizeof(struct message) + sizeof(struct clt_message);
	msg = (struct message *)malloc(msg_len);
	if (!msg)
		pthread_exit(NULL);

	memset(msg, 0, msg_len);
	msg->header.magic_num = MAGIC;
	msg->header.msg_length = msg_len;
	clt_msg = (struct clt_message *)msg->priv_data;
	memcpy(clt_msg->user, &user, sizeof(user));

	while(1) {
		clt_msg->cmd = random()%CMD_MOTOR_MAX;
		printf("user_name: %s  userid: 0x%x  cmd: %d\n",
			clt_msg->user.alias, clt_msg->user.usrid, clt_msg->cmd);
		i++;
		if (i >= 5) {
			clt_msg->user.logon_status = 0;
			msg_send(fd, &msg, msg_len, 0);
			sleep(1);
			break;
		}
		msg_send(fd, &msg, msg_len, 0);
		sleep(3);
	}
	pthread_exit(NULL);
}

static int client_authentication(int fd)
{
	struct message *clt_msg;
	struct message *recv_msg;
	size_t msg_len;
	int err;

	msg_len = sizeof(struct message) + sizeof(struct user_info);
	clt_msg = (struct message *)malloc(msg_len);
	if (clt_msg) {
		printf("Failed to alloc memory for message\n");
		return -1;
	}

	memset(clt_msg, 0, msg_len);
	clt_msg->header.magic_num = MAGIC;
	clt_msg->header.msg_length = msg_len;
	memcpy(clt_msg->priv_data, &user, sizeof(user));

	err = msg_send(fd, &clt_msg, msg_len, 0);
	if (err < 0) {
		printf("Failed to msg_send user info to server\n");
		goto fail;
	}

	msg_len = AES_PADDING_LEN(sizeof(*recv_msg) + sizeof(u32));
	err = msg_recv(fd, recv_msg, msg_len, 0);
	if (err < 0) {
		printf("Failed to get authentication result\n");
		goto fail;
	}

	if (*((u32 *)recv_msg->priv_data) != 1) {
		printf("[%x] %s: Authentication failed\n", user.usrid, user.alias);
		goto fail;
	}

	user.logon_status = 1;

	return 0;

fail:
	if (clt_msg)
		free(clt_msg);
	if (recv_msg)
		free(recv_msg);

	return -1;
}

static void *client_socket_thread(void *arg)
{
	int fd, err;
	struct sockaddr_in serv_addr;
	struct thread_info *tinfo = (struct thread_info *)arg;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		printf("failed to create socket: %s\n", strerror(errno));
		pthread_exit(NULL);
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(tinfo->port);
	inet_pton(AF_INET, tinfo->data, &serv_addr.sin_addr.s_addr);

	err = connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if (err < 0) {
		printf("failed to connect to server: %s\n", strerror(errno));
		pthread_exit(NULL);
	}

	if (tinfo->port == CTRD_PORT)
		client_read(fd);

	if (tinfo->port == CTWR_PORT)
		client_write(fd);

	close(fd);
	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	int i;
	struct thread_info tinfo[] = {
		{
			.port = CTRD_PORT,
			.thread_socket = client_socket_thread,
		},
		{
			.port = CTWR_PORT,
			.thread_socket = client_socket_thread,
		},
	};

	if (argc < 2) {
		printf("Usage: %s <ip addr>\n", argv[0]);
		return 0;
	}

	memset(&user, 0, sizeof(user));
	user.usrid = USER_ID;
	memcpy(user.alias, USER_NAME, sizeof(user.alias) - 1);
	memcpy(user.passwd, PASSWD, sizeof(user.passwd) - 1);

	for (i = 0; i < ARRAY_SIZE(tinfo); i++) {
		tinfo[i].data = argv[1];
		if (tinfo[i].thread_socket)
			pthread_create(&tinfo[i].tid, NULL, tinfo[i].thread_socket, &tinfo[i]);
	}

	for (i = 0; i < ARRAY_SIZE(tinfo); i++) {
		if (tinfo[i].thread_socket)
			pthread_join(tinfo[i].tid, NULL);
	}

	return 0;
}
