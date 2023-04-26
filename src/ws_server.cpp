#include <iostream>
#include <set>
#include <string>
#include <polyfills.h>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

/*#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>*/
#include <websocketpp/common/thread.hpp>

#include "ws_server.h"
#include "fx_chain_utils.h"
#include "util.h"

typedef websocketpp::server<websocketpp::config::asio> server;
typedef websocketpp::message_buffer::message<websocketpp::message_buffer::alloc::con_msg_manager> message_type;
typedef websocketpp::message_buffer::alloc::con_msg_manager<message_type> con_msg_man_type;

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

using websocketpp::lib::thread;
using websocketpp::lib::mutex;
using websocketpp::lib::lock_guard;
using websocketpp::lib::unique_lock;
using websocketpp::lib::condition_variable;

/* on_open insert connection_hdl into channel
 * on_close remove connection_hdl from channel
 * on_message queue send to all channels
 */

enum action_type {
    SUBSCRIBE,
    UNSUBSCRIBE,
    MESSAGE
};

struct action {
    action(action_type t, connection_hdl h) : type(t), hdl(h) {}
    action(action_type t, connection_hdl h, server::message_ptr m)
        : type(t), hdl(h), msg(m) {}
    action(action_type t, server::message_ptr m) : type(t), msg(m) {}

    action_type type;
    websocketpp::connection_hdl hdl;
    server::message_ptr msg;
};

typedef websocketpp::lib::shared_ptr<websocketpp::lib::thread> thread_ptr;

// pack struct for wire transmission
#pragma pack(push, 1)
struct s_stream_frame_buffer {
    uint16_t stream_id;
    int16_t data[1];
};
#pragma pack(pop)

struct s_stream {
    uint16_t id;
    unsigned channels;
    std::string label;
    s_stream_frame_buffer* stream_frame_buffer;
};

class broadcast_server {
public:
    broadcast_server() {
        // Initialize Asio Transport
        m_server.init_asio();

        // Register handler callbacks
        m_server.set_open_handler(bind(&broadcast_server::on_open, this, ::_1));
        m_server.set_close_handler(bind(&broadcast_server::on_close, this, ::_1));
        m_server.set_message_handler(bind(&broadcast_server::on_message, this, ::_1, ::_2));

        manager = websocketpp::lib::make_shared<con_msg_man_type>();
    }

    ~broadcast_server() {
        if (is_quit)
            return;

        is_quit = true;
        m_action_cond.notify_one();
        delete conf_str;
        stop_server();
        free_streams();
    }

    void stop_server() {
        unique_lock<mutex> lock(m_server_init_lock);

        while (!is_initialized)
        {
            m_server_init_cond.wait(lock);
        }

        lock.unlock();
        m_server.stop();
    }

    void run(uint16_t port) {
        {
            lock_guard<mutex> guard(m_server_init_lock);

            // listen on specified port
            m_server.listen(port);

            // Start the server accept loop
            m_server.start_accept();

            // std::this_thread::sleep_for(std::chrono::milliseconds(5000));

            is_initialized = true;
        }
        m_server_init_cond.notify_all();

        if (!is_quit)
        {
            // Start the ASIO io_service run loop
            try
            {
                m_server.run();
            } catch (const std::exception &e)
            {
                std::cout << e.what() << std::endl;
            }
        }

        std::cout << "broadcast_server::run terminated" << std::endl;
    }

    void on_open(connection_hdl hdl) {
        needs_data = true;
        {
            lock_guard<mutex> guard(m_action_lock);
            //std::cout << "on_open" << std::endl;
            m_actions.push(action(SUBSCRIBE, hdl));
        }
        m_action_cond.notify_one();
    }

    void on_close(connection_hdl hdl) {
        {
            lock_guard<mutex> guard(m_action_lock);
            //std::cout << "on_close" << std::endl;
            m_actions.push(action(UNSUBSCRIBE, hdl));
        }
        m_action_cond.notify_one();
    }

    void on_message(connection_hdl hdl, server::message_ptr msg) {
        std::string ws_payload = msg->get_payload();
        size_t idx = ws_payload.find(':');
        std::string type = ws_payload;
        std::string payload = "";
        if (idx != std::string::npos) {
            type = ws_payload.substr(0, idx);
            payload = ws_payload.substr(idx + 1, ws_payload.length());
        }
        if (type == "getConfig") {
            std::stringstream ss;
            ss << "{\"type\": \"config\", \"payload\": \"" << conf_str << "\"}";
            send_msg(ss.str());
        }
        if (type == "setConfig") {
            strcpy(conf_str, payload.c_str());
            b_should_reload_config = true;
            std::stringstream ss;
            ss << "{\"type\": \"config\", \"payload\": \"" << conf_str << "\"}";
            send_msg(ss.str());
        }
        // queue message up for sending by processing thread
        /* {
            lock_guard<mutex> guard(m_action_lock);
            //std::cout << "on_message" << std::endl;
            m_actions.push(action(MESSAGE, hdl, msg));
        }
        m_action_cond.notify_one(); */
    }

