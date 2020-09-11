#ifndef CRYPTO_H
#define CRYPTO_H

#define AES_PADDING_LEN(len)  \
	((len % AES_BLOCK_SIZE) ? \
	(AES_BLOCK_SIZE - (len % AES_BLOCK_SIZE) + len) : len)

void aes_encrypt(u8 *raw_buf, u8 **encrpy_buf, int len);
void aes_decrypt(u8 *raw_buf, u8 **decrpy_buf, int len);
int md5_encrypt(u8 *raw_buf, u8 *enc_buf);
int msg_aes_encrypt(u8 *raw_buf, int len);
int msg_aes_decrypt(u8 *raw_buf, int len);

#endif
