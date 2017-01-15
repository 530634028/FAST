# Download and set up Tensorflow

include(cmake/Externals.cmake)

if(WIN32)
    set(GIT_EXECUTABLE "git.exe")
else(WIN32)
    set(GIT_EXECUTABLE "git")
endif()

ExternalProject_Add(tensorflow
        PREFIX ${FAST_EXTERNAL_BUILD_DIR}/tensorflow
        BINARY_DIR ${FAST_EXTERNAL_BUILD_DIR}/tensorflow
        GIT_REPOSITORY "https://github.com/smistad/tensorflow.git"
        GIT_TAG "master"
        # Need to override this because tensorflow creates a file in the source dir
        # and cmake files to stash these files
        UPDATE_COMMAND
            ${GIT_EXECUTABLE} pull origin master
        # Must use CONFIGURE_COMMAND because CMakeLists.txt is not in the src root of tensorflow
        CONFIGURE_COMMAND
            ${CMAKE_COMMAND}
            ${FAST_EXTERNAL_BUILD_DIR}/tensorflow/src/tensorflow/tensorflow/contrib/cmake/
            -G${CMAKE_GENERATOR}
            -DCMAKE_BUILD_TYPE:STRING=Release
            -Dtensorflow_BUILD_PYTHON_BINDINGS=OFF
            -Dtensorflow_BUILD_CC_EXAMPLE=OFF
            #-Dtensorflow_ENABLE_GPU=ON     # Will only work on NVIDIA for windows atm
            -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF
            -DCMAKE_INSTALL_MESSAGE:BOOL=LAZY
            -DCMAKE_INSTALL_PREFIX:STRING=${FAST_EXTERNAL_INSTALL_DIR}

)

if(WIN32)
    # Tensorflow is built static on windows, need to include all dependecies
    list(APPEND LIBRARIES
        tensorflow.lib
        libprotobuf.lib
        libjpeg.lib
        libpng12_static.lib
        giflib.lib
        jsoncpp.lib
        farmhash.lib
        highwayhash.lib
	gpr.lib
	grpc_unsecure.lib
	grpc++_unsecure.lib
    )
    set(Tensorflow_LIBRARY ${FAST_EXTERNAL_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}tensorflow${CMAKE_STATIC_LIBRARY_SUFFIX})
else(WIN32)
    list(APPEND LIBRARIES
        ${CMAKE_SHARED_LIBRARY_PREFIX}tensorflow${CMAKE_SHARED_LIBRARY_SUFFIX}
    )
    set(Tensorflow_LIBRARY ${FAST_EXTERNAL_INSTALL_DIR}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}tensorflow${CMAKE_SHARED_LIBRARY_SUFFIX})
endif(WIN32)
list(APPEND FAST_EXTERNAL_DEPENDENCIES tensorflow)
