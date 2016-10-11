//
// Created by owt50 on 2016/10/9.
//

#ifndef ATFRAMEWORK_LIBSIMULATOR_SIMULATOR_BASE_H
#define ATFRAMEWORK_LIBSIMULATOR_SIMULATOR_BASE_H

#pragma once

#include <std/smart_ptr.h>
#include <std/functional.h>
#include <lock/spin_lock.h>
#include <config/compiler_features.h>
#include <map>
#include <set>
#include <list>
#include <fstream>

#include <uv.h>

#include "simulator_player_impl.h"

#include <cli/cmd_option.h>

class simulator_base {
public:
    typedef std::shared_ptr<simulator_player_impl> player_ptr_t;
    typedef std::function<void (util::cli::callback_param)> cmd_fn_t;

    struct cmd_wrapper_t {
        typedef std::map<std::string, cmd_wrapper_t> value_type;
        value_type children;
        std::string name;
        std::shared_ptr<util::cli::cmd_option_ci> owner;

        cmd_wrapper_t(std::shared_ptr<util::cli::cmd_option_ci> o, const std::string& n);

        // create a child node
        cmd_wrapper_t& operator[](const std::string& name);

        // bind a cmd handle
        cmd_wrapper_t& bind(cmd_fn_t fn, const std::string& description);
    };

public:
    simulator_base();
    virtual ~simulator_base();

    int init();

    int run(int argc, const char* argv[]);

    int stop();

    template<typename Ty>
    bool insert_player(std::shared_ptr<Ty> player) {
        return insert_player(std::dynamic_pointer_cast<simulator_player_impl>(player));
    }

    bool insert_player(player_ptr_t player);
    void remove_player(const std::string& id, bool is_close = true);
    void remove_player(player_ptr_t player);
    player_ptr_t get_player_by_id(const std::string& id);

    inline bool is_closing() const { return is_closing_; }

    template<typename Ty>
    std::shared_ptr<Ty> create_player(const std::string& host, int port) {
        if (is_closing_) {
            return std::shared_ptr<Ty>();
        }

        std::shared_ptr<Ty> ret = std::make_shared<Ty>();
        player_ptr_t bret = std::dynamic_pointer_cast<simulator_player_impl>(ret);
        ret->watcher_ = bret;

        if(0 != ret->connect(host, port)) {
            return std::shared_ptr<Ty>();
        }

        connecting_players_.insert(bret);
        return ret;
    }

    inline cmd_wrapper_t& reg_req() { return *root_; }
    inline uv_loop_t* get_loop() { return &loop_; }
    inline const char* get_exec() const { return exec_path_; }
    inline std::shared_ptr<util::cli::cmd_option> get_option_manager() { return args_mgr_; }
    inline std::shared_ptr<util::cli::cmd_option_ci> get_cmd_manager() { return cmd_mgr_; }
    inline std::vector<unsigned char>& get_msg_buffer() { return shell_opts_.buffer_; }

    // this must be thread-safe
    int insert_cmd(player_ptr_t player, const std::string& cmd);
    void set_current_player(player_ptr_t p) { cmd_player_ = p; }
    const player_ptr_t& get_current_player() const { return cmd_player_; }

    virtual int dispatch_message(player_ptr_t player, const void* buffer, size_t sz) = 0;
    virtual void exec_cmd(player_ptr_t player, const std::string& cmd) = 0;

    static void libuv_on_async_cmd(uv_async_t* handle);

    static char **libedit_completion(const char *, int, int);
    static char* libedit_complete_cmd_generator(const char* text, int state);
    static void libedit_thd_main(void* arg);
private:
    bool is_closing_;
    const char* exec_path_;
    uv_loop_t loop_;
    uv_async_t async_cmd_;
    player_ptr_t cmd_player_;
    uv_thread_t thd_cmd_;
    std::map<std::string, player_ptr_t> players_;
    std::set<player_ptr_t> connecting_players_;
    std::shared_ptr<util::cli::cmd_option_ci> cmd_mgr_;
    std::shared_ptr<util::cli::cmd_option> args_mgr_;
    std::shared_ptr<cmd_wrapper_t> root_;

protected:
    typedef struct {
        std::string history_file;
        std::string protocol_log;
        bool no_interactive;
        std::string read_file;
        std::vector<std::string> cmds;
        std::vector<unsigned char> buffer_;
    } shell_cmd_opts_t;
    shell_cmd_opts_t shell_opts_;

