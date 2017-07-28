#include "ComputationThread.hpp"
#include "SimpleWindow.hpp"
#include "View.hpp"
#include <QGLContext>

namespace fast {

uint64_t ComputationThread::mTimestep = 0;
StreamingMode ComputationThread::mStreamingMode = STREAMING_MODE_PROCESS_ALL_FRAMES;

ComputationThread::ComputationThread(QThread* mainThread) {
    mUpdateThreadIsStopped = false;
    mIsRunning = false;
    mMainThread = mainThread;
    mTimestep = 0;
}

void ComputationThread::addView(View* view) {
    mViews.push_back(view);
}

void ComputationThread::clearViews() {
    mViews.clear();
}

ComputationThread::~ComputationThread() {
    reportInfo() << "Computation thread object destroyed" << Reporter::end();
}

bool ComputationThread::isRunning() {
    return mIsRunning;
}

void ComputationThread::run() {
    // This is run in the secondary (computation thread)
    {
        std::unique_lock<std::mutex> lock(mUpdateThreadMutex); // this locks the mutex
        mIsRunning = true;
    }
    QGLContext* mainGLContext = Window::getMainGLContext();
    mainGLContext->makeCurrent();
    mTimestep = 0;

    while(true) {
        mTimestep++;
        //std::cout << "TIMESTEP: " << mTimestep << std::endl;
        // Update renderers' input before lock mutexes. This will ensure that renderering can happen while computing
        for(View* view : mViews) {
            view->updateRenderersInput(mTimestep, mStreamingMode);
        }
        // Lock mutex of all renderers before update renderers. This will ensure that rendering is synchronized.
        for(View* view : mViews) {
            //view->lockRenderers();
        }
        for(View* view : mViews) {
            view->updateRenderers(mTimestep, mStreamingMode);
        }
        for(View* view : mViews) {
            //view->unlockRenderers();
        }
        std::unique_lock<std::mutex> lock(mUpdateThreadMutex); // this locks the mutex
        if(mUpdateThreadIsStopped) {
            // Move GL context back to main thread
            mainGLContext->doneCurrent();
            mainGLContext->moveToThread(mMainThread);
            mIsRunning = false;
            break;
        }
    }


    emit finished();
    reportInfo() << "Computation thread has finished in run()" << Reporter::end();
    mUpdateThreadConditionVariable.notify_one();
}

void ComputationThread::stop() {
    for(View* view : mViews) {
        view->stopRenderers();
    }
    // This is run in the main thread
    std::unique_lock<std::mutex> lock(mUpdateThreadMutex); // this locks the mutex
    mUpdateThreadIsStopped = true;
    // Block until mIsRunning is set to false
    while(mIsRunning) {
        // Unlocks the mutex and wait until someone calls notify.
        // When it wakes, the mutex is locked again and mUpdateIsRunning is checked.
        mUpdateThreadConditionVariable.wait(lock);
    }
}


}
