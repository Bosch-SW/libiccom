#######################################################################
# Copyright (c) 2021 Robert Bosch GmbH
# Artem Gulyaev <Artem.Gulyaev@de.bosch.com>
#
# This code is licensed under the Mozilla Public License Version 2.0
# License text is available in the file ’LICENSE.txt’, which is part of
# this source code package.
#
# SPDX-identifier: MPL-2.0
#
#######################################################################

# This is the ICCom statistics plotting Octave script. It is intended
# to be used in Octave to plot the ICCom statistical data vs time and
# so estimate the actual communication parameters.

LOG_FILENAME = "iccom.log"
LOG_INTERVAL_SEC = 10.0
CONSUMER_BYTES_RCV_COL_IDX = 12

D = importdata(LOG_FILENAME, ' ')

y = D.data(:, CONSUMER_BYTES_RCV_COL_IDX)
m = length(y)
x = (1:m)' * LOG_INTERVAL_SEC

X = [ones(m, 1) x]
theta = (pinv(X'*X))*X'*y

scatter(x, y)
set(gca, "fontsize", 20)
title(["Consumer bytes received, average rate is " num2str(theta(2), 0) " b/s"])
xlabel(["runtime, seconds (assuming " num2str(LOG_INTERVAL_SEC)
        "sec per log record)"])
ylabel("Consumer bytes received, [bytes]")

hold on;
plot(X(:,2), X*theta, '-')
