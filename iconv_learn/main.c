#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iconv.h>

// UTF-8 转 GBK 函数
int utf8_to_gbk(const char *utf8, char **gbk, size_t *gbk_len)
{
	iconv_t cd = iconv_open("GBK", "UTF-8");
	if (cd == (iconv_t)-1) {
		perror("iconv_open failed");
		return -1;
	}
	size_t utf8_len = strlen(utf8);
	*gbk_len = utf8_len * 2; // GBK可能需要更多字节
	*gbk = malloc(*gbk_len);
	if (*gbk == NULL) {
		perror("malloc failed");
		iconv_close(cd);
		return -1;
	}
	char *in_buf = (char *)utf8;
	char *out_buf = *gbk;
	size_t in_bytes_left = utf8_len;
	size_t out_bytes_left = *gbk_len;
	if (iconv(cd, &in_buf, &in_bytes_left, &out_buf, &out_bytes_left) ==
	    (size_t)-1) {
		perror("iconv failed");
		free(*gbk);
		iconv_close(cd);
		return -1;
	}
	iconv_close(cd);
	return 0;
}

int main()
{
	const char *utf8_str = "编码字符串: 你好，世界！,"; // UTF-8 编码字符串
	char *gbk_str;
	size_t gbk_len;
	if (utf8_to_gbk(utf8_str, &gbk_str, &gbk_len) == 0) {
		printf("UTF-8 编码字符串: %s\n", utf8_str);
		printf("GBK 编码字符串: %s\n", gbk_str);
		printf("GBK 编码字符串: %.*s\n", (int)gbk_len, gbk_str);
		free(gbk_str); // 释放内存
	}
	return 0;
}
