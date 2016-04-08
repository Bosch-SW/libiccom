#!/bin/bash

#**********************************************************************
# Copyright (c) 2021 Robert Bosch GmbH
# Artem Gulyaev <Artem.Gulyaev@de.bosch.com>
#
# This code is licensed under the Mozilla Public License Version 2.0
# License text is available in the file ’LICENSE.txt’, which is part of
# this source code package.
#
# SPDX-identifier: MPL-2.0
#
#**********************************************************************

# This script can run **only on the target environment** (with ICCom kernel
# stack available).
#
# It gathers the ICCom stack communication statistics over the
# run time and writes it into a file, which can later be imported
# into Octave or other SW to analyze and plot the results, such as
# average bandwidth, error rates, etc..
#
# To run the logger just run it:
#   iccom_statistics_logger.sh
#
# The log file will be created in the current directory.
#
# NOTE: to work with the report in Octave run:
#
#   D = importdata("your_iccom.log", ' ')
#   scatter(D.data(:, 12), 1:length(D.data(:, 12)))
#   title("Consumer bytes received")
#   xlabel("log record")
#   ylabel("Consumer bytes received")

LOG_INTERVAL_SEC=10

# prints to stdout the last digit in the line
function get_st_val
{
    sed 's/.*\s\+\([[:digit:]]\+\)$/\1/g'
}

function iccom_log_header
{
    rec="Header generated on: $(date +%d-%m-%y_%H-%M-%S)\n"
    rec="DATE_AND_TIME TL_X_DONE TL_BYTES_XFERED"
    rec="${rec} PKG_XFERED PKG_SND_OK PKG_RCV_OK PKG_SND_FAIL PKG_RCV_FAIL PKG_IN_TX_QUEUE"
    rec="${rec} PKT_RCV_OK MSG_RCV_OK MSG_RCV_RX_READY CONSUMER_BYTES_RCV"

    echo "${rec}"
}

# writes to stdout the following record
#   DATE_TIME  NUMBER_OF_PACKAGES_SENT  NUMBER_OF_PACKAGES_FAILED
function iccom_log
{
    st=$(cat /proc/iccom/statistics | sed '/^\s*$/q')

    TL_X_DONE=$(echo "${st}" | grep -e "^transport_layer:\s\+xfers\s\+done:" | get_st_val)
    TL_BYTES_XFERED=$(echo "${st}" | grep -e "^transport_layer:\s\+bytes\s\+xfered:" | get_st_val)

    PKG_XFERED=$(echo "${st}" | grep -e "^packages:\s\+xfered\s\+total:" | get_st_val)
    PKG_SND_OK=$(echo "${st}" | grep -e "^packages:\s\+sent\s\+ok:" | get_st_val)
    PKG_RCV_OK=$(echo "${st}" | grep -e "^packages:\s\+received\s\+ok:" | get_st_val)
    PKG_SND_FAIL=$(echo "${st}" | grep -e "^packages:\s\+sent\s\+fail:" | get_st_val)
    PKG_RCV_FAIL=$(echo "${st}" | grep -e "^packages:\s\+received\s\+fail:" | get_st_val)
    PKG_IN_TX_QUEUE=$(echo "${st}" | grep -e "^packages:\s\+in\s\+tx\s\+queue:" | get_st_val)

    PKT_RCV_OK=$(echo "${st}" | grep -e "^packets:\s\+received\s\+ok:" | get_st_val)

    MSG_RCV_OK=$(echo "${st}" | grep -e "^messages:\s\+received\s\+ok:" | get_st_val)
    MSG_RCV_RX_READY=$(echo "${st}" | grep -e "^messages:\s\+ready\s\+rx:" | get_st_val)

    CONSUMER_BYTES_RCV=$(echo "${st}" | grep -e "^bandwidth:\s\+consumer\s\+bytes\s\+received:" | get_st_val)

    rec="$(date "+%D %T") ${TL_X_DONE} ${TL_BYTES_XFERED}"
    rec="${rec} ${PKG_XFERED} ${PKG_SND_OK} ${PKG_RCV_OK} ${PKG_SND_FAIL} ${PKG_RCV_FAIL} ${PKG_IN_TX_QUEUE}"
    rec="${rec} ${PKT_RCV_OK} ${MSG_RCV_OK} ${MSG_RCV_RX_READY} ${CONSUMER_BYTES_RCV}"

    echo "${rec}"
}

lognamebase="iccom.log"
logname="$(date +%d-%m-%y_%H-%M-%S)_${lognamebase}"

[ -e "/proc/iccom/statistics" ] || {
    echo "It seems you're trying to run me on a system without"
    echo "ICCom kernel stack modules installed. I only can gather"
    echo "the ICCom statistics by interfacing with ICCom kernel module."
    echo ""
    echo "Possible solutions:"
    echo "* run me on a system with ICCom module installed/inserted"
    echo "* probably you just forgot to insert the module then just run:"
    echo "     insmod iccom"

    exit -1
}

echo "Started logging to file: ${logname}"

header="$(iccom_log_header)"
echo "${header}" > "${logname}"
echo "${header}"

while (true); do
    rec="$(iccom_log)"
    echo "${rec}" >> "${logname}"
    echo "${rec}"
    sleep ${LOG_INTERVAL_SEC}
done
