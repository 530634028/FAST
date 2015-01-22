#include "catch.hpp"
#include "MetaImageExporter.hpp"
#include "MetaImageImporter.hpp"
#include "Image.hpp"
#include "DataComparison.hpp"

using namespace fast;

TEST_CASE("No filename given to the MetaImageExporter", "[fast][MetaImageExporter]") {
    Image::pointer image = Image::New();
    MetaImageExporter::pointer exporter = MetaImageExporter::New();
    exporter->setInputData(image);
    CHECK_THROWS(exporter->update());
}

TEST_CASE("No input given to the MetaImageExporter", "[fast][MetaImageExporter]") {
    MetaImageExporter::pointer exporter = MetaImageExporter::New();
    exporter->setFilename("asd");
    CHECK_THROWS(exporter->update());
}

TEST_CASE("Write a 2D image with the MetaImageExporter", "[fast][MetaImageExporter]") {
    // Create some metadata
    Vector3f spacing;
    spacing[0] = 1.2;
    spacing[1] = 2.3;
    spacing[2] = 1;
    Vector3f offset;
    offset[0] = 2.2;
    offset[1] = 3.3;
    offset[2] = 3.1;
    Vector3f centerOfRotation;
    centerOfRotation[0] = 3.2;
    centerOfRotation[1] = 4.3;
    centerOfRotation[2] = 5.0;
    Matrix3f transformMatrix;
    transformMatrix(0,0) = 0.2;
    transformMatrix(1,0) = 1.3;
    transformMatrix(2,0) = 2.0;
    transformMatrix(0,1) = 3.0;
    transformMatrix(1,1) = 4.0;
    transformMatrix(2,1) = 5.0;
    transformMatrix(0,2) = 6.0;
    transformMatrix(1,2) = 7.0;
    transformMatrix(2,2) = 8.0;

    unsigned int width = 32;
    unsigned int height = 46;
    for(unsigned int components = 1; components <= 4; components++) {
        for(unsigned int typeNr = 0; typeNr < 5; typeNr++) { // for all types
            DataType type = (DataType)typeNr;

            Image::pointer image = Image::New();
            void* data = allocateRandomData(width*height*components, type);
            image->create2DImage(width, height, type, components, Host::getInstance(), data);

            // Set metadata
            image->setSpacing(spacing);
            image->setOffset(offset);
            image->setCenterOfRotation(centerOfRotation);
            image->setTransformMatrix(transformMatrix);

            // Export image
            MetaImageExporter::pointer exporter = MetaImageExporter::New();
            exporter->setFilename("MetaImageExporterTest2D.mhd");
            exporter->setInputData(image);
            exporter->update();

            // Import image back again
            MetaImageImporter::pointer importer = MetaImageImporter::New();
            importer->setFilename("MetaImageExporterTest2D.mhd");

            importer->update();
            Image::pointer image2 = importer->getOutputData<Image>();

            // Check that the image properties are correct
            for(unsigned int i = 0; i < 3; i++) {
                CHECK(spacing[i] == Approx(image2->getSpacing()[i]));
                CHECK(offset[i] == Approx(image2->getOffset()[i]));
                CHECK(centerOfRotation[i] == Approx(image2->getCenterOfRotation()[i]));
            }
            for(unsigned int i = 0; i < 3; i++) {
            for(unsigned int j = 0; j < 3; j++) {
                CHECK(transformMatrix(i,j) == Approx(image2->getTransformMatrix()(i,j)));
            }}


            CHECK(image2->getWidth() == width);
            CHECK(image2->getHeight() == height);
            CHECK(image2->getDepth() == 1);
            CHECK(image2->getDataType() == type);
            CHECK(image2->getNrOfComponents() == components);
            CHECK(image2->getDimensions() == 2);

            ImageAccess access = image2->getImageAccess(ACCESS_READ);
            void* data2 = access.get();
            CHECK(compareDataArrays(data, data2, width*height*components, type) == true);
            deleteArray(data, type);
        }
    }
}


