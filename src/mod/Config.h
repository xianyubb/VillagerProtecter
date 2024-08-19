#pragma once

#include <string>


struct Config {
    int version = 2;
    std::string ScoreName = "money"; // 计分板名称
    int         cast      = 1000; // 开启无敌花费金额
};


extern Config config;
