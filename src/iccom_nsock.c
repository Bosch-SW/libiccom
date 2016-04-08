/**********************************************************************
* Copyright (c) 2021 Robert Bosch GmbH
* Artem Gulyaev <Artem.Gulyaev@de.bosch.com>
*
* This code is licensed under the Mozilla Public License Version 2.0
* License text is available in the file ’LICENSE.txt’, which is part of
* this source code package.
*
* SPDX-identifier: MPL-2.0
*
**********************************************************************/

/* This file provides the ICCom network sockets driver convenience
 * interface to the user space programs for local debugging and
 * testing purposes without involving actual target system.
 *
 * NOTE: the ICCom library IF remains unchanged and binary compatible
 *  to the standard ICCom library.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <linux/netlink.h>

#include "iccom.h"
#include "utils.h"

// DEV STACK
// @@@@@@@@@@@@@
//
// BACKLOG:
//
// @@@@@@@@@@@@@
// DEV STACK END

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------- BUILD TIME CONFIGURATION ----------------------- */
// if defined then debug messages are printed
//#define ICCOM_API_DEBUG

/* -------------------- MACRO DEFINITIONS ------------------------------ */

/* -------------------- FORWARD DECLARATIONS --------------------------- */

int __iccom_receive_data_pure(const int sock_fd, void *const receive_buffer
                              , const size_t buffer_size);

/* ------------------- GLOBAL VARIABLES / CONSTANTS -------------------- */

#define SERVICE_STR_SIZE 64
#define HOST_STR_SIZE 256

// @target_address identifies the target address
//      to connect to (port  == channel number, but IP is undefined).
struct iccom_lib_cfg {
        char target_host_address[HOST_STR_SIZE];
};

// NOTE: for now the target host is static: localhost
struct iccom_lib_cfg iccom_current_config = {"localhost"};


/* ------------------- ICCOM SOCKETS CONVENIENCE API ------------------- */

// TODO: migrate to new call with target host parameter.
//
// NOTE: the channel number corresponds to the target **server** socket
//      number.
//
// See iccom.h
int iccom_open_socket(const unsigned int channel)
{
        log("ICCom lib in network sockets mode, opening: %s:%d"
            , iccom_current_config.target_host_address, channel);
        if (iccom_channel_verify(channel) < 0) {
                log("Failed to open the socket: "
                    "channel (%d) is out of bounds see "
                    "iccom_channel_verify(...) for more info."
                    , channel);
                return -EINVAL;
        }

        // iccom works for now as a client, testing-server apps
        // will most probably use the std network socket
        // libs

        // SEE: https://man7.org/linux/man-pages/man3/getaddrinfo.3.html
        //      for example

        struct addrinfo hints;
        char server_service[SERVICE_STR_SIZE];
        if (snprintf(&server_service[0], sizeof(server_service), "%d", channel)
                    >= sizeof(server_service)) {
                log("Target server service was truncated from %d. "
                    "Failed to open channel.", channel);
                return -EINVAL;
        }

        int res;
        struct addrinfo *addr_list;

        // Obtain address(es) matching host/port
        // * IPv4/IPv6; TCP; Any protocol;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = 0;
        hints.ai_protocol = 0;

        res = getaddrinfo(iccom_current_config.target_host_address
                          , server_service, &hints, &addr_list);
        if (res != 0) {
                log("Failed to open channel. Error in server address "
                    "resolution: %s", gai_strerror(res));
                return -EINVAL;
        }

        struct addrinfo *curr_addr;
        int sock_fd;
        for (curr_addr = addr_list; curr_addr != NULL; curr_addr = curr_addr->ai_next) {
                sock_fd = socket(curr_addr->ai_family, curr_addr->ai_socktype
                                 , curr_addr->ai_protocol);
                if (sock_fd == -1) {
                        int err = errno;
                        log("Note: couldn't open one of addr candidates on "
                            "channel: %d, host: %s; error: %d(%s)", channel
                            , iccom_current_config.target_host_address
                            , err, strerror(err));
                        continue;
                }

                res = connect(sock_fd, curr_addr->ai_addr, curr_addr->ai_addrlen);
                if (res == -1) {
                        int err = errno;
                        log("Note: couldn't connect to one of addr candidates on "
                            "channel: %d, host: %s; error: %d(%s)", channel
                            , iccom_current_config.target_host_address
                            , err, strerror(err));
                        close(sock_fd);
                        continue;
                }

                // connected
                break;
        }
        freeaddrinfo(addr_list);

        if (curr_addr == NULL) {
                log("ERROR: could not connect to any address channel: %d"
                    ", host: %s", channel, iccom_current_config.target_host_address);
                return -EPIPE;
        }

        return sock_fd;
}