    typedef struct {
        util::lock::spin_lock lock;
        std::list<std::pair<player_ptr_t, std::string> > cmds;
    } shell_cmd_data_t;
    shell_cmd_data_t shell_cmd_manager_;
};

template<typename TPlayer, typename TMsg>
class simulator_msg_base : public simulator_base {
public:
    typedef TMsg msg_t;
    typedef TPlayer player_t;
    typedef std::shared_ptr<player_t> player_ptr_t;

    typedef std::function<void (player_ptr_t, msg_t&)> rsp_fn_t;

    struct cmd_sender_t {
        player_ptr_t player;
        std::list<msg_t> requests;
        simulator_base* self;
    };

public:
    virtual ~simulator_msg_base();

    bool insert_player(player_ptr_t player) {
        return simulator_base::insert_player<TPlayer>(player);
    }

    void reg_rsp(uint32_t msg_id, rsp_fn_t fn) {
        msg_id_handles_[msg_id] = fn;
    }

    void reg_rsp(const std::string& msg_name, rsp_fn_t fn) {
        msg_name_handles_[msg_name] = fn;
    }

    static cmd_sender_t& get_sender(util::cli::callback_param param) {
        return *reinterpret_cast<cmd_sender_t*>(param.get_ext_param());
    }

    void write_protocol(const msg_t& msg) {
        const std::string& text = dump_message(msg);
        if(text.empty() || shell_opts_.protocol_log.empty()) {
            return;
        }

        if (!proto_file.is_open()) {
            proto_file.open(shell_opts_.protocol_log.c_str(), std::ios::app | std::ios::out | std::ios::binary);
        }

        if (proto_file.is_open()) {
            proto_file<< "======================================"<< std::endl<< text<< std::endl;
        }
    }

    virtual int dispatch_message(simulator_base::player_ptr_t bp, const void* buffer, size_t sz) UTIL_CONFIG_OVERRIDE {
        player_ptr_t player = std::dynamic_pointer_cast<player_t >(bp);
        msg_t msg;
        int ret = unpack_message(msg, buffer, sz);
        if (ret < 0) {
            return ret;
        }

        write_protocol(msg);
        return dispatch_message(player, msg);
    }

    virtual int dispatch_message(player_ptr_t player, msg_t& msg) {
        uint32_t msg_id = pick_message_id(msg);
        if (msg_id != 0) {
            std::map<uint32_t, rsp_fn_t>::iterator iter = msg_id_handles_.find(msg_id);
            if (msg_id_handles_.end() != iter && iter->second) {
                iter->second(player, msg);
            }
        }

        std::string msg_name = pick_message_name(msg);
        if (!msg_name.empty()) {
            std::map<std::string, rsp_fn_t>::iterator iter = msg_name_handles_.find(msg_id);
            if (msg_name_handles_.end() != iter && iter->second) {
                iter->second(player, msg);
            }
        }

        return 0;
    }

    virtual void exec_cmd(simulator_base::player_ptr_t p, const std::string& cmd) UTIL_CONFIG_OVERRIDE {
        cmd_sender_t sender;
        sender.self = this;
        sender.player = std::dynamic_pointer_cast<player_t>(p);
        get_cmd_manager()->start(cmd, true, &sender);

        for(std::list<msg_t>::iterator iter = sender.requests.begin(); sender.player && iter != sender.requests.end(); ++ iter) {
            size_t msg_len = get_msg_buffer().size();
            if (pack_message(*iter, &get_msg_buffer()[0], msg_len) >= 0) {
                write_protocol(*iter);
                sender.player->on_write_message(&get_msg_buffer()[0], msg_len);
            }
        }
    }

    virtual uint32_t pick_message_id(const msg_t& msg) const {
        return 0;
    };

    virtual std::string pick_message_name(const msg_t& msg) const {
        return std::string();
    }

    virtual const std::string& dump_message(const msg_t& msg) { return std::string(); }

    virtual int pack_message(const msg_t& msg, void* buffer, size_t& sz) const = 0;
    virtual int unpack_message(msg_t& msg, const void* buffer, size_t sz) const = 0;
private:
    std::map<uint32_t, rsp_fn_t> msg_id_handles_;
    std::map<std::string, rsp_fn_t> msg_name_handles_;
    std::fstream proto_file;
};


#endif //ATFRAMEWORK_LIBSIMULATOR_SIMULATOR_BASE_H