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

/* This file provides the ICCom sockets driver python adapter
 * to run ICCom sockets directly from python scripts.
 */

// NOTE: order matters, Python.h should be the first include
//      see: https://docs.python.org/3/extending/extending.html
#define PY_SSIZE_T_CLEAN

// the only way to work with `python setup.py` script
#if    !defined(PYTHON_TGT_MINOR_VERSION_6)     \
    && !defined(PYTHON_TGT_MINOR_VERSION_7)     \
    && !defined(PYTHON_TGT_MINOR_VERSION_8)
#define PYTHON_TGT_MINOR_VERSION_7
#endif

#ifdef PYTHON_TGT_MINOR_VERSION_6
#include "python3.6/Python.h"
#include "python3.6/structmember.h"
#endif

#ifdef PYTHON_TGT_MINOR_VERSION_7
#include "python3.7m/Python.h"
#include "python3.7m/structmember.h"
#endif

#ifdef PYTHON_TGT_MINOR_VERSION_8
#include "python3.8/Python.h"
#include "python3.8/structmember.h"
#endif

#include "iccom.h"
#include <stdio.h>
#include <stddef.h> /* For offsetof */

/* ---------------- Python adapter part constants ---------------------- */

static const int EBUF_LEN = 512;

/* ---------------- Python adapter part forward declarations ----------- */


static PyObject *iccom_socket_open_py(PyObject *self, PyObject *args);
static PyObject *iccom_socket_close_py(PyObject *self, PyObject *args);
static PyObject *iccom_send_py(PyObject *self, PyObject *args);
static PyObject *iccom_receive_py(PyObject *self, PyObject *args);

static PyObject *iccom_channel_verify_py(PyObject *self, PyObject *args);
static PyObject *iccom_get_socket_read_timeout_py(PyObject *self, PyObject *args);
static PyObject *iccom_set_socket_read_timeout_py(PyObject *self, PyObject *args);

static PyObject *iccom_loopback_get_py(PyObject *self, PyObject *args);
static PyObject *iccom_loopback_is_active_py(PyObject *self, PyObject *args);
static PyObject *iccom_loopback_disable_py(PyObject *self, PyObject *args);
static PyObject *iccom_loopback_enable_py(PyObject *self, PyObject *args);
static PyObject *iccom_loopback_cfg_str_py(PyObject *self);

/* ---------------- Python adapter header ------------------------------ */

// method table
static PyMethodDef iccom_methods[] = {
        {"open", iccom_socket_open_py, METH_VARARGS, "Open ICCom socket."
         " First argument - ICCom channel number. Returns opened socket"
         " file descriptor."}
        , {"close", iccom_socket_close_py, METH_VARARGS, "Close ICCom socket."
           " First argument - socket file descriptor."}
        , {"send", iccom_send_py, METH_VARARGS, "Send data via ICCom socket."
           " First argument - iccom socket file descriptor. Second argument - "
           " bytearray to send."}
        , {"receive", iccom_receive_py, METH_VARARGS, "Read data from ICCom socket."
           " First argument - the socket file descriptor to read the data from."
           " Returns the bytearray of data read from socket."}
        , {"channel_verify", iccom_channel_verify_py, METH_VARARGS
           , "Verifies the channel number validity. First argument - channel number."
             " Returns True, if channel value is correct to use in ICCom, False else."}
        , {"get_socket_read_timeout", iccom_get_socket_read_timeout_py, METH_VARARGS
           , "Returns the current socket timeout [ms] value given by socket file"
             " descriptor. 0 means no timeout. First argument - the socket"
             " file descriptor."}
        , {"set_socket_read_timeout", iccom_set_socket_read_timeout_py, METH_VARARGS
           , "Sets the appropriate timeout value [ms] to the socket given"
             " by socket file descriptor. First argument - socket file descriptor."
             " Second argument - the timeout value to set [ms]."}
        , {"loopback_get", iccom_loopback_get_py, METH_VARARGS
            , "Returns current loopback configuration."}
        , {"loopback_is_active", iccom_loopback_is_active_py, METH_VARARGS
            , "Returns True if loopback is enabled now, False else."}
        , {"loopback_disable", iccom_loopback_disable_py, METH_VARARGS
           , "Disables loopback."}
        , {"loopback_enable", iccom_loopback_enable_py, METH_VARARGS
            , "Enables loopback. Arguments: (from_channel, to_channel, range_shift)."}
        , {NULL, NULL, 0, NULL}        /* Sentinel */
};

