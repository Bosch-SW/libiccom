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

/* This file provides the ICCom sockets driver convenience
 * interface to the user space programs, by avoiding a lot of
 * boiler plate in ICCom sockets communication establishing.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#include "iccom.h"
#include "utils.h"

// DEV STACK
// @@@@@@@@@@@@@
//
//      * Review/verify the library code.
//
// BACKLOG:
//
//      * Add assertions to the IccomSocket class methods
//      * avoid using function names to provide
//        the log prefix for printing out the channel data
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

// If defined, then ICCom is to be built with developer
// user hints information (undefine it if code footprint size is
// relevant).
//
// NOTE: this macro in general is propagated from build system
//      build options
//
//#define ICCOM_HINTS

/* -------------------- FORWARD DECLARATIONS --------------------------- */

int __iccom_receive_data_pure(const int sock_fd, void *const receive_buffer
                              , const size_t buffer_size);

/* ------------------- GLOBAL VARIABLES / CONSTANTS -------------------- */

// the destination address: "to kernel"
// NOTE: it is const by use, don't whant to make ugly const cast
static struct sockaddr_nl dest_addr = {
        .nl_family = AF_NETLINK
        , .nl_pad = 0
        , .nl_pid = 0 /* For Linux Kernel */
        , .nl_groups = 0 /* unicast */
};

// the sender address: "from kernel"
// NOTE: it is const by use, don't whant to make ugly const cast
static struct sockaddr_nl remote_addr = {
        .nl_family = AF_NETLINK
        , .nl_pad = 0
        , .nl_pid = 0 /* From Linux Kernel */
        , .nl_groups = 0 /* unicast */
};

/* ------------------- ICCOM SOCKETS CONVENIENCE API ------------------- */

