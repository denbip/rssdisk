project(rssdisk_client)
cmake_minimum_required(VERSION 2.8)

ADD_DEFINITIONS(-std=c++14) # -g
aux_source_directory(. SRC_LIST)

#AD HEADERS VISIBILITY
file(GLOB_RECURSE LibFiles *)
add_custom_target(headers SOURCES ${LibFiles})

set(PAHT_LIB "../../libs")

file(GLOB LIB_FILES
    "${PAHT_LIB}/basefunc_std.cpp"
    "${PAHT_LIB}/commpression_zlib.cpp"
    "${PAHT_LIB}/json/jsoncpp.cpp"
    "${PAHT_LIB}/network/network_std.cpp"
    "${PAHT_LIB}/DH/dh.cpp"
    "${PAHT_LIB}/md5.cpp"
    "${PAHT_LIB}/network/tcp_client.cpp"
    "${PAHT_LIB}/gost_28147_89/gost_89.cpp"
    "rssdisk_pool.cpp" )

#ADD_LIBRARY(${PROJECT_NAME} SHARED ${SRC_LIST} ${LIB_FILES})
ADD_LIBRARY(${PROJECT_NAME} STATIC ${SRC_LIST} ${LIB_FILES})
target_link_libraries(${PROJECT_NAME} z cryptopp)


