//
// FAIR WARNING:
// This is a proof of concept, written by a humble MSc student, probably
// full of vulnerabilities and bugs. I'll be glad to fix them if you find
// some, but please don't use this code in any production-like environment.
// The fact that you give an encryption key as a command line argument
// should be a pretty strong hint, but, hey, you never know.
//

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <endian.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <openssl/evp.h>

#define DEFAULT_SECTOR_SIZE 512
#define AES_KEY_LENGTH 32

#define EVP_OR_GOTO_OUT(x) res = x; if (res != 1) { perror( #x ); goto out; }

static volatile int interrupted = 0;

typedef union aes_iv_plain64 {
	struct {
		uint64_t index;
		uint8_t zero_pad[AES_KEY_LENGTH-8];
	} sector;
	unsigned char aes_iv[AES_KEY_LENGTH];
} aes_iv_plain64;

typedef int (*blockdev_fn)(unsigned char*, int, unsigned char*, int*,
                           uint64_t, unsigned char*);

int blockdev_encrypt(unsigned char *plain_buf, int plain_len,
	unsigned char *cipher_buf, int *_cipher_len, uint64_t sector,
	unsigned char key[])
{
	aes_iv_plain64 iv;
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	int res = 1;
	int cipher_len = *_cipher_len;

	if (ctx == NULL) {
		perror("EVP_CIPHER_CTX_new()");
		res = 1;
		goto out;
	}

	memset(&iv, 0, sizeof(iv));
	iv.sector.index = htole64(sector);

	EVP_OR_GOTO_OUT(EVP_EncryptInit(ctx, EVP_aes_128_xts(), key, iv.aes_iv))
	EVP_OR_GOTO_OUT(EVP_EncryptUpdate(ctx, cipher_buf, &cipher_len, plain_buf, plain_len))
	int cipher_len_added = 0;
	EVP_OR_GOTO_OUT(EVP_EncryptFinal(ctx, cipher_buf + cipher_len, &cipher_len_added))
	cipher_len += cipher_len_added;

out:
	if (res == 1)
		*_cipher_len = cipher_len;
	return (res != 1);
}

int blockdev_decrypt(unsigned char *cipher_buf, int cipher_len,
	unsigned char *plain_buf, int *_plain_len, uint64_t sector,
	unsigned char key[])
{
	aes_iv_plain64 iv;
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	int res = 1;
	int plain_len = 0;

	if (ctx == NULL) {
		perror("EVP_CIPHER_CTX_new()");
		res = 1;
		goto out;
	}

	memset(&iv, 0, sizeof(iv));
	iv.sector.index = htole64(sector);

	EVP_OR_GOTO_OUT(EVP_DecryptInit(ctx, EVP_aes_128_xts(), key, iv.aes_iv))
	EVP_OR_GOTO_OUT(EVP_DecryptUpdate(ctx, plain_buf, &plain_len, cipher_buf, cipher_len))
	int plain_len_added = 0;
	EVP_OR_GOTO_OUT(EVP_DecryptFinal(ctx, plain_buf + plain_len, &plain_len_added))
	plain_len += plain_len_added;

out:
	if (res == 1)
		*_plain_len = plain_len;
	return (res != 1);
}

void sig_handler(int signo)
{
	if (signo != SIGINT)
		return;
	interrupted = 1;
}

void print_usage(const char* name)
{
	fprintf(stderr, "Usage: %s [-e|-d] [-s sector_size] [-o output_file]"
		"[-i input file] -k encryption_key\n", name);
	exit(1);
}

int parse_key(const char *str, unsigned char key[])
{
	if (strlen(str) != 64)
		return 1;
	for (int i = 0; i < 32; i++) {
		int res = sscanf(str + 2*i, "%02x", (unsigned int*)&(key[i]));
		if (res != 1)
			return 2;
	}
	return 0;
}