// See iccom.h
int iccom_set_socket_read_timeout(const int sock_fd, const int ms)
{
        if (ms < 0) {
                log("Number of milliseconds shoud be >= 0");
                return -EINVAL;
        }
        struct timeval timeout;
        timeout.tv_sec = ms / 1000;
        timeout.tv_usec = (ms - timeout.tv_sec * 1000) * 1000;

        int res = setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO
                             , &timeout, sizeof(timeout));
        if (res != 0) {
                int err = errno;
                log("Failed to set the timeout %dms for socket %d"
                    ", error: %d(%s)"
                    , ms, sock_fd, err, strerror(err));
                return -err;
        }
        return 0;
}

// See iccom.h
int iccom_get_socket_read_timeout(const int sock_fd)
{
        struct timeval timeout;
        unsigned int size = sizeof(timeout);
        int res = getsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO
                             , (void*)&timeout, &size);

        if (res != 0) {
                int err = errno;
                log("Failed to get the timeout value for socket %d"
                    ", error: %d(%s)"
                    , sock_fd, err, strerror(err));
                return -err;
        }

        return timeout.tv_sec * 1000 + timeout.tv_usec / 1000;
}

// See iccom.h
void iccom_close_socket(const int sock_fd)
{
        if (close(sock_fd) < 0) {
                int err = errno;
                log("Failed to close the socket %d; "
                    "error code: %d(%s)", sock_fd
                    , err, strerror(err));
        }
}

// See iccom.h
int iccom_send_data_nocopy(const int sock_fd, const void *const buf
                           , const size_t buf_size_bytes
                           , const size_t data_offset
                           , const size_t data_size_bytes)
{
        if (buf_size_bytes != NLMSG_SPACE(data_size_bytes)) {
                log("Buffer size %zu doesn't match data size %zu."
                    , buf_size_bytes, data_size_bytes);
                return -EINVAL;
        }
        if (data_offset != NLMSG_LENGTH(0)) {
                log("The user data (message) offset %zu doesn't"
                    " match expected value: %d."
                    , data_offset, NLMSG_LENGTH(0));
                return -EINVAL;
        }
        if (data_size_bytes > ICCOM_SOCKET_MAX_MESSAGE_SIZE_BYTES) {
                log("Can't send messages larger than: %d bytes."
                    , ICCOM_SOCKET_MAX_MESSAGE_SIZE_BYTES);
                return -E2BIG;
        }
        if (data_size_bytes == 0) {
                log("Message to send is of zero size. Nothing to send");
                return -EINVAL;
        }
        if (!buf) {
                log("Null buffer pointer. Nothing to send.");
                return -EINVAL;
        }

        // we use the same netlink configuration for now
        // to keep old apps running, even those ones which
        // use directly accessible netlink buffer
        struct nlmsghdr *const nl_msg = (struct nlmsghdr *const)buf;

        memset(nl_msg, 0, sizeof(*nl_msg));
        nl_msg->nlmsg_len = data_size_bytes;

        ssize_t res = write(sock_fd, buf, buf_size_bytes);

        if (res < 0) {
                int err = errno;
                log("Sending of the message to channel failed, error:"
                       " %d(%s)", err, strerror(err));
                return -err;
        }
        if (res != buf_size_bytes) {
                log("Message  truncation occured.");
                return -EPIPE;
        }

        return 0;
}

