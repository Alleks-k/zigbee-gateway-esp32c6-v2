#include <stdint.h>

#include "esp_zigbee_core.h"

#if CONFIG_GATEWAY_SELF_TEST_APP
void send_on_off_command(uint16_t short_addr, uint8_t endpoint, uint8_t on_off)
{
    (void)short_addr;
    (void)endpoint;
    (void)on_off;
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    (void)signal_struct;
}
#endif
