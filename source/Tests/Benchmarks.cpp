#include "catch.hpp"
#include "MetaImageImporter.hpp"
#include "MetaImageStreamer.hpp"
#include "GaussianSmoothingFilter.hpp"
#include "SliceRenderer.hpp"
#include "SurfaceRenderer.hpp"
#include "SurfaceExtraction.hpp"
#include "ImageRenderer.hpp"
#include "SimpleWindow.hpp"
#include "DeviceManager.hpp"
#include "SeededRegionGrowing.hpp"
#include "Skeletonization.hpp"
#include "ImageImporter.hpp"
#include "ImageRenderer.hpp"
#include "BinaryThresholding.hpp"

using namespace fast;

TEST_CASE("Pipeline A (static)", "[fast][benchmark][visual]") {
    MetaImageImporter::pointer importer = MetaImageImporter::New();
    importer->setFilename(std::string(FAST_TEST_DATA_DIR)+"/US-3Dt/US-3Dt_0.mhd");
    importer->enableRuntimeMeasurements();

    GaussianSmoothingFilter::pointer filter = GaussianSmoothingFilter::New();
    filter->setInput(importer->getOutput());
    filter->setMaskSize(5);
    filter->setStandardDeviation(2.0);

    SurfaceExtraction::pointer extractor = SurfaceExtraction::New();
    extractor->setInput(filter->getOutput());
    extractor->setThreshold(200);

    SurfaceRenderer::pointer renderer = SurfaceRenderer::New();
    renderer->setInput(extractor->getOutput());

    SimpleWindow::pointer window = SimpleWindow::New();
    window->addRenderer(renderer);
    window->setTimeout(2*1000); // timeout after 2 seconds
    window->runMainLoop();

    importer->getRuntime()->print();
    filter->getRuntime()->print();
    extractor->getRuntime()->print();
    renderer->getRuntime()->print();
    float total = importer->getRuntime()->getSum() +
            filter->getRuntime()->getSum() +
            extractor->getRuntime()->getSum() +
            renderer->getRuntime()->getSum();
    std::cout << "Total runtime was: " << total << std::endl;
}

TEST_CASE("Pipeline A (dynamic)", "[fast][benchmark][visual]") {
    MetaImageStreamer::pointer streamer = MetaImageStreamer::New();
    streamer->setFilenameFormat(std::string(FAST_TEST_DATA_DIR)+"/US-3Dt/US-3Dt_#.mhd");
    streamer->setStreamingMode(STREAMING_MODE_PROCESS_ALL_FRAMES);
    streamer->enableRuntimeMeasurements();

    GaussianSmoothingFilter::pointer filter = GaussianSmoothingFilter::New();
    filter->setInput(streamer->getOutput());
    filter->setMaskSize(5);
    filter->setStandardDeviation(2.0);
    filter->enableRuntimeMeasurements();

    SurfaceExtraction::pointer extractor = SurfaceExtraction::New();
    extractor->setInput(filter->getOutput());
    extractor->setThreshold(200);
    extractor->enableRuntimeMeasurements();

    SurfaceRenderer::pointer renderer = SurfaceRenderer::New();
    renderer->setInput(extractor->getOutput());
    renderer->enableRuntimeMeasurements();

    SimpleWindow::pointer window = SimpleWindow::New();
    window->addRenderer(renderer);
    window->setTimeout(15*1000); // timeout after 10 seconds
    window->runMainLoop();

    streamer->getRuntime()->print();
    filter->getRuntime()->print();
    extractor->getRuntime()->print();
    renderer->getRuntime()->print();
    float total = streamer->getRuntime()->getSum() +
		filter->getRuntime()->getSum() +
		extractor->getRuntime()->getSum() +
		renderer->getRuntime()->getSum();
	total = total / 84; // number of frames
    std::cout << "Average runtime was: " << total << std::endl << std::endl;
}

