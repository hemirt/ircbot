cmake_minimum_required (VERSION 3.8)

SET(Boost_USE_STATIC_LIBS OFF)
SET(Boost_USE_MULTITHREAD ON)
FIND_PACKAGE(Boost 1.60.0 REQUIRED COMPONENTS system thread regex)

IF(Boost_FOUND)
	INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
	LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})
ENDIF(Boost_FOUND)

find_package(SQLite3)

include_directories(${SQLite3_INCLUDE_DIRS})

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)

SET(USED_LIBS ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY} ${Boost_REGEX_LIBRARY} ${SQLite3_LIBRARIES} Threads::Threads CURL::libcurl)

set(ircchatbot_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ircclient.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ircclient.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/ircauthsequence.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/ircmessage.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/ircmessage.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/chatbot.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/chatbot.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/database.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/database.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/users.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/users.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/commandshandler.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/commandshandler.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/channels.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/channels.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/echopage.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/echopage.cpp
)

add_executable (ircchatbot ${ircchatbot_SOURCES})
TARGET_LINK_LIBRARIES(ircchatbot ${USED_LIBS})