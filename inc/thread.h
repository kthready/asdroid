#ifndef ASSISTANT_THREAD_H
#define ASSISTANT_THREAD_H

enum thread_type {
	PHONE,
	ASDROID
};

struct thread_info {
	pthread_t tid;
	int port;
	void *(*thread_socket)(void *);
	void *(*thread_work)(void *);
	void *data;
};

#endif
