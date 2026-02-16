set(gateway_core_state_srcs
    "src/gateway_state.c"
    "src/gateway_state_lock.c"
    "src/gateway_state_lock_freertos.c"
)

if(CONFIG_GATEWAY_STATE_ALLOW_NOOP_LOCK_BACKEND)
    list(APPEND gateway_core_state_srcs "src/gateway_state_lock_noop.c")
endif()

idf_component_register(
    SRCS
        ${gateway_core_state_srcs}
    INCLUDE_DIRS
        "include"
    REQUIRES
        gateway_shared_config
)