int main(int argc, char* argv[])
{
	if (argc < 2)
		print_usage(argv[0]);

	int encrypting = -1;
	int sector_size = DEFAULT_SECTOR_SIZE;
	unsigned char aes_key[AES_KEY_LENGTH] = {0};
	FILE *in = stdin;
	FILE *out = stdout;
	int c, res, i;
	while ((c = getopt(argc, argv, "edi:o:k:s:")) != -1) {
		if ((c == 'e' && encrypting == 0) || (c == 'd' && encrypting == 1)) {
			fprintf(stderr, "Error: conflicting -e/-d options\n");
			print_usage(argv[0]);
		}
		switch (c) {
		case 'e':
			encrypting = 1;
			break;
		case 'd':
			encrypting = 0;
			break;
		case 'i':
			in = fopen(optarg, "rb");
			if (in == NULL) {
				fprintf(stderr, "Error: unable to open %s\n",
					optarg);
				perror("fopen()");
				return errno;
			}
			break;
		case 'o':
			out = fopen(optarg, "wb");
			if (out == NULL) {
				fprintf(stderr, "Error: unable to open %s\n",
					optarg);
				perror("fopen()");
				return errno;
			}
			break;
		case 'k':
			res = parse_key(optarg, aes_key);
			if (res != 0) {
				fprintf(stderr, "Invalid key format, use "
					"a 64-char hexadecimal string\n");
				print_usage(argv[0]);
				return res;
			}
			break;
		case 's':
			sector_size = strtoul(optarg, NULL, 10);
			if (sector_size <= 1) {
				fprintf(stderr, "Invalid sector size\n");
				print_usage(argv[0]);
			}
		default:
			print_usage(argv[0]);
		}
	}

	if (encrypting < 0) {
		fprintf(stderr, "Error: must specify an operation -e or -d\n");
		print_usage(argv[0]);
	}
	blockdev_fn op = (encrypting ? &blockdev_encrypt : &blockdev_decrypt);
	for (i = 0; i < AES_KEY_LENGTH; i++)
		if (aes_key[i] != 0)
			break;
	if (i >= AES_KEY_LENGTH) {
		fprintf(stderr, "Error: no key or key=0 given\n");
		print_usage(argv[0]);
	}

	if (signal(SIGINT, sig_handler) == SIG_ERR)
		fprintf(stderr, " [!] Can't catch SIGINTs\n");

	unsigned char *readbuf = (unsigned char*)calloc(1, sector_size);
	unsigned char *writebuf = (unsigned char*)calloc(1, sector_size);
	if (readbuf == NULL || writebuf == NULL)
		return ENOMEM;

	time_t start = time(NULL);
	uint64_t sector;
	for (sector = 0; !interrupted; sector++) {
		int read = (int)fread(readbuf, 1, sector_size, in);
		if (read != sector_size) {
			if (ferror(in) || (!encrypting && read != 0)) {
				fprintf(stderr, " [!] I/O error: %d bytes "
					"returned, error %d\n", read, errno);
				res = 4;
				break;
			}
			else if (encrypting && read > 0) {
				fprintf(stderr, " [*] Last block was %d bytes,"
					" 0-padding to %d bytes\n", read,
					(int)sector_size);
				for (int i = read; i < sector_size; i++)
					readbuf[i] = 0;
			} else {
				break;
			}
		}
		int handled = sector_size;
		res = op(readbuf, sector_size, writebuf, &handled, sector, aes_key);
		if (res != 0 || handled != sector_size) {
			fprintf(stderr, "Error: %s code %d at sector %llu\n",
				(encrypting ? "encryption" : "decryption"),
				res, (unsigned long long)sector);
			break;
		}
		int written = fwrite(writebuf, 1, sector_size, out);
		if (written != sector_size) {
			fprintf(stderr, "Error: I/O failed while writing\n");
			res = errno;
			break;
		}
		if (read != sector_size)
			break;
	}

	if (res != 0)
		fprintf(stderr, " [!] Error code %d\n", res);
	if (interrupted)
		fprintf(stderr, " [!] Received SIGINT, stopping\n");

	unsigned long duration = time(NULL) - start;
	fprintf(stderr, " [+] %lld bytes written in %ld seconds (%.3f MiB/s)\n",
		(unsigned long long)(sector*sector_size), duration,
		((double)sector*sector_size)/(duration*1024*1024));

	if (out != stdout)
		fclose(out);
	if (in != stdin)
		fclose(in);
	return res;
}
