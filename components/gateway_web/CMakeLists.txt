set(gateway_web_srcs
    "src/web_server.c"
    "src/http_routes.c"
    "gateway_web_placeholder.c")

set(gateway_web_reqs
    gateway_web_api
    gateway_web_ws
    gateway_web_static
    esp_http_server)

set(gateway_web_priv_reqs
)

set(gateway_web_priv_include_dirs
)

if(CONFIG_GATEWAY_SELF_TEST_APP)
    list(APPEND gateway_web_srcs "test/gateway_web_self_tests.c")
    list(APPEND gateway_web_reqs
        gateway_net
        unity)
    list(APPEND gateway_web_priv_reqs
        gateway_core_state
        gateway_net
        unity)
    list(APPEND gateway_web_priv_include_dirs
        "${CMAKE_SOURCE_DIR}/components/gateway_net/include"
        "${IDF_PATH}/components/unity/include"
        "${IDF_PATH}/components/unity/unity/src")
endif()

idf_component_register(
    SRCS
        ${gateway_web_srcs}
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        ${gateway_web_priv_include_dirs}
    REQUIRES
        ${gateway_web_reqs}
    PRIV_REQUIRES
        ${gateway_web_priv_reqs}
)
