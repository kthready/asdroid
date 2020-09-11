#ifndef USER_VERIFY_H
#define USER_VERIFY_H

#include <message.h>

struct user_item {
	u32 usrid;
	u32 reserved;
	char alias[16];
	char md5[32];
};

int user_verification(struct user_info *user);

#endif
