#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <common.h>
#include <message.h>
#include <crypto.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <user_verify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <openssl/md5.h>

#define USER_DB_FILE	"user.db"

static struct user_item *user_table = NULL;

static struct user_item *get_user_table(void)
{
	int fd;
	off_t file_size;

	if (user_table)
		return user_table;

	fd = open(USER_DB_FILE, O_RDONLY);
	if (fd < 0) {
		printf("%s: Failed to open %s\n", __func__, USER_DB_FILE);
		return NULL;
	}

	file_size = lseek(fd, 0, SEEK_END);
	if ((file_size < 1) ||
	    (file_size % sizeof(struct user_item) != 0)) {
		printf("%s: Failed to get user list size， %ld\n", __func__, file_size);
		goto err;
	}

	lseek(fd, 0, SEEK_SET);

	user_table = (struct user_item *)malloc(file_size + sizeof(struct user_item));
	if (!user_table) {
		printf("%s: Failed to malloc mem for user list\n", __func__);
		goto err;
	}

	memset(user_table, 0, file_size + sizeof(struct user_item));

	if (file_size != read(fd, user_table, file_size)) {
		printf("%s: Failed to read from %s\n", __func__, USER_DB_FILE);
		goto err;
	}

	close(fd);

	return user_table;

err:
	close(fd);
	if (user_table)
		free(user_table);
	return NULL;
}

static int md5_verification(struct user_info *user, struct user_item *user_item)
{
	char md5[16] = {0};

	md5_encrypt(user->alias, md5, sizeof(user->alias) + sizeof(user->passwd));

	return memcmp(md5, user_item->md5, sizeof(md5));
}

int user_verification(struct user_info *user)
{
	struct user_item *entry;

	entry = get_user_table();
	if (!entry)
		return -1;

	while (entry->uinfo.alias[0]) {
		if (entry->uinfo.usrid == user->usrid)
			return md5_verification(user, entry);
		entry++;
	}

	printf("%s: Not found user %s in the user list\n", __func__, user->alias);

	return -1;
}

int get_user_logon_status(struct user_info *user)
{
	struct user_item *entry = NULL;

	entry = get_user_table();
	if (!entry)
		return -1;

	while (entry->uinfo.alias[0]) {
		if (entry->uinfo.usrid == user->usrid)
			return entry->uinfo.logon_status;
		entry++;
	}

	printf("No found user %s\n", user->alias);
	return -1;
}

int set_user_logon_status(struct user_info *user, int status)
{
	struct user_item *entry = NULL;

	entry = get_user_table();
	if (!entry)
		return -1;

	while (entry->uinfo.alias[0]) {
		if (entry->uinfo.usrid == user->usrid) {
			entry->uinfo.logon_status = status;
			return 0;
		}
		entry++;
	}

	printf("No found user %s\n", user->alias);

	return -1;
}

