/*
 * Copyright (C) 2004 Andrew Mihal
 *
 * This file is part of Enblend.
 *
 * Enblend is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Enblend is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Enblend; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <list>
#include <vector>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include <boost/random/mersenne_twister.hpp>

#include "common.h"
#include "enblend.h"

#include "vigra/impex.hxx"
#include "vigra/stdimage.hxx"
#include "vigra_ext/stdcachedfileimage.hxx"

using std::cout;
using std::cerr;
using std::endl;
using std::list;

using vigra::CachedFileImageDirector;
using vigra::BCFImage;
using vigra::BRGBCFImage;
using vigra::Diff2D;
using vigra::DCFImage;
using vigra::DRGBCFImage;
using vigra::FCFImage;
using vigra::FRGBCFImage;
using vigra::ICFImage;
using vigra::ImageExportInfo;
using vigra::ImageImportInfo;
using vigra::IRGBCFImage;
using vigra::SCFImage;
using vigra::SRGBCFImage;
using vigra::StdException;
using vigra::UICFImage;
using vigra::UIRGBCFImage;
using vigra::USCFImage;
using vigra::USRGBCFImage;

using enblend::enblendMain;
using enblend::EnblendROI;

// Random number generator
boost::mt19937 Twister;

// Global values from command line parameters.
int Verbose = 1;
unsigned int MaximumLevels = 0;
bool OneAtATime = true;
bool Wraparound = false;
double StitchMismatchThreshold = 0.4;
bool GimpAssociatedAlphaHack = false;
bool UseLabColor = false;

//uint32 OutputWidth = 0;
//uint32 OutputHeight = 0;

//uint16 PlanarConfig;
//uint16 Photometric;

//// Union bounding box.
//uint32 UBBFirstX;
//uint32 UBBLastX;
//uint32 UBBFirstY;
//uint32 UBBLastY;

//// The region of interest for pyramids.
//uint32 ROIFirstX;
//uint32 ROILastX;
//uint32 ROIFirstY;
//uint32 ROILastY;

/** Print the usage information and quit. */
void printUsageAndExit() {
    cout << "==== enblend, version " << VERSION << " ====" << endl;
    cout << "Usage: enblend [options] -o OUTPUT INPUTS" << endl;
    cout << endl;
    cout << "Common options:" << endl;
    cout << " -a                Pre-assemble non-overlapping images" << endl;
    cout << " -h                Print this help message" << endl;
    cout << " -l number         Maximum number of levels to use" << endl;
    cout << " -o filename       Write output to file" << endl;
    cout << " -v                Verbose" << endl;
    cout << " -w                Blend across -180/+180 boundary" << endl;

    cout << endl << "Extended options:" << endl;
    cout << " -b kilobytes      Image cache block size (default=1MB)" << endl;
    cout << " -c                Use CIE L*a*b* color space" << endl;
    cout << " -g                Associated alpha hack for Gimp (ver. < 2) and Cinepaint" << endl;
    cout << " -m megabytes      Use this much memory before going to disk (default=1GB)" << endl;
    //TODO stitch mismatch avoidance is work-in-progress.
    //cout << " -t float          Stitch mismatch threshold, [0.0, 1.0]" << endl;
    // deprecated
    //cout << " -s                Blend images one at a time, in the order given" << endl;
    exit(1);
}

