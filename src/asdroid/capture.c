#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <video.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <linux/videodev2.h>

static int video_open(struct asdroid_video *this)
{
	if (!this->dev_name)
		return -1;

	this->fd = open(this->dev_name, O_RDWR);
	if (this->fd < 0) {
		printf("Failed to open %s: %s\n", this->dev_name, strerror(errno));
		return -1;
	}

	return this->fd;
}

static void video_release(struct asdroid_video *this)
{
	int n;
	enum v4l2_buf_type type;

	if (this->fd < 0)
		return;

	if (this->status == STREAM_ON) {
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		ioctl(this->fd, VIDIOC_STREAMOFF, &type);
	}

	for(n = 0; n < this->frame_num; n++) {
		if (this->fb != NULL && this->fb[n].start != NULL)
			munmap(this->fb[n].start, this->fb[n].length);
	}

	if (this->fb)
		free(this->fb);

	close(this->fd);
}

static int get_video_capability(struct asdroid_video *this)
{
	if (this->fd < 0)
		return -1;

	if (ioctl(this->fd, VIDIOC_QUERYCAP, &this->cap) < 0) {
		printf("Failed to get video capability\n");
		return -1;
	} else {
		printf("driver:\t\t%s\n", this->cap.driver);
		printf("card:\t\t%s\n", this->cap.card);
		printf("bus_info:\t%s\n", this->cap.bus_info);
		printf("version:\t%d\n", this->cap.version);
		printf("capabilities:\t%x\n", this->cap.capabilities);

		if ((this->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == V4L2_CAP_VIDEO_CAPTURE) {
			printf("Device %s: supports capture.\n", this->dev_name);
		}

		if ((this->cap.capabilities & V4L2_CAP_STREAMING) == V4L2_CAP_STREAMING) {
			printf("Device %s: supports streaming.\n", this->dev_name);
		}
	}

	return 0;
}

static int enum_frame_intervals(struct asdroid_video *this, int pixfmt, int width, int height)
{
	int ret;
	struct v4l2_frmivalenum fival;

	memset(&fival, 0, sizeof(struct v4l2_frmivalenum));
	fival.index = 0;
	fival.pixel_format = pixfmt;
	fival.width = width;
	fival.height = height;

	printf("Time interval between frame: ");
	while ((ret = ioctl(this->fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival)) == 0) {
		if (fival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
			printf("%u/%u, ", fival.discrete.numerator, fival.discrete.denominator);
		} else if (fival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
			printf("{min { %u/%u } .. max { %u/%u } }, ",
			fival.stepwise.min.numerator, fival.stepwise.min.numerator,
			fival.stepwise.max.denominator, fival.stepwise.max.denominator);
			break;
		} else if (fival.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
			printf("{min { %u/%u } .. max { %u/%u } / "
			"stepsize { %u/%u } }, ",
			fival.stepwise.min.numerator, fival.stepwise.min.denominator,
			fival.stepwise.max.numerator, fival.stepwise.max.denominator,
			fival.stepwise.step.numerator, fival.stepwise.step.denominator);
			break;
		}
		fival.index++;
	}
	printf("\n");

	if (ret != 0 && errno != EINVAL) {
		printf("ERROR enumerating frame intervals: %d\n", errno);
		return errno;
	}

	return 0;
}

static int get_video_support_format(struct asdroid_video *this)
{
	struct v4l2_fmtdesc fmtdesc;
	struct v4l2_frmsizeenum frmsize;
	int ret;

	if (this->fd < 0)
		return -1;

	fmtdesc.index = 0;
	fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	printf("Support format:\n");

	while (ioctl(this->fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
		printf("%d.%s\n", fmtdesc.index + 1, fmtdesc.description);
		frmsize.pixel_format = fmtdesc.pixelformat;
		frmsize.index = 0;
		while (ioctl(this->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
			if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
				printf("{ discrete: width = %u, height = %u }\n",
						frmsize.discrete.width, frmsize.discrete.height);
				ret = enum_frame_intervals(this, frmsize.pixel_format,
							   frmsize.discrete.width, 
							   frmsize.discrete.height);
				if (ret)
					printf("Unable to enumerate frame sizes.\n");
			} else if (frmsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
				printf("{ continuous: min { width = %u, height = %u } .. "
					"max { width = %u, height = %u } }\n",
					frmsize.stepwise.min_width, frmsize.stepwise.min_height,
					frmsize.stepwise.max_width, frmsize.stepwise.max_height);
			} else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
				printf("{ stepwise: min { width = %u, height = %u } .. "
						"max { width = %u, height = %u } "
						"stepsize { width = %u, height = %u } }\n",
						frmsize.stepwise.min_width, frmsize.stepwise.min_height,
						frmsize.stepwise.max_width, frmsize.stepwise.max_height,
						frmsize.stepwise.step_width, frmsize.stepwise.step_height);
			}
			frmsize.index++;
		}
		fmtdesc.index++;
	}

	return 0;
}

static int get_video_fmt(struct asdroid_video *this)
{
	
	if (this->fd < 0)
		return -1;

	printf("Get Video Format...\n");

	memset(&this->fmt, 0, sizeof(this->fmt));

	this->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (ioctl(this->fd, VIDIOC_G_FMT, &this->fmt) < 0) {
		printf("Unable to get video format\n");
		return -1;
	} else {
		printf("Format type:\t\t%d\n", this->fmt.type);
		printf("pix.pixelformat:\t%c%c%c%c\n",
			this->fmt.fmt.pix.pixelformat & 0xFF,
			(this->fmt.fmt.pix.pixelformat >> 8) & 0xFF,
			(this->fmt.fmt.pix.pixelformat >> 16) & 0xFF,
			(this->fmt.fmt.pix.pixelformat >> 24) & 0xFF);
		printf("pix.height:\t\t%d\n", this->fmt.fmt.pix.height);
		printf("pix.width:\t\t%d\n", this->fmt.fmt.pix.width);
		printf("pix.field:\t\t%d\n", this->fmt.fmt.pix.field);
	}

	return 0;
}

static int set_video_fmt(struct asdroid_video *this)
{
	if (this->fd < 0)
		return -1;

	printf("Set Video Format...\n");

	if (ioctl(this->fd, VIDIOC_S_FMT, &this->fmt) < 0) {
		printf("Unable to set video format\n");
		return -1;
	}

	return 0;
}

static int get_video_stream_param(struct asdroid_video *this)
{
	if (this->fd < 0)
		return -1;

	if (ioctl(this->fd, VIDIOC_G_PARM, &this->param) < 0) {
		printf("Unable to get video frame rate\n");
		return -1;	   
	} else {
		printf("numerator:%d\ndenominator:%d\n",
			this->param.parm.capture.timeperframe.numerator,
			this->param.parm.capture.timeperframe.denominator);
	}

	return 0;
}

static int set_video_stream_param(struct asdroid_video *this)
{
	if (this->fd < 0)
		return -1;

	if (ioctl(this->fd, VIDIOC_S_PARM, &this->param) < 0) {
		printf("Unable to set video frame rate\n");
		return -1;
	}

	return 0;
}

static int video_alloc_mem(struct asdroid_video *this)
{
	int n;
	struct v4l2_buffer buf;
	struct v4l2_requestbuffers req;

	if (this->fd < 0)
		return -1;

	req.count = this->frame_num;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	if (ioctl(this->fd, VIDIOC_REQBUFS, &req) < 0) {
		printf("request for buffers error\n");
		return -1;
	}

	this->fb = calloc(req.count, sizeof(struct frame_buf));
	if (!this->fb) {
		printf("out of memory!\n");
		return -1;
	}

	memset(this->fb, 0, req.count * sizeof(struct frame_buf));

	for (n = 0; n < req.count; n++) {
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = n;

		if (ioctl(this->fd, VIDIOC_QUERYBUF, &buf) < 0) {
			printf("query buffer error\n");
			return -1;
		}

		this->fb[n].length = buf.length;
		this->fb[n].start = mmap(NULL, buf.length, PROT_READ|PROT_WRITE, MAP_SHARED,
					 this->fd, buf.m.offset);
		if (this->fb[n].start == MAP_FAILED) {
			printf("buffer map error\n");
			return -1;
		}
	}

    /* queue buffers */
    for (n = 0; n < req.count; n++) {
		memset(&buf, 0, sizeof(struct v4l2_buffer));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = n;

		if (ioctl(this->fd, VIDIOC_QBUF, &buf) < 0) {
			perror("queue buffers failed");
			return -1;
		}
	}

	return 0;	
}

static int camera_is_read_ready(int fd)
{
	int sel_ret;
	fd_set fds;
	struct timeval tv;

	if (fd < 0)
		return -1;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	tv.tv_sec = 2;
	tv.tv_usec = 0;

	sel_ret = select(fd + 1, &fds, NULL, NULL, &tv);
	if (sel_ret == -1) {
		if (errno == EINTR) {
			return -1;
		}

		return -1;
	}

	if (sel_ret == 0) {
		printf("select time out\n");
		return -1;
	}

	return 0;
}

static int video_stream_on(struct asdroid_video *this)
{
	enum v4l2_buf_type type;
	int ret;

	if (this->fd < 0)
		return -1;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(this->fd, VIDIOC_STREAMON, &type) < 0) {
		printf("video stream on failed\n");
		return -1;
	}

	this->status = STREAM_ON;
	return 0;
}

