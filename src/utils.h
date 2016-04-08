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

// The file contains some useful utils which might be reused
// in more than one ICCom implementation.
//
// NOTE: those utils which are exported as module IF, are declared
//      in the iccom.h.

/* -------------------- BUILD TIME CONFIGURATION ----------------------- */

#define LIBICCOM_LOG_PREFIX "libiccom: "
// TODO: grab it from kernel header
#define ICCOM_LOOPBACK_IF_CTRL_FILE_PATH "/proc/iccomif/loopbackctl"

/* -------------------- MACRO DEFINITIONS ------------------------------ */

#define ICCOM_CHANNEL_AREA_PRIME 1
#define ICCOM_CHANNEL_AREA_LOOPBACK 2
#define ICCOM_CHANNEL_AREA_ANY 3

#define log(fmt, ...)                                                         \
        printf(LIBICCOM_LOG_PREFIX "%s: " fmt "\n", __func__, ## __VA_ARGS__);

/* -------------------- ROUTINES DECLARATIONS -------------------------- */

int __iccom_channel_verify(const unsigned int channel
        , const int area, const char* const comment);
