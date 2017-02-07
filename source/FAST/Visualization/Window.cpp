#include <GL/glew.h>
#include "Window.hpp"
#include <QApplication>
#include <QOffscreenSurface>
#include <QEventLoop>

namespace fast {

QOpenGLContext* Window::mMainGLContext = NULL;
QOffscreenSurface* Window::mSurface = NULL;
QApplication* Window::mQApp = NULL;

class FASTApplication : public QApplication {
public:
    FASTApplication(int &argc, char **argv) : QApplication(argc, argv) {
    }

    virtual ~FASTApplication() {
        Reporter::info() << "FASTApplication (QApplication) is destroyed" << Reporter::end;
    }

    // reimplemented from QApplication so we can throw exceptions in slots
    virtual bool notify(QObject *receiver, QEvent *event) {
        try {
            return QApplication::notify(receiver, event);
        } catch(Exception &e) {
            Reporter::error() << "FAST exception caught in Qt event handler " << e.what() << Reporter::end;
        } catch(cl::Error &e) {
            Reporter::error() << "OpenCL exception caught in Qt event handler " << e.what() << "(" << getCLErrorString(e.err()) << ")" << Reporter::end;
        } catch(std::exception &e) {
            Reporter::error() << "Std exception caught in Qt event handler " << e.what() << Reporter::end;
        }
        return false;
    }
};

Window::Window() {
    mThread = NULL;
    mTimeout = 0;
    initializeQtApp();
	mEventLoop = NULL;
    mWidget = new WindowWidget;
    mEventLoop = new QEventLoop(mWidget);
    mWidget->connect(mWidget, SIGNAL(widgetHasClosed()), this, SLOT(stop()));
    mWidget->setWindowTitle("FAST");
    mWidget->setContentsMargins(0, 0, 0, 0);

    // default window size
    mWidth = 512;
    mHeight = 512;
    mFullscreen = false;
}

void Window::enableFullscreen() {
    mFullscreen = true;
}

void Window::disableFullscreen() {
    mFullscreen = false;
}

void Window::cleanup() {
    delete mQApp;
}

void Window::initializeQtApp() {
    // Make sure only one QApplication is created
    if(!QApplication::instance()) {
        Reporter::info() << "Creating new QApp" << Reporter::end;
        // Create some dummy argc and argv options as QApplication requires it
        int* argc = new int[1];
        *argc = 0;
        QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
        mQApp = new FASTApplication(*argc,NULL);
         // Create computation GL context, if it doesn't exist
        if(mMainGLContext == NULL) {
            Reporter::info() << "Creating new GL context for computation thread" << Reporter::end;

            // Create GL context to be shared with the CL contexts
            mSurface = new QOffscreenSurface();
            mSurface->setFormat(QSurfaceFormat::defaultFormat());
            mSurface->create();
            mMainGLContext = new QOpenGLContext;
            mMainGLContext->setShareContext(QOpenGLContext::globalShareContext());
            mMainGLContext->setFormat(mSurface->format());
            mMainGLContext->create();
            if(!mMainGLContext->isValid()) {
                throw Exception("Qt GL context is invalid!");
            }
            mMainGLContext->makeCurrent(mSurface);
            GLenum err = glewInit();
            if(err != GLEW_OK)
                throw Exception("GLEW init error");
            mMainGLContext->doneCurrent();
            //atexit(cleanup);
        }
    } else {
        Reporter::info() << "QApp already exists.." << Reporter::end;
    }

    // There is a bug in AMD OpenCL related to comma (,) as decimal point
    // This will change decimal point to dot (.)
    struct lconv * lc;
    lc = localeconv();
    if(strcmp(lc->decimal_point, ",") == 0) {
        Reporter::warning() << "Your system uses comma as decimal point." << Reporter::end;
        Reporter::warning() << "This will now be changed to dot to avoid any comma related bugs." << Reporter::end;
        setlocale(LC_NUMERIC, "C");
        // Check again to be sure
        lc = localeconv();
        if(strcmp(lc->decimal_point, ",") == 0) {
            throw Exception("Failed to convert decimal point to dot.");
        }
    }
}


void Window::stop() {
    reportInfo() << "Stop signal recieved.." << Reporter::end;
    stopComputationThread();
    if(mEventLoop != NULL)
        mEventLoop->quit();
    /*
    // Make sure event is not quit twice
    if(!mWidget->getView()->hasQuit()) {
        // This event occurs when window is closed or timeout is reached
        mWidget->getView()->quit();
    }
    */
}

View* Window::createView() {
    View* view = new View();
    mWidget->addView(view);

    return view;
}

void Window::start() {
    mWidget->resize(mWidth,mHeight);

    if(mFullscreen) {
        mWidget->showFullScreen();
    } else {
        mWidget->show();
    }

    if(mTimeout > 0) {
        QTimer* timer = new QTimer(mWidget);
        timer->start(mTimeout);
        timer->setSingleShot(true);
        mWidget->connect(timer,SIGNAL(timeout()),mWidget,SLOT(close()));
    }

    startComputationThread();

    mEventLoop->exec(); // This call blocks and starts rendering
}

Window::~Window() {
    // Cleanup
    reportInfo() << "Destroying window.." << Reporter::end;
    // Event loop is child of widget
    //reportInfo() << "Deleting event loop" << Reporter::end;
    //if(mEventLoop != NULL)
    //    delete mEventLoop;
    reportInfo() << "Deleting widget" << Reporter::end;
    //if(mWidget != NULL)
        //delete mWidget;
    reportInfo() << "Finished deleting window widget" << Reporter::end;
    if(mThread != NULL) {
        mThread->stop();
        delete mThread;
        mThread = NULL;
    }

    reportInfo() << "Window destroyed" << Reporter::end;
}

void Window::setTimeout(unsigned int milliseconds) {
    mTimeout = milliseconds;
}

QOpenGLContext* Window::getMainGLContext() {
    if(mMainGLContext == NULL) {
        throw Exception("No OpenGL context created");
        //initializeQtApp();
    }

    return mMainGLContext;
}

void Window::startComputationThread() {
    if(mThread == NULL) {
        // Start computation thread using QThreads which is a strange thing, see https://mayaposch.wordpress.com/2011/11/01/how-to-really-truly-use-qthreads-the-full-explanation/
        reportInfo() << "Trying to start computation thread" << Reporter::end;
        mThread = new ComputationThread(QThread::currentThread());
        QThread* thread = new QThread();
        mThread->moveToThread(thread);
        connect(thread, SIGNAL(started()), mThread, SLOT(run()));
        connect(mThread, SIGNAL(finished()), thread, SLOT(quit()));
        //connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));

        for(int i = 0; i < getViews().size(); i++)
            mThread->addView(getViews()[i]);
        QOpenGLContext* mainGLContext = Window::getMainGLContext();
        if(!mainGLContext->isValid()) {
            throw Exception("QGL context is invalid!");
        }

        mainGLContext->makeCurrent(mainGLContext->surface());
        mainGLContext->doneCurrent();
        mainGLContext->moveToThread(thread);
        thread->start();
        reportInfo() << "Computation thread started" << Reporter::end;
    }
}

void Window::stopComputationThread() {
    reportInfo() << "Trying to stop computation thread" << Reporter::end;
    if(mThread != NULL) {
        mThread->stop();
        delete mThread;
        mThread = NULL;
    }
    reportInfo() << "Computation thread stopped" << Reporter::end;
}

std::vector<View*> Window::getViews() const {
    return mWidget->getViews();
}

View* Window::getView(uint i) const {
    return mWidget->getViews().at(i);
}

void Window::setWidth(uint width) {
    mWidth = width;
}

void Window::setHeight(uint height) {
    mHeight = height;
}

QSurface* Window::getSurface() {
    return mSurface;
}

} // end namespace fast
