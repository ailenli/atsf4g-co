//
// Created by owt50 on 2016/10/9.
//

#ifndef ATFRAMEWORK_LIBSIMULATOR_SIMULATOR_PLAYER_IMPL_H
#define ATFRAMEWORK_LIBSIMULATOR_SIMULATOR_PLAYER_IMPL_H

#pragma once

#include <string>
#include <std/smart_ptr.h>

#include <uv.h>

class simulator_base;

class simulator_player_impl {
public:
    typedef std::shared_ptr<simulator_player_impl> ptr_t;

protected:
    simulator_player_impl();
    virtual ~simulator_player_impl() = 0;

public:

    inline const std::string& get_id() const { return id_; }
    bool set_id(const std::string& id);

    virtual void on_connected(uv_connect_t *req, int status) = 0;
    virtual void on_alloc(size_t suggested_size, uv_buf_t* buf) = 0;
    virtual void on_read_data(ssize_t nread, const uv_buf_t *buf) = 0;
    virtual void on_read_message(const void* buffer, size_t sz) = 0;
    virtual void on_written_data(uv_write_t *req, int status) = 0;
    virtual int on_write_message(void *buffer, uint64_t sz) = 0;
    /**
     * @brief start to close player resource
     */
    virtual void on_close();
    /**
     * @brief already closed and can not access any resource any more
     */
    virtual void on_closed();

    int connect(const std::string& host, int port);

    int write_message(void *buffer, uint64_t sz);
    int read_message(const void *buffer, uint64_t sz);

    int close();

    // this function must be thread-safe
    int insert_cmd(const std::string &cmd);
private:
    static void libuv_on_dns_callback(uv_getaddrinfo_t *req, int status, struct addrinfo *res);
    static void libuv_on_connected(uv_connect_t *req, int status);
    static void libuv_on_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
    static void libuv_on_read_data(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
    static void libuv_on_written_data(uv_write_t *req, int status);
    static void libuv_on_closed(uv_handle_t *handle);

private:
    bool is_closing_;
    int port_;
    std::string id_;
    simulator_base* owner_;
    std::weak_ptr<simulator_player_impl> watcher_;

    struct libuv_data_t {
        uv_tcp_t tcp_sock;
        uv_connect_t connect_req;
        uv_getaddrinfo_t dns_req;
        uv_write_t write_req;
        uv_timer_t tick_timer;
    };
    libuv_data_t network_;
    ptr_t network_write_holder_;

    friend class simulator_base;
};


#endif //ATF4G_CO_SIMULATOR_PLAYER_IMPL_H