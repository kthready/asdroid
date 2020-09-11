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
#include <crypto.h>
#include <thread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

int msg_send(int sockfd, const void *buf, size_t len, int flags)
{
	u8 **enc_buf;
	int err;

	err = msg_aes_encrypt(buf, enc_buf, len);
	if (err < 0)
		return err;

	err = send(sockfd, *enc_buf, AES_PADDING_LEN(len), flags);
	sync();
	free(*enc_buf);

	return err;
}

int msg_recv(int sockfd, void *buf, size_t len, int flags)
{
	u8 *tmp;
	int err;

	tmp = malloc(len);
	if (!tmp)
		return -1;

	memset(tmp, 0, len);
	err = recv(sockfd, tmp, len, flags);
	if (err < 0)
		goto fail;

	if (msg_aes_decrypt(tmp, &buf, len) < 0)
		err = -1;

	if ((struct message *)buf->header.magic_num != MAGIC)
		err = -1;

fail:
	free(tmp);
	return err;
}

