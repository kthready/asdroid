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
	u8 *enc_buf;
	int err;

	enc_buf = (u8 *)malloc(AES_PADDING_LEN(len));
	if (!enc_buf)
		return -1;

	err = msg_aes_encrypt(buf, &enc_buf, len);
	if (err < 0)
		return err;

	err = send(sockfd, enc_buf, AES_PADDING_LEN(len), flags);

	free(enc_buf);

	return err;
}

int msg_recv(int sockfd, struct message **buf, size_t len, int flags)
{
	u8 *tmp;
	int err;
	struct message *ptr;

	tmp = malloc(len);
	if (!tmp)
		return -1;

	memset(tmp, 0, len);
	err = recv(sockfd, tmp, len, flags);
	if (err < 0)
		goto fail;

	if (msg_aes_decrypt(tmp, (u8 **)buf, len) < 0)
		err = -1;

	ptr = *buf;

	if (ptr->header.magic_num != MAGIC)
		err = -1;

fail:
	free(tmp);
	return err;
}