TEST_CASE("Write a 3D image with the MetaImageExporter", "[fast][MetaImageExporter]") {
    // Create some metadata
    Vector3f spacing;
    spacing[0] = 1.2;
    spacing[1] = 2.3;
    spacing[2] = 1;
    Vector3f offset;
    offset[0] = 2.2;
    offset[1] = 3.3;
    offset[2] = 3.1;
    Vector3f centerOfRotation;
    centerOfRotation[0] = 3.2;
    centerOfRotation[1] = 4.3;
    centerOfRotation[2] = 5.0;
    Matrix3f transformMatrix;
    transformMatrix(0,0) = 0.2;
    transformMatrix(1,0) = 1.3;
    transformMatrix(2,0) = 2.0;
    transformMatrix(0,1) = 3.0;
    transformMatrix(1,1) = 4.0;
    transformMatrix(2,1) = 5.0;
    transformMatrix(0,2) = 6.0;
    transformMatrix(1,2) = 7.0;
    transformMatrix(2,2) = 8.0;

    unsigned int width = 32;
    unsigned int height = 22;
    unsigned int depth = 20;
    for(unsigned int components = 1; components <= 4; components++) {
        for(unsigned int typeNr = 0; typeNr < 5; typeNr++) { // for all types
            DataType type = (DataType)typeNr;

            Image::pointer image = Image::New();
            void* data = allocateRandomData(width*height*depth*components, type);
            image->create3DImage(width, height, depth, type, components, Host::getInstance(), data);

            // Set metadata
            image->setSpacing(spacing);
            image->setOffset(offset);
            image->setCenterOfRotation(centerOfRotation);
            image->setTransformMatrix(transformMatrix);

            // Export image
            MetaImageExporter::pointer exporter = MetaImageExporter::New();
            exporter->setFilename("MetaImageExporterTest3D.mhd");
            exporter->setInputData(image);
            exporter->update();

            // Import image back again
            MetaImageImporter::pointer importer = MetaImageImporter::New();
            importer->setFilename("MetaImageExporterTest3D.mhd");
            importer->update();
            Image::pointer image2 = importer->getOutputData<Image>(0);

            // Check that the image properties are correct
            for(unsigned int i = 0; i < 3; i++) {
                CHECK(spacing[i] == Approx(image2->getSpacing()[i]));
                CHECK(offset[i] == Approx(image2->getOffset()[i]));
                CHECK(centerOfRotation[i] == Approx(image2->getCenterOfRotation()[i]));
            }
            for(unsigned int i = 0; i < 3; i++) {
            for(unsigned int j = 0; j < 3; j++) {
                CHECK(transformMatrix(i,j) == Approx(image2->getTransformMatrix()(i,j)));
            }}

            CHECK(image2->getWidth() == width);
            CHECK(image2->getHeight() == height);
            CHECK(image2->getDepth() == depth);
            CHECK(image2->getDataType() == type);
            CHECK(image2->getNrOfComponents() == components);
            CHECK(image2->getDimensions() == 3);

            ImageAccess access = image2->getImageAccess(ACCESS_READ);
            void* data2 = access.get();
            CHECK(compareDataArrays(data, data2, width*height*depth*components, type) == true);
            deleteArray(data, type);
        }
    }
}

TEST_CASE("Write a compressed 2D image with the MetaImageExporter", "[fast][MetaImageExporter]") {
    // Create some metadata
    Vector3f spacing;
    spacing[0] = 1.2;
    spacing[1] = 2.3;
    spacing[2] = 1;
    Vector3f offset;
    offset[0] = 2.2;
    offset[1] = 3.3;
    offset[2] = 3.1;
    Vector3f centerOfRotation;
    centerOfRotation[0] = 3.2;
    centerOfRotation[1] = 4.3;
    centerOfRotation[2] = 5.0;
    Matrix3f transformMatrix;
    transformMatrix(0,0) = 0.2;
    transformMatrix(1,0) = 1.3;
    transformMatrix(2,0) = 2.0;
    transformMatrix(0,1) = 3.0;
    transformMatrix(1,1) = 4.0;
    transformMatrix(2,1) = 5.0;
    transformMatrix(0,2) = 6.0;
    transformMatrix(1,2) = 7.0;
    transformMatrix(2,2) = 8.0;

    unsigned int width = 32;
    unsigned int height = 46;
    for(unsigned int components = 1; components <= 4; components++) {
        INFO("Nr of components: " << components);
        for(unsigned int typeNr = 0; typeNr < 5; typeNr++) { // for all types
            INFO("Type nr: " << typeNr);
            DataType type = (DataType)typeNr;

            Image::pointer image = Image::New();
            void* data = allocateRandomData(width*height*components, type);
            image->create2DImage(width, height, type, components, Host::getInstance(), data);

            // Set metadata
            image->setSpacing(spacing);
            image->setOffset(offset);
            image->setCenterOfRotation(centerOfRotation);
            image->setTransformMatrix(transformMatrix);

            // Export image
            MetaImageExporter::pointer exporter = MetaImageExporter::New();
            exporter->setFilename("MetaImageExporterTest2D.mhd");
            exporter->setInputData(image);
            exporter->enableCompression();
            exporter->update();

            // Import image back again
            MetaImageImporter::pointer importer = MetaImageImporter::New();
            importer->setFilename("MetaImageExporterTest2D.mhd");

            importer->update();
            Image::pointer image2 = importer->getOutputData<Image>();

            // Check that the image properties are correct
            for(unsigned int i = 0; i < 3; i++) {
                CHECK(spacing[i] == Approx(image2->getSpacing()[i]));
                CHECK(offset[i] == Approx(image2->getOffset()[i]));
                CHECK(centerOfRotation[i] == Approx(image2->getCenterOfRotation()[i]));
            }
            for(unsigned int i = 0; i < 3; i++) {
            for(unsigned int j = 0; j < 3; j++) {
                CHECK(transformMatrix(i,j) == Approx(image2->getTransformMatrix()(i,j)));
            }}


            CHECK(image2->getWidth() == width);
            CHECK(image2->getHeight() == height);
            CHECK(image2->getDepth() == 1);
            CHECK(image2->getDataType() == type);
            CHECK(image2->getNrOfComponents() == components);
            CHECK(image2->getDimensions() == 2);

            ImageAccess access = image2->getImageAccess(ACCESS_READ);
            void* data2 = access.get();
            CHECK(compareDataArrays(data, data2, width*height*components, type) == true);
            deleteArray(data, type);
        }
    }
}

