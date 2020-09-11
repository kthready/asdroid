#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <common.h>
#include <message.h>

#define USER_LIST "userinfo"

static struct user_item *get_user_list(char *userinfo, u32 *item_num)
{
	FILE *fp;
	long file_size;
	struct user_item *user_list;

	fp = fopen(userinfo, "rb");
	if (!fp) {
		printf("Failed to open %s\n", USER_LIST);
		return NULL;
	}

	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	if ((file_size < 1) ||
	    (file_size % sizeof(struct user_info) != 0)) {
		printf("Failed to get user list size\n");
		goto err;
	}

	fseek(fp, 0, SEEK_SET);

	*item_num = file_size % sizeof(struct user_info);

	user_list = (struct user_info *)malloc(file_size);
	if (!user_list) {
		printf("Failed to malloc mem for user list\n");
		goto err;
	}

	if (file_size != fread(user_list, file_size, 1, fp)) {
		printf("Failed to read from %s\n", USER_LIST);
		goto err;
	}

	fclose(fp);
	return user_list;

err:
	fclose(fp);
	return NULL;
}

static int md5_verification(struct user_info *user, struct user_item *user_item)
{
	char md5[32] = {0};

	md5_encrypt(user->alias, md5, sizeof(user->alias) + sizeof(user->passwd));

	return memcmp(md5, user_item->md5, sizeof(md5));
}

int user_verification(struct user_info *user)
{
	int i;
	u32 item_num;
	struct user_item *user_list;

	user_list = get_user_list(USER_LIST, &item_num);
	if (!user_list)
		return -1;

	for (i = 0; i < item_num; i++)
		if (user_list[i].usrid == user->usrid)
			return md5_verification(user, &user_list[i]);

	if (i == item_num)
		printf("Not found user %s in the user list\n", user->alias);

	return -1;
}

