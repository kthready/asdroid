#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <crypto.h>
#include <message.h>
#include <sys/types.h>
#include <openssl/md5.h>


#define USER_ID_BASE	0x8D00
#define USER_DB_FILE	"user.db"

int main(int argc, char *argv[])
{
	struct user_item item;
	struct user_item *user_list = NULL;
	char *name, *passwd;
	u32 name_len, passwd_len;
	int fd = 0, ret, i = 0;

	if (argc < 3) {
		printf("Missing arguments\n");
		printf("Usage:\n");
		printf("./adduser <user_name> <passwd>");
		return -1;
	}

	name = argv[1];
	passwd = argv[2];

	name_len = strlen(name);
	passwd_len = strlen(passwd);

	if (name_len == 0 || name_len > sizeof(item.uinfo.alias) - 1) {
		printf("Error: name length is too long\n");
		ret = -1;
	}
	if (passwd_len == 0 || passwd_len > sizeof(item.uinfo.passwd) - 1) {
		printf("Error: passwd length is too long\n");
		ret = -1;
	}
	if (ret < 0)
		return ret;

	user_list = malloc(sizeof(*user_list));
	if (!user_list) {
		printf("Failed to allocate memory\n");
		return -1;
	}

	fd = open(USER_DB_FILE, O_RDWR | O_APPEND | O_CREAT, 0644);
	if (fd < 0) {
		printf("Failed to open %s\n", USER_DB_FILE);
		ret = fd;
		goto exit;
	}

	do {
		ret = read(fd, user_list, sizeof(*user_list));
		if (ret < 0) {
			printf("Failed to read from %s\n", USER_DB_FILE);
			goto exit;
		}
		if (ret == 0)
			break;
		if (!memcmp(user_list->uinfo.alias, name, sizeof(user_list->uinfo.alias)))
			break;
		i++;
	} while(ret == sizeof(*user_list));

	if (ret == 0) {
		printf("Not found user %s in the user data base, append...\n", name);
	} else if (ret < sizeof(*user_list)) {
		printf("User data base is broken\n");
		ret = -1;
		goto exit;
	} else if (ret == sizeof(*user_list)) {
		printf("User %s already in the data base\n", name);
		printf("User ID : %u\n", user_list->uinfo.usrid);
		ret = 0;
		goto exit;
	}

	memset(&item, 0, sizeof(item));
	memcpy(item.uinfo.alias, name, name_len);
	memcpy(item.uinfo.passwd, passwd, passwd_len);
	item.uinfo.usrid = USER_ID_BASE + i;
	md5_encrypt(item.uinfo.alias, item.md5, sizeof(item.uinfo.alias) + sizeof(item.uinfo.passwd));

	ret = write(fd, &item, sizeof(item));
	if (ret != sizeof(item)) {
		printf("Failed to write user %s info to %s\n", name, USER_DB_FILE);
		ret = -1;
		goto exit;
	}

	ret = 0;

exit:
	if (user_list)
		free(user_list);
	if (fd > 0)
		close(fd);

	return ret;
}

