#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include <openssl/sha.h>
#include <openssl/rand.h>

// 19位掩码 (0x7FFFF = 524287)
#define MASK_19BIT 0x7FFFF

// 标准Base64字符集
const char BASE64_CHARS[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

typedef struct {
	uint32_t round_keys[4];
} DateTimeCipher;

// 初始化密码器
void init_cipher(DateTimeCipher *cipher, const char *key)
{
	uint8_t digest[SHA256_DIGEST_LENGTH];
	// SHA256_CTX ctx;

	// SHA256_Init(&ctx);
	// SHA256_Update(&ctx, key, strlen(key));
	// SHA256_Final(digest, &ctx);
	SHA256((uint8_t *)key, strlen(key), digest);

	// 从SHA256摘要派生4个32位轮密钥
	for (int i = 0; i < 4; i++) {
		cipher->round_keys[i] = ((uint32_t)digest[i * 4] << 24) |
					((uint32_t)digest[i * 4 + 1] << 16) |
					((uint32_t)digest[i * 4 + 2] << 8) |
					(uint32_t)digest[i * 4 + 3];
	}
}

// Feistel轮函数
void feistel_round(uint32_t *left, uint32_t *right, uint32_t key)
{
	uint32_t temp = *right;
	uint32_t mixed = (*right ^ key) + 0x9e3779b9;
	mixed = (mixed ^ (mixed >> 16)) * 0x85ebca6b;
	mixed = (mixed ^ (mixed >> 13)) & 0xFFFFFFFF;
	*right = *left ^ mixed;
	*left = temp;
}

// 优化为处理38位数据的Feistel网络
void feistel_round_38bit(uint32_t *left, uint32_t *right, uint32_t key)
{
	// 保存原始右半部分
	uint32_t temp = *right;

	// 38位数据优化：只处理有效位
	// 原始数据：左19位，右19位（共38位）
	// 但为了效率，我们使用32位整数，高位填充0

	// 轮函数：使用密钥混合
	uint32_t mixed = (*right ^ key) + 0x9e3779b9;

	// 非线性变换（优化38位）
	mixed = (mixed ^ (mixed >> 10)) * 0x85ebca6b; // 调整移位量适应38位
	mixed = (mixed ^ (mixed >> 8)) &
		MASK_19BIT; // 19位掩码 (0x7FFFF = 2^19-1)

	// 确保结果在19位范围内
	*right = (*left ^ mixed) & MASK_19BIT;
	*left = temp;
}

// 压缩日期时间和用户信息
uint64_t pack_datetime_user(uint8_t month, uint8_t day, uint8_t hour,
			    uint8_t minute, const char *user)
{
	// 验证输入范围
	if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 ||
	    minute > 59 || strlen(user) != 3) {
		fprintf(stderr, "Invalid input parameters\n");
		exit(1);
	}

	// 压缩日期时间到20位：月(4)日(5)时(5)分(6)
	uint32_t dt_part = (month << 16) | (day << 11) | (hour << 6) | minute;

	// 压缩用户到18位（每个字符6位）
	uint32_t user_part = 0;
	for (int i = 0; i < 3; i++) {
		const char *pos = strchr(BASE64_CHARS, user[i]);
		if (!pos) {
			fprintf(stderr, "Invalid user character: %c\n",
				user[i]);
			exit(1);
		}
		int idx = pos - BASE64_CHARS;
		user_part = (user_part << 6) | idx;
	}

	// 组合为64位整数
	return ((uint64_t)dt_part << 18) | user_part;
}

// 解压日期时间和用户信息
void unpack_datetime_user(uint64_t packed, uint8_t *month, uint8_t *day,
			  uint8_t *hour, uint8_t *minute, char *user)
{
	// 提取用户部分（低18位）
	uint32_t user_part = packed & 0x3FFFF;
	for (int i = 2; i >= 0; i--) {
		int idx = (user_part >> (i * 6)) & 0x3F;
		user[2 - i] = BASE64_CHARS[idx];
	}
	user[3] = '\0';

	// 提取日期时间部分（高20位）
	uint32_t dt_part = packed >> 18;
	*minute = dt_part & 0x3F;
	*hour = (dt_part >> 6) & 0x1F;
	*day = (dt_part >> 11) & 0x1F;
	*month = (dt_part >> 16) & 0x0F;
}

// 时间窗口验证（分钟为单位）
int validate_time_window(uint8_t month, uint8_t day, uint8_t hour,
			 uint8_t minute, int window_minutes)
{
	time_t now = time(NULL);
	struct tm *tm_now = localtime(&now);

	// 构造解密出的时间
	struct tm tm_decrypt = { .tm_year = tm_now->tm_year, // 使用当前年
				 .tm_mon = month - 1,
				 .tm_mday = day,
				 .tm_hour = hour,
				 .tm_min = minute,
				 .tm_sec = 0 };

	time_t decrypt_time = mktime(&tm_decrypt);
	if (decrypt_time == -1) {
		fprintf(stderr, "Invalid decrypted time\n");
		return 0;
	}

	// 计算时间差（分钟）
	double diff_minutes = difftime(now, decrypt_time) / 60.0;

	// 检查是否在时间窗口内
	return (diff_minutes >= -window_minutes &&
		diff_minutes <= window_minutes);
}

