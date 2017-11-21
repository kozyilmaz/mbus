
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <mbus/client.h>
#include <mbus/json.h>

#include "base64.h"

#define DEFAULT_MODE		mode_receiver
#define DEFAULT_NAME		"org.mbus.client.file-transfer"
#define DEFAULT_PREFIX		"./"
#define DEFAULT_TIMEOUT		30000

enum mode {
	mode_unknown,
	mode_receiver,
	mode_sender,
};

struct receiver_param {
	const char *prefix;
	int result;
	int finished;
};

struct sender_param {
	const char *name;
	const char *source;
	const char *destination;
	int timeout;
	int result;
	int finished;
};

static enum mode mode_value (const char *mode)
{
	if (strcasecmp(mode, "receiver") == 0) {
		return mode_receiver;
	}
	if (strcasecmp(mode, "r") == 0) {
		return mode_receiver;
	}
	if (strcasecmp(mode, "sender") == 0) {
		return mode_sender;
	}
	if (strcasecmp(mode, "s") == 0) {
		return mode_sender;
	}
	return mode_unknown;
}

static const char * mode_string (enum mode mode)
{
	if (mode == mode_receiver) {
		return "receiver";
	}
	if (mode == mode_sender) {
		return "sender";
	}
	return "unknown";
}

static struct option longopts[] = {
	{"mode"			, required_argument	, 0, 'm' },
	{"name"			, required_argument	, 0, 'n' },
	{"prefix"		, required_argument	, 0, 'p' },
	{"source"		, required_argument	, 0, 's' },
	{"destination"		, required_argument	, 0, 'd' },
	{"timeout"		, required_argument	, 0, 't' },
	{"help"			, no_argument	   	, 0, 'h' },
	{0			, 0                	, 0, 0 }
};

static void usage (const char *name)
{
	fprintf(stdout, "%s options:\n", name);
	fprintf(stdout, "\n");
	fprintf(stdout, "%s --mode receiver --name name [--prefix prefix]\n", name);
	fprintf(stdout, "%s --mode sender --name name --source source --destination destination [--timeout timeout]\n", name);
	fprintf(stdout, "\n");
	fprintf(stdout, "  -m / --mode: running mode (default: %s)\n", mode_string(DEFAULT_MODE));
	fprintf(stdout, "  modes:\n");
	fprintf(stdout, "    receiver\n");
	fprintf(stdout, "      -n / --name     : application name (default: %s)\n", DEFAULT_NAME);
	fprintf(stdout, "      -p / --prefix   : file prefix (default: %s)\n", DEFAULT_PREFIX);
	fprintf(stdout, "    sender\n");
	fprintf(stdout, "      -n / --name     : receiver application name (default: %s)\n", DEFAULT_NAME);
	fprintf(stdout, "      -s / --source     : file source\n");
	fprintf(stdout, "      -d / --destination: file destination\n");
	fprintf(stdout, "      -t / --timeout    : request timeout milliseconds (default: %d)\n", DEFAULT_TIMEOUT);
	fprintf(stdout, "example:\n");
	fprintf(stdout, "  %s --mode receiver --name org.mbus.client.file-transfer --prefix /tmp\n", name);
	fprintf(stdout, "  %s --mode sender --name org.mbus.client.file-transfer --source source --destination destination --timeout 30000\n", name);
	mbus_client_usage();
}

static int mbus_client_receiver_callback_command_put (struct mbus_client *client, void *context, struct mbus_client_message *message)
{
	int rc;
	int fd;
	const char *s;
	const char *d;
	const char *e;
	char *fname;
	char *decoded;
	size_t decoded_length;
	struct receiver_param *param = context;
	(void) client;
	fd = -1;
	fname = NULL;
	decoded = NULL;
	s = mbus_json_get_string_value(mbus_client_message_routine_request_payload(message), "source", NULL);
	if (s == NULL) {
		fprintf(stderr, "source is invalid\n");
		goto bail;
	}
	d = mbus_json_get_string_value(mbus_client_message_routine_request_payload(message), "destination", NULL);
	if (d == NULL) {
		fprintf(stderr, "destination is invalid\n");
		goto bail;
	}
	e = mbus_json_get_string_value(mbus_client_message_routine_request_payload(message), "encoded", NULL);
	if (d == NULL) {
		fprintf(stderr, "encoded is invalid\n");
		goto bail;
	}
	decoded = (char *) base64_decode((unsigned char *) e, strlen(e), &decoded_length);
	if (decoded == NULL) {
		fprintf(stderr, "can not decode source: %s\n", s);
		goto bail;
	}
	fname = malloc(strlen(param->prefix) + strlen(d) + 1);
	if (fname == NULL) {
		fprintf(stderr, "can not allocate memory\n");
		goto bail;
	}
	sprintf(fname, "%s%s", param->prefix, d);
	fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		fprintf(stderr, "can not open file: %s\n", fname);
		goto bail;
	}
	rc = write(fd, decoded, decoded_length);
	if (rc != (int) decoded_length) {
		fprintf(stderr, "can not write destination: %s\n", d);
		goto bail;
	}
	close(fd);
	free(fname);
	free(decoded);
	return 0;
