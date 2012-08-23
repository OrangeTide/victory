#ifndef NET_H
#define NET_H

struct net_listen {
	int fd;
};

struct net_socket {
	int fd;
};

int net_listen(struct net_listen *handle, const char *node, const char *service,
	size_t desc_len, char *desc);
int net_accept(struct net_listen *listen_handle, struct net_socket *socket,
	size_t desc_len, char *desc);
#endif
