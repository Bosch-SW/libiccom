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

cmake_minimum_required(VERSION 3.5.0)

include(GNUInstallDirs)

include("compiler.cmake")

set(project_name "libiccom" C)

set(lib_target_name "iccom")
set(lib_target_name_s "${lib_target_name}_static")

project("${project_name}")

if(NOT UNIX)
    message(FATAL_ERROR "Sorry, libiccom is a dedicated Linux library.")
endif()

################## parameters ###############

option(ICCOM_USE_NETWORK_SOCKETS
"If not set (default) then libiccom will be built to run with ICCom
kernel module. If set, then libiccom will use the bekerly sockets
for communication with backend applications."
       OFF)
option(ICCOM_USE_HINTS
"If defined, then ICCom is to be built with developer
(and user) hints information which might strongly help
avoiding googling for an issue reasons in case of facing
one. NOTE: To reduce the code footprint size disable it.
Usually it is good idea to disable it in production."
       OFF)

################## sources ##################
# libiccom library
set(public_headers
    "include/iccom.h"
)

set(src_files
    "src/utils.c"
)

if(ICCOM_USE_NETWORK_SOCKETS)
    message(STATUS "NOTE: using network (Bekerly) sockets IF, see option: ICCOM_USE_NETWORK_SOCKETS")
    list(APPEND src_files
        "src/iccom_nsock.c"
    )
else()
    message(STATUS "NOTE: using ICCom classical netlink sockets IF, see option: ICCOM_USE_NETWORK_SOCKETS")
    list(APPEND src_files
        "src/iccom.c"
    )
endif()

# python wrapper
set(python_wrapper_files
    "${CMAKE_CURRENT_SOURCE_DIR}/src/python3-wrapper/iccom_py.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/python3-wrapper/setup.py"
)

################## target ##################
add_library("${lib_target_name}" SHARED ${src_files})
add_library("${lib_target_name_s}" STATIC ${src_files})

# only for IDEs
add_custom_target(iccom_py3_adapter SOURCES ${python_wrapper_files})

########## target build configuration ########
if(ICCOM_USE_HINTS)
    message(STATUS "NOTE: using ICCom developer/user hints, see option: ICCOM_USE_HINTS")
    target_compile_definitions("${lib_target_name}" PRIVATE ICCOM_HINTS)
    target_compile_definitions("${lib_target_name_s}" PRIVATE ICCOM_HINTS)
else()
    message(STATUS "NOTE: NOT using ICCom developer/user hints, see option: ICCOM_USE_HINTS")
endif()

################## compiler ##################
set_salt_default_c_config("${lib_target_name}")
set_salt_default_c_config("${lib_target_name_s}")

#compiler flags
#
# NOTE: -Wl,--version-script=iccom.export
#   is used to export only dedicated symbols in the export table
#   of the *.so file, to save loading time via shortened export
#   table
target_compile_options("${lib_target_name}"
    PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/iccom.export")
target_compile_options("${lib_target_name_s}"
    PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/iccom.export")

################## includes ##################

target_include_directories("${lib_target_name}" PRIVATE ./include)
target_include_directories("${lib_target_name_s}" PRIVATE ./include)

set_target_properties("${lib_target_name}" PROPERTIES
                        PUBLIC_HEADER ${public_headers})
set_target_properties("${lib_target_name_s}" PROPERTIES
                        PUBLIC_HEADER ${public_headers})
set_target_properties("${lib_target_name_s}" PROPERTIES
                        OUTPUT_NAME "${lib_target_name_s}")

################## install  ##################

set(install_dir "${CMAKE_INSTALL_LIBDIR}")
set(headers_install_dir "${CMAKE_INSTALL_INCLUDEDIR}")

message(STATUS "Project ${project_name} install dir: ${install_dir}")
message(STATUS "Project ${project_name} headers install dir: ${headers_install_dir}")

install(TARGETS ${lib_target_name} ${lib_target_name_s}
  LIBRARY DESTINATION ${install_dir}
  ARCHIVE DESTINATION ${install_dir}
  PUBLIC_HEADER DESTINATION ${headers_install_dir}
)

############### Python adapter ###############

set(ICCOM_PYTHON_ADAPTER_PYTHON_MINOR_VER
    7
    CACHE STRING "Defines the target minor python3 version")

add_custom_command(
    OUTPUT python_adapter_output
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/python3-wrapper"
    COMMAND python3 setup.py build_ext -DPYTHON_TGT_MINOR_VERSION_${ICCOM_PYTHON_ADAPTER_PYTHON_MINOR_VER} || echo "*** ERROR ***: Python extension build failed. Please try other python minor version, for example: cmake PATH_TO_SRCS -DICCOM_PYTHON_ADAPTER_PYTHON_MINOR_VER=6"
)

add_custom_target(python_adapter
    DEPENDS python_adapter_output
)

install(CODE "execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/python3-wrapper COMMAND python3 setup.py install)"
    EXCLUDE_FROM_ALL
    COMPONENT python_adapter)