TEST_CASE("Pipeline B", "[fast][benchmark][visual]") {
    MetaImageImporter::pointer importer = MetaImageImporter::New();
    importer->setFilename(std::string(FAST_TEST_DATA_DIR) + "CT-Abdomen.mhd");
    importer->enableRuntimeMeasurements();

    SeededRegionGrowing::pointer segmentation = SeededRegionGrowing::New();
    segmentation->setInput(importer->getOutput());
    segmentation->addSeedPoint(223,282,387);
    segmentation->addSeedPoint(251,314,148);
    /*
    segmentation->addSeedPoint(231,281,313);
    segmentation->addSeedPoint(260,284,187);
    segmentation->addSeedPoint(370,290,111);
    segmentation->addSeedPoint(134,293,111);
    */
    segmentation->setIntensityRange(150, 5000);
    segmentation->enableRuntimeMeasurements();

    SurfaceExtraction::pointer extraction = SurfaceExtraction::New();
    extraction->setInput(segmentation->getOutput());
    extraction->enableRuntimeMeasurements();

    /*
    VTKSurfaceFileExporter::pointer exporter = VTKSurfaceFileExporter::New();
    exporter->setFilename("bones.vtk");
    exporter->setInput(extraction->getOutput());
    exporter->update();
    */

    SurfaceRenderer::pointer surfaceRenderer = SurfaceRenderer::New();
    surfaceRenderer->setInput(extraction->getOutput());
    surfaceRenderer->enableRuntimeMeasurements();

    SliceRenderer::pointer sliceRenderer = SliceRenderer::New();
    sliceRenderer->setInput(importer->getOutput());
    sliceRenderer->setIntensityWindow(1000);
    sliceRenderer->setIntensityLevel(0);
    sliceRenderer->enableRuntimeMeasurements();

	SimpleWindow::pointer window = SimpleWindow::New();
    window->addRenderer(surfaceRenderer);
    //window->addRenderer(sliceRenderer);
    window->setTimeout(2*1000); // timeout after 2 seconds
    window->runMainLoop();
    importer->getRuntime()->print();
    segmentation->getRuntime()->print();
    extraction->getRuntime()->print();
    surfaceRenderer->getRuntime()->print();
    sliceRenderer->getRuntime()->print();

    float total = importer->getRuntime()->getSum()+
                segmentation->getRuntime()->getSum()+
                extraction->getRuntime()->getSum()+
                surfaceRenderer->getRuntime()->getSum() +
                sliceRenderer->getRuntime()->getSum();
    std::cout << "Total runtime was: " << total << std::endl;
}

TEST_CASE("Pipeline C", "[fast][benchmark][visual]") {
    ImageImporter::pointer importer = ImageImporter::New();
    importer->setFilename(std::string(FAST_TEST_DATA_DIR) + "retina.png");
    importer->enableRuntimeMeasurements();

    BinaryThresholding::pointer thresholding = BinaryThresholding::New();
    thresholding->setInput(importer->getOutput());
    thresholding->setLowerThreshold(0.5);
    thresholding->enableRuntimeMeasurements();

    Skeletonization::pointer skeletonization = Skeletonization::New();
    skeletonization->setInput(thresholding->getOutput());
    skeletonization->enableRuntimeMeasurements();

    ImageRenderer::pointer renderer = ImageRenderer::New();
    renderer->setInput(skeletonization->getOutput());
    renderer->setIntensityWindow(1);
    renderer->setIntensityLevel(0.5);
    renderer->enableRuntimeMeasurements();
    SimpleWindow::pointer window = SimpleWindow::New();
    window->addRenderer(renderer);
    window->setTimeout(2000);
    window->runMainLoop();
    importer->getRuntime()->print();
    thresholding->getRuntime()->print();
    skeletonization->getRuntime()->print();
    renderer->getRuntime()->print();
    float total = importer->getRuntime()->getSum() +
            thresholding->getRuntime()->getSum() +
            skeletonization->getRuntime()->getSum() +
            renderer->getRuntime()->getSum();
    std::cout << "Total runtime was: " << total << std::endl;
}
