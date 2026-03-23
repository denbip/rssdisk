cmake_minimum_required(VERSION 2.8)
project(rssdisk)

aux_source_directory(. SRC_LIST)

#set(CMAKE_BUILD_TYPE Debug)

ADD_DEFINITIONS(-std=c++14 -DLOGGING_ENABLED2 -Dbasefunc_std_no_fstream2 -Ddisable_cout_for_debug -Ddont_use_openssl_prime -Dtcp_client_log_on_connect -Depoll_v2_control_activity)

#AD HEADERS VISIBILITY
file(GLOB_RECURSE LibFiles *)
add_custom_target(headers SOURCES ${LibFiles})

SET(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} ${GCC_LINK_FLAGS})

set(PAHT_LIB "../../libs")
file(GLOB LIB_FILES "${PAHT_LIB}/basefunc_std.cpp" "hstf.cpp" "${PAHT_LIB}/base64.cpp" "${PAHT_LIB}/curl_http.cpp" "disk_server.cpp"
    "extentions/j_manager.cpp" "extentions/e_manager.cpp" "extentions/c_manager.cpp" "extentions/b_tree.cpp"
    "${PAHT_LIB}/commpression_zlib.cpp" "${PAHT_LIB}/json/jsoncpp.cpp" "${PAHT_LIB}/md5.cpp" "${PAHT_LIB}/network/epoll_v2.cpp"
    "${PAHT_LIB}/DH/dh.cpp" "${PAHT_LIB}/commpression_zlib.cpp" "${PAHT_LIB}/network/network_std.cpp"
    "${PAHT_LIB}/gost_28147_89/gost_89.cpp")

add_executable(${PROJECT_NAME} "main.cpp" ${LIB_FILES})
target_link_libraries(${PROJECT_NAME} curl dl pthread boost_system boost_iostreams boost_filesystem boost_thread z cryptopp)
