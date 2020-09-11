#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <common.h>
#include <openssl/aes.h>
#include <openssl/md5.h>

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

static u8 *padding(u8 *buf, int size)
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

void aes_encrypt(u8 *raw_buf, u8 **encryp_buf, int len)
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

	MD5Init(&md5);
	MD5Update(&md5, raw_buf, raw_len);
	MD5Final(&md5, enc_buf);    
}

int msg_aes_encrypt(u8 *raw_buf, u8 **encryp_buf, int len)
{
	int padlen;
	u8 *pad_buf = NULL;

	pad_buf = padding(raw_buf, len);
	if (!pad_buf) {
		printf("Padding failed\n");
		return -1;
	}

	padlen = AES_PADDING_LEN(len);

	*encryp_buf = (u8 *)malloc(padlen);
	if (!enc_buf) {
		printf("Failed to alloc memory for encrypt buffer\n");
		return -1;
	}

	aes_encrypt(raw_buf, &encryp_buf, padlen);

	return 0;
}

int msg_aes_decrypt(u8 *raw_buf, u8 **decryp_buf, int len)
{
	if (!*decryp_buf)
		*decryp_buf = (u8 *)malloc(len);

	if (!*decryp_buf) {
		printf("Failed to alloc memory for encrypt buffer\n");
		return -1;
	}

	aes_decrpyt(raw_buf, decryp_buf, len);

	return 0;
}
