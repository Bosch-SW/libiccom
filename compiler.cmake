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

function(set_salt_default_c_config target_name)
    set_property(TARGET "${target_name}" PROPERTY
                 VERBOSE_MAKEFILE "ON")

    set_property(TARGET "${target_name}" PROPERTY C_STANDARD 11)
    set_property(TARGET "${target_name}" PROPERTY C_STANDARD_REQUIRED ON)

    set_property(TARGET "${target_name}" PROPERTY
                 C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
    set_property(TARGET "${target_name}" PROPERTY
                 C_FLAGS "${CMAKE_C_FLAGS}   \
                     -Os                     \
                     -Wpedantic              \
                     -Wmissing-declarations  \
                     -Wextra                 \
                     -Wall                   \
                     -Werror                 \
                     -Wswitch-default        \
                     -Wswitch-enum           \
                     ")
    set_property(TARGET "${target_name}" PROPERTY
                 C_FLAGS_DEBUG "${CMAKE_C_FLAGS}   \
                     -fprofile-arcs          \
                     -ftest-coverage         \
                     ")

endfunction()

function(set_salt_default_cxx_config target_name)
    set_property(TARGET "${target_name}" PROPERTY
                 VERBOSE_MAKEFILE "ON")

    set_property(TARGET "${target_name}" PROPERTY CXX_STANDARD 16)
    set_property(TARGET "${target_name}" PROPERTY CXX_STANDARD_REQUIRED ON)

    set_property(TARGET "${target_name}" PROPERTY CXX_EXTENSIONS OFF)

    set_property(TARGET "${target_name}" PROPERTY
                 CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
    set_property(TARGET "${target_name}" PROPERTY
                 CXX_FLAGS
                     "${CMAKE_CXX_FLAGS}     \
                     -Os                     \
                     -Wpedantic              \
                     -Woverloaded-virtual    \
                     -Wuninitialized         \
                     -Wmissing-declarations  \
                     -Winit-self             \
                     -Wextra                 \
                     -Wall                   \
                     -Weffc++                \
                     -fno-rtti               \
                     ")
    set_property(TARGET "${target_name}" PROPERTY
                 CXX_FLAGS_DEBUG  "${CMAKE_CXX_FLAGS} \
                     -Werror                 \
                     -fprofile-arcs          \
                     -ftest-coverage         \
                     ")
endfunction()



