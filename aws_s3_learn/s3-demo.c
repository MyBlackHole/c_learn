/**
 * aws-c-s3 v0.12.0 完整使用示例
 * 连接华为云 OBS，执行 PUT 上传和 GET 下载操作
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <aws/common/common.h>
#include <aws/common/condition_variable.h>
#include <aws/common/mutex.h>
#include <aws/common/string.h>
#include <aws/io/event_loop.h>
#include <aws/io/host_resolver.h>
#include <aws/io/channel_bootstrap.h>
#include <aws/io/stream.h>
#include <aws/io/file_utils.h>
#include <aws/http/http.h>
#include <aws/http/request_response.h>
#include <aws/auth/auth.h>
#include <aws/auth/credentials.h>
#include <aws/auth/signing.h>
#include <aws/s3/s3_client.h>

/* ==================== 配置区域 ==================== */
#define OBS_ENDPOINT "obs.cn-north-4.myhuaweicloud.com"
#define OBS_REGION "cn-north-4"
#define BUCKET_NAME "your-bucket-name"
#define OBJECT_KEY "test/upload-demo.txt"

#define ACCESS_KEY_ID getenv("OBS_ACCESS_KEY_ID")
#define SECRET_ACCESS_KEY getenv("OBS_SECRET_ACCESS_KEY")

#define LOCAL_FILE_UPLOAD "./upload.txt"
#define LOCAL_FILE_DOWNLOAD "./downloaded.txt"

/* ==================== 全局状态 ==================== */
struct completion_state {
	struct aws_mutex mutex;
	struct aws_condition_variable cvar;
	bool completed;
	int error_code;
	struct aws_byte_buf result_body;
	struct aws_allocator *allocator;
};

/* ==================== 回调函数 ==================== */
static void
s_on_request_finished(struct aws_s3_meta_request *meta_request,
		      const struct aws_s3_meta_request_result *result,
		      void *user_data)
{
	(void)meta_request;
	struct completion_state *state = (struct completion_state *)user_data;

	aws_mutex_lock(&state->mutex);

	if (result->error_code != AWS_ERROR_SUCCESS) {
		state->error_code = result->error_code;
		fprintf(stderr, "Request failed: %s (code: %d)\n",
			aws_error_name(result->error_code), result->error_code);
		if (result->error_response_body &&
		    result->error_response_body->len > 0) {
			fprintf(stderr, "Response: %.*s\n",
				(int)result->error_response_body->len,
				result->error_response_body->buffer);
		}
	} else {
		printf("Request completed successfully!\n");
		state->error_code = AWS_ERROR_SUCCESS;
	}

	state->completed = true;
	aws_condition_variable_notify_one(&state->cvar);
	aws_mutex_unlock(&state->mutex);
}

static int s_on_get_response_body(struct aws_s3_meta_request *meta_request,
				  const struct aws_byte_cursor *body,
				  uint64_t range_start, void *user_data)
{
	struct completion_state *state = (struct completion_state *)user_data;
	aws_mutex_lock(&state->mutex);

	if (aws_byte_buf_reserve(&state->result_body,
				 state->result_body.len + body->len) ==
	    AWS_OP_SUCCESS) {
		aws_byte_buf_append(&state->result_body, body);
	}

	aws_mutex_unlock(&state->mutex);
	return AWS_OP_SUCCESS;
}

static int s_wait_for_completion(struct completion_state *state)
{
	aws_mutex_lock(&state->mutex);
	while (!state->completed) {
		aws_condition_variable_wait(&state->cvar, &state->mutex);
	}
	int err = state->error_code;
	aws_mutex_unlock(&state->mutex);
	return err;
}

/* ==================== 初始化函数 ==================== */
static int s_init_aws_environment(void)
{
	struct aws_allocator *allocator = aws_default_allocator();

	aws_common_library_init(allocator);
	aws_io_library_init(allocator);
	aws_http_library_init(allocator);
	aws_auth_library_init(allocator);

	return AWS_OP_SUCCESS;
}

static void s_cleanup_aws_environment(void)
{
	aws_auth_library_clean_up();
	aws_http_library_clean_up();
	aws_io_library_clean_up();
	aws_common_library_clean_up();
}

static struct aws_s3_client *
s_create_s3_client(struct aws_allocator *allocator,
		   struct aws_event_loop_group *elg,
		   struct aws_host_resolver *resolver,
		   struct aws_client_bootstrap *bootstrap, const char *region)
{
	struct aws_s3_client_config config = {
		.client_bootstrap = bootstrap,
		.region = aws_byte_cursor_from_c_str(region),
		.tls_mode = AWS_MR_TLS_ENABLED,
		.throughput_target_gbps = 10.0,
		.part_size = 8 * 1024 * 1024,
		.enable_read_backpressure = true,
		.initial_read_window = 4 * 1024 * 1024,
	};

