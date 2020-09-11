#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#define OPENSSLKEY "test.key"
#define PUBLICKEY  "test_pub.key"
#define BUFFSIZE   1024

typedef unsigned char u8;

static u8 *encrypt(u8 *str, u8 *path_key)
{
	u8 *p_en = NULL;
	RSA *p_rsa = NULL;
	FILE *file = NULL;
	int plen = 0;
	int err;

	file = fopen(path_key, "rb");
	if (!file) {
		perror("fopen() error");
		goto End;
	}

	p_rsa = PEM_read_RSA_PUBKEY(file, NULL, NULL, NULL);
	if (!p_rsa) {
		ERR_print_errors_fp(stdout);
		goto End;
	}

	plen = strlen(str);
	p_en = (char *)malloc(plen + 1);
	if (!p_en) {
		perror("malloc() error");
		goto End;
	}
	memset(p_en, 0, plen + 1);

	err = RSA_public_encrypt(plen, (u8 *)str, (u8 *)p_en, p_rsa, RSA_NO_PADDING);
	if (err < 0) {
		perror("RSA_public_encrypt() error");
		goto End;
	}

End:
	if (p_rsa)
		RSA_free(p_rsa);
	if (file)
		fclose(file);

	return p_en;
}

static u8 *decrypt(u8 *str, u8 *path_key)
{
	u8 *p_de = NULL;
	RSA *p_rsa = NULL;
	FILE *file = NULL;
	int rsa_len = 0;
	int err;

	file = fopen(path_key, "rb");
	if (!file) {
		perror("fopen() error");
		goto End;
	}

	p_rsa = PEM_read_RSAPrivateKey(file, NULL, NULL, NULL);
	if (!p_rsa) {
		ERR_print_errors_fp(stdout);
		goto End;
	}

	rsa_len = RSA_size(p_rsa);

	p_de = (char *)malloc(rsa_len + 1);
	if (!p_de) {
		perror("malloc() error");
		goto End;
	}

	memset(p_de, 0, rsa_len + 1);

	err = RSA_private_decrypt(rsa_len, (u8 *)str, (u8 *)p_de, p_rsa, RSA_NO_PADDING);
	if (err < 0) {
		perror("RSA_public_encrypt() error");
		goto End;
	}

End:
	if (p_rsa)
		RSA_free(p_rsa);
	if (file)
		fclose(file);

	return p_de;
}

int main(int argc, char *argv[])
{
	u8 *ptf_en, *ptf_de;
	int rsa_len;
	int i = 0;

	if (argc < 2) {
		printf("Missing argument\n");
		return 0;
	}

	ptf_en = encrypt(argv[1], PUBLICKEY, &rsa_len);
	printf("ptf_en len %d\n", rsa_len);
	for (i = 0; i < rsa_len; i++)
		printf("%02x ", (u8)ptf_en[i]);
	printf("\n");

	ptf_de = decrypt(ptf_en, OPENSSLKEY);
	printf("ptf_de is   :%s\n", ptf_de);

	if (ptf_en)
		free(ptf_en);
	if (ptf_de)
		free(ptf_de);

	return 0;
}

