#ifndef WEB_SERVER_H
#define WEB_SERVER_H

void start_web_server(void);
void stop_web_server(void);
int web_server_get_ws_client_count(void);
void web_server_broadcast_ws_status(void);

#endif
