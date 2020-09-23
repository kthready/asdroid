#ifndef ASSISTANT_MESSAGE_H
#define ASSISTANT_MESSAGE_H

#include <user_verify.h>

#define CTRD_PORT	8000
#define CTWR_PORT	8001
#define ASRD_PORT	8002
#define ASWR_PORT	8003
#define LOGON_PORT	8004

#define MAGIC		0xacedfead

enum as_command {
	CMD_LOGON,
	CMD_KICK,
	CMD_MOTOR_LF,
	CMD_MOTOR_RT,
	CMD_MOTOR_FW,
	CMD_MOTOR_BK,
	CMD_MOTOR_MAX
};

enum msg_flag {
	MSG_START,
	MSG_END,
	MSG_ERR,
	CMD_DENY
};

struct rwfd {
	int rdfd;
	int wrfd;
};

struct as_message {
	u32 length;
	int flag;
};

struct clt_message {
	enum as_command cmd;
	struct user_info user;
	int flag;
};

struct msg_header {
	u32 magic_num;
	u32 msg_length;
};

struct message {
	struct msg_header header;
	char priv_data[0];
};

int msg_send(int sockfd, const void *buf, size_t len, int flags);
int msg_recv(int sockfd, void **buf, size_t len, int flags);

#endif
