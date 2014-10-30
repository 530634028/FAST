#ifndef EXECUTIONDEVICE_HPP_
#define EXECUTIONDEVICE_HPP_

#include "Object.hpp"
#include "SmartPointers.hpp"
#include "Context.hpp"

namespace fast {

class ExecutionDevice : public Object {
    public:
        typedef SharedPointer<ExecutionDevice> pointer;
        bool isHost() {return mIsHost;};
        virtual ~ExecutionDevice() {};
    protected:
        bool mIsHost;

};

class Host : public ExecutionDevice {
    FAST_OBJECT(Host)
    private:
        Host() {mIsHost = true;};
};

class OpenCLDevice : public ExecutionDevice, public oul::Context {
    FAST_OBJECT(OpenCLDevice)
    public:
        cl::CommandQueue getCommandQueue();
        cl::Device getDevice();

        OpenCLDevice(std::vector<cl::Device> devices, unsigned long * glContext = NULL) : oul::Context(devices, glContext, false)
        {mIsHost = false;mGLContext = glContext;};
        unsigned long * getGLContext() { return mGLContext; };
        std::string getName() {
            return getDevice().getInfo<CL_DEVICE_NAME>();
        }
    private:
        OpenCLDevice() {mIsHost = false;};
        unsigned long * mGLContext;

};

} // end namespace fast

#endif /* EXECUTIONDEVICE_HPP_ */