bail:	if (decoded != NULL) {
		free(decoded);
	}
	if (fd >= 0) {
		close(fd);
	}
	if (fname != NULL) {
		unlink(fname);
		free(fname);
	}
	return -1;
}

static void mbus_client_receiver_callback_connect (struct mbus_client *client, void *context, enum mbus_client_connect_status status)
{
	int rc;
	struct receiver_param *param = context;
	if (status == mbus_client_connect_status_success) {
		rc = mbus_client_register(client, "command.put", mbus_client_receiver_callback_command_put, param);
		if (rc != 0) {
			param->result = -1;
			param->finished = 1;
		}
	} else {
		param->result = -1;
		param->finished = 1;
	}
}

static void mbus_client_sender_callback_command_put_result (struct mbus_client *client, void *context, struct mbus_client_message *message)
{
	struct sender_param *param = context;
	(void) client;
	param->result = mbus_client_message_command_response_result(message);
	param->finished = 1;
}

static void mbus_client_sender_callback_connect (struct mbus_client *client, void *context, enum mbus_client_connect_status status)
{
	int rc;
	int fd;
	struct stat st;
	char *buffer;
	size_t buffer_length;
	char *encoded;
	size_t encoded_length;
	struct mbus_json *request;
	struct sender_param *param = context;
	(void) client;
	fd = -1;
	buffer = NULL;
	encoded = NULL;
	request = NULL;
	if (status != mbus_client_connect_status_success) {
		goto bail;
	}
	rc = stat(param->source, &st);
	if (rc < 0) {
		fprintf(stderr, "can not open source: %s\n", param->source);
		goto bail;
	}
	buffer_length = st.st_size;
	buffer = malloc(buffer_length + 1);
	if (buffer == NULL) {
		fprintf(stderr, "can not allocate memory\n");
		goto bail;
	}
	fd = open(param->source, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "can not open source: %s\n", param->source);
		goto bail;
	}
	rc = read(fd, buffer, buffer_length);
	if (rc != (int) buffer_length) {
		fprintf(stderr, "can not read source: %s\n", param->source);
		goto bail;
	}
	buffer[rc] = '\0';
	encoded = (char *) base64_encode((unsigned char *) buffer, buffer_length, &encoded_length);
	if (encoded == NULL) {
		fprintf(stderr, "can not encode source: %s\n", param->source);
		goto bail;
	}
	request = mbus_json_create_object();
	if (request == NULL) {
		fprintf(stderr, "can not create request\n");
		goto bail;
	}
	rc = mbus_json_add_string_to_object_cs(request, "source", param->source);
	if (rc != 0) {
		fprintf(stderr, "can not create request\n");
		goto bail;
	}
	rc = mbus_json_add_string_to_object_cs(request, "destination", param->destination);
	if (rc != 0) {
		fprintf(stderr, "can not create request\n");
		goto bail;
	}
	rc = mbus_json_add_string_to_object_cs(request, "encoded", encoded);
	if (rc != 0) {
		fprintf(stderr, "can not create request\n");
		goto bail;
	}
	rc = mbus_client_command(client, param->name, "command.put", request, mbus_client_sender_callback_command_put_result, param);
	if (rc != 0) {
		fprintf(stderr, "can not execute command\n");
		goto bail;
	}
	mbus_json_delete(request);
	free(encoded);
	free(buffer);
	return;
bail:	if (request != NULL) {
		mbus_json_delete(request);
	}
	if (encoded != NULL) {
		free(encoded);
	}
	if (buffer != NULL) {
		free(buffer);
	}
	param->result = -1;
	param->finished = 1;
	return;
}

