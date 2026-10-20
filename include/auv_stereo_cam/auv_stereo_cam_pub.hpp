#ifndef AUV_STEREO_CAM_PUB_HPP
#define AUV_STEREO_CAM_PUB_HPP

#include "auv_stereo_cam/auv_stereo_cam_extract.hpp"

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "cv_bridge/cv_bridge.hpp"
#include <opencv2/opencv.hpp>
#include "image_transport/image_transport.hpp"

using namespace std::chrono_literals;

namespace auv_stereo_cam{
    class StereoCamPub : public rclcpp::Node{
        public:
            explicit StereoCamPub(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());
            virtual ~StereoCamPub();
        private:
            void capture_loop();

            std::string camera_path_;
            int frame_width_;
            int frame_height_;
            int frame_fps_;
            bool stereo_camera_;
            bool auto_exposure_;
            int manual_exposure_unit_;

            image_transport::Publisher left_cam_pub_;
            image_transport::Publisher right_cam_pub_;
            image_transport::Publisher single_cam_pub_;

            cv::VideoCapture cap_;
            std::thread capture_thread_;
            std::atomic<bool> is_running_{false};

    };
}

#endif