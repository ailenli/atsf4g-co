//
// Created by owt50 on 2016/9/26.
//

#include "task_manager.h"

task_manager::task_manager() {
}

task_manager::~task_manager() {
}

int task_manager::init() {
    native_mgr_ = mgr_t::create();

    return 0;
}

int task_manager::start_task(id_t task_id, moyo_no1::message_container& msg) {
    int res = native_mgr_->start(task_id, &msg);
    if (res < 0) {
        WLOGERROR("start task 0x%llx failed.", reinterpret_cast<unsigned long long>(task_id));

        // 错误码
        return moyo_no1::err::EN_SYS_NOTFOUND;
    }

    return 0;
}

int task_manager::resume_task(id_t task_id, moyo_no1::message_container& msg) {
    int res = native_mgr_->resume(task_id, &msg);
    if (res < 0) {
        WLOGERROR("resume task 0x%llx failed.", reinterpret_cast<unsigned long long>(task_id));

        // 错误码
        return moyo_no1::err::EN_SYS_NOTFOUND;
    }

    return 0;
}

int task_manager::tick(time_t sec, int nsec) {
    native_mgr_->tick(sec, nsec);
    return 0;
}

size_t task_manager::get_stack_size() const {
    return 512 * 1024;
    // TODO read from configure
    // return LogicConfig::Instance()->GetCfgLogic().m_iTaskStackSize;
}

int task_manager::add_task(const std::shared_ptr<task_t>& task, time_t timeout) {
    int res = 0;
    if (0 == timeout) {
        // TODO read default timeout from configure
        // res = native_mgr_->add_task(task, timeout, LogicConfig::Instance()->GetCfgLogic().m_iMsgTimeoutSec);
        res = native_mgr_->add_task(task, timeout, 10);
    } else {
        res = native_mgr_->add_task(task, timeout);
    }

    if (res < 0) {
        WLOGERROR("add task failed, res: %d", res);
        return moyo_no1::err::EN_SYS_PARAM;
    }

    return moyo_no1::err::EN_SUCCESS;
}