/**
 * Examples/Visualization/extractSurfaceAndRender.cpp
 *
 * If you edit this example, please also update the wiki and source code file in the repository.
 */
#include "ImageFileImporter.hpp"
#include "SurfaceExtraction.hpp"
#include "MeshRenderer.hpp"
#include "SimpleWindow.hpp"

using namespace fast;

int main() {
    // Import CT image
    ImageFileImporter::pointer importer = ImageFileImporter::New();
    importer->setFilename(std::string(FAST_TEST_DATA_DIR) + "CT-Abdomen.mhd");

    // Extract surface mesh using a threshold value
    SurfaceExtraction::pointer extraction = SurfaceExtraction::New();
    extraction->setInputConnection(importer->getOutputPort());
    extraction->setThreshold(300);

    // Render and visualize the mesh
    MeshRenderer::pointer surfaceRenderer = MeshRenderer::New();
    surfaceRenderer->setInputConnection(extraction->getOutputPort());
    surfaceRenderer->enableRuntimeMeasurements();

	SimpleWindow::pointer window = SimpleWindow::New();
    window->addRenderer(surfaceRenderer);
    window->setTimeout(5*1000); // automatically close window after 5 seconds
    window->start();
}