// module definition
static struct PyModuleDef iccom_module = {
    PyModuleDef_HEAD_INIT
    , .m_name = "iccom"
    , .m_doc = "The ICCom IF Python adapter"
    , .m_size = -1
    , .m_methods = iccom_methods
};

/* ---------------- Python adapter part (ext. classes) ----------------- */

// Python wrapper around
typedef struct {
    PyObject_HEAD
    loopback_cfg cfg;
} PyLoopbackCfg;

static PyMemberDef LoopbackCfg_members[] = {
        {"from_ch", T_UINT, offsetof(PyLoopbackCfg, cfg)
                            + offsetof(loopback_cfg, from_ch)
         , 0, "beginning of a src mapping range (inclusive)"
        }
        , {"to_ch", T_UINT, offsetof(PyLoopbackCfg, cfg)
                            + offsetof(loopback_cfg, to_ch)
           , 0, "end of a src mapping range (inclusive)"
        }
        , {"range_shift", T_INT, offsetof(PyLoopbackCfg, cfg)
                            + offsetof(loopback_cfg, range_shift)
         , 0, "the shift between src and dsc mapping ranges"
        }
        , {NULL}
};

static PyTypeObject loopbackCfgType = {
        PyVarObject_HEAD_INIT(NULL, 0)
        .tp_name = "iccom.LoopbackCfgType"
        , .tp_doc = "defines the ICCom IF loopback mapping rule"
        , .tp_basicsize = sizeof(PyLoopbackCfg)
        , .tp_itemsize = 0
        , .tp_flags = Py_TPFLAGS_DEFAULT
        , .tp_new = PyType_GenericNew
        , .tp_members = LoopbackCfg_members
        , .tp_str = iccom_loopback_cfg_str_py
};

// initialization function
PyMODINIT_FUNC
PyInit_python3_libiccom(void)
{
        if (PyType_Ready(&loopbackCfgType) < 0) {
                return NULL;
        }

        PyObject *pymod = PyModule_Create(&iccom_module);
        if (pymod == NULL) {
                return NULL;
        }

        Py_INCREF(&loopbackCfgType);
        if (PyModule_AddObject(pymod, "LoopbackCfg", (PyObject *) &loopbackCfgType) < 0) {
                Py_DECREF(&loopbackCfgType);
                Py_DECREF(pymod);
                return NULL;

        }
        return pymod;
}

/* ---------------- Python adapter part (methods) ---------------------- */

// Wrapper around:
//      int iccom_open_socket(const unsigned int channel)
//
// THROWS
//
// RETURNS:
//      socket file descriptor
static PyObject *iccom_socket_open_py(PyObject *self, PyObject *args)
{
        unsigned int channel = 0;

        if (!PyArg_ParseTuple(args, "I", &channel)) {
                return NULL;
        }

        int res = iccom_open_socket(channel);

        if (res >= 0) {
                return Py_BuildValue("i", res);
        }

        char error_string[EBUF_LEN];

        switch(-res) {
        case EINVAL:
                snprintf(error_string, sizeof(error_string)
                         , "Failed to open the netlink socket: "
                           "channel (%d) is out of bounds see "
                           "iccom.channel_verify(...) for more info."
                         , channel);
                PyErr_SetString(PyExc_ValueError, (const char*)error_string);
                break;
        default:
                snprintf(error_string, sizeof(error_string)
                         , "Failed to open the netlink socket: "
                           "channel (%d): system error code: %d (%s)"
                         , channel, -res, strerror(-res));
                PyErr_SetString(PyExc_IOError, (const char*)error_string);
                break;
        }

        return NULL;
}

// Wrapper around:
//      void iccom_close_socket(const int sock_fd)
static PyObject *iccom_socket_close_py(PyObject *self, PyObject *args)
{
        int fd = 0;

        if (!PyArg_ParseTuple(args, "i", &fd)) {
                return NULL;
        }

        iccom_close_socket(fd);

        Py_RETURN_NONE;
}

