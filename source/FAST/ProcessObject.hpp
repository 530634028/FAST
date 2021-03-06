#ifndef PIPELINE_OBJECT_HPP
#define PIPELINE_OBJECT_HPP

#include "FAST/SmartPointers.hpp"
#include "FAST/Utility.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "FAST/Object.hpp"
#include "FAST/Data/DataObject.hpp"
#include "RuntimeMeasurement.hpp"
#include "RuntimeMeasurementManager.hpp"
#include "FAST/ExecutionDevice.hpp"
#include "FAST/DeviceManager.hpp"
#include "FAST/Config.hpp"
#include "FAST/Attribute.hpp"
#include "FAST/DataPort.hpp"

namespace fast {


class OpenCLProgram;
class ProcessObject;

class FAST_EXPORT  ProcessObject : public virtual Object {
    public:
        virtual ~ProcessObject();
        void update(uint64_t timestep, StreamingMode streamingMode = STREAMING_MODE_PROCESS_ALL_FRAMES);
        typedef SharedPointer<ProcessObject> pointer;

        // Runtime stuff
        RuntimeMeasurement::pointer getRuntime();
        RuntimeMeasurement::pointer getRuntime(std::string name);
        RuntimeMeasurementsManager::pointer getAllRuntimes();
        void enableRuntimeMeasurements();
        void disableRuntimeMeasurements();

        // Device stuff
        void setMainDevice(ExecutionDevice::pointer device);
        void setMainDeviceCriteria(const DeviceCriteria& citeria);
        ExecutionDevice::pointer getMainDevice() const;
        void setDevice(uint deviceNumber, ExecutionDevice::pointer device);
        void setDeviceCriteria(uint deviceNumber, const DeviceCriteria& criteria);
        ExecutionDevice::pointer getDevice(uint deviceNumber) const;

        DataPort::pointer getOutputPort(uint portID = 0);
        DataPort::pointer getInputPort(uint portID = 0);
        void setInputConnection(DataPort::pointer port);
        void setInputConnection(uint portID, DataPort::pointer port);
        void setInputData(DataObject::pointer data);
        void setInputData(uint portID, DataObject::pointer data);
        int getNrOfInputConnections() const;
        int getNrOfOutputPorts() const;

        virtual std::string getNameOfClass() const = 0;
        static std::string getStaticNameOfClass() {
            return "ProcessObject";
        }
        virtual void loadAttributes();
        std::shared_ptr<Attribute> getAttribute(std::string id);
        std::unordered_map<std::string, std::shared_ptr<Attribute>> getAttributes();
        void setAttributes(std::vector<std::shared_ptr<Attribute>> attributes);

        /**
         * Used to stop a pipeline.
         */
        void stopPipeline();

        void setModified(bool modified);
    protected:
        ProcessObject();
        // Flag to indicate whether the object has been modified
        // and should be executed again
        bool mIsModified;

        // Used to avoid duplicate executes for same timestep
        uint64_t mLastTimestepExecuted = std::numeric_limits<uint64_t>::max();

        // Pure virtual method for executing the pipeline object
        virtual void execute()=0;
        virtual void preExecute();
        virtual void postExecute();

        template <class DataType>
        void createInputPort(uint portID, bool required = true);
        template <class DataType>
        void createOutputPort(uint portID);

        template <class DataType>
        DataObject::pointer getInputData(uint portID = 0);
        template <class DataType>
        DataObject::pointer getOutputData(uint portID = 0);
        void addOutputData(uint portID, DataObject::pointer data);

        bool hasNewInputData(uint portID);

        virtual void waitToFinish() {};


        RuntimeMeasurementsManager::pointer mRuntimeManager;

        void createOpenCLProgram(std::string sourceFilename, std::string name = "");
        cl::Program getOpenCLProgram(
                SharedPointer<OpenCLDevice> device,
                std::string name = "",
                std::string buildOptions = ""
        );

        void createFloatAttribute(std::string id, std::string name, std::string description, float initialValue);
        void createIntegerAttribute(std::string id, std::string name, std::string description, int initialValue);
        void createBooleanAttribute(std::string id, std::string name, std::string description, bool initialValue);
        void createStringAttribute(std::string id, std::string name, std::string description, std::string initialValue);
        float getFloatAttribute(std::string id);
        int getIntegerAttribute(std::string id);
        bool getBooleanAttribute(std::string id);
        std::string getStringAttribute(std::string id);
        std::vector<float> getFloatListAttribute(std::string id);
        std::vector<int> getIntegerListAttribute(std::string id);
        std::vector<bool> getBooleanListAttribute(std::string id);
        std::vector<std::string> getStringListAttribute(std::string id);

        void changeDeviceOnInputs(uint deviceNumber, ExecutionDevice::pointer device);

        std::unordered_map<uint, bool> mRequiredInputs;
        std::unordered_map<uint, std::vector<uint> > mInputDevices;
        std::unordered_map<uint, ExecutionDevice::pointer> mDevices;
        std::unordered_map<uint, DeviceCriteria> mDeviceCriteria;

        // New pipeline
        std::unordered_map<uint, DataPort::pointer> mInputConnections;
        std::unordered_map<uint, std::vector<WeakPointer<DataPort>>> mOutputConnections;
        std::unordered_map<uint, bool> mInputPorts;
        std::unordered_set<uint> mOutputPorts;
        // <port id, timestep>, register the last timestep of data which this PO executed with
        std::unordered_map<uint, std::pair<DataObject::pointer, uint64_t>> mLastProcessed;

        void validateInputPortExists(uint portID);
        void validateOutputPortExists(uint portID);



        std::unordered_map<std::string, SharedPointer<OpenCLProgram> > mOpenCLPrograms;

        std::unordered_map<std::string, std::shared_ptr<Attribute>> mAttributes;

};


template<class DataType>
void ProcessObject::createInputPort(uint portID, bool required) {
        mInputPorts[portID] = required;
}


template<class DataType>
void ProcessObject::createOutputPort(uint portID) {
        mOutputPorts.insert(portID);
}

template<class DataType>
DataObject::pointer ProcessObject::getInputData(uint portID) {
        validateInputPortExists(portID);
        DataPort::pointer port = mInputConnections.at(portID);
        DataObject::pointer data = port->getNextFrame();
        mLastProcessed[portID] = std::make_pair(data, data->getTimestamp());
        return data;
}

template<class DataType>
DataObject::pointer ProcessObject::getOutputData(uint portID) {
        validateOutputPortExists(portID);
        // Generate a new output data object
        typename DataType::pointer returnData = DataType::New();

        addOutputData(portID, returnData);

        // Return it
        return returnData;
}




}; // end namespace fast


#endif
