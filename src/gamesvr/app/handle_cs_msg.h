//
// Created by owt50 on 2016/10/9.
//

#ifndef _LOGIC_SVR_APP_HANDLE_CS_MSG_H
#define _LOGIC_SVR_APP_HANDLE_CS_MSG_H

#pragma once

#include <design_pattern/singleton.h>

class app_handle_cs_msg: public ::util::design_pattern::singleton<app_handle_cs_msg> {
public:
    int init();
};

#endif //_LOGIC_SVR_APP_HANDLE_CS_MSG_H