// Wrapper around:
//      int __iccom_receive_data_pure(const int sock_fd
//                                    , void *const receive_buffer
//                                    , const size_t buffer_size)
static PyObject *iccom_receive_py(PyObject *self, PyObject *args)
{
        int fd = 0;

        if (!PyArg_ParseTuple(args, "i", &fd)) {
                return NULL;
        }

        const unsigned int max_msg_size = iccom_get_max_payload_size();
        const size_t buffer_size = iccom_get_required_buffer_size(max_msg_size);

        char buff[buffer_size];

        // TODO: not pure REPLACE WITH nocopy
        int res = __iccom_receive_data_pure(fd, (void *)buff, buffer_size);

        // timeout
        if (res == 0) {
                Py_RETURN_NONE;
        }

        // data
        if (res > 0) {
                return PyByteArray_FromStringAndSize((const char *)buff, (Py_ssize_t)res);
        }

        // error
        char error_string[EBUF_LEN];

        switch(-res) {
        case ENFILE:
                snprintf(error_string, sizeof(error_string)
                         , "incoming buffer size %zu is too small for netlink"
                           " message (min size is %ld)"
                         , buffer_size, iccom_get_required_buffer_size(0));
                PyErr_SetString(PyExc_OverflowError, (const char*)error_string);
                break;
        case EINVAL:
                snprintf(error_string, sizeof(error_string)
                         , "data offset output is not set");
                PyErr_SetString(PyExc_ValueError, (const char*)error_string);
                break;
        case EPIPE:
                snprintf(error_string, sizeof(error_string)
                         , "received netlink header data incorrect");
                PyErr_SetString(PyExc_IOError, (const char*)error_string);
                break;
        default:
                snprintf(error_string, sizeof(error_string)
                         , "Failed to read data from socket, "
                           "system error code: %d (%s)"
                         , -res, strerror(-res));
                PyErr_SetString(PyExc_IOError, (const char*)error_string);
                break;
        }

        return NULL;
}

// Wrapper around:
//      int iccom_send_data(const int sock_fd
//                          , const void *const data
//                          , const size_t data_size_bytes)
static PyObject *iccom_send_py(PyObject *self, PyObject *args)
{
        int fd = 0;
        PyByteArrayObject *data_obj = NULL;

        if (!PyArg_ParseTuple(args, "iY", &fd, &data_obj)) {
                return NULL;
        }

        const char * const data = PyByteArray_AsString((PyObject *)data_obj);
        ssize_t data_size = PyByteArray_Size((PyObject *)data_obj);

        if (data_size < 0) {
                PyErr_SetString(PyExc_ValueError, "Array size should not be negative");
                return NULL;
        }

        int res = iccom_send_data(fd, data, data_size);

        // all fine
        if (res >= 0) {
                Py_RETURN_NONE;
        }

        // error
        char error_string[EBUF_LEN];

        switch(-res) {
        case E2BIG:
                snprintf(error_string, sizeof(error_string)
                         , "Can't send messages larger than: %ld bytes."
                         , iccom_get_max_payload_size());
                PyErr_SetString(PyExc_ValueError, (const char*)error_string);
                break;
        case EINVAL:
                snprintf(error_string, sizeof(error_string)
                         , "wrong parameter: either no data, or data "
                           "size is not positive");
                PyErr_SetString(PyExc_ValueError, (const char*)error_string);
                break;
        case ENOMEM:
                snprintf(error_string, sizeof(error_string)
                         , "send buffer allocation failed");
                PyErr_SetString(PyExc_MemoryError, (const char*)error_string);
                break;
        default:
                snprintf(error_string, sizeof(error_string)
                         , "Failed to write data to socket, "
                           "system error code: %d (%s)"
                         , -res, strerror(-res));
                PyErr_SetString(PyExc_IOError, (const char*)error_string);
                break;
        }

        return NULL;
}

static PyObject *iccom_channel_verify_py(PyObject *self, PyObject *args)
{
        int ch = 0;

        if (!PyArg_ParseTuple(args, "i", &ch)) {
                return NULL;
        }

        return iccom_channel_verify(ch) >= 0 ? Py_True : Py_False;
}

static PyObject *iccom_set_socket_read_timeout_py(PyObject *self, PyObject *args)
{
        int fd = 0;
        int ms = 0;

        if (!PyArg_ParseTuple(args, "ii", &fd, &ms)) {
                return NULL;
        }

        int res = iccom_set_socket_read_timeout(fd, ms);

        if (res >= 0) {
                Py_RETURN_NONE;
        }

        // error
        char error_string[EBUF_LEN];

        switch(-res) {
        case EINVAL:
                snprintf(error_string, sizeof(error_string)
                         , "Wrong parameter(s): the set of passed parameters"
                           " is not valid.");
                PyErr_SetString(PyExc_ValueError, (const char*)error_string);
                break;
        default:
                snprintf(error_string, sizeof(error_string)
                         , "Failed with system error code: %d (%s)"
                         , -res, strerror(-res));
                PyErr_SetString(PyExc_IOError, (const char*)error_string);
                break;
        }

        return NULL;
}

