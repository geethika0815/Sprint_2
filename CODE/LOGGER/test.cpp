#pragma once

#include "logger.h"

#include <assert.h>

#include <thread>


int main()
{
        const char* name = "User";
        LOG_INFO("Hello *WELCOME TO CHAT BOX APPLICATION*%s", name);

        LOG_CRITICAL("OH NO!", name);

        LOG_WARN("Hello %d %d", 15, 28);

        return 0;
}