    void send(server::message_ptr msg) {
        // queue message up for sending by processing thread
        {
            lock_guard<mutex> guard(m_action_lock);
            //std::cout << "on_message" << std::endl;
            m_actions.push(action(MESSAGE, msg));
        }
        m_action_cond.notify_one();
    }

    void send_msg(websocketpp::frame::opcode::value op, void* payload, size_t len) {
        message_type::ptr msg = manager->get_message(op, len);
        msg->set_payload(payload, len);
        send(msg);
    }

    void send_msg(const std::string &payload) {
        message_type::ptr msg = manager->get_message();
        msg->set_opcode(websocketpp::frame::opcode::text);
        msg->set_payload(payload);
        send(msg);
    }

    void send_channel_data(void* payload, size_t len, unsigned stream_id) {
        s_stream stream = streams[stream_id];
        memcpy(stream.stream_frame_buffer->data, payload, len);
        size_t s = sizeof(s_stream_frame_buffer) - sizeof(int16_t);
        send_msg(websocketpp::frame::opcode::BINARY, (void*)stream.stream_frame_buffer, s + len);
    }

    void process_messages() {
        while (!is_quit) {
            unique_lock<mutex> lock(m_action_lock);

            while (m_actions.empty() && !is_quit) {
                m_action_cond.wait(lock);
            }

            if (is_quit)
                break;

            action a = m_actions.front();
            m_actions.pop();

            lock.unlock();

            if (a.type == SUBSCRIBE) {
                lock_guard<mutex> guard(m_connection_lock);
                m_connections.insert(a.hdl);
            }
            else if (a.type == UNSUBSCRIBE) {
                lock_guard<mutex> guard(m_connection_lock);
                m_connections.erase(a.hdl);
            }
            else if (a.type == MESSAGE) {
                lock_guard<mutex> guard(m_connection_lock);

                con_list::iterator it;
                for (it = m_connections.begin(); it != m_connections.end(); ++it) {
                    try {
                        m_server.send(*it, a.msg);
                    }
                    // catch (websocketpp::lib::error_code e) {
                    catch (...) {
                        std::cout << "error" << std::endl;
                    }
                }
            }
            else {
                // undefined.
            }
        }

        std::cout << "broadcast_server::process_messages terminated" << std::endl;
    }

    bool should_reload_config() {
        return b_should_reload_config;
    }

    void push_stream(unsigned stream_id, unsigned channels, unsigned frame_size, const char* label) {
        s_stream stream;
        stream.id = stream_id;
        stream.channels = channels;
        stream.label = label;
        //stream.stream_frame_buffer.stream_id = stream_id;
        //stream.stream_frame_buffer.data = new s_stream_frame_buffer[sizeof(int16_t) * channels * frame_size];
        //s_stream_frame_buffer* stream_frame_buffer = new s_stream_frame_buffer[sizeof(int16_t) * channels * frame_size];

        size_t s = sizeof(s_stream_frame_buffer) - sizeof(int16_t);
        stream.stream_frame_buffer = (s_stream_frame_buffer*)malloc(s + sizeof(int16_t) * channels * frame_size);
        stream.stream_frame_buffer->stream_id = stream_id;

        streams[stream_id] = stream;
    }

    void free_streams() {
        for (auto &p : streams)
        {
            free_stream(&p.second);
        } 
    }

    void free_stream(s_stream *stream) {
        free(stream->stream_frame_buffer);
    }

    char* conf_str;
    conf_t* conf;
    bool b_should_reload_config = false;
    bool done = false;
    bool needs_data = false;
    bool should_send = false;
    std::vector<thread_ptr> ts;
    std::atomic<bool> is_initialized = false;
    std::atomic<bool> is_quit = false;

private:
    typedef std::set<connection_hdl, std::owner_less<connection_hdl>> con_list;

    server m_server;
    con_list m_connections;
    std::queue<action> m_actions;

    mutex m_action_lock;
    mutex m_connection_lock;
    mutex m_server_init_lock;
    condition_variable m_action_cond;
    condition_variable m_server_init_cond;

    con_msg_man_type::ptr manager;
    std::map<unsigned, s_stream> streams;
};

