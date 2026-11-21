#ifndef FD_USB_02B10_PUB_HPP
#define FD_USB_02B10_PUB_HPP

#include "fd_usb_02b10_ros2/fd_usb_02b10_extract.hpp"

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "cv_bridge/cv_bridge.hpp"
#include <opencv2/opencv.hpp>
#include "image_transport/image_transport.hpp"

using namespace std::chrono_literals;

namespace fd_usb_02b10_ros2 {
    class FDUSB02B10Pub : public rclcpp::Node {
        public:
            explicit FDUSB02B10Pub(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());
            virtual ~FDUSB02B10Pub();
        private:
            void capture_loop();

            std::string camera_path_;
            int frame_width_;
            int frame_height_;
            int frame_fps_;
            bool stereo_camera_;
            bool auto_exposure_;
            int manual_exposure_unit_;
            bool camera_flip_;

            image_transport::Publisher left_cam_pub_;
            image_transport::Publisher right_cam_pub_;
            image_transport::Publisher single_cam_pub_;

            cv::VideoCapture cap_;
            std::thread capture_thread_;
            std::atomic<bool> is_running_{false};
    };
}

#endif