static PyObject *iccom_get_socket_read_timeout_py(PyObject *self, PyObject *args)
{
        int fd = 0;

        if (!PyArg_ParseTuple(args, "i", &fd)) {
                return NULL;
        }

        int res =  iccom_get_socket_read_timeout(fd);

        if (res >= 0) {
                return PyLong_FromLong(res);
        }

        // error
        char error_string[EBUF_LEN];

        switch(-res) {
        default:
                snprintf(error_string, sizeof(error_string)
                         , "Failed with system error code: %d (%s)"
                         , -res, strerror(-res));
                PyErr_SetString(PyExc_IOError, (const char*)error_string);
                break;
        }

        return NULL;
}

// Wrapper around:
//    int iccom_loopback_enable(const unsigned int from_ch, const unsigned int to_ch
//                              const int range_shift);
static PyObject *iccom_loopback_enable_py(PyObject *self, PyObject *args)
{
        struct loopback_cfg lbk_cfg;

        if (!PyArg_ParseTuple(args, "IIi", &(lbk_cfg.from_ch)
                              , &(lbk_cfg.to_ch), &(lbk_cfg.range_shift))) {
                return NULL;
        }


        int res = iccom_loopback_enable(lbk_cfg.from_ch, lbk_cfg.to_ch
                                        , lbk_cfg.range_shift);

        // all fine
        if (res >= 0) {
                Py_RETURN_NONE;
        }

        // error
        char error_string[EBUF_LEN];

        switch(-res) {
        case EINVAL:
                snprintf(error_string, sizeof(error_string)
                         , "Wrong parameter(s): the set of passed parameters"
                           " is not valid.");
                PyErr_SetString(PyExc_ValueError, (const char*)error_string);
                break;
        case EBADF:
                snprintf(error_string, sizeof(error_string)
                         , "ICCom IF loopback ctl file open failed."
                           " This might be caused either by permissions, either by "
                           " non-existing file (which means that ICCom Sockets driver"
                           " is not loaded)");
                PyErr_SetString(PyExc_ValueError, (const char*)error_string);
                break;
        case EIO:
                snprintf(error_string, sizeof(error_string)
                         , "Write to the ICCom loopback ctl file failed.");
                PyErr_SetString(PyExc_MemoryError, (const char*)error_string);
                break;
        default:
                snprintf(error_string, sizeof(error_string)
                         , "Failed with system error code: %d (%s)"
                         , -res, strerror(-res));
                PyErr_SetString(PyExc_IOError, (const char*)error_string);
                break;
        }

        return NULL;
}

// Wrapper around:
//      int iccom_loopback_disable(void);
static PyObject *iccom_loopback_disable_py(PyObject *self, PyObject *args)
{
        int res = iccom_loopback_disable();

        // all fine
        if (res >= 0) {
                Py_RETURN_NONE;
        }

        // error
        char error_string[EBUF_LEN];

        snprintf(error_string, sizeof(error_string)
                 , "Failed with system error code: %d (%s)"
                 , -res, strerror(-res));
        PyErr_SetString(PyExc_IOError, (const char*)error_string);

        return NULL;
}

// Wrapper around:
//      bool iccom_loopback_is_active(void);
static PyObject *iccom_loopback_is_active_py(PyObject *self, PyObject *args)
{
        return iccom_loopback_is_active() ? Py_True : Py_False;
}

// Wrapper around:
//      int iccom_loopback_get(struct loopback_cfg *const out);
static PyObject *iccom_loopback_get_py(PyObject *self, PyObject *args)
{
        PyLoopbackCfg *cfg = (PyLoopbackCfg *)(loopbackCfgType.tp_alloc(
                                                    &loopbackCfgType, 0));

        if (!cfg) {
                return (PyObject *)NULL;
        }

        int res = iccom_loopback_get(&cfg->cfg);
        if (res < 0) {
                Py_DECREF(cfg);

                char error_string[EBUF_LEN];

                snprintf(error_string, sizeof(error_string)
                         , "Failed to get loopback config from the system: "
                           "error: %d", -res);
                PyErr_SetString(PyExc_IOError, (const char*)error_string);
                return (PyObject*)NULL;
        }

        return (PyObject*)cfg;
}

// RETURNS:
//      Human readable representation of the loopback_cfg object
static PyObject *iccom_loopback_cfg_str_py(PyObject *self)
{
        PyLoopbackCfg *obj = (PyLoopbackCfg *)self;

        return PyUnicode_FromFormat("ICCom Loopback config object: "
                                    "{from_ch: %u, to_ch: %u, range_shift: %d}"
                                    , obj->cfg.from_ch, obj->cfg.to_ch
                                    , obj->cfg.range_shift);
}