void* setup_ws_server() {
    try {
        broadcast_server *server_instance = new broadcast_server;

        server_instance->ts.push_back(
            websocketpp::lib::make_shared<websocketpp::lib::thread>(&broadcast_server::run, server_instance, 9002));

        server_instance->ts.push_back(
            websocketpp::lib::make_shared<websocketpp::lib::thread>(bind(&broadcast_server::process_messages, server_instance)));

        for (size_t i = 0; i < server_instance->ts.size(); i++)
        {
            server_instance->ts[i]->detach();
        }

        return server_instance;
    }
    catch (websocketpp::exception const& e) {
        std::cout << e.what() << std::endl;
    }

    return NULL;
}

void destroy_ws_server(void *ws_server) {
    broadcast_server *server_instance = (broadcast_server *)ws_server;
    if (!server_instance || server_instance->is_quit)
        return;
    delete server_instance;
    ws_server = NULL;
}

#define CONFIG_LINE_BUF_SIZE (256)
#define CONFIG_SIZE (2048)

int ws_server_parse_config(char* buf, char* config)
{
    char dummy[CONFIG_SIZE];
    if (sscanf(buf, " %s", dummy) == EOF)
        return 0; // blank line
    if (sscanf(buf, " %[#]", dummy) == 1)
        return 0; // comment#
    if (sscanf(buf, " fx = %s", dummy) == 1)
    {
        strcat(config, "fx = ");
        strcat(config, dummy);
        strcat(config, "\\n");
        return 0;
    }
    return 0; // unknown
}

void get_config(char* config_file_path, char* config)
{
    FILE* f = fopen(config_file_path, "r");
    char buf[CONFIG_LINE_BUF_SIZE];
    int line_number = 0;
    while (fgets(buf, sizeof buf, f))
    {
        ++line_number;
        int err = ws_server_parse_config(buf, config);
        if (err)
            fprintf(stderr, "error line %d: %d\n", line_number, err);
    }
}

int ws_server_done(void* ws_server) {
    broadcast_server *server_instance = (broadcast_server *)ws_server;
    if (!server_instance || server_instance->is_quit)
        return -1;

    return (server_instance->done) ? 1 : 0;
}

int maybe_reload_config(void* ws_server) {
    broadcast_server *server_instance = (broadcast_server *)ws_server;
    if (!server_instance || server_instance->is_quit)
        return -1;

    if (server_instance->should_reload_config())
    {
        ws_server_update_config(ws_server);
        server_instance->needs_data = true;
        return 1;
    }
    return 0;
}

void ws_server_send(void* buffer, size_t len, void* ws_server, unsigned stream_id) {
    broadcast_server *server_instance = (broadcast_server *)ws_server;
    if (!server_instance || server_instance->is_quit)
        return;

    if (server_instance->done) { // first packet
        if (server_instance->needs_data) {
            server_instance->needs_data = false;
            server_instance->should_send = true;
        }
        server_instance->done = false;
    }
    if (server_instance->should_send) {
        server_instance->send_channel_data(buffer, len, stream_id);
    }
}

void ws_server_send_done(void* ws_server) {
    broadcast_server *server_instance = (broadcast_server *)ws_server;
    if (!server_instance || server_instance->is_quit)
        return;

    server_instance->done = true;
    if (server_instance->should_send) {
        server_instance->should_send = false;
        server_instance->send_msg("{\"type\": \"done\", \"payload\": \"\"}");
    } else {
        if (!maybe_reload_config(ws_server)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void ws_server_load_config(void* ws_server, char* config_file_path, conf_t* conf) {
    broadcast_server *server_instance = (broadcast_server *)ws_server;
    if (!server_instance || server_instance->is_quit)
        return;

    char* conf_str = new char[CONFIG_SIZE];
    conf_str[0] = 0;
    get_config(config_file_path, conf_str);
    // printf("%s", config);
    server_instance->conf_str = conf_str;
    server_instance->conf = conf;
}

int ws_server_should_reload_config(void* ws_server) {
    broadcast_server *server_instance = (broadcast_server *)ws_server;
    if (!server_instance || server_instance->is_quit)
        return -1;

    return (server_instance->should_reload_config())? 1 : 0;
}

void ws_server_update_config(void* ws_server) {
    broadcast_server *server_instance = (broadcast_server *)ws_server;
    if (!server_instance || server_instance->is_quit)
        return;

    fx_chain_free(server_instance->conf->chain);
    int line_number = 0;

    std::string delimiter = "\\n";
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string s = server_instance->conf_str;
    std::string token;
    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        ++line_number;
        int err = parse_config((char*)token.c_str(), server_instance->conf);
        if (err)
            fprintf(stderr, "error line %d: %d\n", line_number, err);
    }

    server_instance->b_should_reload_config = false;
}

void ws_server_push_stream(void* ws_server, unsigned stream_id, unsigned channels, unsigned frame_size, const char* label) {
    broadcast_server *server_instance = (broadcast_server *)ws_server;
    if (!server_instance || server_instance->is_quit)
        return;

    server_instance->push_stream(stream_id, channels, frame_size, label);
}