	struct aws_s3_client *client = aws_s3_client_new(allocator, &config);
	if (!client) {
		fprintf(stderr, "Failed to create S3 client\n");
	}

	return client;
}

/* ==================== PUT 上传操作 ==================== */
static int s_put_object(struct aws_s3_client *client,
			struct aws_allocator *allocator, const char *bucket,
			const char *key, const char *file_path,
			struct aws_credentials *credentials, const char *region,
			const char *endpoint)
{
	struct completion_state state = {
		.mutex = AWS_MUTEX_INIT,
		.cvar = AWS_CONDITION_VARIABLE_INIT,
		.completed = false,
		.error_code = AWS_ERROR_SUCCESS,
		.result_body = { 0 },
		.allocator = allocator,
	};

	/* 读取本地文件内容 */
	struct aws_byte_buf file_content;
	aws_byte_buf_init(&file_content, allocator, 0);
	if (aws_byte_buf_init_from_file(&file_content, allocator, file_path) !=
	    AWS_OP_SUCCESS) {
		fprintf(stderr, "Failed to read file: %s\n", file_path);
		return -1;
	}

	printf("Read %zu bytes from %s\n", file_content.len, file_path);

	char host_buf[512];
	snprintf(host_buf, sizeof(host_buf), "%s.%s", bucket, endpoint);
	char uri_buf[1024];
	snprintf(uri_buf, sizeof(uri_buf), "https://%s/%s", host_buf, key);

	struct aws_byte_cursor host_cursor =
		aws_byte_cursor_from_c_str(host_buf);
	struct aws_byte_cursor uri_cursor = aws_byte_cursor_from_c_str(uri_buf);

	/* 创建 HTTP PUT 请求 */
	struct aws_http_message *message =
		aws_http_message_new_request(allocator);
	if (!message) {
		fprintf(stderr, "Failed to create HTTP message\n");
		aws_byte_buf_clean_up(&file_content);
		return -1;
	}

	/* 设置请求路径 */
	aws_http_message_set_request_path(message, uri_cursor);

	/* 设置 Host header */
	struct aws_http_header host_header = {
		.name = aws_byte_cursor_from_c_str("Host"),
		.value = host_cursor,
	};

	aws_http_message_add_header(message, host_header);

	/* 设置 Content-Type header */
	struct aws_http_header content_type_header = {
		.name = aws_byte_cursor_from_c_str("Content-Type"),
		.value = aws_byte_cursor_from_c_str("text/plain"),
	};
	aws_http_message_add_header(message, content_type_header);

	/* 设置 Content-Length header */
	char content_len_str[32];
	snprintf(content_len_str, sizeof(content_len_str), "%zu",
		 file_content.len);

	struct aws_http_header content_len_header = {
		.name = aws_byte_cursor_from_c_str("Content-Length"),
		.value = aws_byte_cursor_from_c_str(content_len_str),
	};
	aws_http_message_add_header(message, content_len_header);

	/* 创建输入流并设置到消息体 */
	struct aws_input_stream *body_stream =
		aws_input_stream_new_from_file(allocator, file_path);
	aws_http_message_set_body_stream(message, body_stream);

	/* 构建签名配置 */
	struct aws_signing_config_aws signing_config = {
		.credentials = credentials,
		.region = aws_byte_cursor_from_c_str(region),
		.service = aws_byte_cursor_from_c_str("s3"),
		.algorithm = AWS_SIGNING_ALGORITHM_V4,
		.signature_type = AWS_ST_HTTP_REQUEST_HEADERS,
		.signed_body_header = AWS_SBHT_NONE,
	};

	/* 构建 PUT 请求选项 */
	struct aws_s3_meta_request_options put_options = {
		.type = AWS_S3_META_REQUEST_TYPE_PUT_OBJECT,
		.message = message,
		.signing_config = &signing_config,
		.finish_callback = s_on_request_finished,
		.user_data = &state,
	};

	struct aws_s3_meta_request *meta_request =
		aws_s3_client_make_meta_request(client, &put_options);

	if (!meta_request) {
		fprintf(stderr, "Failed to create PUT meta request\n");
		aws_input_stream_destroy(body_stream);
		aws_http_message_destroy(message);
		aws_byte_buf_clean_up(&file_content);
		return -1;
	}

	int result = s_wait_for_completion(&state);

	/* 清理资源 */
	aws_s3_meta_request_release(meta_request);
	aws_input_stream_destroy(body_stream);
	aws_http_message_destroy(message);
	aws_byte_buf_clean_up(&file_content);

	return result;
}

