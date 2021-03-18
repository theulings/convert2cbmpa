/*
    Copyright (c) 2021, Alexander Theulings. All rights reserved.
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define BUILD_VERSION 1

#include <fstream>
#include <unistd.h>
#include <Magick++.h>
#include <libcollie/collieImage.h>
#include <nlohmann/json.hpp>
#include <climits>

bool verbose = false;
bool info = false;

void printHelp(){
    printf("convert2cbmpa version %i\n", BUILD_VERSION);
    printf("A simple tool to convert various image formats to the Collie bmpa format using Magick++.");
    printf("Usage: convert2cbmpa [options] [bmp file] [cif file]\n");
    printf("Options:\n");
    printf("-i <json file>  Define an information file.");
    printf("-h              Print this help and exit.\n");
    printf("-v              Verbose mode.\n");
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printHelp();
        return(0);
    }

    int option;
    while((option = getopt(argc, argv, "ihv")) != -1){
        switch(option){
            case 'h':
                printHelp();
                return(0);

            case 'v':
                verbose = true;
                break;

            case 'i':
                info = true;
                break;
        }
    }

    collieBMPA out;
    std::string outComment = "";
    out.animationCount = 0;
    out.gridW = 0;
    out.gridH = 0;
    out.rotatePointX = 0;
    out.rotatePointY = 0;

    if (info){
        if (verbose)
            printf("Using info file: %s\n", argv[optind]);

        std::string infoIn, infoStr;

        std::ifstream infoFile(argv[optind], std::ifstream::in);

        while (getline (infoFile, infoIn)){
            infoStr += infoIn;
        }
        infoFile.close();
        nlohmann::json info = nlohmann::json::parse(infoStr);
        std::string err;
        if(info.contains("comment")){
            outComment = info["comment"].get<std::string>();
        }

        if (info.contains("gridW")){
            out.gridW = info["gridW"].get<unsigned short>();
        }else{
            out.gridW = 0;
        }

        if (info.contains("gridH")){
            out.gridH = info["gridH"].get<unsigned short>();
        }else{
            out.gridH = 0;
        }

        if (info.contains("rotatePointX")){
            out.rotatePointX = info["rotatePointX"].get<unsigned short>();
        }else{
            out.rotatePointX = 0;
        }

        if (info.contains("rotatePointY")){
            out.rotatePointY = info["rotatePointY"].get<unsigned short>();
        }else{
            out.rotatePointY = 0;
        }

        if (info.contains("animations")){
            if (!info["animations"].is_array()){
                printf("Error parsing json - \"animations\" must be an array.");
                exit(0);
            }

            for (int i = 0; i < info["animations"].array().size(); i++){
                if (!info["animations"][i].contains("baseFrame") ||
                    info["animations"][i].contains("startFrame") ||
                    !info["animations"][i].contains("endFrame") ||
                    !info["animations"][i].contains("rate")){
                        printf("Animation at %i is missing a value.", i);
                        exit(0);
                    }

                collieAnimationInfo a;
                a.baseFrame = info["animations"][i]["baseFrame"].get<unsigned short>();
                a.startFrame = info["animations"][i]["startFrame"].get<unsigned short>();
                a.endFrame = info["animations"][i]["endFrame"].get<unsigned short>();
                a.rate = info["animations"][i]["rate"].get<unsigned short>();

                out << a;
            }
        }
        optind ++;
    }

    if (verbose)
        printf("Init image magick\n");

    Magick::InitializeMagick(nullptr);


    try {
        if (verbose)
            printf("Loading image %s\n", argv[optind]);

        Magick::Image image;
        image.read(argv[optind]);

        out.width = image.size().width();
        out.height = image.size().height();

        out.pixels = new collieBMPAPixel[out.width * out.height];
        Magick::PixelPacket p;
        int pTarget;
        for (int y = 0; y < image.size().height(); y++){
            for (int x = 0; x < image.size().width(); x++){
                pTarget = (y * image.size().width()) + x;
                p = image.getPixels(x, y, 1, 1)[0];

                out.pixels[pTarget].r = (unsigned char)((float)((float)p.red / MaxRGB) * UCHAR_MAX);
                out.pixels[pTarget].g = (unsigned char)((float)((float)p.green / MaxRGB) * UCHAR_MAX);
                out.pixels[pTarget].b = (unsigned char)((float)((float)p.blue / MaxRGB) * UCHAR_MAX);
                out.pixels[pTarget].a = (unsigned char)(255 - (int)((float)((float)p.opacity / MaxRGB) * UCHAR_MAX));
            }
        }
    }catch (Magick::Exception &error_){
        printf("Magick++ exception %s\n", error_.what());
        exit(0);
    }

    optind ++;

    if (verbose)
        printf("Writing cbmpa %s\n", argv[optind]);


    std::ofstream outFile(argv[optind], std::ofstream::out);
    outFile.write(outComment.c_str(), outComment.length());
    outFile.write("\0", 1);
    outFile.write((char*)&out.width, sizeof(out.width));
    outFile.write((char*)&out.height, sizeof(out.height));
    outFile.write((char*)&out.gridW, sizeof(out.gridW));
    outFile.write((char*)&out.gridH, sizeof(out.gridH));
    outFile.write((char*)&out.rotatePointX, sizeof(out.rotatePointX));
    outFile.write((char*)&out.rotatePointY, sizeof(out.rotatePointY));
    int pTarget;
    for (int y = 0; y < out.height; y++){
        for (int x = 0; x < out.width; x++){
            pTarget = (y * out.width) + x;
            outFile.write((char*)&out.pixels[pTarget].r, sizeof(unsigned char));
            outFile.write((char*)&out.pixels[pTarget].g, sizeof(unsigned char));
            outFile.write((char*)&out.pixels[pTarget].b, sizeof(unsigned char));
            outFile.write((char*)&out.pixels[pTarget].a, sizeof(unsigned char));
        }
    }

    outFile.write((char*)&out.animationCount, sizeof(out.animationCount));

    for (int i = 0; i < out.animationCount; i++){
        outFile.write((char*)&out.animations[i].baseFrame, sizeof(unsigned short));
        outFile.write((char*)&out.animations[i].startFrame, sizeof(unsigned short));
        outFile.write((char*)&out.animations[i].endFrame, sizeof(unsigned short));
        outFile.write((char*)&out.animations[i].rate, sizeof(unsigned short));
    }

    outFile.close();

    return 0;
}
