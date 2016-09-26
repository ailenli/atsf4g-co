//
// Created by owt50 on 2016/9/26.
//

#include <protocol/pbdesc/svr.container.pb.h>

#include "task_action_base.h"

task_action_base::task_action_base(): task_id_(0), ret_code_(0), rsp_code_(0) {}
task_action_base::~task_action_base() {}

const char* task_action_base::name() const {
    const char *ret = typeid(*this).name();
    if (NULL == ret) {
        return "RTTI Unavailable: task_action_base";
    }

    // some compiler will generate number to mark the type
    while (ret && *ret >= '0' && *ret <= '9') {
        ++ret;
    }
    return ret;
}

int task_action_base::operator()(void *priv_data) {
    if (NULL != priv_data) {
        request_msg_.Swap(reinterpret_cast<hello::message_container*>(priv_data));
    }

    task_manager::task_t* task = cotask::this_task::get<task_manager::task_t>();
    if (NULL == task) {
        WLOGERROR("task convert failed, must in task.");
        return moyo_no1::err::EN_SYS_INIT;
    }

    task_id_ = task->get_id();
    ret_code_ = (*this)(request_msg_);

    // 响应OnSuccess(这时候任务的status还是running)
    if (cotask::EN_TS_RUNNING == task->get_status() && res >= 0) {
        int ret = on_success();
        send_rsp_msg();
        return ret;
    }

    if (hello::err::EN_SUCCESS == ret_code_) {
        ret_code_ = moyo_no1::err::EN_SYS_UNKNOWN;
    }

    if (hello::EN_SUCCESS == rsp_code_) {
        rsp_code_ = moyo_no1::EN_ERR_UNKNOWN;
    }

    WLOGERROR("task %s [%" PRIu64 "] ret code %d, rsp code %d\n",
        Name(), GetTaskId(), ret_code_, rsp_code_);

    // 响应OnTimeout
    if (cotask::EN_TS_TIMEOUT == task->get_status()) {
        on_timeout();
        int ret = on_failed();
        send_rsp_msg();
        return ret;
    }

    // 如果不是running且不是timeout，可能是其他原因被kill掉了，响应OnFailed
    int ret = on_failed();
    send_rsp_msg();
    return ret;
}

int task_action_base::on_success() {
    return 0;
}

int task_action_base::on_failed() {
    return 0;
}

int task_action_base::on_timeout() {
    return 0;
}

uint64_t task_action_base::get_task_id() const {
    return task_id_;
}

hello::message_container& task_action_base::get_request() {
    return request_msg_;
}

const hello::message_container& task_action_base::get_request() const {
    return request_msg_;
}