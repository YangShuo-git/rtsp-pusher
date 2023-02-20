#include "common_looper.h"
#include "log.h"

void* CommonLooper::trampoline(void *p)
{
    LogInfo("into");
    ((CommonLooper*)p)->SetRunning(true);
    ((CommonLooper*)p)->Loop();
     ((CommonLooper*)p)->SetRunning(false);
    LogInfo("leave");
    return nullptr;
}


CommonLooper::CommonLooper():request_abort_(false), running_(false)
{
    
}

CommonLooper::~CommonLooper()
{
    Stop();
}

RET_CODE CommonLooper::Start()
{
    LogInfo("into");
    worker_ = new std::thread(trampoline, this);
    if (!worker_)
    {
        LogError("Fail to new std::thread");
        return RET_FAIL;
    }
    
}

void CommonLooper::Stop()
{
    request_abort_ = true;
    if (worker_)
    {
        worker_->join();
        delete worker_;
        worker_ = nullptr;
    }
}

bool CommonLooper::Running()
{
    return running_;
}

void CommonLooper::SetRunning(bool running)
{
    running_ = running;
}
