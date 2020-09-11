#ifndef ASSISTANT_VIDEO_CAPTURE_H
#define ASSISTANT_VIDEO_CAPTURE_H

#include <common.h>
#include <linux/videodev2.h>

#define VIDEO_DEV		"/dev/video0"
#define FRAME_NUM		4
#define IMAGEWIDTH		1280
#define IMAGEHEIGHT		720

struct asdroid_video;
typedef void video_callback_t(struct asdroid_video *, void *, int);

enum stream_status {
	STREAM_ON,
	STREAM_OFF
};

struct frame_buf {
	void *start;
	u32 length;
	u64 timestamp;
};

struct asdroid_video_ops {
	int (*open)(struct asdroid_video *);
	void (*release)(struct asdroid_video *);
	int (*get_capability)(struct asdroid_video *);
	int (*get_support_format)(struct asdroid_video *);
	int (*get_fmt)(struct asdroid_video *);
	int (*set_fmt)(struct asdroid_video *);
	int (*get_stream_param)(struct asdroid_video *);
	int (*set_stream_param)(struct asdroid_video *);
	int (*alloc_mem)(struct asdroid_video *);
	int (*stream_on)(struct asdroid_video *);
	int (*stream_off)(struct asdroid_video *);
	int (*capture)(struct asdroid_video *, video_callback_t, void *);
};

struct asdroid_video {
	int fd;
	char *dev_name;
	u32 frame_num;
	enum stream_status status;
	struct frame_buf *fb;
	struct v4l2_format fmt;
	struct v4l2_capability cap;
	struct v4l2_streamparm param;
	struct asdroid_video_ops video_ops;
};

int register_video_ops(struct asdroid_video *this);
void unregister_video_ops(struct asdroid_video *this);

#endif