int main (int argc, char *argv[])
{
	int rc;

	int c;
	int _argc;
	char **_argv;

	enum mode o_mode;
	const char *o_prefix;
	const char *o_name;
	const char *o_source;
	const char *o_destination;
	int o_timeout;

	struct mbus_client *mbus_client;
	struct mbus_client_options mbus_options;

	struct receiver_param receiver_param;
	struct sender_param sender_param;

	o_mode = DEFAULT_MODE;
	o_prefix = DEFAULT_PREFIX;
	o_name = DEFAULT_NAME;
	o_source = NULL;
	o_destination = NULL;
	o_timeout = DEFAULT_TIMEOUT;

	_argc = 0;
	_argv = NULL;

	memset(&receiver_param, 0, sizeof(struct receiver_param));
	memset(&sender_param, 0, sizeof(struct sender_param));
	mbus_client = NULL;

	_argv = malloc(sizeof(char *) * argc);
	if (_argv == NULL) {
		fprintf(stderr, "can not allocate memory\n");
		goto bail;
	}
	for (_argc = 0; _argc < argc; _argc++) {
		_argv[_argc] = argv[_argc];
	}

	while ((c = getopt_long(_argc, _argv, ":", longopts, NULL)) != -1) {
		switch (c) {
			case 'm':
				o_mode = mode_value(optarg);
				if (o_mode == mode_unknown) {
					fprintf(stderr, "invalid mode: %s\n", optarg);
					goto bail;
				}
				break;
			case 'p':
				o_prefix = optarg;
				break;
			case 'n':
				o_name = optarg;
				break;
			case 's':
				o_source = optarg;
				break;
			case 'd':
				o_destination = optarg;
				break;
			case 't':
				o_timeout = atoi(optarg);
				break;
			case 'h':
				usage(argv[0]);
				goto bail;
		}
	}

	rc = mbus_client_options_from_argv(&mbus_options, argc, argv);
	if (rc != 0) {
		fprintf(stderr, "can not parse options\n");
		goto bail;
	}
	if (o_mode == mode_receiver) {
		memset(&receiver_param, 0, sizeof(struct receiver_param));
		receiver_param.prefix = o_prefix;
		mbus_options.name = (char *) o_name;
		mbus_options.callbacks.connect = mbus_client_receiver_callback_connect;
		mbus_options.callbacks.context = &receiver_param;
	} else if (o_mode == mode_sender) {
		if (o_source == NULL) {
			fprintf(stderr, "source is invalid\n");
			goto bail;
		}
		if (o_destination == NULL) {
			fprintf(stderr, "destination is invalid\n");
			goto bail;
		}
		memset(&sender_param, 0, sizeof(struct sender_param));
		sender_param.name = o_name;
		sender_param.source = o_source;
		sender_param.destination = o_destination;
		sender_param.timeout = o_timeout;
		mbus_options.callbacks.connect = mbus_client_sender_callback_connect;
		mbus_options.callbacks.context = &sender_param;
	} else {
		fprintf(stderr, "invalid mode: %d\n", o_mode);
		goto bail;
	}

	mbus_client = mbus_client_create(&mbus_options);
	if (mbus_client == NULL) {
		fprintf(stderr, "can not create client\n");
		goto bail;
	}
	rc = mbus_client_connect(mbus_client);
	if (rc != 0) {
		fprintf(stderr, "can not connect client\n");
		goto bail;
	}

	while (1) {
		rc = mbus_client_run(mbus_client, MBUS_CLIENT_DEFAULT_RUN_TIMEOUT);
		if (rc != 0) {
			fprintf(stderr, "client run failed\n");
			goto bail;
		}
		if (o_mode == mode_receiver) {
			if (receiver_param.finished == 1 &&
			    mbus_client_has_pending(mbus_client) == 0) {
				break;
			}
		} else if (o_mode == mode_sender) {
			if (sender_param.finished == 1 &&
			    mbus_client_has_pending(mbus_client) == 0) {
				break;
			}
		}
	}

	mbus_client_destroy(mbus_client);
	free(_argv);
	if (o_mode == mode_receiver) {
		return receiver_param.result;
	}
	if (o_mode == mode_sender) {
		return sender_param.result;
	}
	return 0;
bail:	if (_argv != NULL) {
		free(_argv);
	}
	if (mbus_client != NULL) {
		mbus_client_destroy(mbus_client);
	}
	return -1;
}
