#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "api_usecases.h"

void start_web_server(api_usecases_handle_t usecases);
void stop_web_server(void);
int web_server_get_ws_client_count(void);
void web_server_broadcast_ws_status(void);

#endif
