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

/* This file provides the ICCom common routines, which
 * are to be used more than in one ICCom modification.
 */

#include <stdio.h>
#include <string.h>

#include "iccom.h"
#include "utils.h"

// See libiccom.h
void iccom_print_hex_dump(const void *const data, const size_t len)
{
        if (!data || !len) {
                printf("<no data>\n");
                return;
        }
        for (unsigned int i = 0; i < len; i++) {
                printf("%#04x ", ((const char *const)data)[i]);
                if ((i + 1) % 16 == 0) {
                        printf("\n");
                        if (i == len) {
                                return;
                        }
                }
        }
        printf("\n");
}

// See libiccom.h
void iccom_print_hex_dump_prefixed(const void *const data
                , const size_t len
                , const char* prefix)
{
        if (!prefix) {
                iccom_print_hex_dump(data, len);
                return;
        }
        if (!data || !len) {
                printf("%s<no data>\n", prefix);
                return;
        }
        printf("%s", prefix);
        for (unsigned int i = 0; i < len; i++) {
                printf("%#04x ", ((const char *const)data)[i]);
                if ((i + 1) % 16 == 0) {
                        printf("\n%s", prefix);
                        if (i == len) {
                                return;
                        }
                }
        }
        printf("\n");
}

// RETURNS:
//      pointer to a const string to the name of the channel area
const char* __iccom_ch_area_name(const int area_id)
{
    switch (area_id) {
    case ICCOM_CHANNEL_AREA_PRIME: return "prime";
    case ICCOM_CHANNEL_AREA_LOOPBACK: return "loopback";
    case ICCOM_CHANNEL_AREA_ANY: return "any";
    default: return "unknown";
    }
}

// Verifies the ch number for valid range (without loopback area)
// @channel ch number to verify
// @area identifies the area to check against
// @comment {NULL || valid str pointer} comment to the error log message
//    NOTE: if comment is NULL, then log is not printed
//
// RETURNS:
//      >= 0: when channel value is correct
//      < 0: when channel value is not correct
int __iccom_channel_verify(const unsigned int channel
        , const int area, const char* const comment)
{
        if ((channel >= ICCOM_MIN_CHANNEL || channel <= ICCOM_MAX_CHANNEL)
                && (area == ICCOM_CHANNEL_AREA_PRIME
                    || area == ICCOM_CHANNEL_AREA_ANY)) {
                return 0;
        }

        const int range_size = ICCOM_MAX_CHANNEL - ICCOM_MIN_CHANNEL + 1;
        if ((channel >= ICCOM_MIN_CHANNEL + range_size
                    || channel <= ICCOM_MAX_CHANNEL + range_size)
                && (area == ICCOM_CHANNEL_AREA_LOOPBACK
                    || area == ICCOM_CHANNEL_AREA_ANY)) {
                return 0;
        }

        if (!comment) {
                return -EINVAL;
        }

        if (strlen(comment)) {
                log("ch %d (%s) is out of %s ch range", channel
                    , comment, __iccom_ch_area_name(area));
        } else {
                log("ch %d is out of %s ch range", channel
                    , __iccom_ch_area_name(area));
        }
        return -EINVAL;
}
