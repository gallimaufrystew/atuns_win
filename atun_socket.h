/*
 * File:   tun_socket.h
 * Author: 19020107
 *
 * Created on April 11, 2018
 */

#ifndef TUN_SOCKET_INCLUDED_H
#define TUN_SOCKET_INCLUDED_H

#include "atun_sys.h"
#include "atun_config.h"

#if __linux__

typedef int atun_sock_t;

#define sock_errno errno
#define ERR_CONNECT_INPROGRESS  EINPROGRESS

#else

typedef SOCKET atun_sock_t;

#define sock_errno WSAGetLastError()
#define ERR_CONNECT_INPROGRESS  WSAEWOULDBLOCK

#endif

void atun_close_sock(atun_sock_t fd);
atun_int_t atun_set_nonblock(atun_sock_t fd);
atun_int_t atun_set_block(atun_sock_t fd);
atun_sock_t atun_listen_init();
atun_int_t valid_ip(const std::string &host, sockaddr_in &sa);
atun_int_t async_connect(atun_sock_t fd, sockaddr *addr, atun_int_t sock_len);
bool atun_sock_can_try_again();
atun_int_t async_connect_by_hostname(std::string &host, atun_port_t port);

#endif /* TUN_SOCKET_INCLUDED_H */
