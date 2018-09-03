cmake_minimum_required(VERSION 3.12)
project(Network_Radio)

find_package ( Threads REQUIRED )

set(CMAKE_CXX_STANDARD 14)

macro (add_executable _name)
    # invoke built-in add_executable
    _add_executable(${ARGV})
    if (TARGET ${_name})
        target_link_libraries(${_name} ${CMAKE_THREAD_LIBS_INIT} )
    endif()
endmacro()

add_executable(
        Sender
        sender/sender.cpp
        common/err.cpp
        common/err.h
        common/logger.cpp
        common/logger.h
        common/debug.h
        common/helper.cpp
        common/helper.h)

add_executable(
        Receiver
        receiver/receiver.cpp
        common/err.cpp
        common/err.h
        common/logger.cpp
        common/logger.h
        common/debug.h
        common/helper.cpp
        common/helper.h
        receiver/telnet.cpp
        receiver/telnet.h
        receiver/discover.cpp
        receiver/discover.h
        receiver/retransmit.cpp
        receiver/retransmit.h
        receiver/outUploader.cpp
        receiver/outUploader.h common/logger.h common/logger.cpp common/debug.h)