// See iccom.h
int iccom_send_data(const int sock_fd, const void *const data
                    , const size_t data_size_bytes)
{
        if (data_size_bytes > ICCOM_SOCKET_MAX_MESSAGE_SIZE_BYTES) {
                log("Can't send messages larger than: %d bytes."
                    , ICCOM_SOCKET_MAX_MESSAGE_SIZE_BYTES);
                return -E2BIG;
        }
        if (data_size_bytes <= 0) {
                log("Zero data size. Nothing to send.");
                return -EINVAL;
        }
        if (!data) {
                log("Null data pointer. Nothing to send.");
                return -EINVAL;
        }

        // TODO: drop this transformation in next ICCom version,
        //     when ICCom migrates to network sockets.
        const size_t nl_total_msg_size = NLMSG_SPACE(data_size_bytes);
        struct nlmsghdr *nl_msg = (struct nlmsghdr *)malloc(nl_total_msg_size);
        if (!nl_msg) {
                log("Could not allocate message buffer of size: %zu"
                    , nl_total_msg_size);
                return -ENOMEM;
        }

        memcpy(NLMSG_DATA(nl_msg), data, data_size_bytes);

        int res = iccom_send_data_nocopy(sock_fd, (void*)nl_msg
                           , nl_total_msg_size
                           , NLMSG_LENGTH(0)
                           , data_size_bytes);

        free(nl_msg);

        return res;
}

// See iccom.h
int iccom_receive_data_nocopy(
                const int sock_fd, void *const receive_buffer
                , const size_t buffer_size, int *const data_offset__out)
{
        if (buffer_size <= NLMSG_SPACE(0)) {
                log("incoming buffer size %zu is too small for netlink message"
                    " (min size is %d)", buffer_size, NLMSG_SPACE(0));
                return -ENFILE;
        }
        if (!data_offset__out) {
                log("data_offset__out is not set.");
                return -EINVAL;
        }

        ssize_t len = read(sock_fd, receive_buffer, buffer_size);

        if (len < 0) {
                int err = errno;
                // timeout not an error
                if (err == EAGAIN) {
                        return 0;
                }
                log("Error reading data from socket (fd: %d): %d(%s)"
                    , sock_fd, err, strerror(err));
                return -err;
        } else if (len == 0) {
                // ICCOM: interrupted from read by signal
                //  before any data came,
                // ICCOM OVER TCP: OR: the socket has been closed
                return 0;
        }

        if (len < sizeof(struct nlmsghdr)) {
                log("The truncated data received from the socket: %d. "
                    "Dropping message.", sock_fd);
                return -EBADE;
        }

        struct nlmsghdr *const nl_header = (struct nlmsghdr *)receive_buffer;
        const size_t data_size_bytes = nl_header->nlmsg_len;
        const size_t nl_total_msg_size = NLMSG_SPACE(data_size_bytes);

        if (len != nl_total_msg_size) {
                log("Inconsistent data lenght declared (%lu) and "
                    "actual data size (%ld). Socket: %d."
                    "Dropping message.", nl_total_msg_size, len, sock_fd);
                return -EBADE;
        }

        *data_offset__out = NLMSG_LENGTH(0);
        return data_size_bytes;
}

// See iccom.h
// TODO: rename __iccom_receive_data_pure into iccom_receive_data
//       and this version of iccom_receive_data to be deleted
//       NOTE: THIS WILL REQUIRE ALL DEPENDENT PROJECT TO MIGRATE
int iccom_receive_data(const int sock_fd, void *const receive_buffer
                       , const size_t buffer_size, int *const data_offset__out)
{
    return iccom_receive_data_nocopy(sock_fd, receive_buffer
                                     , buffer_size, data_offset__out);
}

// NOTE: NOT TO BE EXPORTED NOR EXPOSED FOR NOW (no external use expected
//      at least)
// TODO: to be renamed later to iccom_receive_data(...)
//      and iccom_receive_data(...) -> iccom_receive_data_nocopy(...)
//
// Exactly the same as iccom_receive_data(...) but moves the received
// data to the beginning of the provided buffer. This introduces overhead
// surely, but somtimes convenient.
//
// NOTE: receive buffer is still used to get the whole netlink message
//      so, it must be big enough to contain netlink header + padding
//      + message data.
int __iccom_receive_data_pure(const int sock_fd, void *const receive_buffer
                            , const size_t buffer_size)
{
        int data_offset = 0;
        int res = iccom_receive_data(sock_fd, receive_buffer, buffer_size
                                     , &data_offset);

        if (res <= 0) {
            return res;
        }

        memmove(receive_buffer, ((char*)receive_buffer) + data_offset, res);
        return res;
}


#ifdef __cplusplus
} /* extern C */
#endif
