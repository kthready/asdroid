#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <common.h>
#include <crypto.h>
#include <openssl/aes.h>
#include <openssl/md5.h>

#define AESKEY "8cc72b05705d5c46f412af8cbed55aad"
#define AESIV "667b02a85c61c786def4521b060265e8"

static u8 *str2hex(u8 *str)
{
	u8 *hex = NULL;
	u32 str_len = strlen(str);
	u32 i = 0;

	assert((str_len % 2) == 0);

	hex = (u8 *)malloc(str_len / 2);
	if (!hex)
		return hex;

	for (i = 0; i < str_len; i += 2 )
		sscanf(str + i,"%2hhx", &hex[i/2]);

	return hex;
}

static u8 *padding(const u8 *buf, int size)
{
	u8 *out = NULL;
	int pidding_size = 0;
	int i;

	if (size % AES_BLOCK_SIZE)
		pidding_size = AES_BLOCK_SIZE - (size % AES_BLOCK_SIZE);

	out = (u8 *)malloc(size + pidding_size);
	if (!out)
		return out;

	if (pidding_size != 0)
		memset(out + size, 0, pidding_size);

	memcpy(out, buf, size);

	return out;
}

void aes_encrypt(const u8 *raw_buf, u8 **encryp_buf, int len)
{
	AES_KEY aes;
	u8 *key = str2hex(AESKEY);
	u8 *iv = str2hex(AESIV);

	assert(key != NULL);
	assert(iv != NULL);

	AES_set_encrypt_key(key, 128, &aes);
	AES_cbc_encrypt(raw_buf, *encryp_buf, len, &aes, iv, AES_ENCRYPT);

	free(key);
	free(iv);
}

void aes_decrypt(u8 *raw_buf, u8 **decryp_buf, int len)
{
	AES_KEY aes;
	u8 *key = str2hex(AESKEY);
	u8 *iv = str2hex(AESIV);

	assert(key != NULL);
	assert(iv != NULL);

	AES_set_decrypt_key(key, 128, &aes);
	AES_cbc_encrypt(raw_buf, *decryp_buf, len, &aes, iv, AES_DECRYPT);

	free(key);
	free(iv);
}

void md5_encrypt(u8 *raw_buf, u8 *enc_buf, u32 raw_len)
{
	MD5_CTX md5;

	assert(raw_buf != NULL);
	assert(enc_buf != NULL);

	MD5_Init(&md5);
	MD5_Update(&md5, raw_buf, raw_len);
	MD5_Final(enc_buf, &md5);    
}

int msg_aes_encrypt(const u8 *raw_buf, u8 **encryp_buf, int len)
{
	u8 *pad_buf = NULL;

	pad_buf = padding(raw_buf, len);
	if (!pad_buf) {
		printf("Padding failed\n");
		return -1;
	}

	aes_encrypt(pad_buf, encryp_buf, AES_PADDING_LEN(len));

	if (pad_buf)
		free(pad_buf);

	return 0;
}

int msg_aes_decrypt(u8 *raw_buf, u8 **decryp_buf, int len)
{
	aes_decrypt(raw_buf, decryp_buf, len);

	return 0;
}
