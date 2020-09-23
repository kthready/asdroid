#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <pthread.h>
#include <fcntl.h>
#include <common.h>
#include <message.h>
#include <thread.h>
#include <video.h>
#include <crypto.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

static struct asdroid_video as_video;

static int asdroid_video_on(struct asdroid_video *as_video)
{
	if (!as_video)
		return -1;

	as_video->video_ops.open(as_video);
	as_video->video_ops.get_capability(as_video);
	as_video->video_ops.get_support_format(as_video);

	memset(&as_video->fmt, 0, sizeof(as_video->fmt));
	as_video->video_ops.get_fmt(as_video);
	as_video->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	as_video->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
	as_video->fmt.fmt.pix.height = IMAGEHEIGHT;
	as_video->fmt.fmt.pix.width = IMAGEWIDTH;
	as_video->fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
	as_video->video_ops.set_fmt(as_video);

	as_video->video_ops.alloc_mem(as_video);
	as_video->video_ops.stream_on(as_video);

	return 0;
}

static int asdroid_video_off(struct asdroid_video *as_video)
{
	if (!as_video)
		return -1;

	as_video->video_ops.stream_off(as_video);
	as_video->video_ops.release(as_video);

	return 0;	
}

static void asdroid_video_handle(int fd)
{
	struct message *pre_msg = NULL;
	struct message *vd_msg = NULL;
	struct as_message *as_msg = NULL;
	struct message *kick_msg = NULL;
	size_t pre_msg_len, vd_msg_len, kick_msg_len;
	int err;

	char buf[] = "Python is all about automating repetitive tasks, leaving more time for your other SEO efforts.\n";

	pre_msg_len = AES_PADDING_LEN(sizeof(struct message) + sizeof(struct as_message));
	vd_msg_len = AES_PADDING_LEN(sizeof(struct message) + sizeof(buf));
	kick_msg_len = AES_PADDING_LEN(sizeof(struct message) + sizeof(struct as_message));

	pre_msg = (struct message *)malloc(pre_msg_len);
	if (!pre_msg) {
		printf("%s: Failed to alloc memory for prev message\n", __func__);
		goto exit;
	}

	kick_msg = (struct message *)malloc(kick_msg_len);
	if (!kick_msg) {
		printf("%s: Failed to alloc memory for kick message\n", __func__);
		goto exit;
	}

	memset(kick_msg, 0, kick_msg_len);
	memset(pre_msg, 0, pre_msg_len);
	pre_msg->header.msg_length = sizeof(struct as_message);
	pre_msg->header.magic_num = MAGIC;
	as_msg = (struct as_message *)pre_msg->priv_data;
	as_msg->length = vd_msg_len;

	vd_msg = (struct message *)malloc(vd_msg_len);
	if (!vd_msg) {
		printf("%s: Failed to alloc memory for video message\n", __func__);
		goto exit;
	}

	memset(vd_msg, 0, vd_msg_len);
	vd_msg->header.msg_length = sizeof(buf);
	vd_msg->header.magic_num = MAGIC;
	memcpy(vd_msg->priv_data, buf, sizeof(buf));

	while(1) {
		err = msg_recv(fd, &kick_msg, kick_msg_len, 0);
		if (err < 0) {
			printf("%s: Failed to recv message from server\n", __func__);
			break;
		}

		printf("%s: send data\n", __func__);
		err = msg_send(fd, pre_msg, pre_msg_len, 0);
		if (err < 0) {
			printf("%s: Failed to send prev message to server\n", __func__);
			break;
		}

		sleep(2);
		err = msg_send(fd, vd_msg, vd_msg_len, 0);
		if (err < 0) {
			printf("%s: Failed to send video message to server\n", __func__);
			break;
		}

		printf("%s: send done\n", __func__);
	}

exit:
	if (vd_msg)
		free(vd_msg);
	if (pre_msg)
		free(pre_msg);
	if (kick_msg)
		free(kick_msg);
	pthread_exit(NULL);
}

static void motor_action(enum as_command cmd)
{
	printf("%s: motor cmd %d\n", __func__, cmd);
}

static void asdroid_motor_handle(int fd)
{
	enum as_command cmd;

	while(1) {
		recv(fd, &cmd, sizeof(cmd), 0);
		motor_action(cmd);
	}
	pthread_exit(NULL);
}

static void *asdroid_socket_thread(void *arg)
{
	int fd, err;
	pthread_t tid0, tid1;
	struct sockaddr_in serv_addr;
	struct thread_info *tinfo = (struct thread_info *)arg;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		printf("%s: failed to create socket: %s\n", __func__, strerror(errno));
		pthread_exit(NULL);
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(tinfo->port);
	inet_pton(AF_INET, tinfo->data, &serv_addr.sin_addr.s_addr);

	err = connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if (err < 0) {
		printf("%s: failed to connect to server: %s\n", __func__, strerror(errno));
		pthread_exit(NULL);
	}

	if (tinfo->port == ASRD_PORT)
		asdroid_motor_handle(fd);

	if (tinfo->port == ASWR_PORT)
		asdroid_video_handle(fd);

	close(fd);
	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	int i;

	//memset(&as_video, 0, sizeof(as_video));
	//as_video.dev_name = VIDEO_DEV;
	//register_video_ops(&as_video);

	struct thread_info tinfo[] = {
		{
			.port = ASRD_PORT,
			.thread_socket = asdroid_socket_thread,
		},
		{
			.port = ASWR_PORT,
			.thread_socket = asdroid_socket_thread,
		},
	};

	if (argc < 2) {
		printf("%s: Usage: %s <ip addr>\n", __func__, argv[0]);
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(tinfo); i++) {
		tinfo[i].data = argv[1];
		pthread_create(&tinfo[i].tid, NULL, tinfo[i].thread_socket, &tinfo[i]);
	}

	for (i = 0; i < ARRAY_SIZE(tinfo); i++) {
		pthread_join(tinfo[i].tid, NULL);
	}

	//unregister_video_ops(&as_video);

	return 0;
}