// See iccom.h
int iccom_open_socket(const unsigned int channel)
{
        if (iccom_channel_verify(channel) < 0) {
                log("Failed to open the netlink socket: "
                    "channel (%d) is out of bounds see "
                    "iccom_channel_verify(...) for more info."
                    , channel);
                return -EINVAL;
        }

        int sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ICCOM);
        if (sock_fd < 0) {
                int err = errno;
                log("Failed to open the netlink socket: "
                    "netlink_family: %d; error code: %d(%s)"
                    , NETLINK_ICCOM, err, strerror(err));
#ifdef ICCOM_HINTS
                 if (err == EPROTONOSUPPORT) {
                    log("\n\nHINT: this usually means that ICCom v1.0 kernel\n"
                        "    module is not installed/inserted in the kernel.\n"
                        "HINT: if you just want to use ordinary Bekerly\n"
                        "    sockets for ICCom communication (compatible with\n"
                        "    backend mock application and with ICCom v2.0) then\n"
                        "    compile the libiccom with ICCOM_USE_NETWORK_SOCKETS\n"
                        "    build option.\n");
                }
#endif
                return -err;
        }

        struct sockaddr_nl src_addr;
        memset(&src_addr, 0, sizeof(src_addr));
        src_addr.nl_family = AF_NETLINK;
        // For ICCom we always bind to the port id = desired channel id
        // as long in ICCom we are bound to the named channel end-to-end
        // pipe instead of to the port on one of its ends.
        src_addr.nl_pid = channel;

        int res = bind(sock_fd, (struct sockaddr*)&src_addr
                       , sizeof(src_addr));
        if (res < 0) {
                int err = errno;
                log("Failed to bind the socket to channel %d; "
                    "error code: %d(%s)", channel, err
                    , strerror(err));
                log("Closing the socket.");
                iccom_close_socket(sock_fd);
                return -err;
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

        struct nlmsghdr *const nl_msg = (struct nlmsghdr *const)buf;

        memset(nl_msg, 0, sizeof(*nl_msg));
        nl_msg->nlmsg_len = NLMSG_LENGTH(data_size_bytes);

        struct iovec iov = { (void *)nl_msg, nl_msg->nlmsg_len };
        const struct msghdr msg = { &dest_addr, sizeof(dest_addr),
                                    &iov, 1, NULL, 0, 0 };

#ifdef ICCOM_API_DEBUG
        if (!NLMSG_OK(nl_msg, buf_size_bytes)) {
                log("Netlink header data incorrect, TX buff:");
                log("    [SND] ---- netlink packet data begin ----");
                iccom_print_hex_dump_prefixed(nl_msg, buf_size_bytes
                                              , LIBICCOM_LOG_PREFIX
                                                "iccom_send_data_nocopy:    ");
                log("    [SND] ---- netlink packet data end   ----");
                return -EPIPE;
        }

        log("Libiccom: SND");
        log("    original payload size: %zu", data_size_bytes);
        log("    nlmsg_len: %d", nl_msg->nlmsg_len);
        log("    payload size: %d", NLMSG_PAYLOAD(nl_msg, 0));
        log("    [SND] ------ payload data begin --------");
        iccom_print_hex_dump_prefixed((const char*)NLMSG_DATA(nl_msg)
                                      , data_size_bytes
                                      , LIBICCOM_LOG_PREFIX
                                        "iccom_send_data_nocopy:     ");
        log("    [SND] ------- payload data end ---------");
#endif

        ssize_t res = sendmsg(sock_fd, &msg, 0);
        if (res < 0) {
                int err = errno;
                log("sending of the message failed, error:"
                       " %d(%s)", err, strerror(err));
                return -err;
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

        struct nlmsghdr *const nl_header = (struct nlmsghdr *)receive_buffer;

        struct iovec iov = { receive_buffer, buffer_size };
        struct msghdr msg = { &remote_addr, sizeof(remote_addr),
                              &iov, 1, NULL, 0, 0 };

        ssize_t len = recvmsg(sock_fd, &msg, MSG_WAITALL | MSG_TRUNC);

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
                // interrupted from read by signal
                return 0;
        }

        if (msg.msg_flags & MSG_TRUNC) {
                log("The message from socket (fs: %d) was truncated"
                    " and part of it was lost. Dropping message.", sock_fd);
                return -EOVERFLOW;
        }
        if (msg.msg_flags & MSG_CTRUNC) {
                log("The message control data from socket (fs: %d)"
                    " was truncated. Dropping message", sock_fd);
                return -EOVERFLOW;
        }
        if (msg.msg_flags & MSG_ERRQUEUE) {
                log("The socket error message was received"
                    " from socket (fs: %d). Dropping message.", sock_fd);
                return -EBADE;
        }

        if (!NLMSG_OK(nl_header, len)) {
                log("Netlink header data incorrect.");
                log("    Packet received len: %ld", len);
                log("    Packet declared len: %d", nl_header->nlmsg_len);
                log("    Packet type: %d", nl_header->nlmsg_type);
                log("    Sizeof NL header: %lu", sizeof(nl_header));
                log("RX buffer (whole):");
                log("    [RCV] ---- netlink message data begin ----");
                iccom_print_hex_dump_prefixed(
                                (const char * const)receive_buffer
                                , buffer_size
                                , LIBICCOM_LOG_PREFIX
                                  "iccom_receive_data_nocopy:     ");
                log("    [RCV] ----- netlink message data end -----");
                return -EPIPE;
        }

        int data_len = NLMSG_PAYLOAD(nl_header, 0);
        *data_offset__out = NLMSG_LENGTH(0);

#ifdef ICCOM_API_DEBUG
        log("Libiccom: RCV");
        log("    msg:");
        log("      nlmsg_len: %d", nl_header->nlmsg_len);
        log("      nlmsg_type: %d", nl_header->nlmsg_type);
        log("      nlmsg_flags: %d", nl_header->nlmsg_flags);
        log("      nlmsg_seq: %d", nl_header->nlmsg_seq);
        log("      nlmsg_pid: %d", nl_header->nlmsg_pid);
        log("      payload size: %d", data_len);
        log("    [RCV] ------ payload data begin --------");
        iccom_print_hex_dump_prefixed(
                        (const char*)NLMSG_DATA(nl_header)
                        , data_len
                        , LIBICCOM_LOG_PREFIX
                          "iccom_receive_data_nocopy:     ");
        log("    [RCV] ------- payload data end ---------");
#endif

        return data_len;
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

int iccom_loopback_enable(const unsigned int from_ch, const unsigned int to_ch
                          , const int range_shift)
{
        if (to_ch < from_ch) {
                log("to_ch (%d) must be > from_ch (%d)", to_ch, from_ch);
                return -EINVAL;
        }
        if (__iccom_channel_verify(from_ch, ICCOM_CHANNEL_AREA_PRIME
                                   , "from_ch") < 0) {
                return -EINVAL;
        }
        if (__iccom_channel_verify(to_ch, ICCOM_CHANNEL_AREA_PRIME
                                   , "to_ch") < 0) {
                return -EINVAL;
        }
        if (((int)to_ch) < -range_shift) {
                log("range_shift can not shift to the negative area");
                return -EINVAL;
        }
        const unsigned int dst_from_ch = from_ch + range_shift;
        const unsigned int dst_to_ch = to_ch + range_shift;
        if (__iccom_channel_verify(dst_from_ch, ICCOM_CHANNEL_AREA_ANY
                                   , "shifted from_ch") < 0) {
                return -EINVAL;
        }
        if (__iccom_channel_verify(dst_to_ch, ICCOM_CHANNEL_AREA_ANY
                                   , "shifted to_ch") < 0) {
                return -EINVAL;
        }
        if ((dst_from_ch <= to_ch) && (dst_to_ch >= from_ch)) {
                log("range_shift should shift the channel region in such a way"
                    " which avoids overlapping of original and resulting"
                    " regions");
                return -EINVAL;
        }

        FILE *ctl_file = fopen(ICCOM_LOOPBACK_IF_CTRL_FILE_PATH, "w");

        if (!ctl_file) {
                const int err = errno;
                log("ICCom IF loopback ctl file open failed, error: %d", err);
                log("this might be caused either by permissions, either by "
                    " non-existing file (which means that ICCom Sockets driver"
                    " is not loaded)");
                return -EBADF;
        }

        int ret_val = 0;
        if (fprintf(ctl_file, "%d %d %d\n", from_ch, to_ch, range_shift) < 0) {
                const int err = ferror(ctl_file);
                log("ICCom IF loopback ctl file write failed, error: %d", err);
                ret_val = -EIO;
        }

        fclose(ctl_file);
        return 0;
}

int iccom_loopback_disable(void)
{
        FILE *ctl_file = fopen(ICCOM_LOOPBACK_IF_CTRL_FILE_PATH, "w");

        if (!ctl_file) {
                const int err = errno;
                log("ICCom IF loopback ctl file open failed, error: %d", err);
                log("this might be caused either by permissions, either by "
                    " non-existing file (which means that ICCom Sockets driver"
                    " is not loaded)");
                return -err;
        }

        int ret_val = 0;
        if (fprintf(ctl_file, "0 0 0\n") < 0) {
                const int err = ferror(ctl_file);
                log("ICCom IF loopback ctl file write failed, error: %d", err);
                ret_val = -err;
        }

        fclose(ctl_file);
        return ret_val;
}

char iccom_loopback_is_active(void)
{
        loopback_cfg lb_cfg;
        if (iccom_loopback_get(&lb_cfg) < 0) {
                return 0;
        }
        return (lb_cfg.range_shift != 0) ? 1 : 0;
}

int iccom_loopback_get(loopback_cfg *const out)
{
        if (out == NULL) {
                log("no output ptr is provided");
                return -EINVAL;
        }

        FILE *ctl_file = fopen(ICCOM_LOOPBACK_IF_CTRL_FILE_PATH, "r");

        if (!ctl_file) {
                const int err = errno;
                log("ICCom IF loopback ctl file open failed, error: %d", err);
                log("this might be caused either by permissions, either by "
                    " non-existing file (which means that ICCom Sockets driver"
                    " is not loaded)");
                return -err;
        }

        const int paramenters_count = 3;
        int ret_val = 0;
        if (fscanf(ctl_file, "%d %d %d", &(out->from_ch), &(out->to_ch)
                   , &(out->range_shift)) != paramenters_count) {
                const int err = errno;
                log("ICCom IF loopback ctl read&parsing op, file error: %d,"
                    " gen error: %d", ferror(ctl_file), err);
                ret_val = -EIO;
        }

        fclose(ctl_file);
        return ret_val;
}


#ifdef __cplusplus
} /* extern C */
#endif

