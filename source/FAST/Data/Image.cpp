#include "Image.hpp"
#include "HelperFunctions.hpp"
#include "FAST/Exception.hpp"
#include "FAST/Utility.hpp"
#include "FAST/SceneGraph.hpp"

namespace fast {

bool Image::isDataModified() {
    if (!mHostDataIsUpToDate)
        return true;

    boost::unordered_map<OpenCLDevice::pointer, bool>::iterator it;
    for (it = mCLImagesIsUpToDate.begin(); it != mCLImagesIsUpToDate.end();
            it++) {
        if (it->second == false)
            return true;
    }

    for (it = mCLBuffersIsUpToDate.begin(); it != mCLBuffersIsUpToDate.end();
            it++) {
        if (it->second == false)
            return true;
    }

    return false;
}

bool Image::isAnyDataBeingAccessed() {
    if (mHostDataIsBeingAccessed)
        return true;

    boost::unordered_map<OpenCLDevice::pointer, bool>::iterator it;
    for (it = mCLImagesAccess.begin(); it != mCLImagesAccess.end(); it++) {
        if (it->second)
            return true;
    }

    for (it = mCLBuffersAccess.begin(); it != mCLBuffersAccess.end(); it++) {
        if (it->second)
            return true;
    }

    return false;
}

// Pad data with 1, 2 or 3 channels to 4 channels with 0
template <class T>
void * padData(T * data, unsigned int size, unsigned int nrOfComponents) {
    T * newData = new T[size*4]();
    for(unsigned int i = 0; i < size; i++) {
    	if(nrOfComponents == 1) {
            newData[i*4] = data[i];
    	} else if(nrOfComponents == 2) {
            newData[i*4] = data[i*2];
            newData[i*4+1] = data[i*2+1];
    	} else {
            newData[i*4] = data[i*3];
            newData[i*4+1] = data[i*3+1];
            newData[i*4+2] = data[i*3+2];
    	}
    }
    return (void *)newData;
}

void * adaptDataToImage(void * data, cl_channel_order order, unsigned int size, DataType type, unsigned int nrOfComponents) {
    // Because no OpenCL images support 3 channels,
    // the data has to be padded to 4 channels if the nr of components is 3
    // Also, not all CL platforms support CL_R and CL_RG images
    if(order == CL_RGBA && nrOfComponents != 4) {
        switch(type) {
            fastSwitchTypeMacro(return padData<FAST_TYPE>((FAST_TYPE*)data, size, nrOfComponents))
        }
    }

    return data;
}

// Remove padding from a data array created by padData
template <class T>
void * removePadding(T * data, unsigned int size, unsigned int nrOfComponents) {
     T * newData = new T[size*nrOfComponents];
    for(unsigned int i = 0; i < size; i++) {
    	if(nrOfComponents == 1) {
            newData[i] = data[i*4];
    	} else if(nrOfComponents == 2) {
            newData[i*2] = data[i*4];
            newData[i*2+1] = data[i*4+1];
    	} else {
            newData[i*3] = data[i*4];
            newData[i*3+1] = data[i*4+1];
            newData[i*3+2] = data[i*4+2];
    	}
    }
    return (void *)newData;
}

void * adaptImageDataToHostData(void * data, cl_channel_order order, unsigned int size, DataType type, unsigned int nrOfComponents) {
    // Because no OpenCL images support 3 channels,
    // the data has to be padded to 4 channels if the nr of components is 3.
    // Also, not all CL platforms support CL_R and CL_RG images
    // This function removes that padding
    if(order == CL_RGBA && nrOfComponents != 4) {
        switch(type) {
            fastSwitchTypeMacro(return removePadding<FAST_TYPE>((FAST_TYPE*)data, size, nrOfComponents))
        }
    }

    return data;
}



void Image::transferCLImageFromHost(OpenCLDevice::pointer device) {

    // Special treatment for images with 3 components because an OpenCL image can only have 1, 2 or 4 channels
	// And if the device does not support 1 or 2 channels
    cl::ImageFormat format = getOpenCLImageFormat(device, mDimensions == 2 ? CL_MEM_OBJECT_IMAGE2D : CL_MEM_OBJECT_IMAGE3D, mType, mComponents);
    if(format.image_channel_order == CL_RGBA && mComponents != 4) {
        void * tempData = adaptDataToImage(mHostData, CL_RGBA, mWidth*mHeight*mDepth, mType, mComponents);
        device->getCommandQueue().enqueueWriteImage(*(cl::Image*)mCLImages[device],
        CL_TRUE, oul::createOrigoRegion(), oul::createRegion(mWidth, mHeight, mDepth), 0,
                0, tempData);
        deleteArray(tempData, mType);
    } else {
        device->getCommandQueue().enqueueWriteImage(*(cl::Image*)mCLImages[device],
        CL_TRUE, oul::createOrigoRegion(), oul::createRegion(mWidth, mHeight, mDepth), 0,
                0, mHostData);
    }
}

void Image::transferCLImageToHost(OpenCLDevice::pointer device) {
    // Special treatment for images with 3 components because an OpenCL image can only have 1, 2 or 4 channels
	// And if the device does not support 1 or 2 channels
    cl::ImageFormat format = getOpenCLImageFormat(device, mDimensions == 2 ? CL_MEM_OBJECT_IMAGE2D : CL_MEM_OBJECT_IMAGE3D, mType, mComponents);
    if(format.image_channel_order == CL_RGBA && mComponents != 4) {
        void * tempData = allocateDataArray(mWidth*mHeight*mDepth,mType,4);
        device->getCommandQueue().enqueueReadImage(*(cl::Image*)mCLImages[device],
        CL_TRUE, oul::createOrigoRegion(), oul::createRegion(mWidth, mHeight, mDepth), 0,
                0, tempData);
        mHostData = adaptImageDataToHostData(tempData,CL_RGBA, mWidth*mHeight*mDepth,mType,mComponents);
        deleteArray(tempData, mType);
    } else {
        if(!mHostHasData) {
            // Must allocate memory for host data
            mHostData = allocateDataArray(mWidth*mHeight*mDepth,mType,mComponents);
			mHostHasData = true;
        }
        device->getCommandQueue().enqueueReadImage(*(cl::Image*)mCLImages[device],
        CL_TRUE, oul::createOrigoRegion(), oul::createRegion(mWidth, mHeight, mDepth), 0,
                0, mHostData);
    }
}

bool Image::hasAnyData() {
    return mHostHasData || mCLImages.size() > 0 || mCLBuffers.size() > 0;
}

void Image::updateOpenCLImageData(OpenCLDevice::pointer device) {

    // If data exist on device and is up to date do nothing
    if (mCLImagesIsUpToDate.count(device) > 0 && mCLImagesIsUpToDate[device]
            == true)
        return;

    bool updated = false;
    if (mCLImagesIsUpToDate.count(device) == 0) {
        // Data is not on device, create it
        cl::Image * newImage;
        if(mDimensions == 2) {
            newImage = new cl::Image2D(device->getContext(),
            CL_MEM_READ_WRITE, getOpenCLImageFormat(device, CL_MEM_OBJECT_IMAGE2D, mType,mComponents), mWidth, mHeight);
        } else {
            newImage = new cl::Image3D(device->getContext(),
            CL_MEM_READ_WRITE, getOpenCLImageFormat(device, CL_MEM_OBJECT_IMAGE3D, mType,mComponents), mWidth, mHeight, mDepth);
        }

        if(hasAnyData()) {
            mCLImagesIsUpToDate[device] = false;
        } else {
            mCLImagesIsUpToDate[device] = true;
            updated = true;
        }
        mCLImages[device] = newImage;
    }

    // Find which data is up to date
	if (!mCLImagesIsUpToDate[device]) {
		if (mHostDataIsUpToDate) {
			// Transfer host data to this device
			transferCLImageFromHost(device);
			updated = true;
		} else {
			boost::unordered_map<OpenCLDevice::pointer, bool>::iterator it;
			for (it = mCLImagesIsUpToDate.begin(); it != mCLImagesIsUpToDate.end();
				it++) {
				if (it->second == true) {
					// Transfer from this device(it->first) to device
					// TODO should use copy image to image here, if possible
					transferCLImageToHost(it->first);
					transferCLImageFromHost(device);
					mHostDataIsUpToDate = true;
					updated = true;
					break;
				}
			}
			for (it = mCLBuffersIsUpToDate.begin(); it != mCLBuffersIsUpToDate.end();
				it++) {
				if (it->second == true) {
					// Transfer from this device(it->first) to device
					// TODO should use copy buffer to image here, if possible
					transferCLBufferToHost(it->first);
					transferCLImageFromHost(device);
					mHostDataIsUpToDate = true;
					updated = true;
					break;
				}
			}
		}
	}

    if (!updated)
        throw Exception(
                "Data was not updated because no data was marked as up to date");
}

OpenCLBufferAccess::pointer Image::getOpenCLBufferAccess(
        accessType type,
        OpenCLDevice::pointer device) {

    if(!isInitialized())
        throw Exception("Image has not been initialized.");

    if(mImageIsBeingWrittenTo)
        throw Exception("Requesting access to an image that is already being written to.");
    if (type == ACCESS_READ_WRITE) {
        if (isAnyDataBeingAccessed()) {
            throw Exception(
                    "Trying to get write access to an object that is already being accessed");
        }
        mImageIsBeingWrittenTo = true;
    }
    updateOpenCLBufferData(device);
    if (type == ACCESS_READ_WRITE) {
        setAllDataToOutOfDate();
        updateModifiedTimestamp();
    }
    mCLBuffersAccess[device] = true;
    mCLBuffersIsUpToDate[device] = true;

    // Now it is guaranteed that the data is on the device and that it is up to date
	OpenCLBufferAccess::pointer accessObject(new OpenCLBufferAccess(mCLBuffers[device], &mCLBuffersAccess[device], &mImageIsBeingWrittenTo));
	return std::move(accessObject);
}

unsigned int Image::getBufferSize() const {
    unsigned int bufferSize = mWidth*mHeight;
    if(mDimensions == 3) {
        bufferSize *= mDepth;
    }
    bufferSize *= getSizeOfDataType(mType,mComponents);

    return bufferSize;
}

void Image::updateOpenCLBufferData(OpenCLDevice::pointer device) {

    // If data exist on device and is up to date do nothing
    if (mCLBuffersIsUpToDate.count(device) > 0 && mCLBuffersIsUpToDate[device]
            == true)
        return;

    bool updated = false;
    if (mCLBuffers.count(device) == 0) {
        // Data is not on device, create it
        unsigned int bufferSize = getBufferSize();
        cl::Buffer * newBuffer = new cl::Buffer(device->getContext(),
        CL_MEM_READ_WRITE, bufferSize);

        if(hasAnyData()) {
            mCLBuffersIsUpToDate[device] = false;
        } else {
            mCLBuffersIsUpToDate[device] = true;
            updated = true;
        }
        mCLBuffers[device] = newBuffer;
    }


    // Find which data is up to date
    if(!mCLBuffersIsUpToDate[device]) {
        if (mHostDataIsUpToDate) {
            // Transfer host data to this device
            transferCLBufferFromHost(device);
            updated = true;
        } else {
            boost::unordered_map<OpenCLDevice::pointer, bool>::iterator it;
            for (it = mCLImagesIsUpToDate.begin(); it != mCLImagesIsUpToDate.end();
                    it++) {
                if (it->second == true) {
                    // Transfer from this device(it->first) to device
                    transferCLImageToHost(it->first);
                    transferCLBufferFromHost(device);
                    mHostDataIsUpToDate = true;
                    updated = true;
                    break;
                }
            }
            for (it = mCLBuffersIsUpToDate.begin(); it != mCLBuffersIsUpToDate.end();
                    it++) {
                if (it->second == true) {
                    // Transfer from this device(it->first) to device
                    transferCLBufferToHost(it->first);
                    transferCLBufferFromHost(device);
                    mHostDataIsUpToDate = true;
                    updated = true;
                    break;
                }
            }
        }
    }

    if (!updated)
        throw Exception(
                "Data was not updated because no data was marked as up to date");

    mCLBuffersIsUpToDate[device] = true;
}

void Image::transferCLBufferFromHost(OpenCLDevice::pointer device) {
    unsigned int bufferSize = getBufferSize();
    device->getCommandQueue().enqueueWriteBuffer(*mCLBuffers[device],
        CL_TRUE, 0, bufferSize, mHostData);
}

void Image::transferCLBufferToHost(OpenCLDevice::pointer device) {
	if (!mHostHasData) {
		// Must allocate memory for host data
		mHostData = allocateDataArray(mWidth*mHeight*mDepth, mType, mComponents);
		mHostHasData = true;
	}
    unsigned int bufferSize = getBufferSize();
    device->getCommandQueue().enqueueReadBuffer(*mCLBuffers[device],
        CL_TRUE, 0, bufferSize, mHostData);
}

void Image::updateHostData() {
    // It is the host data that has been modified, no need to update
    if (mHostDataIsUpToDate)
        return;

    bool updated = false;
    if (!mHostHasData) {
        // Data is not initialized, do that first
        unsigned int size = mWidth*mHeight*mComponents;
        if(mDimensions == 3)
            size *= mDepth;
        mHostData = allocateDataArray(mWidth*mHeight*mDepth,mType,mComponents);
        if(hasAnyData()) {
            mHostDataIsUpToDate = false;
        } else {
            mHostDataIsUpToDate = true;
            updated = true;
        }
        mHostHasData = true;
    }

    // Find which data is up to date
    boost::unordered_map<OpenCLDevice::pointer, bool>::iterator it;
    for (it = mCLImagesIsUpToDate.begin(); it != mCLImagesIsUpToDate.end();
            it++) {
        if (it->second == true) {
            // transfer from this device to host
            transferCLImageToHost(it->first);
            updated = true;
            break;
        }
    }
    for (it = mCLBuffersIsUpToDate.begin(); it != mCLBuffersIsUpToDate.end();
            it++) {
        if (it->second == true) {
            // transfer from this device to host
            transferCLBufferToHost(it->first);
            updated = true;
            break;
        }
    }
    if (!updated)
        throw Exception(
                "Data was not updated because no data was marked as up to date");
}

void Image::setAllDataToOutOfDate() {
    mHostDataIsUpToDate = false;
    boost::unordered_map<OpenCLDevice::pointer, bool>::iterator it;
    for (it = mCLImagesIsUpToDate.begin(); it != mCLImagesIsUpToDate.end();
            it++) {
        it->second = false;
    }
    for (it = mCLBuffersIsUpToDate.begin(); it != mCLBuffersIsUpToDate.end();
            it++) {
        it->second = false;
    }
}

OpenCLImageAccess::pointer Image::getOpenCLImageAccess(
        accessType type,
        OpenCLDevice::pointer device) {

    if(!isInitialized())
        throw Exception("Image has not been initialized.");
    if(mImageIsBeingWrittenTo)
        throw Exception("Requesting access to an image that is already being written to.");

    // Check for write access
    if (type == ACCESS_READ_WRITE) {
        if (isAnyDataBeingAccessed()) {
            throw Exception(
                    "Trying to get write access to an object that is already being accessed");
        }
        mImageIsBeingWrittenTo = true;
    }
    updateOpenCLImageData(device);
    if (type == ACCESS_READ_WRITE) {
        setAllDataToOutOfDate();
        updateModifiedTimestamp();
    }
    mCLImagesAccess[device] = true;
    mCLImagesIsUpToDate[device] = true;

    // Now it is guaranteed that the data is on the device and that it is up to date
    if(mDimensions == 2) {
        OpenCLImageAccess::pointer accessObject(new OpenCLImageAccess((cl::Image2D*)mCLImages[device], &mCLImagesAccess[device], &mImageIsBeingWrittenTo));
        return accessObject;
    } else {
        OpenCLImageAccess::pointer accessObject(new OpenCLImageAccess((cl::Image3D*)mCLImages[device], &mCLImagesAccess[device], &mImageIsBeingWrittenTo));
        return accessObject;
    }
}

Image::Image() {
    mHostData = NULL;
    mHostHasData = false;
    mHostDataIsUpToDate = false;
    mHostDataIsBeingAccessed = false;
    mIsDynamicData = false;
    mImageIsBeingWrittenTo = false;
    mSpacing = Vector3f(1,1,1);
    mMaxMinInitialized = false;
    mAverageInitialized = false;
    mIsInitialized = false;
}

ImageAccess::pointer Image::getImageAccess(accessType type) {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");
    if(mImageIsBeingWrittenTo)
        throw Exception("Requesting access to an image that is already being written to.");

    if (type == ACCESS_READ_WRITE) {
        if (isAnyDataBeingAccessed()) {
            throw Exception(
                    "Trying to get write access to an object that is already being accessed");
        }
        mImageIsBeingWrittenTo = true;
    }
    updateHostData();
    if(type == ACCESS_READ_WRITE) {
        // Set modified to true since it wants write access
        setAllDataToOutOfDate();
        updateModifiedTimestamp();
    }

    mHostDataIsUpToDate = true;
    mHostDataIsBeingAccessed = true;

	Image::pointer image = mPtr.lock();
	ImageAccess::pointer accessObject(new ImageAccess(mHostData, image, &mHostDataIsBeingAccessed, &mImageIsBeingWrittenTo));
	return std::move(accessObject);
}

void Image::create(
        VectorXui size,
        DataType type,
        unsigned int nrOfComponents) {

    if(size.rows() > 2 && size.z() > 1) {
        // 3D
        create(size.x(), size.y(), size.z(), type, nrOfComponents);
    } else {
        // 2D
        create(size.x(), size.y(), type, nrOfComponents);
    }
}

void Image::create(
        VectorXui size,
        DataType type,
        unsigned int nrOfComponents,
        ExecutionDevice::pointer device,
        const void* data) {

    if(size.rows() > 2 && size.z() > 1) {
        // 3D
        create(size.x(), size.y(), size.z(), type, nrOfComponents, device, data);
    } else {
        // 2D
        create(size.x(), size.y(), type, nrOfComponents, device, data);
    }
}

void Image::create(
        unsigned int width,
        unsigned int height,
        unsigned int depth,
        DataType type,
        unsigned int nrOfComponents) {

    getSceneGraphNode()->reset(); // reset scene graph node
    freeAll(); // delete any old data

    mWidth = width;
    mHeight = height;
    mDepth = depth;
    mBoundingBox = BoundingBox(Vector3f(width, height, depth));
    mDimensions = 3;
    mType = type;
    mComponents = nrOfComponents;
    updateModifiedTimestamp();
    mIsInitialized = true;
}

void Image::create(
        unsigned int width,
        unsigned int height,
        unsigned int depth,
        DataType type,
        unsigned int nrOfComponents,
        ExecutionDevice::pointer device,
        const void* data) {

    getSceneGraphNode()->reset(); // reset scene graph node
    freeAll(); // delete any old data

    mWidth = width;
    mHeight = height;
    mDepth = depth;
    mBoundingBox = BoundingBox(Vector3f(width, height, depth));
    mDimensions = 3;
    mType = type;
    mComponents = nrOfComponents;
    if(device->isHost()) {
        mHostData = allocateDataArray(width*height*depth, type, nrOfComponents);
        memcpy(mHostData, data, getSizeOfDataType(type, nrOfComponents)*width*height*depth);
        mHostHasData = true;
        mHostDataIsUpToDate = true;
    } else {
        OpenCLDevice::pointer clDevice = device;
        cl::Image3D* clImage;
        void * tempData = adaptDataToImage((void *)data, getOpenCLImageFormat(clDevice, CL_MEM_OBJECT_IMAGE3D, type, nrOfComponents).image_channel_order, width*height*depth, type, nrOfComponents);
        clImage = new cl::Image3D(
            clDevice->getContext(),
            CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
            getOpenCLImageFormat(clDevice, CL_MEM_OBJECT_IMAGE3D, type, nrOfComponents),
            width, height, depth,
            0, 0,
            tempData
        );
        if(data != tempData) {
            deleteArray(tempData, type);
        }
        mCLImages[clDevice] = clImage;
        mCLImagesIsUpToDate[clDevice] = true;
        mCLImagesAccess[clDevice] = false;
    }
    updateModifiedTimestamp();
    mIsInitialized = true;
}

void Image::create(
        unsigned int width,
        unsigned int height,
        DataType type,
        unsigned int nrOfComponents) {

    getSceneGraphNode()->reset(); // reset scene graph node
    freeAll(); // delete any old data

    mWidth = width;
    mHeight = height;
    mBoundingBox = BoundingBox(Vector3f(width, height, 0));
    mDepth = 1;
    mDimensions = 2;
    mType = type;
    mComponents = nrOfComponents;
    updateModifiedTimestamp();
    mIsInitialized = true;
}




void Image::create(
        unsigned int width,
        unsigned int height,
        DataType type,
        unsigned int nrOfComponents,
        ExecutionDevice::pointer device,
        const void* data) {

    getSceneGraphNode()->reset(); // reset scene graph node
    freeAll(); // delete any old data

    mWidth = width;
    mHeight = height;
    mDepth = 1;
    mBoundingBox = BoundingBox(Vector3f(width, height, 0));
    mDimensions = 2;
    mType = type;
    mComponents = nrOfComponents;
    if(device->isHost()) {
        mHostData = allocateDataArray(width*height, type, nrOfComponents);
        memcpy(mHostData, data, getSizeOfDataType(type, nrOfComponents) * width * height);
        mHostHasData = true;
        mHostDataIsUpToDate = true;
    } else {
        OpenCLDevice::pointer clDevice = device;
        cl::Image2D* clImage;
        void * tempData = adaptDataToImage((void *)data, getOpenCLImageFormat(clDevice, CL_MEM_OBJECT_IMAGE2D, type, nrOfComponents).image_channel_order, width*height, type, nrOfComponents);
        clImage = new cl::Image2D(
            clDevice->getContext(),
            CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
            getOpenCLImageFormat(clDevice, CL_MEM_OBJECT_IMAGE2D, type, nrOfComponents),
            width, height,
            0,
            tempData
        );
        if(data != tempData) {
            deleteArray(tempData, type);
        }
        mCLImages[clDevice] = clImage;
        mCLImagesIsUpToDate[clDevice] = true;
        mCLImagesAccess[clDevice] = false;
    }
    updateModifiedTimestamp();
    mIsInitialized = true;
}

bool Image::isInitialized() const {
    return mIsInitialized;
}

void Image::free(ExecutionDevice::pointer device) {
    // Delete data on a specific device
    if(device->isHost()) {
        deleteArray(mHostData, mType);
        mHostHasData = false;
    } else {
        OpenCLDevice::pointer clDevice = device;
        // Delete any OpenCL images
        delete mCLImages[clDevice];
        mCLImages.erase(clDevice);
        mCLImagesIsUpToDate.erase(clDevice);
        mCLImagesAccess.erase(clDevice);
        // Delete any OpenCL buffers
        delete mCLBuffers[clDevice];
        mCLBuffers.erase(clDevice);
        mCLBuffersIsUpToDate.erase(clDevice);
        mCLBuffersAccess.erase(clDevice);
    }
}

void Image::freeAll() {
    // Delete OpenCL Images
    boost::unordered_map<OpenCLDevice::pointer, cl::Image*>::iterator it;
    for (it = mCLImages.begin(); it != mCLImages.end(); it++) {
        delete it->second;
    }
    mCLImages.clear();
    mCLImagesIsUpToDate.clear();
    mCLImagesAccess.clear();

    // Delete OpenCL buffers
    boost::unordered_map<OpenCLDevice::pointer, cl::Buffer*>::iterator it2;
    for (it2 = mCLBuffers.begin(); it2 != mCLBuffers.end(); it2++) {
        delete it2->second;
    }
    mCLBuffers.clear();
    mCLBuffersIsUpToDate.clear();
    mCLBuffersAccess.clear();

    // Delete host data
    if(mHostHasData) {
        this->free(Host::getInstance());
    }
}

unsigned int Image::getWidth() const {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");
    return mWidth;
}

unsigned int Image::getHeight() const {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");
    return mHeight;
}

unsigned int Image::getDepth() const {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");
    return mDepth;
}

Vector3ui Image::getSize() const {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");
    return Vector3ui(mWidth, mHeight, mDepth);
}

unsigned char Image::getDimensions() const {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");
    return mDimensions;
}

DataType Image::getDataType() const {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");
    return mType;
}

unsigned int Image::getNrOfComponents() const {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");
    return mComponents;
}

Vector3f fast::Image::getSpacing() const {
    return mSpacing;
}

void fast::Image::setSpacing(Vector3f spacing) {
    mSpacing = spacing;
}

/*
void Image::updateSceneGraphTransformation() const {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");

    // Create linear transformation matrix
    AffineTransformation transformation;
    transformation.linear() = mTransformMatrix;
    transformation.translation() = mOffset;
    transformation.scale(mSpacing);

    SceneGraphNode::pointer node = getSceneGraphNode();
    node->setTransformation(transformation);
}
*/


void Image::calculateMaxAndMinIntensity() {
    // Calculate max and min if image has changed or it is the first time
    if(!mMaxMinInitialized || mMaxMinTimestamp != getTimestamp()) {

        unsigned int nrOfElements = mWidth*mHeight*mDepth*mComponents;
        if(mHostHasData && mHostDataIsUpToDate) {
            // Host data is up to date, calculate min and max on host
            ImageAccess::pointer access = getImageAccess(ACCESS_READ);
            void* data = access->get();
            switch(mType) {
            case TYPE_FLOAT:
            case TYPE_SNORM_INT16:
            case TYPE_UNORM_INT16:
                getMaxAndMinFromData<float>(data,nrOfElements,&mMinimumIntensity,&mMaximumIntensity);
                break;
            case TYPE_INT8:
                getMaxAndMinFromData<char>(data,nrOfElements,&mMinimumIntensity,&mMaximumIntensity);
                break;
            case TYPE_UINT8:
                getMaxAndMinFromData<uchar>(data,nrOfElements,&mMinimumIntensity,&mMaximumIntensity);
                break;
            case TYPE_INT16:
                getMaxAndMinFromData<short>(data,nrOfElements,&mMinimumIntensity,&mMaximumIntensity);
                break;
            case TYPE_UINT16:
                getMaxAndMinFromData<ushort>(data,nrOfElements,&mMinimumIntensity,&mMaximumIntensity);
                break;
            }
        } else {
            // TODO the logic here can be improved. For instance choose the best device
            // Find some OpenCL image data or buffer data that is up to date
            bool found = false;
            boost::unordered_map<OpenCLDevice::pointer, bool>::iterator it;
            for (it = mCLImagesIsUpToDate.begin(); it != mCLImagesIsUpToDate.end(); it++) {
                if(it->second == true) {
                    OpenCLDevice::pointer device = it->first;
                    if(mDimensions == 2) {
                        OpenCLImageAccess::pointer access = getOpenCLImageAccess(ACCESS_READ, device);
                        cl::Image2D* clImage = access->get2DImage();
                        getMaxAndMinFromOpenCLImage(device, *clImage, mType, &mMinimumIntensity, &mMaximumIntensity);
                    } else {
                        if(device->getDevice().getInfo<CL_DEVICE_EXTENSIONS>().find("cl_khr_3d_image_writes") == std::string::npos) {
                            // Writing to 3D images is not supported on this device
                            // Copy data to buffer instead and do the max min calculation on the buffer instead
                            OpenCLBufferAccess::pointer access = getOpenCLBufferAccess(ACCESS_READ, device);
                            cl::Buffer* buffer = access->get();
                            getMaxAndMinFromOpenCLBuffer(device, *buffer, nrOfElements, mType, &mMinimumIntensity, &mMaximumIntensity);
                        } else {
                            OpenCLImageAccess::pointer access = getOpenCLImageAccess(ACCESS_READ, device);
                            cl::Image3D* clImage = access->get3DImage();
                            getMaxAndMinFromOpenCLImage(device, *clImage, mType, &mMinimumIntensity, &mMaximumIntensity);
                        }
                    }
                    found = true;
                }
            }

            if(!found) {
                for (it = mCLBuffersIsUpToDate.begin(); it != mCLBuffersIsUpToDate.end(); it++) {
                    if(it->second == true) {
                        OpenCLDevice::pointer device = it->first;
                        OpenCLBufferAccess::pointer access = getOpenCLBufferAccess(ACCESS_READ, device);
                        cl::Buffer* buffer = access->get();
                        getMaxAndMinFromOpenCLBuffer(device, *buffer, nrOfElements, mType, &mMinimumIntensity, &mMaximumIntensity);
                        found = true;
                    }
                }
            }
        }

        // Update timestamp
        mMaxMinTimestamp = getTimestamp();
        mMaxMinInitialized = true;
    }
}

float Image::calculateAverageIntensity() {
     if(!isInitialized())
        throw Exception("Image has not been initialized.");

    // Calculate max and min if image has changed or it is the first time
    if(!mAverageInitialized || mAverageIntensityTimestamp != getTimestamp()) {
        unsigned int nrOfElements = mWidth*mHeight*mDepth;
        if(mHostHasData && mHostDataIsUpToDate) {
            Report::info() << "calculating sum on host" << Report::end;
            // Host data is up to date, calculate min and max on host
            ImageAccess::pointer access = getImageAccess(ACCESS_READ);
            void* data = access->get();
            switch(mType) {
            case TYPE_FLOAT:
                mAverageIntensity = getSumFromData<float>(data,nrOfElements) / nrOfElements;
                break;
            case TYPE_INT8:
                mAverageIntensity = getSumFromData<char>(data,nrOfElements) / nrOfElements;
                break;
            case TYPE_UINT8:
                mAverageIntensity = getSumFromData<uchar>(data,nrOfElements) / nrOfElements;
                break;
            case TYPE_INT16:
                mAverageIntensity = getSumFromData<short>(data,nrOfElements) / nrOfElements;
                break;
            case TYPE_UINT16:
                mAverageIntensity = getSumFromData<ushort>(data,nrOfElements) / nrOfElements;
                break;
            }
        } else {
            Report::info() << "calculating sum with OpenCL" << Report::end;
            // TODO the logic here can be improved. For instance choose the best device
            // Find some OpenCL image data or buffer data that is up to date
            bool found = false;
            boost::unordered_map<OpenCLDevice::pointer, bool>::iterator it;
            for (it = mCLImagesIsUpToDate.begin(); it != mCLImagesIsUpToDate.end(); it++) {
                if(it->second == true) {
                    OpenCLDevice::pointer device = it->first;
                    float sum;
                    if(mDimensions == 2) {
                        OpenCLImageAccess::pointer access = getOpenCLImageAccess(ACCESS_READ, device);
                        cl::Image2D* clImage = access->get2DImage();
                        getIntensitySumFromOpenCLImage(device, *clImage, mType, &sum);
                    } else {
                        if(!device->isWritingTo3DTexturesSupported()) {
                            // Writing to 3D images is not supported on this device
                            // Copy data to buffer instead and do the max min calculation on the buffer instead
                            OpenCLBufferAccess::pointer access = getOpenCLBufferAccess(ACCESS_READ, device);
                            cl::Buffer* buffer = access->get();
                            // TODO
                            throw Exception("Not implemented yet");
                            //getMaxAndMinFromOpenCLBuffer(device, *buffer, nrOfElements, mType, &mMinimumIntensity, &mMaximumIntensity);
                        } else {
                            OpenCLImageAccess::pointer access = getOpenCLImageAccess(ACCESS_READ, device);
                            cl::Image3D* clImage = access->get3DImage();
                            // TODO
                            throw Exception("Not implemented yet");
                            //getIntensitySumFromOpenCLImage(device, *clImage, mType, &sum);
                        }
                    }
                    mAverageIntensity = sum / nrOfElements;
                    found = true;
                }
            }

            if(!found) {
                for (it = mCLBuffersIsUpToDate.begin(); it != mCLBuffersIsUpToDate.end(); it++) {
                    if(it->second == true) {
                        OpenCLDevice::pointer device = it->first;
                        OpenCLBufferAccess::pointer access = getOpenCLBufferAccess(ACCESS_READ, device);
                        cl::Buffer* buffer = access->get();
                        // TODO
                            throw Exception("Not implemented yet");
                        //getMaxAndMinFromOpenCLBuffer(device, *buffer, nrOfElements, mType, &mMinimumIntensity, &mMaximumIntensity);
                        found = true;
                    }
                }
            }
        }

        // Update timestamp
        mAverageIntensityTimestamp = getTimestamp();
        mAverageInitialized = true;
    }

    return mAverageIntensity;
}

float Image::calculateMaximumIntensity() {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");
    calculateMaxAndMinIntensity();

    return mMaximumIntensity;
}

float Image::calculateMinimumIntensity() {
    if(!isInitialized())
        throw Exception("Image has not been initialized.");
    calculateMaxAndMinIntensity();

    return mMinimumIntensity;
}

void Image::createFromImage(
        Image::pointer image) {
    // Create image first
    create(image->getSize(), image->getDataType(), image->getNrOfComponents());

    // Copy metadata
    setSpacing(image->getSpacing());
    updateModifiedTimestamp();
}


Image::pointer Image::copy(ExecutionDevice::pointer device) {
    Image::pointer clone = Image::New();
    clone->createFromImage(mPtr.lock());

    // If device is host, get data from this image to host
    if(device->isHost()) {
        ImageAccess::pointer readAccess = this->getImageAccess(ACCESS_READ);
        ImageAccess::pointer writeAccess = clone->getImageAccess(ACCESS_READ_WRITE);

        void* input = readAccess->get();
        void* output = writeAccess->get();
        switch(getDataType()) {
            fastSwitchTypeMacro(memcpy(output, input, sizeof(FAST_TYPE)*getWidth()*getHeight()*getDepth()*getNrOfComponents()));
        }
    } else {
        // If device is not host
        OpenCLDevice::pointer clDevice = device;
        if(getDimensions() == 2) {
            OpenCLImageAccess::pointer readAccess = this->getOpenCLImageAccess(ACCESS_READ, clDevice);
            OpenCLImageAccess::pointer writeAccess = clone->getOpenCLImageAccess(ACCESS_READ_WRITE, clDevice);
            cl::Image2D* input = readAccess->get2DImage();
            cl::Image2D* output = writeAccess->get2DImage();

            clDevice->getCommandQueue().enqueueCopyImage(
                    *input,
                    *output,
                    oul::createOrigoRegion(),
                    oul::createOrigoRegion(),
                    oul::createRegion(getWidth(), getHeight(), 1)
            );
        } else {
            OpenCLImageAccess::pointer readAccess = this->getOpenCLImageAccess(ACCESS_READ, clDevice);
            OpenCLImageAccess::pointer writeAccess = clone->getOpenCLImageAccess(ACCESS_READ_WRITE, clDevice);
            cl::Image3D* input = readAccess->get3DImage();
            cl::Image3D* output = writeAccess->get3DImage();

            clDevice->getCommandQueue().enqueueCopyImage(
                    *input,
                    *output,
                    oul::createOrigoRegion(),
                    oul::createOrigoRegion(),
                    oul::createRegion(getWidth(), getHeight(), getDepth())
            );
        }
    }

    return clone;
}

BoundingBox Image::getTransformedBoundingBox() const {
    AffineTransformation T = SceneGraph::getAffineTransformationFromData(DataObject::pointer(mPtr.lock()));

    // Add image spacing
    T.scale(getSpacing());

    return getBoundingBox().getTransformedBoundingBox(T);
}

} // end namespace fast;


