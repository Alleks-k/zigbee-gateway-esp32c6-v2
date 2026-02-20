#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "api_usecases.h"
#include "ws_manager.h"

void start_web_server(ws_manager_handle_t ws_manager, api_usecases_handle_t usecases);
void stop_web_server(void);
int web_server_get_ws_client_count(void);
void web_server_broadcast_ws_status(void);

#endif
