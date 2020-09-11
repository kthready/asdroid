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
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <video.h>

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
#if 0
static void asdroid_video_callback(struct asdroid_video *as_video, void *arg, int n)
{
	int fd = *(int *)arg;
	struct as_message msg;

	memset(&msg, 0, sizeof(msg));
	msg.length = as_video->fb[n].length;
	send(fd, &msg, sizeof(msg), 0);
	send(fd, as_video->fb[n].start , as_video->fb[n].length, 0);
}

static void *asdroid_video_handle(void *arg)
{
	asdroid_video_on(&as_video);

	as_video.video_ops.capture(&as_video, asdroid_video_callback, arg);

	pthread_exit(NULL);
}
#else
static void asdroid_video_handle(int fd)
{
	struct as_message msg;
	struct as_message ack;
	char buf[] = "Python is all about automating repetitive tasks, leaving more time for your other SEO efforts.";

	memset(&msg, 0, sizeof(msg));
	msg.length = sizeof(buf);
	msg.magic = MAGIC;

	while(1) {
		recv(fd, &ack, sizeof(ack), 0);
		if (ack.magic != MAGIC)
			exit(0);
		printf("send data\n");
		send(fd, &msg, sizeof(msg), 0);
		sleep(2);
		send(fd, buf, sizeof(buf), 0);
		printf("send done\n");
	}

	pthread_exit(NULL);
}
#endif
static void motor_action(enum as_command cmd)
{
	printf("motor cmd %d\n", cmd);
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
		printf("failed to create socket: %s\n", strerror(errno));
		pthread_exit(NULL);
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(tinfo->port);
	inet_pton(AF_INET, tinfo->data, &serv_addr.sin_addr.s_addr);

	err = connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if (err < 0) {
		printf("failed to connect to server: %s\n", strerror(errno));
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
		printf("Usage: %s <ip addr>\n", argv[0]);
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