/* ==================== GET 下载操作 ==================== */
static int s_get_object(struct aws_s3_client *client,
			struct aws_allocator *allocator, const char *bucket,
			const char *key, const char *output_path,
			struct aws_credentials *credentials, const char *region,
			const char *endpoint)
{
	struct completion_state state = {
		.mutex = AWS_MUTEX_INIT,
		.cvar = AWS_CONDITION_VARIABLE_INIT,
		.completed = false,
		.error_code = AWS_ERROR_SUCCESS,
		.result_body = { 0 },
		.allocator = allocator,
	};
	aws_byte_buf_init(&state.result_body, allocator, 0);

	char host_buf[512];
	snprintf(host_buf, sizeof(host_buf), "%s.%s", bucket, endpoint);
	char uri_buf[1024];
	snprintf(uri_buf, sizeof(uri_buf), "https://%s/%s", host_buf, key);

	struct aws_byte_cursor host_cursor =
		aws_byte_cursor_from_c_str(host_buf);
	struct aws_byte_cursor uri_cursor = aws_byte_cursor_from_c_str(uri_buf);

	/* 创建 HTTP GET 请求 */
	struct aws_http_message *message =
		aws_http_message_new_request(allocator);
	if (!message) {
		fprintf(stderr, "Failed to create HTTP message\n");
		aws_byte_buf_clean_up(&state.result_body);
		return -1;
	}

	/* 设置请求路径 */
	aws_http_message_set_request_path(message, uri_cursor);

	/* 设置 Host header */
	/* 设置 Host header */
	struct aws_http_header host_header = {
		.name = aws_byte_cursor_from_c_str("Host"),
		.value = host_cursor,
	};

	aws_http_message_add_header(message, host_header);

	/* 构建签名配置 */
	struct aws_signing_config_aws signing_config = {
		.credentials = credentials,
		.region = aws_byte_cursor_from_c_str(region),
		.service = aws_byte_cursor_from_c_str("s3"),
		.algorithm = AWS_SIGNING_ALGORITHM_V4,
		.signature_type = AWS_ST_HTTP_REQUEST_HEADERS,
		.signed_body_header = AWS_SBHT_NONE,
	};

	/* 构建 GET 请求选项 */
	struct aws_s3_meta_request_options get_options = {
		.type = AWS_S3_META_REQUEST_TYPE_GET_OBJECT,
		.message = message,
		.signing_config = &signing_config,
		.finish_callback = s_on_request_finished,
		.headers_callback = NULL,
		.body_callback = s_on_get_response_body,
		.user_data = &state,
	};

	struct aws_s3_meta_request *meta_request =
		aws_s3_client_make_meta_request(client, &get_options);

	if (!meta_request) {
		fprintf(stderr, "Failed to create GET meta request\n");
		aws_http_message_destroy(message);
		aws_byte_buf_clean_up(&state.result_body);
		return -1;
	}

	int result = s_wait_for_completion(&state);

	if (result == AWS_ERROR_SUCCESS && state.result_body.len > 0) {
		FILE *outfile = fopen(output_path, "wb");
		if (outfile) {
			fwrite(state.result_body.buffer, 1,
			       state.result_body.len, outfile);
			fclose(outfile);
			printf("Saved %zu bytes to %s\n", state.result_body.len,
			       output_path);
		} else {
			fprintf(stderr, "Failed to open output file: %s\n",
				output_path);
			result = -1;
		}
	}

	/* 清理资源 */
	aws_s3_meta_request_release(meta_request);
	aws_http_message_destroy(message);
	aws_byte_buf_clean_up(&state.result_body);

	return result;
}

