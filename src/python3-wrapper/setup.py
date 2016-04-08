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

from distutils.core import setup, Extension
import sys, os

# real documentation: https://github.com/python/cpython/blob/master/Lib/distutils/extension.py
iccom_module = Extension('python3_libiccom'
                         , libraries = ['iccom']
                         , sources = ['iccom_py.c'])

setup(name = 'python3_libiccom'
      , version = '1.0'
      , description = 'ICCom python3 interface'
      , author = 'Artem Gulyaev <Artem.Gulyaev@de.bosch.com>'
      , ext_modules = [iccom_module])