TEST_CASE("Write a compressed 3D image with the MetaImageExporter", "[fast][MetaImageExporter]") {
    // Create some metadata
    Vector3f spacing;
    spacing[0] = 1.2;
    spacing[1] = 2.3;
    spacing[2] = 1;
    Vector3f offset;
    offset[0] = 2.2;
    offset[1] = 3.3;
    offset[2] = 3.1;
    Vector3f centerOfRotation;
    centerOfRotation[0] = 3.2;
    centerOfRotation[1] = 4.3;
    centerOfRotation[2] = 5.0;
    Matrix3f transformMatrix;
    transformMatrix(0,0) = 0.2;
    transformMatrix(1,0) = 1.3;
    transformMatrix(2,0) = 2.0;
    transformMatrix(0,1) = 3.0;
    transformMatrix(1,1) = 4.0;
    transformMatrix(2,1) = 5.0;
    transformMatrix(0,2) = 6.0;
    transformMatrix(1,2) = 7.0;
    transformMatrix(2,2) = 8.0;

    unsigned int width = 32;
    unsigned int height = 22;
    unsigned int depth = 20;
    for(unsigned int components = 1; components <= 4; components++) {
        for(unsigned int typeNr = 0; typeNr < 5; typeNr++) { // for all types
            DataType type = (DataType)typeNr;

            Image::pointer image = Image::New();
            void* data = allocateRandomData(width*height*depth*components, type);
            image->create3DImage(width, height, depth, type, components, Host::getInstance(), data);

            // Set metadata
            image->setSpacing(spacing);
            image->setOffset(offset);
            image->setCenterOfRotation(centerOfRotation);
            image->setTransformMatrix(transformMatrix);

            // Export image
            MetaImageExporter::pointer exporter = MetaImageExporter::New();
            exporter->setFilename("MetaImageExporterTest3D.mhd");
            exporter->setInputData(image);
            exporter->enableCompression();
            exporter->update();

            // Import image back again
            MetaImageImporter::pointer importer = MetaImageImporter::New();
            importer->setFilename("MetaImageExporterTest3D.mhd");
            importer->update();
            Image::pointer image2 = importer->getOutputData<Image>(0);

            // Check that the image properties are correct
            for(unsigned int i = 0; i < 3; i++) {
                CHECK(spacing[i] == Approx(image2->getSpacing()[i]));
                CHECK(offset[i] == Approx(image2->getOffset()[i]));
                CHECK(centerOfRotation[i] == Approx(image2->getCenterOfRotation()[i]));
            }
            for(unsigned int i = 0; i < 3; i++) {
            for(unsigned int j = 0; j < 3; j++) {
                CHECK(transformMatrix(i,j) == Approx(image2->getTransformMatrix()(i,j)));
            }}

            CHECK(image2->getWidth() == width);
            CHECK(image2->getHeight() == height);
            CHECK(image2->getDepth() == depth);
            CHECK(image2->getDataType() == type);
            CHECK(image2->getNrOfComponents() == components);
            CHECK(image2->getDimensions() == 3);

            ImageAccess access = image2->getImageAccess(ACCESS_READ);
            void* data2 = access.get();
            CHECK(compareDataArrays(data, data2, width*height*depth*components, type) == true);
            deleteArray(data, type);
        }
    }
}
