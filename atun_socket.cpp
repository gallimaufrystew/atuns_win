/*
 * atun_socket.cpp
 */

#include "atun_socket.h"
#include "atun_conn.h"
#include "atun_select.h"
#include "atun_config.h"

extern port_map_t port_map;

atun_sock_t atun_listen_init()
{
    atun_sock_t ls;
    struct sockaddr_in addr = {};

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_map[443].second);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls < 0) {
        // fatal...
		std::cout << "11" << "\n";
        return ATUN_ERROR;
    }

    int on = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(int));

    if (bind(ls, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        // fatal...
		std::cout << "22 " << sock_errno << "\n";
        return ATUN_ERROR;
    }

    if (listen(ls, ATUN_CONFIG_BACKLOG) < 0) {
        // fatal...
		std::cout << "33" << "\n";
        return ATUN_ERROR;
    }

    return ls;
}

static void *fill_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

atun_int_t valid_ip(const std::string &host, sockaddr_in &sa)
{
    atun_int_t res = inet_pton(AF_INET, host.c_str(), &(sa.sin_addr));
    return res == 1;
}

atun_int_t
async_connect_by_hostname(std::string &host, atun_port_t port)
{
	atun_sock_t   sock;
	int           ret;
    struct addrinfo hints = {}, *sainfo, *p;
    char  addr_text[INET6_ADDRSTRLEN] = {};

    hints.ai_family = AF_INET;// AF_UNSPEC AF_INET6
    hints.ai_socktype = SOCK_STREAM;

    char service[128] = {};
    std::sprintf(service, "%d", port);

    if ((ret = getaddrinfo(host.c_str(), service, &hints, &sainfo)) != 0) {
        fprintf(stderr, "connect by hostname: %s\n", gai_strerror(ret));
        return ATUN_ERROR;
    }

    for (p = sainfo; p ; p = p->ai_next) {

        if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            continue;
        }

        atun_set_nonblock(sock);

        atun_int_t ret = async_connect(sock, p->ai_addr, p->ai_addrlen);
        if (ret < 0) {
            atun_close_sock(sock);
            continue;
        }

        break;
    }

    if (p == nullptr) {
        std::cout << "connect have exhausted all..." << "\n";
        return ATUN_ERROR;
    }

    void *addr = fill_in_addr((struct sockaddr *)p->ai_addr);
    inet_ntop(p->ai_family, addr, addr_text, sizeof(addr_text));

    std::cout << "connection to upstream " << addr_text << "\n";

    freeaddrinfo(sainfo);

    return sock;
}

static atun_int_t
check_connect_status(atun_sock_t fd)
{
    fd_set rset, wset;
    struct timeval tv = {10, 0};

    FD_ZERO(&rset);
    FD_SET(fd, &rset);

    wset = rset;

#if __linux__
	atun_int_t ret = select(fd + 1, &rset, &wset, nullptr, &tv);
#elif _WIN32
	atun_int_t ret = select(0, &rset, &wset, nullptr, &tv);
#endif

    if (ret == 0) {
        std::cout << "select timeout" << "\n";
        return ATUN_ERROR;
    }

    if (ret < 0) {
        std::cout << "select err:" << sock_errno << "\n";
        return ATUN_ERROR;
    }

    if (FD_ISSET(fd, &rset) || FD_ISSET(fd, &wset)) {

        atun_err_t err;
        socklen_t len = sizeof(err);

		std::cout << "check sock status\n";

        // If an error occurred, Berkeley-derived implementations
        // of getsockopt return 0 with the pending error returned
        // in our variable error. But Solaris causes getsockopt
        // itself to return –1 with errno set to the pending error
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&err, &len) == -1) {
            // possible?
            return ATUN_ERROR;
        }

        if (err) {
            std::fprintf(stderr, "connect: %d\n", sock_errno);
            return ATUN_ERROR;
        }
    }

    return fd;
}

atun_int_t async_connect(atun_sock_t fd, sockaddr *addr, atun_int_t sock_len)
{
    int ret = connect(fd, addr, (int32_t)sock_len);
    if (ret == 0) {
		std::cout << "connect 11\n";
        return fd;
    }

	if (sock_errno == ERR_CONNECT_INPROGRESS) {
		std::cout << "connect 22\n";
        return check_connect_status(fd);
    }

	std::cout << "connect 33" << ret << " " << sock_errno << "\n";

    return ATUN_ERROR;
}

bool atun_sock_can_try_again()
{
#if __linux__
	return sock_errno == EAGAIN || sock_errno == EINTR;
#elif _WIN32
	return sock_errno == WSAEWOULDBLOCK;
#endif
}

void atun_close_sock(atun_sock_t fd)
{
#if __linux__
    close(fd);
#elif _WIN32
    closesocket(fd);
#endif
}

#if __linux__

#if (HAVE_IOCTL)

/* if possible call ioctl
 * save one system call
 */
atun_int_t atun_set_nonblock(atun_sock_t fd)
{
    atun_int_t nb = 1;
    return ioctl(fd, FIONBIO, &nb);
}

atun_int_t atun_set_block(atun_sock_t fd)
{
    atun_int_t nb = 0;
    return ioctl(fd, FIONBIO, &nb);
}

#else

/* fcntl, standardized by POSIX
 * call this function for consistent behavior
 */
atun_int_t atun_ioctl(atun_sock_t fd, bool non_block)
{

    int flags;

    /* Set the socket blocking (if non_block is zero) or non-blocking.
     * Note that fcntl(2) for F_GETFL and F_SETFL can't be
     * interrupted by a signal.
     */
    if ((flags = fcntl(fd, F_GETFL)) == -1) {
        return -1;
    }

    if (non_block) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    if (fcntl(fd, F_SETFL, flags) == -1) {
        return -1;
    }
    return 0;
}

atun_int_t atun_set_nonblock(atun_sock_t fd)
{
    return atun_ioctl(fd, true);
}

atun_int_t atun_set_block(atun_sock_t fd)
{
    return atun_ioctl(fd, false);
}

#endif

#elif _WIN32

atun_int_t atun_set_nonblock(atun_sock_t fd)
{
    unsigned long  nb = 1;
    return ioctlsocket(fd, FIONBIO, &nb);
}

atun_int_t atun_set_block(atun_sock_t fd)
{
    unsigned long  nb = 0;
    return ioctlsocket(fd, FIONBIO, &nb);
}

#endif
