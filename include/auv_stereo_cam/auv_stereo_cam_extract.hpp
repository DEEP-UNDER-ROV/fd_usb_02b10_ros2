#ifndef AUV_STEREO_CAM_EXTRACT_HPP
#define AUV_STEREO_CAM_EXTRACT_HPP

#include <opencv2/opencv.hpp>
#include <string>

int CameraSetup(cv::VideoCapture& cap, const std::string& cameraPath, int stereoCamera, int frameWidth, int frameHeight, int frameFPS, bool autoExposure, int manualExposureUnit);

int ExtractCameraFeed(cv::VideoCapture& cap, bool stereoCamera, cv::Mat& singleCamera, cv::Mat& leftCamera, cv::Mat& rightCamera, int frameWidth, int frameHeight);

#endif