#include <cassert>
#include <cmath>
#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <math.h>
#include <time.h>
#include <utility>
#include <algorithm>
#include <vector>

#define PI 3.14159265


using namespace cv;

#include "SWTTextDetection.h"
using namespace SWT;


namespace DetectText {

    void SWTFirstPass (Mat edgeImage, Mat gradientX, Mat gradientY, bool dark_on_light, Mat & SWTImage, std::vector<Ray> & rays) {
        for( int row = 0; row < edgeImage.rows; row++ ){
            float* ptr = (float*)SWTImage.ptr(row);
            for ( int col = 0; col < edgeImage.cols; col++ ){
                *ptr++ = -1;
            }
        }

        for( int row = 0; row < edgeImage.rows; row++ ){
            for ( int col = 0; col < edgeImage.cols; col++ ){
                uchar canny = edgeImage.at<uchar>(row, col);
                if (canny == 0) continue;

                float dx = gradientX.at<float>(row, col);
                float dy = gradientY.at<float>(row, col);
                float mag = sqrt(dx * dx + dy * dy);
                dx = dx / mag;
                dy = dy / mag;

                if (dark_on_light){
                    dx = -dx;
                    dy = -dy;
                } 

                Ray ray;
                SWTPoint p;
                p.x = row;
                p.y = col;
                ray.p = p;
                std::vector<SWTPoint> points;
                points.push_back(p);
                float curPosX = (float) row + 0.5;
                float curPosY = (float) col + 0.5;
                int curPixX = row;
                int curPixY = col;

                float inc = 0.05;
                while(true) {
                    curPosX += inc * dx;
                    curPosY += inc * dy;
                    if ((int)(floor(curPosX)) != curPixX || (int)(floor(curPosY)) != curPixY) {
                        curPixX = (int)(floor(curPosX));
                        curPixY = (int)(floor(curPosY));
                        if (curPixX < 0 || (curPixX >= SWTImage.cols) || curPixY < 0 || (curPixY >= SWTImage.rows)) {
                            break;
                        }
                        SWTPoint pt;
                        pt.x = curPixX;
                        pt.y = curPixY;
                        points.push_back(pt);
                        if (edgeImage.at<uchar>(curPixY, curPixX) > 0) {
                            ray.q = pt;
                            float G_xt = gradientX.at<float>(curPixY,curPixX);
                            float G_yt = gradientY.at<float>(curPixY,curPixX);
                            mag = sqrt( (G_xt * G_xt) + (G_yt * G_yt) );
                            if (dark_on_light){
                                G_xt = -G_xt;
                                G_yt = -G_yt;
                            }

                            if (acos(dx * G_xt + dy * G_yt) < PI/2.0 ) {
                                float length = sqrt( ((float)ray.q.x - (float)ray.p.x)*((float)ray.q.x - (float)ray.p.x) + ((float)ray.q.y - (float)ray.p.y)*((float)ray.q.y - (float)ray.p.y));
                                for (std::vector<SWTPoint>::iterator pit = points.begin(); pit != points.end(); pit++) {
                                    if (SWTImage.at<float>(pit->y, pit->x) < 0) {
                                        SWTImage.at<float>(pit->y, pit->x) = length;
                                    } else {
                                        SWTImage.at<float>(pit->y, pit->x) = std::min(length, SWTImage.at<float>(pit->y, pit->x));
                                    }
                                }
                                ray.points = points;
                                rays.push_back(ray);
                            }
                            break; 
                        }
                    }
                }
                 
            }
        }
        for (int i = 0; i < 10000; i++) {
            std::cout << rays.size() << " rays were found" << std::endl;
        }
        namedWindow( "Grayscale Image", WINDOW_AUTOSIZE ); 
        imshow( "Grayscale Image", edgeImage );  
        waitKey(0);
    }
    bool sortBySWT (const SWTPoint &lhs, const SWTPoint &rhs) {
        return lhs.SWT < rhs.SWT;
    }


    void SWTSecondPass (Mat & SWTImage, std::vector<Ray> & rays) {
        for (std::vector<Ray>::iterator rit = rays.begin(); rit != rays.end(); rit++) {
            for (std::vector<SWTPoint>::iterator pit = rit->points.begin(); pit != rit->points.end(); pit++) {
                pit->SWT = SWTImage.at<float>(pit->y, pit->x);
            }
            std::sort(rit->points.begin(), rit->points.end(), sortBySWT);
            float median = (rit -> points[rit -> points.size()/2]).SWT;
            for (std::vector<SWTPoint>::iterator pit = rit->points.begin(); pit != rit->points.end(); pit++) {
                SWTImage.at<float>(pit->y, pit->x) = std::min(pit->SWT, median);
            }
            
        }
    }

    Mat textDetection (const Mat& input_image, bool dark_on_light) {
        assert (input_image.depth() == CV_8U);
        assert (input_image.channels() == 3);

        // Convert to grayscale
        Mat grayImage(input_image.size(), CV_8UC1);
        cvtColor(input_image, grayImage, CV_RGB2GRAY);
        // Create Canny Image
        double threshold_low = 175;
        double threshold_high = 320;
        Mat canny_edge_image(input_image.size(), CV_8UC1);
        Canny(grayImage, canny_edge_image, threshold_low, threshold_high, 3) ;
        // imwrite ( "canny.png", edgeImage);
        // namedWindow( "Canny Edges", WINDOW_AUTOSIZE ); 
        // imshow( "Canny Edges", canny_edge_image );  
        // waitKey(0);

        // Create gradient X, gradient Y
        Mat gaussianImage( input_image.size(), CV_32FC1);
        grayImage.convertTo(gaussianImage, CV_32FC1, 1./255.);
        // namedWindow( "Grayscale Image", WINDOW_AUTOSIZE ); 
        // imshow( "Grayscale Image", gaussianImage );  
        // waitKey(0);
        
        Mat gradientX( input_image.size(), CV_32FC1 );
        Mat gradientY( input_image.size(), CV_32FC1 );
        GaussianBlur(gaussianImage, gaussianImage, Size(5, 5), 0);
        Scharr(gaussianImage, gradientX, -1, 1, 0);
        Scharr(gaussianImage, gradientY, -1, 0, 1);
        GaussianBlur(gradientX, gradientX, Size(3, 3), 0);
        GaussianBlur(gradientY, gradientY, Size(3, 3), 0);
        // namedWindow( "Blurred Grayscale Image", WINDOW_AUTOSIZE ); 
        // imshow( "Blurred Grayscale Image", gaussianImage );  
        // waitKey(0);
        // Calculate SWT and return ray vectors
        std::vector<Ray> rays;
        Mat SWTImage( input_image.size(), CV_32FC1 );
        
        SWTFirstPass (canny_edge_image, gradientX, gradientY, dark_on_light, SWTImage, rays );
        SWTSecondPass ( SWTImage, rays );

    }
}

int main() {
    string imagePath = "/home/opencv-dev/Desktop/bottle.jpeg";
    cv::Mat image = imread(imagePath);
    DetectText::textDetection(image, 0);
    cvDestroyAllWindows();
    return 0;
}
