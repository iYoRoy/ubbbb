#pragma once
#include <iostream>
#include <sstream>
#include <cstdio>
#include <signal.h>
#include <fstream>
#include <mutex>
// 静态链接 mirai-cpp 要在引入头文件前定义这个宏
#define MIRAICPP_STATICLIB
#include <mirai.h>