// 加密函数
void encrypt(DateTimeCipher *cipher, uint8_t month, uint8_t day, uint8_t hour,
	     uint8_t minute, const char *user, uint64_t *output)
{
	uint64_t num = pack_datetime_user(month, day, hour, minute, user);

	uint32_t left = (num >> 19) & MASK_19BIT;
	uint32_t right = num & MASK_19BIT;

	// 4轮Feistel加密
	for (int i = 0; i < 4; i++) {
		// feistel_round(&left, &right, cipher->round_keys[i]);
		feistel_round_38bit(&left, &right,
				    cipher->round_keys[i] & MASK_19BIT);
	}

	uint64_t encrypted = ((uint64_t)left << 19) | right;
	*output = encrypted;
}

// 解密函数
int decrypt(DateTimeCipher *cipher, uint64_t ciphertext, uint8_t *month,
	    uint8_t *day, uint8_t *hour, uint8_t *minute, char *user)
{
	uint64_t num = ciphertext;

	uint32_t left = (num >> 19) & MASK_19BIT;
	uint32_t right = num & MASK_19BIT;

	// 4轮Feistel解密（逆序轮密钥）
	for (int i = 3; i >= 0; i--) {
		// feistel_round(&right, &left, cipher->round_keys[i]);
		feistel_round_38bit(&right, &left,
				    cipher->round_keys[i] & MASK_19BIT);
	}

	uint64_t packed = ((uint64_t)left << 19) | right;

	unpack_datetime_user(packed, month, day, hour, minute, user);

	return 0;
}

// 正确将64位整数编码为6字符Base64字符串
void uint64_to_bytes(uint64_t num, char *output)
{
	for (int i = 0; i < 8; i++) {
		int idx = (num >> (i * 6)) & 0x3F;
		output[7 - i] = BASE64_CHARS[idx];
	}
	output[8] = '\0';
}

// 正确将8字符Base64字符串解码为64位整数
int bytes_to_uint64(char *ciphertext, uint64_t *output)
{
	// Base64转整数（38位）
	uint64_t num = 0;
	for (int i = 0; i < 8; i++) {
		const char *pos = strchr(BASE64_CHARS, ciphertext[i]);
		if (!pos) {
			fprintf(stderr, "Invalid Base64 character: %c\n",
				ciphertext[i]);
			return EXIT_FAILURE;
		}
		int idx = pos - BASE64_CHARS;
		// 左移并添加新位（总共36位）
		num = (num << 6) | idx;
	}
	*output = num;
	return EXIT_SUCCESS;
}

int test_38_base64()
{
	uint64_t num = 223155398934;
	char ciphertext[9];
	uint64_to_bytes(num, ciphertext);
	printf("%ld - %s\n", num, ciphertext);
	uint64_t decrypted;
	if (bytes_to_uint64(ciphertext, &decrypted) == 0) {
		printf("解密结果: %ld\n", decrypted);
	} else {
		printf("解密失败\n");
		return EXIT_FAILURE;
	}
	if (decrypted != num) {
		printf("Base64编码/解码错误\n");
		return EXIT_FAILURE;
	}
	printf("Base64编码/解码成功\n");
	return EXIT_SUCCESS;
}

int main()
{
	time_t now = time(NULL);
	struct tm *tm_now = localtime(&now);

	DateTimeCipher cipher;
	init_cipher(&cipher, "My$ecr3tK3y!");

	uint64_t encrypted;
	encrypt(&cipher, tm_now->tm_mon, tm_now->tm_mday, tm_now->tm_hour,
		tm_now->tm_min, "rdb", &encrypted);
	printf("encrypt 加密结果: %ld\n", encrypted);

	char ciphertext[9];
	uint64_to_bytes(encrypted, ciphertext);
	printf("base64 加密结果: %ld - %s\n", encrypted, ciphertext);

	uint64_t decrypted;
	bytes_to_uint64(ciphertext, &decrypted);
	printf("base64 解密结果: %s - %ld\n", ciphertext, decrypted);

	uint8_t month, day, hour, minute;
	char user[4];

	if (decrypt(&cipher, decrypted, &month, &day, &hour, &minute, user) ==
	    0) {
		printf("decrypt 解密结果: %02d-%02d-%02d-%02d %s\n", month, day,
		       hour, minute, user);
	} else {
		printf("decrypt 解密失败\n");
	}

	// 时间窗口验证
	return validate_time_window(month, day, hour, minute, 30);
}

// 数据压缩方案：
// 时间部分（20位）：
//	月份(4位)：1-12 → 4位存储
// 	日期(5位)：1-31 → 5位存储
// 	小时(5位)：0-23 → 5位存储
// 	分钟(6位)：0-59 → 6位存储
// 用户部分（18位）：
//	3个BASE64字符 → 每个字符6位 (64种可能)
//
// 总占用：38位 → 填充到64位加密
//
//
//
// ❯ xmake run demo_time_compressed1
// encrypt 加密结果: 131870699341
// base64 加密结果: 131870699341 - AB60Gy9N
// base64 解密结果: AB60Gy9N - 131870699341
// decrypt 解密结果: 07-11-17-32 rdb
