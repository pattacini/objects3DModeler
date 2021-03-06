# Copyright: (C) 2015 iCub Facility - Istituto Italiano di Tecnologia
# Authors: Tanis Mar
# CopyPolicy: Released under the terms of the GNU GPL v2.0.

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
SET(PROJECTNAME toolFeatExt)
PROJECT(${PROJECTNAME})

include(YarpIDL)
# Thrift compile definition file to generate source code into the desired directory
set(generated_libs_dir "${CMAKE_CURRENT_SOURCE_DIR}")
yarp_idl_to_dir(toolFeatures3D.thrift ${generated_libs_dir})

file(GLOB source src/*.cpp)
file(GLOB header include/*.h)
set(idl toolFeatures3D.thrift)

source_group("Source Files" FILES ${source})
source_group("Header Files" FILES ${header})
source_group("IDL Files" FILES ${idl})

include_directories(${PCL_INCLUDE_DIRS})
link_directories(${PCL_LIBRARY_DIRS})
add_definitions(${PCL_DEFINITIONS})

include_directories(${YarpCloud_INCLUDE_DIRS})

include_directories(${YARP_INCLUDE_DIRS})
link_directories(${ICUBCONTRIB_INSTALL_PREFIX}/lib/x86_64-linux-gnu) 
include_directories(${ICUBCONTRIB_INSTALL_PREFIX}/include)

include(toolFeatures3D_thrift.cmake)
include_directories(${generated_libs_dir}/include)

add_executable(${PROJECTNAME} ${source} ${header} ${idl})
target_link_libraries(${PROJECTNAME} ${YARP_LIBRARIES}  YarpCloud ${PCL_LIBRARIES})
install(TARGETS ${PROJECTNAME} DESTINATION bin)

yarp_install(FILES ${PROJECTNAME}.xml DESTINATION ${ICUBCONTRIB_MODULES_INSTALL_DIR})