int main(int argc, char** argv) {

    // The name of the output file.
    char *outputFileName = NULL;

    // List of input files.
    list<char*> inputFileNameList;
    list<char*>::iterator inputFileNameIterator;

    // Parse command line.
    int c;
    extern char *optarg;
    extern int optind;
    while ((c = getopt(argc, argv, "ab:cghl:m:o:st:vw")) != -1) {
        switch (c) {
            case 'a': {
                OneAtATime = false;
                break;
            }
            case 'b': {
                int kilobytes = atoi(optarg);
                if (kilobytes < 1) {
                    cerr << "enblend: cache block size must be 1 or more."
                         << endl;
                    printUsageAndExit();
                }
                CachedFileImageDirector::v().setBlockSize((long long)kilobytes << 10);
                break;
            }
            case 'c': {
                UseLabColor = true;
                break;
            }
            case 'g': {
                GimpAssociatedAlphaHack = true;
                break;
            }
            case 'h': {
                printUsageAndExit();
                break;
            }
            case 'l': {
                int levels = atoi(optarg);
                if (levels < 1) {
                    cerr << "enblend: levels must be 1 or more."
                         << endl;
                    printUsageAndExit();
                }
                MaximumLevels = (unsigned int)levels;
                break;
            }
            case 'm': {
                int megabytes = atoi(optarg);
                if (megabytes < 1) {
                    cerr << "enblend: memory limit must be 1 or more."
                         << endl;
                    printUsageAndExit();
                }
                CachedFileImageDirector::v().setAllocation((long long)megabytes << 20);
                break;
            }
            case 'o': {
                if (outputFileName != NULL) {
                    cerr << "enblend: more than one output file specified."
                         << endl;
                    printUsageAndExit();
                    break;
                }
                int len = strlen(optarg) + 1;
                outputFileName = new char[len];
                if (outputFileName == NULL) {
                    cerr << endl
                         << "enblend: out of memory (in main for outputFileName)" << endl;
                    exit(1);
                }
                strncpy(outputFileName, optarg, len);
                break;
            }
            case 's': {
                // Deprecated sequential blending flag.
                OneAtATime = true;
                cerr << "enblend: the -s flag is deprecated." << endl;
                break;
            }
            case 't': {
                StitchMismatchThreshold = strtod(optarg, NULL);
                if (StitchMismatchThreshold < 0.0
                        || StitchMismatchThreshold > 1.0) {
                    cerr << "enblend: threshold must be between "
                         << "0.0 and 1.0 inclusive."
                         << endl;
                    printUsageAndExit();
                }
                break;
            }
            case 'v': {
                Verbose++;
                break;
            }
            case 'w': {
                Wraparound = true;
                break;
            }
            default: {
                printUsageAndExit();
                break;
            }
        }
    }

    // Make sure mandatory output file name parameter given.
    if (outputFileName == NULL) {
        cerr << "enblend: no output file specified." << endl;
        printUsageAndExit();
    }

    // Remaining parameters are input files.
    if (optind < argc) {
        while (optind < argc) {
            inputFileNameList.push_back(argv[optind++]);
        }
    } else {
        cerr << "enblend: no input files specified." << endl;
        printUsageAndExit();
    }

    // List of info structures for each input image.
    list<ImageImportInfo*> imageInfoList;
    list<ImageImportInfo*>::iterator imageInfoIterator;

    bool isColor = false;
    const char *pixelType = NULL;
    EnblendROI inputUnion;

    // Check that all input images have the same parameters.
    inputFileNameIterator = inputFileNameList.begin();
    while (inputFileNameIterator != inputFileNameList.end()) {

        ImageImportInfo *inputInfo = NULL;
        try {
            inputInfo = new ImageImportInfo(*inputFileNameIterator);
        } catch (StdException& e) {
            cerr << endl << "enblend: error opening input file:"
                 << endl << e.what()
                 << endl;
            exit(1);
        }

        // Save this image info in the list.
        imageInfoList.push_back(inputInfo);

        if (Verbose > VERBOSE_INPUT_IMAGE_INFO_MESSAGES) {
            cout << "Input image \""
                 << *inputFileNameIterator
                 << "\" ";

            if (inputInfo->isColor()) cout << "RGB ";

            cout << inputInfo->getPixelType() << " "
                 << "position="
                 << inputInfo->getPosition().x
                 << "x"
                 << inputInfo->getPosition().y
                 << " "
                 << "size="
                 << inputInfo->width()
                 << "x"
                 << inputInfo->height()
                 << endl;
        }

        if (inputInfo->numExtraBands() < 1) {
            // Complain about lack of alpha channel.
            cerr << "enblend: Input image does not have an alpha "
                 << "channel. This is required to determine which pixels "
                 << "contribute to the final image."
                 << endl;
            exit(1);
        }

        // Get input image's position and size.
        Diff2D imageSize(inputInfo->width(), inputInfo->height());
        Diff2D imagePos = inputInfo->getPosition();
        EnblendROI imageROI(imagePos, imageSize);

        if (inputFileNameIterator == inputFileNameList.begin()) {
            // The first input image.
            inputUnion = imageROI;
            isColor = inputInfo->isColor();
            pixelType = inputInfo->getPixelType();
        }
        else {
            // second and later images.
            inputUnion.unite(imageROI, inputUnion);

            if (isColor != inputInfo->isColor()) {
                cerr << "enblend: Input image is "
                     << (inputInfo->isColor() ? "color" : "grayscale")
                     << " but previous images are "
                     << (isColor ? "color" : "grayscale")
                     << "." << endl;
                exit(1);
            }
            if (strcmp(pixelType, inputInfo->getPixelType())) {
                cerr << "enblend: Input image has pixel type "
                     << inputInfo->getPixelType()
                     << " but previous images have pixel type "
                     << pixelType
                     << "." << endl;
                exit(1);
            }
        }

        inputFileNameIterator++;
    }

    // Create the Info for the output file.
    ImageExportInfo outputImageInfo(outputFileName);

    // Pixel type of the output image is the same as the input images.
    outputImageInfo.setPixelType(pixelType);

    // The size of the output image.
    if (Verbose > VERBOSE_INPUT_UNION_SIZE_MESSAGES) {
        // print inputUnion.
        Diff2D ul = inputUnion.getUL();
        Diff2D lr = inputUnion.getLR();
        Diff2D outputImageSize = inputUnion.size();
        cout << "Output image size: "
             << "(" << ul.x << ", " << ul.y << ") -> "
             << "(" << lr.x << ", " << lr.y << ") = ("
             << outputImageSize.x
             << " x "
             << outputImageSize.y
             << ")"
             << endl;
    }

    // Sanity check on the output image file.
    try {
        // This seems to be a reasonable way to check if
        // the output file is going to work after blending
        // is done.
        encoder(outputImageInfo);
    } catch (StdException & e) {
        cerr << endl << "enblend: error opening output file \""
             << outputFileName
             << "\":"
             << endl << e.what()
             << endl;
        exit(1);
    }
    //IRGBImage::Accessor::value_type foo();
    //IRGBImage::Accessor::value_type::value_type bar();
    //cout << sizeof(IRGBImage::Accessor::value_type) << endl;
    //cout << sizeof(IRGBImage::Accessor::value_type::value_type) << endl;
    //IImage::Accessor::value_type bam();
    //IImage::Accessor::value_type::value_type baz();
    //cout << sizeof(IImage::Accessor::value_type) << endl;
    //cout << sizeof(IImage::Accessor::value_type::value_type) << endl;
    // Invoke templatized blender.
    try {
        if (isColor) {
            if (strcmp(pixelType, "UINT8") == 0) {
                enblendMain<BRGBCFImage, SRGBCFImage>(
                        imageInfoList, outputImageInfo, inputUnion);
            //} else if (strcmp(pixelType, "INT16") == 0) {
            //    enblendMain<SRGBImage, IRGBImage>(
            //            imageInfoList, outputImageInfo, inputUnion);
            //} else if (strcmp(pixelType, "UINT16") == 0) {
            //    enblendMain<USRGBImage, IRGBImage>(
            //            imageInfoList, outputImageInfo, inputUnion);
            //} else if (strcmp(pixelType, "INT32") == 0) {
            //    enblendMain<IRGBImage, DRGBImage>(
            //            imageInfoList, outputImageInfo, inputUnion);
            //} else if (strcmp(pixelType, "UINT32") == 0) {
            //    enblendMain<UIRGBImage, DRGBImage>(
            //            imageInfoList, outputImageInfo, inputUnion);
            //} else if (strcmp(pixelType, "FLOAT") == 0) {
            //    enblendMain<FRGBImage, DRGBImage>(
            //            imageInfoList, outputImageInfo, inputUnion);
            //} else if (strcmp(pixelType, "DOUBLE") == 0) {
            //    enblendMain<DRGBImage, DRGBImage>(
            //            imageInfoList, outputImageInfo, inputUnion);
            } else {
                cerr << "enblend: pixel type \""
                     << pixelType
                     << "\" is not supported."
                     << endl;
                exit(1);
            }
        } else {
            //if (strcmp(pixelType, "UINT8") == 0) {
            //    enblendMain<BImage, SImage>(
            //            imageInfoList, outputImageInfo, inputUnion);
            //} else if (strcmp(pixelType, "INT16") == 0) {
            //    enblendMain<SImage, IImage>(
            //            imageInfoList, outputImageInfo, inputUnion);
            //} else if (strcmp(pixelType, "UINT16") == 0) {
            //    enblendMain<USImage, IImage>(
            //            imageInfoList, outputImageInfo, inputUnion);
            //} else if (strcmp(pixelType, "INT32") == 0) {
            //    enblendMain<IImage, DImage>(
            //            imageInfoList, outputImageInfo, inputUnion);
            //} else if (strcmp(pixelType, "UINT32") == 0) {
            //    enblendMain<UIImage, DImage>(
            //            imageInfoList, outputImageInfo, inputUnion);
            //} else if (strcmp(pixelType, "FLOAT") == 0) {
            //    enblendMain<FImage, DImage>(
            //            imageInfoList, outputImageInfo, inputUnion);
            //} else if (strcmp(pixelType, "DOUBLE") == 0) {
            //    enblendMain<DImage, DImage>(
            //            imageInfoList, outputImageInfo, inputUnion);
            //} else {
                cerr << "enblend: pixel type \""
                     << pixelType
                     << "\" is not supported."
                     << endl;
                exit(1);
            //}
        }
    } catch (std::bad_alloc& e) {
        cerr << endl << "enblend: out of memory"
             << endl << e.what()
             << endl;
        exit(1);
    } catch (StdException& e) {
        cerr << endl << "enblend: vigra threw an exception:"
             << endl << e.what()
             << endl;
        exit(1);
    }

    // delete entries in imageInfoList, in case
    // enblend loop returned early.
    imageInfoIterator = imageInfoList.begin();
    while (imageInfoIterator != imageInfoList.end()) {
        delete *imageInfoIterator++;
    }

    delete[] outputFileName;

    return 0;
}

