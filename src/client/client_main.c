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
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define USER_ID		0x8D00
#define USER_NAME	"kthread"
#define PASSWD		"kthread2020"

static struct user_info user;

static void client_read(int fd)
{
	struct message *prev_msg = NULL;
	struct message *vd_msg = NULL;
	struct as_message *as_msg = NULL;
	static u32 length = 0;
	size_t prev_msg_len;
	size_t vd_msg_len;
	int err;

	prev_msg_len = AES_PADDING_LEN(sizeof(struct message) + sizeof(struct as_message));
	prev_msg = (struct message *)malloc(prev_msg_len);
	if (!prev_msg)
		pthread_exit(NULL);

	memset(prev_msg, 0, prev_msg_len);

	while(1) {
		err = msg_recv(fd, &prev_msg, prev_msg_len, 0);
		if (err < 0) {
			printf("%s: Failed to read message from asdroid\n", __func__);
			goto exit;
		}

		as_msg = (struct as_message *)prev_msg->priv_data;
		vd_msg_len = as_msg->length;
		if (!vd_msg || length < vd_msg_len) {
			if (vd_msg)
				free(vd_msg);

			vd_msg = (struct message *)malloc(vd_msg_len);
			if (!vd_msg)
				goto exit;

			memset(vd_msg, 0, vd_msg_len);
			length = vd_msg_len;
		}

		err = msg_recv(fd, &vd_msg, vd_msg_len, 0);
		if (err < 0) {
			printf("%s: Failed to read video from asdroid\n", __func__);
			goto exit;
		}

		printf("%s: length: %d, %s\n", __func__, vd_msg->header.msg_length, vd_msg->priv_data);
	}

exit:
	if (vd_msg)
		free(vd_msg);
	if (prev_msg)
		free(prev_msg);

	pthread_exit(NULL);
}

static void client_write(int fd)
{
	struct message *msg;
	struct clt_message *clt_msg;
	size_t msg_len;
	int i = 0, err;

	msg_len = AES_PADDING_LEN(sizeof(struct message) + sizeof(struct clt_message));
	msg = (struct message *)malloc(msg_len);
	if (!msg)
		pthread_exit(NULL);

	memset(msg, 0, msg_len);
	msg->header.magic_num = MAGIC;
	msg->header.msg_length = sizeof(struct clt_message);
	clt_msg = (struct clt_message *)msg->priv_data;
	memcpy(&clt_msg->user, &user, sizeof(user));

	while(1) {
		clt_msg->cmd = random()%CMD_MOTOR_MAX;
		printf("%s: user_name: %s  userid: 0x%x  cmd: %d\n", __func__,
			clt_msg->user.alias, clt_msg->user.usrid, clt_msg->cmd);
		i++;
		if (i >= 5) {
			clt_msg->user.logon_status = 0;
			err = msg_send(fd, msg, msg_len, 0);
			if (err < 0) {
				printf("Failed to send message to server\n");
			}
			sleep(1);
			break;
		}
		err = msg_send(fd, msg, msg_len, 0);
		if (err < 0) {
			printf("Failed to send message to server\n");
		}
		sleep(3);
	}
	pthread_exit(NULL);
}

static int client_logon(int fd)
{
	struct message *clt_msg;
	struct message *recv_msg;
	size_t msg_len;
	int err;

	msg_len = AES_PADDING_LEN(sizeof(struct message) + sizeof(struct user_info));
	clt_msg = (struct message *)malloc(msg_len);
	if (!clt_msg) {
		printf("%s: Failed to alloc memory for message\n", __func__);
		return -1;
	}

	memset(clt_msg, 0, msg_len);
	clt_msg->header.magic_num = MAGIC;
	clt_msg->header.msg_length = sizeof(struct user_info);
	memcpy(clt_msg->priv_data, &user, sizeof(user));

	err = msg_send(fd, clt_msg, msg_len, 0);
	if (err < 0) {
		printf("%s: Failed to msg_send user info to server\n", __func__);
		goto fail;
	}

	msg_len = AES_PADDING_LEN(sizeof(*recv_msg) + sizeof(u32));
	recv_msg = (struct message *)malloc(msg_len);
	if (!recv_msg) {
		err = -1;
		goto fail;
	}
	memset(recv_msg, 0, sizeof(*recv_msg));
	err = msg_recv(fd, &recv_msg, msg_len, 0);
	if (err < 0) {
		printf("%s: Failed to get authentication result\n", __func__);
		goto fail;
	}

	if (*((u32 *)recv_msg->priv_data) != 1) {
		printf("%s: [%x] %s: Authentication failed\n", __func__, user.usrid, user.alias);
		goto fail;
	}

	user.logon_status = 1;
	printf("%s: client logon successfully\n", __func__);
	err = 0;

fail:
	if (clt_msg)
		free(clt_msg);
	if (recv_msg)
		free(recv_msg);

	return err;
}

static void *client_socket_thread(void *arg)
{
	int fd, err;
	struct sockaddr_in serv_addr;
	struct thread_info *tinfo = (struct thread_info *)arg;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		printf("%s: failed to create socket: %s\n", __func__, strerror(errno));
		pthread_exit(NULL);
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(tinfo->port);
	inet_pton(AF_INET, tinfo->data, &serv_addr.sin_addr.s_addr);

	err = connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if (err < 0) {
		printf("%s: failed to connect to server: %s\n", __func__, strerror(errno));
		pthread_exit(NULL);
	}

	if (tinfo->port == LOGON_PORT && user.logon_status != 1) {
		err = client_logon(fd);
		if (err < 0)
			goto exit;
	}

	if (tinfo->port == CTRD_PORT)
		client_read(fd);

	if (tinfo->port == CTWR_PORT)
		client_write(fd);

exit:
	close(fd);
	pthread_exit(NULL);
}

static int setup_userinfo(struct user_info *user, const char *name, const char *passwd, u32 userid)
{
	if (!user || !passwd || !name)
		return -1;

	if (name) {
		if (strlen(name) > sizeof(user->alias) - 1)
			return -1;
		strncpy(user->alias, name, strlen(name));
	}

	if (passwd) {
		if (strlen(passwd) > sizeof(user->passwd) - 1)
			return -1;
		strncpy(user->passwd, passwd, strlen(passwd));
	}

	user->usrid = userid;

	return 0;
}

int main(int argc, char *argv[])
{
	int i;
	struct thread_info tinfo[] = {
		{
			.port = LOGON_PORT,
			.thread_socket = client_socket_thread,
		},
		{
			.port = CTRD_PORT,
			.thread_socket = client_socket_thread,
		},
		{
			.port = CTWR_PORT,
			.thread_socket = client_socket_thread,
		},
	};

	if (argc != 2) {
		printf("%s: Usage: %s <ip addr>\n", __func__, argv[0]);
		return 0;
	}

	if (!argv[1]) {
		printf("Error: invalid ip address\n");
		return -1;
	}

	memset(&user, 0, sizeof(user));

	if (setup_userinfo(&user, USER_NAME, PASSWD, USER_ID)) {
		printf("Failed to setup user %s info\n", USER_NAME);
		return -1;
	}

	tinfo[0].data = argv[1];
	pthread_create(&tinfo[0].tid, NULL, tinfo[0].thread_socket, &tinfo[0]);
	pthread_join(tinfo[0].tid, NULL);

	if (user.logon_status != 1)
		return 0;

	for (i = 1; i < ARRAY_SIZE(tinfo); i++) {
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
