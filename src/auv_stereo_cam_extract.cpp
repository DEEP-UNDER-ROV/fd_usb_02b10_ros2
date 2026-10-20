#include "auv_stereo_cam/auv_stereo_cam_extract.hpp"

int CameraSetup(cv::VideoCapture& cap, const std::string& cameraPath, int stereoCamera, int frameWidth, int frameHeight, int frameFPS, bool autoExposure, int manualExposureUnit){
    cap.open(cameraPath, cv::CAP_V4L2);

    if (!cap.isOpened()) return 1;

    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));

    cap.set(cv::CAP_PROP_FRAME_WIDTH, frameWidth*(stereoCamera+1));
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, frameHeight);
    cap.set(cv::CAP_PROP_FPS, frameFPS);

    if (autoExposure) cap.set(cv::CAP_PROP_AUTO_EXPOSURE, 3);
    else {
        cap.set(cv::CAP_PROP_AUTO_EXPOSURE, 1);
        cap.set(cv::CAP_PROP_EXPOSURE, manualExposureUnit);
    }

    return 0;
}


int ExtractCameraFeed(cv::VideoCapture& cap, bool stereoCamera, cv::Mat& singleCamera, cv::Mat& leftCamera, cv::Mat& rightCamera, int frameWidth, int frameHeight, bool cameraFlip){
    cv::Mat frame;
    cap.read(frame);

    if(frame.empty()) return 1;

    if(cameraFlip) cv::flip(frame, frame, -1);

    singleCamera = frame;
    if (stereoCamera){
        leftCamera = frame(cv::Rect(0, 0, frameWidth, frameHeight));
        rightCamera = frame(cv::Rect(frameWidth, 0, frameWidth, frameHeight));
    } else{
        leftCamera = frame;
        rightCamera = frame;
    }

    return 0;
}