/* ==================== 主函数 ==================== */
int main(void)
{
	struct aws_allocator *allocator = aws_default_allocator();
	int result = 0;

	if (!ACCESS_KEY_ID || !SECRET_ACCESS_KEY) {
		fprintf(stderr,
			"Please set OBS_ACCESS_KEY_ID and OBS_SECRET_ACCESS_KEY\n");
		fprintf(stderr, "  export OBS_ACCESS_KEY_ID=your-access-key\n");
		fprintf(stderr,
			"  export OBS_SECRET_ACCESS_KEY=your-secret-key\n");
		return -1;
	}

	printf("=== aws-c-s3 v0.12.0 Demo ===\n");
	printf("Endpoint: %s\n", OBS_ENDPOINT);
	printf("Region: %s\n", OBS_REGION);
	printf("Bucket: %s\n", BUCKET_NAME);
	printf("Object Key: %s\n\n", OBJECT_KEY);

	/* 创建测试文件 */
	FILE *f = fopen(LOCAL_FILE_UPLOAD, "w");
	if (f) {
		fprintf(f, "Hello from aws-c-s3 v0.12.0!\n");
		fprintf(f, "This is a test file for upload.\n");
		fprintf(f, "Timestamp: %ld\n", time(NULL));
		fclose(f);
		printf("Created test file: %s\n", LOCAL_FILE_UPLOAD);
	}

	/* 初始化 CRT 环境 */
	if (s_init_aws_environment() != AWS_OP_SUCCESS) {
		fprintf(stderr, "Failed to initialize AWS environment\n");
		return -1;
	}

	/* 创建事件循环组 */
	struct aws_event_loop_group *event_loop_group =
		aws_event_loop_group_new_default(allocator, 0, NULL);
	if (event_loop_group == NULL) {
		fprintf(stderr, "Failed to create event loop group\n");
		s_cleanup_aws_environment();
		return -1;
	}

	/* 创建 DNS 解析器 */
	struct aws_host_resolver_default_options resolver_options = {
		.el_group = event_loop_group,
		.max_entries = 8,
	};
	struct aws_host_resolver *host_resolver =
		aws_host_resolver_new_default(allocator, &resolver_options);
	if (host_resolver == NULL) {
		fprintf(stderr, "Failed to create host resolver\n");
		aws_event_loop_group_release(event_loop_group);
		s_cleanup_aws_environment();
		return -1;
	}

	/* 创建客户端引导程序 */
	struct aws_client_bootstrap_options bootstrap_options = {
		.event_loop_group = event_loop_group,
		.host_resolver = host_resolver,
	};
	struct aws_client_bootstrap *bootstrap =
		aws_client_bootstrap_new(allocator, &bootstrap_options);
	if (bootstrap == NULL) {
		fprintf(stderr, "Failed to create client bootstrap\n");
		aws_host_resolver_release(host_resolver);
		aws_event_loop_group_release(event_loop_group);
		s_cleanup_aws_environment();
		return -1;
	}

	/* 创建 S3 客户端 */
	struct aws_s3_client *s3_client =
		s_create_s3_client(allocator, event_loop_group, host_resolver,
				   bootstrap, OBS_REGION);
	if (!s3_client) {
		fprintf(stderr, "Failed to create S3 client\n");
		aws_client_bootstrap_release(bootstrap);
		aws_host_resolver_release(host_resolver);
		aws_event_loop_group_release(event_loop_group);
		s_cleanup_aws_environment();
		return -1;
	}

	/* 创建凭证 */
	struct aws_string *access_key_id =
		aws_string_new_from_c_str(allocator, ACCESS_KEY_ID);
	struct aws_string *secret_access_key =
		aws_string_new_from_c_str(allocator, SECRET_ACCESS_KEY);

	struct aws_credentials *credentials = aws_credentials_new_from_string(
		allocator, access_key_id, secret_access_key, NULL, UINT64_MAX);

	aws_string_destroy(access_key_id);
	aws_string_destroy(secret_access_key);

	if (!credentials) {
		fprintf(stderr, "Failed to create credentials\n");
		aws_s3_client_release(s3_client);
		aws_client_bootstrap_release(bootstrap);
		aws_host_resolver_release(host_resolver);
		aws_event_loop_group_release(event_loop_group);
		s_cleanup_aws_environment();
		return -1;
	}

	/* 执行 PUT 上传 */
	printf("\n--- Starting PUT operation ---\n");
	result = s_put_object(s3_client, allocator, BUCKET_NAME, OBJECT_KEY,
			      LOCAL_FILE_UPLOAD, credentials, OBS_REGION,
			      OBS_ENDPOINT);

	if (result == AWS_ERROR_SUCCESS) {
		printf("PUT operation SUCCESS\n");

		/* 执行 GET 下载 */
		printf("\n--- Starting GET operation ---\n");
		result = s_get_object(s3_client, allocator, BUCKET_NAME,
				      OBJECT_KEY, LOCAL_FILE_DOWNLOAD,
				      credentials, OBS_REGION, OBS_ENDPOINT);

		if (result == AWS_ERROR_SUCCESS) {
			printf("GET operation SUCCESS\n");
		} else {
			printf("GET operation FAILED\n");
		}
	} else {
		printf("PUT operation FAILED (error code: %d)\n", result);
	}

	/* 清理资源 */
	aws_credentials_release(credentials);
	aws_s3_client_release(s3_client);
	aws_client_bootstrap_release(bootstrap);
	aws_host_resolver_release(host_resolver);
	aws_event_loop_group_release(event_loop_group);
	s_cleanup_aws_environment();

	printf("\n=== Demo completed ===\n");
	return result;
}
