#include "conf.h"

#ifdef __cplusplus
extern "C"
#endif
	void* setup_ws_server();

#ifdef __cplusplus
extern "C"
#endif
    void destroy_ws_server(void *ws_server);

#ifdef __cplusplus
extern "C"
#endif
	int ws_server_done(void* ws_server);

#ifdef __cplusplus
extern "C"
#endif
	void ws_server_send_done(void* ws_server);

#ifdef __cplusplus
extern "C"
#endif
	int maybe_reload_config(void* ws_server);

#ifdef __cplusplus
extern "C"
#endif
	void ws_server_send(void* buffer, size_t len, void* ws_server, unsigned channel_id);

#ifdef __cplusplus
extern "C"
#endif
	int ws_server_should_reload_config(void* ws_server);

#ifdef __cplusplus
extern "C"
#endif
	void ws_server_load_config(void* ws_server, char* config_file_path, conf_t* conf);

#ifdef __cplusplus
extern "C"
#endif
	void ws_server_update_config(void* ws_server);

#ifdef __cplusplus
extern "C"
#endif
	void ws_server_push_stream(void* ws_server, unsigned stream_id, unsigned channels, unsigned frame_size, const char* label);
