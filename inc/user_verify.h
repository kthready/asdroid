#ifndef USER_VERIFY_H
#define USER_VERIFY_H

struct user_info {
	u32 usrid;
	u32 logon_status;
	char alias[16];
	char passwd[16];
};

struct user_item {
	struct user_info uinfo;
	char md5[16];
};

int user_verification(struct user_info *user);
int get_user_logon_status(struct user_info *user);
int set_user_logon_status(struct user_info *user, int status);

#endif
