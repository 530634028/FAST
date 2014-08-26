#include "catch.hpp"
#include "ImageExporter.hpp"
#include "ImageImporter.hpp"
#include "Image.hpp"
#include "DataComparison.hpp"

using namespace fast;

TEST_CASE("No filename given to the ImageExporter", "[fast][ImageExporter]") {
    Image::pointer image = Image::New();
    ImageExporter::pointer exporter = ImageExporter::New();
    exporter->setInput(image);
    CHECK_THROWS(exporter->update());
}

TEST_CASE("No input given to the ImageExporter", "[fast][ImageExporter]") {
    ImageExporter::pointer exporter = ImageExporter::New();
    exporter->setFilename("asd");
    CHECK_THROWS(exporter->update());
}


TEST_CASE("If 3D image is given as input to ImageExporter it throws exception", "[fast][ImageExporter]") {
    Image::pointer image = Image::New();
    image->create3DImage(16,16,16,TYPE_INT8,1,Host::New());
    ImageExporter::pointer exporter = ImageExporter::New();
    exporter->setFilename("asd");
    exporter->setInput(image);
    CHECK_THROWS(exporter->update());
}

TEST_CASE("Write 2D image with the ImageExporter", "[fast][ImageExporter]") {

    unsigned int width = 32;
    unsigned int height = 46;
    unsigned int components = 1;
    DataType type = TYPE_UINT8;

    Image::pointer image = Image::New();
    image->create2DImage(width,height,type,components,Host::New());

    ImageExporter::pointer exporter = ImageExporter::New();
    exporter->setFilename("ImageExporterTest.jpg");
    exporter->setInput(image);
    exporter->update();

    ImageImporter::pointer importer = ImageImporter::New();
    importer->setFilename("ImageExporterTest.jpg");
    Image::pointer image2 = importer->getOutput();
    importer->update();

    CHECK(width == image2->getWidth());
    CHECK(height == image2->getHeight());

}

