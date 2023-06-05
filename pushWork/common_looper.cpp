#include "common_looper.h"
#include "../log/log.h"

void* CommonLooper::trampoline(void *p)
{
    LogInfo("into trampoline");
    ((CommonLooper*)p)->setRunning(true);
    ((CommonLooper*)p)->loop();
     ((CommonLooper*)p)->setRunning(false);
    LogInfo("leave trampoline");
    return nullptr;
}


CommonLooper::CommonLooper():request_abort_(false), running_(false)
{
    
}

CommonLooper::~CommonLooper()
{
    stopThread();
}

RET_CODE CommonLooper::startThread()
{
    LogInfo("into startThread");
    worker_ = new std::thread(trampoline, this);
    if (!worker_)
    {
        LogError("Fail to new thread");
        return RET_FAIL;
    }
    return RET_OK;
}

void CommonLooper::stopThread()
{
    request_abort_ = true;
    if (worker_)
    {
        worker_->join();
        delete worker_;
        worker_ = nullptr;
    }
}

bool CommonLooper::isRunning()
{
    return running_;
}

void CommonLooper::setRunning(bool running)
{
    running_ = running;
}