static int video_stream_off(struct asdroid_video *this)
{
	enum v4l2_buf_type type;
	int ret;

	if (this->fd < 0)
		return -1;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(this->fd, VIDIOC_STREAMOFF, &type) < 0) {
		printf("video stream on failed\n");
		return -1;
	}

	this->status = STREAM_OFF;
	return 0;
}

static int video_capture(struct asdroid_video *this, video_callback_t callback, void *arg)
{
	struct v4l2_buffer buf;
	u64 extra_time = 0;
	u64 cur_time = 0;
	u64 last_time = 0;
	int n;

	if (this->fd < 0)
		return -1;

	if (this->status == STREAM_OFF && this->video_ops.stream_on)
		this->video_ops.stream_on(this);
	else
		return -1;

	while (1) {
		for(n = 0; n < this->frame_num; n++) {
			if (camera_is_read_ready(this->fd) < 0)
				return -1;

			memset(&buf, 0, sizeof(buf));
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = n;
			ioctl(this->fd, VIDIOC_DQBUF, &buf);

			this->fb[n].timestamp = buf.timestamp.tv_sec * 1000000 + buf.timestamp.tv_usec;
			cur_time = this->fb[n].timestamp;
			extra_time = cur_time - last_time;
			last_time = cur_time;
			printf("time_deta:%llu\n", extra_time);
			printf("buf_len:%d\n", this->fb[n].length);
			printf("grab image data OK\n");

			/* TODO */
			callback(this, arg, n);

			ioctl(this->fd, VIDIOC_QBUF, &buf);
		}
	}
	return 0;
}

int register_video_ops(struct asdroid_video *this)
{
	if (!this)
		return -1;

	this->video_ops.open = video_open;
	this->video_ops.release = video_release;
	this->video_ops.get_capability = get_video_capability;
	this->video_ops.get_fmt = get_video_fmt;
	this->video_ops.set_fmt = set_video_fmt;
	this->video_ops.get_stream_param = get_video_stream_param;
	this->video_ops.set_stream_param = set_video_stream_param;
	this->video_ops.alloc_mem = video_alloc_mem;
	this->video_ops.stream_on = video_stream_on;
	this->video_ops.stream_off = video_stream_off;
	this->video_ops.capture = video_capture;

	return 0;
}

void unregister_video_ops(struct asdroid_video *this)
{
	if (this)
		memset(this, 0, sizeof(*this));
}
