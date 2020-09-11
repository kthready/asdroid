#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <openssl/aes.h>

typedef unsigned char u8;
typedef unsigned int u32;

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

static u8 *padding(u8 *buf, int size, int *final_size)
{
        u8 *out = NULL;
        int pidding_size = AES_BLOCK_SIZE - (size % AES_BLOCK_SIZE);
        int i;

        *final_size = size + pidding_size;
        out = (u8 *)malloc(size+pidding_size);
	if (!out)
		return out;

	memset(out, 0, pidding_size);
        memcpy(out, buf, size);

        return out;
}

static void aes_encrpyt(u8 *raw_buf, u8 **encrpy_buf, int len)
{
	AES_KEY aes;
	u8 *key = str2hex(AESKEY);
	u8 *iv = str2hex(AESIV);

	assert(key != NULL);
	assert(iv != NULL);

	AES_set_encrypt_key(key, 128, &aes);
	AES_cbc_encrypt(raw_buf, *encrpy_buf, len, &aes, iv, AES_ENCRYPT);

	free(key);
	free(iv);
}

static void aes_decrpyt(u8 *raw_buf, u8 **decrpy_buf, int len)
{
	AES_KEY aes;
	u8 *key = str2hex(AESKEY);
	u8 *iv = str2hex(AESIV);

	assert(key != NULL);
	assert(iv != NULL);

	AES_set_decrypt_key(key, 128, &aes);
	AES_cbc_encrypt(raw_buf, *decrpy_buf, len, &aes, iv, AES_DECRYPT);

	free(key);
	free(iv);
}

int main(int argc, char *argv[])
{
	u32 len, pad_len, i;
	u8 *pad_buf;
	u8 *encrypt_buf;
	u8 *decrypt_buf;

	if (argc < 2) {
		printf("Missing argument\n");
		return 0;
	}

	assert(argv[1] != NULL);

	len = strlen(argv[1]);
	pad_buf = padding(argv[1], len, &pad_len);
	if (!pad_buf) {
		printf("padding failed\n");
		return -1;
	}

	encrypt_buf = (u8 *)malloc(pad_len);
	if (!encrypt_buf) {
		printf("alloc memory for encrypt_buf failed\n");
		return -1;
	}

	aes_encrpyt(pad_buf, &encrypt_buf, pad_len);
	printf("encrypt data %d\n", pad_len);
	for (i = 0; i < pad_len; i++) {
		printf("%02x", encrypt_buf[i]);
	}
	printf("\n");
	decrypt_buf = (u8 *)malloc(pad_len);
	if (!decrypt_buf) {
		printf("alloc memory for decrypt_buf failed\n");
		return -1;
	}

	aes_decrpyt(encrypt_buf, &decrypt_buf, pad_len);
	printf("%s\n", decrypt_buf);

	free(encrypt_buf);
	free(decrypt_buf);
	free(pad_buf);

	return 0;
}

