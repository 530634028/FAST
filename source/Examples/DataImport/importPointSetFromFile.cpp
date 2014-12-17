/**
 * Examples/DataImport/importPointSetFromFile.cpp
 *
 * If you edit this example, please also update the wiki and source code file in the repository.
 */
#include "VTKPointSetFileImporter.hpp"
#include "PointRenderer.hpp"
#include "SimpleWindow.hpp"

using namespace fast;

int main() {
    // Import line set from vtk file
    VTKPointSetFileImporter::pointer importer = VTKPointSetFileImporter::New();
    importer->setFilename(std::string(FAST_TEST_DATA_DIR) + "Surface_LV.vtk");

    // Renderer image
    PointRenderer::pointer renderer = PointRenderer::New();
    renderer->addInput(importer->getOutput());
    SimpleWindow::pointer window = SimpleWindow::New();
    window->addRenderer(renderer);
    window->setTimeout(5*1000); // automatically close window after 5 seconds
    window->start();
}
