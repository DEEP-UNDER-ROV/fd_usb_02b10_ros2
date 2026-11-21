#include "fd_usb_02b10_ros2/fd_usb_02b10_pub.hpp"

namespace fd_usb_02b10_ros2 {
  FDUSB02B10Pub::FDUSB02B10Pub(const rclcpp::NodeOptions & options) : Node("fd_usb_02b10_pub", options) {

    camera_path_ = this->declare_parameter<std::string>("camera_path", "/dev/fd-usb-02b10");
    frame_width_ = this->declare_parameter<int>("camera_width", 1280);
    frame_height_ = this->declare_parameter<int>("camera_height", 720);
    frame_fps_ = this->declare_parameter<int>("camera_fps", 30);
    stereo_camera_ = this->declare_parameter<bool>("stereo_camera", true);
    auto_exposure_ = this->declare_parameter<bool>("auto_exposure", true);
    manual_exposure_unit_ = this->declare_parameter<int>("manual_exposure_unit", 333);
    camera_flip_ = this->declare_parameter<bool>("camera_flip", false);

    left_cam_pub_ = image_transport::create_publisher(this, "stereo_cam/left_cam", rmw_qos_profile_sensor_data);
    right_cam_pub_ = image_transport::create_publisher(this, "stereo_cam/right_cam", rmw_qos_profile_sensor_data);
    single_cam_pub_ = image_transport::create_publisher(this, "single_cam/feed", rmw_qos_profile_sensor_data);

    is_running_ = true;

    while (CameraSetup(cap_, camera_path_, stereo_camera_, frame_width_, frame_height_, frame_fps_, auto_exposure_, manual_exposure_unit_) && rclcpp::ok() && is_running_) {
      RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 2000, "ERROR : Camera Not FOUND!!! Waiting for Camera...");
    }

    RCLCPP_INFO(this->get_logger(), "Camera Found!!! FD-USB-02B10 Camera Publisher Started!!!");
    
    if (stereo_camera_) RCLCPP_INFO(this->get_logger(), "Stereo Camera is selected (Resolution : %dx%d; MAX FPS : %d)", frame_width_, frame_height_, (frame_fps_ <= 30) ? frame_fps_ : 30);
    else RCLCPP_INFO(this->get_logger(), "Single Camera is selected (Resolution : %dx%d; MAX FPS : %d)", frame_width_, frame_height_, (frame_fps_ <= 30) ? frame_fps_ : 30);

    if (auto_exposure_) RCLCPP_INFO(this->get_logger(), "Auto Exposure is selected");
    else RCLCPP_INFO(this->get_logger(), "Manual Exposure is selected (Manual Exposure Unit : %d)", manual_exposure_unit_);

    capture_thread_ = std::thread(&FDUSB02B10Pub::capture_loop, this);
  }

  FDUSB02B10Pub::~FDUSB02B10Pub() {
    is_running_ = false;
    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }
    if (cap_.isOpened()) {
        cap_.release();
    }
  }
  
  void FDUSB02B10Pub::capture_loop() {
    cv::Mat singleCamera, leftCamera, rightCamera;

    while (rclcpp::ok() && is_running_) {

      int frameLoss = 0;

      while (ExtractCameraFeed(cap_, stereo_camera_, singleCamera, leftCamera, rightCamera, frame_width_, frame_height_, camera_flip_) && rclcpp::ok() && is_running_) {
        RCLCPP_ERROR(this->get_logger(), "ERROR : Frame is Empty!!!"); 
        rclcpp::sleep_for(100ms);
        if (++frameLoss > 10) {
          cap_.release();
          while (CameraSetup(cap_, camera_path_, stereo_camera_, frame_width_, frame_height_, frame_fps_, auto_exposure_, manual_exposure_unit_) && rclcpp::ok() && is_running_) {
            RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 2000, "ERROR : Camera Loss!!! Waiting for Camera...");
          }
          RCLCPP_INFO(this->get_logger(), "Camera Found!!! FD-USB-02B10 Camera Publisher Continued!!!");
        }
      }

      rclcpp::Time now = this->get_clock()->now();

      std_msgs::msg::Header singleCameraHeader, leftCameraHeader, rightCameraHeader;

      singleCameraHeader.stamp = now;
      leftCameraHeader.stamp = now;
      rightCameraHeader.stamp = now;

      singleCameraHeader.frame_id = "single_camera";
      leftCameraHeader.frame_id = "left_camera";
      rightCameraHeader.frame_id = "right_camera";

      sensor_msgs::msg::Image::SharedPtr singleCameraMsg = cv_bridge::CvImage(singleCameraHeader, "bgr8", singleCamera).toImageMsg();
      sensor_msgs::msg::Image::SharedPtr leftCameraMsg = cv_bridge::CvImage(leftCameraHeader, "bgr8", leftCamera).toImageMsg();
      sensor_msgs::msg::Image::SharedPtr rightCameraMsg = cv_bridge::CvImage(rightCameraHeader, "bgr8", rightCamera).toImageMsg();

      single_cam_pub_.publish(singleCameraMsg);
      left_cam_pub_.publish(leftCameraMsg);
      right_cam_pub_.publish(rightCameraMsg);
    }
  }
}