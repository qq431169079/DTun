cmake_minimum_required(VERSION 2.6)
cmake_policy(SET CMP0014 NEW)

project(DTUN)

#platform setup

if (CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: None (CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif ()

set(UTP_DEBUG FALSE)

#out dir

set(DTUN_OUT_DIR ${DTUN_BINARY_DIR}/out)

#set includes/libs

set(DTUN_INCLUDE_DIR "${DTUN_SOURCE_DIR}/include")
include_directories(${DTUN_INCLUDE_DIR})

#find packages

set(Boost_USE_MULTITHREADED ON)

find_package(Threads)
find_package(PkgConfig)
find_package(Boost COMPONENTS date_time filesystem program_options system thread random regex chrono REQUIRED)
pkg_check_modules(LOG4CPLUS log4cplus)
add_definitions(${LOG4CPLUS_CFLAGS})
include_directories(${LOG4CPLUS_INCLUDEDIR})
link_directories(${LOG4CPLUS_LIBRARY_DIRS})
unset(LOG4CPLUS_LIBRARIES CACHE)
set(LOG4CPLUS_LIBRARIES log4cplus CACHE INTERNAL "" FORCE)

#fix output directories

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${DTUN_OUT_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${DTUN_OUT_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${DTUN_OUT_DIR}/lib)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-deprecated-declarations -Wno-unused-function -D_REENTRANT -fPIC -DPIC -std=gnu99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-deprecated-declarations -Wno-unused-function -D_REENTRANT -fPIC -DPIC")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined -Wl,--exclude-libs,ALL")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-undefined -Wl,--exclude-libs,ALL")

add_definitions(${Boost_LIB_DIAGNOSTIC_DEFINITIONS})
add_definitions(-DBADVPN_THREADWORK_USE_PTHREAD)
add_definitions(-DBADVPN_THREAD_SAFE=1)
add_definitions(-DBADVPN_LINUX)
add_definitions(-DBADVPN_LITTLE_ENDIAN)
add_definitions(-DBADVPN_USE_EPOLL)
add_definitions(-DBADVPN_BREACTOR_BADVPN)
add_definitions(-DBADVPN_USE_SIGNALFD)
add_definitions(-D_GNU_SOURCE)
if (UTP_DEBUG)
    add_definitions(-D_DEBUG)
    add_definitions(-DUTP_DEBUG_LOGGING)
endif ()

set (COMMON_HEADERS
    ${DTUN_INCLUDE_DIR}/udt.h
    ${DTUN_INCLUDE_DIR}/utp.h
    ${DTUN_INCLUDE_DIR}/utp_types.h
    ${DTUN_INCLUDE_DIR}/DTun/AppConfig.h
    ${DTUN_INCLUDE_DIR}/DTun/DProtocol.h
    ${DTUN_INCLUDE_DIR}/DTun/SAcceptor.h
    ${DTUN_INCLUDE_DIR}/DTun/SConnection.h
    ${DTUN_INCLUDE_DIR}/DTun/SConnector.h
    ${DTUN_INCLUDE_DIR}/DTun/SHandle.h
    ${DTUN_INCLUDE_DIR}/DTun/SHandler.h
    ${DTUN_INCLUDE_DIR}/DTun/SReactor.h
    ${DTUN_INCLUDE_DIR}/DTun/SignalBlocker.h
    ${DTUN_INCLUDE_DIR}/DTun/SignalHandler.h
    ${DTUN_INCLUDE_DIR}/DTun/SManager.h
    ${DTUN_INCLUDE_DIR}/DTun/StreamAppConfig.h
    ${DTUN_INCLUDE_DIR}/DTun/SysConnection.h
    ${DTUN_INCLUDE_DIR}/DTun/SysConnector.h
    ${DTUN_INCLUDE_DIR}/DTun/SysHandle.h
    ${DTUN_INCLUDE_DIR}/DTun/SysHandler.h
    ${DTUN_INCLUDE_DIR}/DTun/SysManager.h
    ${DTUN_INCLUDE_DIR}/DTun/SysReactor.h
    ${DTUN_INCLUDE_DIR}/DTun/Types.h
    ${DTUN_INCLUDE_DIR}/DTun/UDTAcceptor.h
    ${DTUN_INCLUDE_DIR}/DTun/UDTConnection.h
    ${DTUN_INCLUDE_DIR}/DTun/UDTConnector.h
    ${DTUN_INCLUDE_DIR}/DTun/UDTHandle.h
    ${DTUN_INCLUDE_DIR}/DTun/UDTHandler.h
    ${DTUN_INCLUDE_DIR}/DTun/UDTManager.h
    ${DTUN_INCLUDE_DIR}/DTun/UDTReactor.h
    ${DTUN_INCLUDE_DIR}/DTun/LTUDPAcceptor.h
    ${DTUN_INCLUDE_DIR}/DTun/LTUDPConnection.h
    ${DTUN_INCLUDE_DIR}/DTun/LTUDPConnector.h
    ${DTUN_INCLUDE_DIR}/DTun/LTUDPHandle.h
    ${DTUN_INCLUDE_DIR}/DTun/LTUDPHandleImpl.h
    ${DTUN_INCLUDE_DIR}/DTun/LTUDPManager.h
    ${DTUN_INCLUDE_DIR}/DTun/UTPAcceptor.h
    ${DTUN_INCLUDE_DIR}/DTun/UTPConnection.h
    ${DTUN_INCLUDE_DIR}/DTun/UTPConnector.h
    ${DTUN_INCLUDE_DIR}/DTun/UTPHandle.h
    ${DTUN_INCLUDE_DIR}/DTun/UTPHandleImpl.h
    ${DTUN_INCLUDE_DIR}/DTun/UTPManager.h
    ${DTUN_INCLUDE_DIR}/DTun/OpWatch.h
    ${DTUN_INCLUDE_DIR}/DTun/Utils.h
    ${DTUN_INCLUDE_DIR}/DTun/MTUDiscovery.h
)

include_directories(
    lwip/custom
    lwip/src/include
)

add_subdirectory(udt)
add_subdirectory(libutp)
add_subdirectory(dutil)
add_subdirectory(lwip)
add_subdirectory(dcat)
add_subdirectory(dnode)
add_subdirectory(dmaster)