/*
    // Create output TIFF object.
    TIFF *outputTIFF = TIFFOpen(outputFileName, "w");
    if (outputTIFF == NULL) {
        cerr << "enblend: error opening output TIFF file \""
             << outputFileName
             << "\"" << endl;
        exit(1);
    }

    // Set basic parameters for output TIFF.
    TIFFSetField(outputTIFF, TIFFTAG_ORIENTATION, 1);
    TIFFSetField(outputTIFF, TIFFTAG_SAMPLESPERPIXEL, 4);
    TIFFSetField(outputTIFF, TIFFTAG_BITSPERSAMPLE, 8);

    // Give output tiff the same parameters as first input tiff.
    // Check that all input tiffs are the same size.
    listIterator = inputFileNameList.begin();
    while (listIterator != inputFileNameList.end()) {
        TIFF *inputTIFF = TIFFOpen(*listIterator, "r");

        if (inputTIFF == NULL) {
            // Error opening tiff.
            cerr << "enblend: error opening input TIFF file \""
                 << *listIterator
                 << "\"" << endl;
            exit(1);
        }

        uint16 bitsPerSample = 0;
        TIFFGetField(inputTIFF, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
        if (bitsPerSample != 8) {
            cerr << "enblend: input TIFF file \""
                 << *listIterator
                 << "\" uses " << bitsPerSample << " bits per sample. "
                 << "Only 8 bit TIFFs are currently supported." << endl;
            exit(1);
        }

        if (listIterator == inputFileNameList.begin()) {
            // The first input tiff
            TIFFGetField(inputTIFF, TIFFTAG_PLANARCONFIG, &PlanarConfig);
            TIFFGetField(inputTIFF, TIFFTAG_PHOTOMETRIC, &Photometric);
            TIFFGetField(inputTIFF, TIFFTAG_IMAGEWIDTH, &OutputWidth);
            TIFFGetField(inputTIFF, TIFFTAG_IMAGELENGTH, &OutputHeight);

            TIFFSetField(outputTIFF, TIFFTAG_IMAGEWIDTH, OutputWidth);
            TIFFSetField(outputTIFF, TIFFTAG_IMAGELENGTH, OutputHeight);
            TIFFSetField(outputTIFF, TIFFTAG_PHOTOMETRIC, Photometric);
            TIFFSetField(outputTIFF, TIFFTAG_PLANARCONFIG, PlanarConfig);

            // We already forced this to be a 4-channel-per pixel (above), so 
            // make the fourth channel be alpha if RGB image (see tiff spec)
            // If not specified, Photoshop doesn't open up with transparent area
            if (Photometric == PHOTOMETRIC_RGB) {
                uint16 v[1];				
                v[0] = EXTRASAMPLE_ASSOCALPHA;
                TIFFSetField(outputTIFF, TIFFTAG_EXTRASAMPLES, 1, v);
            }			

            if (Verbose > 0) {
                cout << "Output image size: "
                     << OutputWidth
                     << " x "
                     << OutputHeight << endl;
            }
        }
        else {
            uint32 otherWidth;
            uint32 otherHeight;
            TIFFGetField(inputTIFF, TIFFTAG_IMAGEWIDTH, &otherWidth);
            TIFFGetField(inputTIFF, TIFFTAG_IMAGELENGTH, &otherHeight);

            if (otherWidth != OutputWidth || otherHeight != OutputHeight) {
                cerr << "enblend: input TIFF \""
                     << *listIterator
                     << "\" size "
                     << otherWidth << "x" << otherHeight
                     << " is not the same size as the first TIFF \""
                     << inputFileNameList.front()
                     << "\" size "
                     << OutputWidth << "x" << OutputHeight
                     << endl;
                exit(1);
            }
        }
            
        TIFFClose(inputTIFF);
        listIterator++;
    }

    // Create the initial white image.
    FILE *whiteImageFile = assemble(inputFileNameList, OneAtATime);

    // Main blending loop
    while (!inputFileNameList.empty()) {

        // Create the black image.
        FILE *blackImageFile = assemble(inputFileNameList, OneAtATime);

        // Caluclate the union bounding box of whiteImage and blackImage.
        ubbBounds(whiteImageFile, blackImageFile);

        // Estimate memory usage for this blend step.
        if (Verbose > 0) {
            // Max usage is in mask =
            // ubbWidth * ubbHeight * MaskPixel
            // + 2 * ubbWidth * ubbHeight * uint32
            uint32 ubbPixels = (UBBLastY - UBBFirstY + 1) *
                   (UBBLastX - UBBFirstX + 1);
            long long bytes = ubbPixels * sizeof(MaskPixel)
                    + 2 * ubbPixels * sizeof(uint32);
            cout << "Estimated memory required for this blend step: "
                 << (bytes / 1000000)
                 << "MB" << endl;
        }

        // Create the blend mask.
        FILE *maskFile = createMask(whiteImageFile, blackImageFile);

        // Calculate the ROI bounds and number of levels.
        uint32 levels = roiBounds(maskFile);
        if (levels == 0) {
            // Corner case - the mask indicates that blackImage has no
            // contribution to the output.
            cerr << "enblend: some images are redundant and will not be "
                 << "blended. Try using the -s flag to blend the images "
                 << "in a custom order you specify." << endl;
            closeTmpfile(blackImageFile);
            closeTmpfile(maskFile);
            continue;
        }
        if (MaximumLevels > 0) {
            levels = min(levels, (uint32)MaximumLevels);
        }

        // Estimate disk usage for this blend step.
        if (Verbose > 0) {
            // whiteImageFile = OutputWidth * OutputHeight * uint32
            // blackImageFile = OutputWidth * OutputHeight * uint32
            // maskFile = ubbWidth * ubbHeight * MaskPixel
            // blackLPFile = 4/3 * roiWidth * roiHeight * LPPixel
            // maskGPFile = 4/3 * roiWidth * roiHeight * LPPixel
            long long bytes = (2 * OutputWidth * OutputHeight * sizeof(uint32))
                    + ((UBBLastY - UBBFirstY + 1) * (UBBLastX - UBBFirstX + 1) * sizeof(MaskPixel))
                    + ((ROILastY - ROIFirstY + 1) * (ROILastX - ROIFirstX + 1) * sizeof(LPPixel) * 8 / 3);
            cout << "Estimated temporary disk space required for this blend step: "
                 << (bytes / 1000000)
                 << "MB" << endl;
        }

        // Copy parts of blackImage outside of ROI and inside the black part
        // of the mask into whiteImage.
        copyExcludedPixels(whiteImageFile, blackImageFile, maskFile);

        // Build Laplacian pyramid from blackImage
        FILE *blackLPFile = laplacianPyramidFile(blackImageFile, levels);

        // Done with blackImage temp file. It will be deleted.
        closeTmpfile(blackImageFile);

        // Build Gaussian pyramid from mask.
        FILE *maskGPFile = gaussianPyramidFile(maskFile, levels);

        // Build Laplacian pyramid from whiteImage
        // load whiteImage from disk one row at a time.
        vector<LPPixel*> *whiteLP = laplacianPyramid(whiteImageFile, levels);

        // Blend pyramids
        blend(*whiteLP, blackLPFile, maskGPFile);

        // Done with blackLP and maskGP temp files. They will be deleted.
        closeTmpfile(blackLPFile);
        closeTmpfile(maskGPFile);

        // Collapse result back into whiteImage.
        collapsePyramid(*whiteLP);

        // Copy result into whiteImageFile using maskFile as template.
        copyROIToOutputWithMask((*whiteLP)[0], whiteImageFile, maskFile);

        // Done with maskFile temp file. It will be deleted.
        closeTmpfile(maskFile);

        // Done with whiteLP.
        for (uint32 i = 0; i < levels; i++) {
            free((*whiteLP)[i]);
        }
        delete whiteLP;

    }

    // dump output scanlines.
    if (Verbose > 0) {
        cout << "Writing output file." << endl;
    }

    // Scanline for copying whiteImageFile to outputTIFF.
    uint32 *line = (uint32*)malloc(OutputWidth * sizeof(uint32));
    if (line == NULL) {
        cerr << endl
             << "enblend: out of memory (in main for line)" << endl;
        exit(1);
    }

    rewind(whiteImageFile);
    for (uint32 i = 0; i < OutputHeight; i++) {
        readFromTmpfile(line, sizeof(uint32), OutputWidth, whiteImageFile);
        TIFFWriteScanline(outputTIFF, line, i, 8);
    }

    free(line);

    // close whiteImageFile.
    closeTmpfile(whiteImageFile);

    // close outputTIFF.
    TIFFClose(outputTIFF);

    free(outputFileName);

*/
