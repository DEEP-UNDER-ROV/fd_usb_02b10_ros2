#include "fd_usb_02b10_ros2/fd_usb_02b10_pub.hpp"

namespace fd_usb_02b10_ros2 {

  namespace {
    constexpr std::array<ResolutionSpec, 10> kSupportedResolutions = {{
      {"1920x1520", 1920, 1520, true,  true},   // 1
      {"1920x1512", 1920, 1512, true,  false},  // 2 (Mono only)
      {"1920x1080", 1920, 1080, true,  true},   // 3
      {"1920x1072", 1920, 1072, true,  false},  // 4 (Mono only)
      {"1520x1520", 1520, 1520, false, true},   // 5 (Stereo only)
      {"1080x1080", 1080, 1080, false, true},   // 6 (Stereo only)
      {"1280x720",  1280, 720,  true,  true},   // 7 (Default)
      {"1280x712",  1280, 712,  true,  false},  // 8 (Mono only)
      {"640x480",   640,  480,  true,  true},   // 9
      {"640x472",   640,  472,  true,  false}   // 10 (Mono only)
    }};

    constexpr const char * kResolutionConstraints = "1: 1920x1520 (Mono/Stereo), 2: 1920x1512 (Mono), 3: 1920x1080 (Mono/Stereo), 4: 1920x1072 (Mono), 5: 1520x1520 (Stereo), 6: 1080x1080 (Stereo), 7: 1280x720 (Mono/Stereo), 8: 1280x712 (Mono), 9: 640x480 (Mono/Stereo), 10: 640x472 (Mono). Mono-only and stereo-only modes override stereo_camera.";

    bool resolve_effective_stereo(const ResolutionSpec & spec, bool requested_stereo) {
      if (spec.supports_stereo && !spec.supports_mono) return true;
      if (spec.supports_mono && !spec.supports_stereo) return false;
      return requested_stereo;
    }
  }

  FDUSB02B10Pub::FDUSB02B10Pub(const rclcpp::NodeOptions & options) : Node("fd_usb_02b10_pub", options) {

    // Lamda Function to Help Writing Parameter Descriptor
    auto make_desc = [](const std::string & text,
                        const std::string & constraints = "",
                        bool read_only = false) {
      rcl_interfaces::msg::ParameterDescriptor desc;
      desc.description = text;
      desc.additional_constraints = constraints;
      desc.read_only = read_only;
      return desc;
    };

    camera_path_ = this->declare_parameter<std::string>("camera_path", "/dev/fd-usb-02b10", make_desc("V4L2 device path or persistent udev symlink."));
    
    auto res_desc = make_desc("Single-eye resolution preset index (1-10).", kResolutionConstraints);
    rcl_interfaces::msg::IntegerRange res_range;
    res_range.from_value = 1;
    res_range.to_value = 10;
    res_range.step = 1;
    res_desc.integer_range.push_back(res_range);
    camera_resolution_ = this->declare_parameter<int>("camera_resolution", 7, res_desc);

    if (camera_resolution_ < 1 || camera_resolution_ > 10) {
      RCLCPP_WARN(this->get_logger(), "Invalid startup camera_resolution %d. Falling back to mode 7 (1280x720).", camera_resolution_);
      camera_resolution_ = 7;
    }
    initial_spec_ = kSupportedResolutions[camera_resolution_ - 1];

    requested_stereo_ = this->declare_parameter<bool>("stereo_camera", true,make_desc("Preferred capture mode (true = stereo, false = mono) when supported by the selected resolution."));

    stereo_camera_ = resolve_effective_stereo(initial_spec_, requested_stereo_);
    if (requested_stereo_ != stereo_camera_) {
      RCLCPP_INFO(
        this->get_logger(),
        "Resolution #%d (%s) only supports %s mode; automatically using %s regardless of stereo_camera:=%s.",
        camera_resolution_, initial_spec_.label,
        stereo_camera_ ? "Stereo" : "Mono",
        stereo_camera_ ? "Stereo" : "Mono",
        requested_stereo_ ? "true" : "false");
    }
    
    auto fps_desc = make_desc("Requested capture frame rate (FPS).", "Supports 1, 3, 5, 10, 15, 20, 25, 30 FPS on high-res stereo modes; fixed at 30 FPS on all other modes.");
    rcl_interfaces::msg::IntegerRange fps_range;
    fps_range.from_value = 1;
    fps_range.to_value = 30;
    fps_range.step = 1;
    fps_desc.integer_range.push_back(fps_range);
    frame_fps_ = this->declare_parameter<int>("camera_fps", 30, fps_desc);
    if (frame_fps_ > 30) frame_fps_ = 30;

    auto_exposure_ = this->declare_parameter<bool>("auto_exposure", true, make_desc("True for V4L2 auto exposure; false for manual exposure."));

    auto exp_desc = make_desc("Manual exposure time in units of 1e-4 seconds (used when auto_exposure is false).", "Keep <= (10000 / camera_fps) to maintain target FPS (e.g., <= 333 at 30 FPS).");
    rcl_interfaces::msg::IntegerRange exp_range;
    exp_range.from_value = 3;
    exp_range.to_value = 2047;
    exp_range.step = 1;
    exp_desc.integer_range.push_back(exp_range);
    manual_exposure_unit_ = this->declare_parameter<int>("manual_exposure_unit", 333, exp_desc);

    camera_flip_ = this->declare_parameter<bool>("camera_flip", false, make_desc("Rotate output image 180 degrees immediately without re-initializing the camera."));

    rcl_interfaces::msg::ParameterDescriptor qos_desc;
    qos_desc.description = "Use Reliable QoS instead of Best Effort (SensorDataQoS). Set at startup only.";
    qos_desc.read_only = true;
    bool reliable_qos = this->declare_parameter<bool>("reliable_qos", false, qos_desc);

    rmw_qos_profile_t qos_profile = rmw_qos_profile_sensor_data;
    if (reliable_qos) {
      qos_profile.reliability = RMW_QOS_POLICY_RELIABILITY_RELIABLE;
      qos_profile.depth = 10;
    }

    param_callback_handle_ = this->add_on_set_parameters_callback(std::bind(&FDUSB02B10Pub::on_parameter_change, this, std::placeholders::_1));

    left_cam_pub_ = image_transport::create_publisher(this, "stereo_cam/left_cam", qos_profile);
    right_cam_pub_ = image_transport::create_publisher(this, "stereo_cam/right_cam", qos_profile);
    single_cam_pub_ = image_transport::create_publisher(this, "single_cam/feed", qos_profile);

    is_running_ = true;

    while (CameraSetup(cap_, camera_path_, stereo_camera_, initial_spec_.width, initial_spec_.height, frame_fps_, auto_exposure_, manual_exposure_unit_) && rclcpp::ok() && is_running_) {
      RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 2000, "ERROR : Camera Not FOUND!!! Waiting for Camera...");
    }

    RCLCPP_INFO(this->get_logger(), "Camera Found!!! FD-USB-02B10 Camera Publisher Started!!!");
    
    if (stereo_camera_) RCLCPP_INFO(this->get_logger(), "Stereo Camera is selected (Resolution : %s; MAX FPS : %d)", initial_spec_.label, frame_fps_);
    else RCLCPP_INFO(this->get_logger(), "Single Camera is selected (Resolution : %s; MAX FPS : %d)", initial_spec_.label, frame_fps_);

    if (auto_exposure_) RCLCPP_INFO(this->get_logger(), "Auto Exposure is selected");
    else RCLCPP_INFO(this->get_logger(), "Manual Exposure is selected (Manual Exposure Unit : %d)", manual_exposure_unit_);

    RCLCPP_INFO(this->get_logger(), "QoS Reliability : %s", reliable_qos ? "Reliable" : "Best Effort (SensorDataQoS)");

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

  rcl_interfaces::msg::SetParametersResult FDUSB02B10Pub::on_parameter_change(const std::vector<rclcpp::Parameter> & parameters) {
    rcl_interfaces::msg::SetParametersResult result;
    result.successful = true;

    std::lock_guard<std::mutex> lock(param_mutex_);

    std::string new_path = camera_path_;
    ResolutionSpec new_spec = initial_spec_;
    int new_fps = frame_fps_;
    bool new_stereo = requested_stereo_;
    bool new_auto_exp = auto_exposure_;
    int new_manual_exp = manual_exposure_unit_;
    bool new_flip = camera_flip_;
    bool needs_camera_reinit = false;

    for (const auto & param : parameters) {
      const auto & name = param.get_name();

      if (name == "camera_path") {
        if (param.as_string().empty()) {
          result.successful = false;
          result.reason = "camera_path cannot be empty";
          return result;
        }
        new_path = param.as_string();
        needs_camera_reinit = true;
      } else if (name == "camera_resolution") {
        if (param.as_int() < 1 || param.as_int() > 10) {
          result.successful = false;
          result.reason = "Invalid startup camera_resolution; camera_resolution must be from 1 to 10";
          return result;
        }
        new_spec = kSupportedResolutions[static_cast<int>(param.as_int()) - 1];
        needs_camera_reinit = true;
        stereo_camera_ = resolve_effective_stereo(new_spec, new_stereo);
        camera_resolution_ = param.as_int();
        if (new_stereo!= stereo_camera_){
          RCLCPP_INFO(
          this->get_logger(),
          "Resolution #%d (%s) only supports %s mode; automatically using %s regardless of stereo_camera:=%s.",
          camera_resolution_, initial_spec_.label,
          stereo_camera_ ? "Stereo" : "Mono",
          stereo_camera_ ? "Stereo" : "Mono",
          new_stereo ? "true" : "false");
        }
      } else if (name == "camera_fps") {
        if (param.as_int() < 1 || param.as_int() > 30) {
          result.successful = false;
          result.reason = "camera_fps must be from 1 to 30";
          return result;
        }
        new_fps = static_cast<int>(param.as_int());
        needs_camera_reinit = true;
      } else if (name == "stereo_camera") {
        new_stereo = param.as_bool();
        stereo_camera_ = resolve_effective_stereo(initial_spec_, new_stereo);
        if (new_stereo!= stereo_camera_){
          RCLCPP_INFO(
          this->get_logger(),
          "Resolution #%d (%s) only supports %s mode; automatically using %s regardless of stereo_camera:=%s.",
          camera_resolution_, initial_spec_.label,
          stereo_camera_ ? "Stereo" : "Mono",
          stereo_camera_ ? "Stereo" : "Mono",
          new_stereo ? "true" : "false");
        }else needs_camera_reinit = true;
      } else if (name == "auto_exposure") {
        new_auto_exp = param.as_bool();
        needs_camera_reinit = true;
      } else if (name == "manual_exposure_unit") {
        if (param.as_int() < 3 || param.as_int() > 2047) {
          result.successful = false;
          result.reason = "manual_exposure_unit must be between 3 and 2047";
          return result;
        }
        new_manual_exp = static_cast<int>(param.as_int());
        needs_camera_reinit = true;
      } else if (name == "camera_flip") {
        new_flip = param.as_bool();
      }
    }

    camera_path_ = new_path;
    initial_spec_ = new_spec;
    frame_fps_ = new_fps;
    auto_exposure_ = new_auto_exp;
    manual_exposure_unit_ = new_manual_exp;
    camera_flip_ = new_flip;
    requested_stereo_ = new_stereo;

    if (needs_camera_reinit) {
      reinit_required_.store(true);
      RCLCPP_INFO(this->get_logger(), "Parameter change detected. Scheduling camera re-initialization...");
    }

    return result;
  }
  
  void FDUSB02B10Pub::capture_loop() {
    cv::Mat singleCamera, leftCamera, rightCamera;

    while (rclcpp::ok() && is_running_) {

      std::string path;
      ResolutionSpec spec;
      int fps, manual_exp;
      bool stereo, auto_exp, flip;
      {
        std::lock_guard<std::mutex> lock(param_mutex_);
        path = camera_path_;
        spec = initial_spec_;
        fps = frame_fps_;
        stereo = stereo_camera_;
        auto_exp = auto_exposure_;
        manual_exp = manual_exposure_unit_;
        flip = camera_flip_;
      }

      if (reinit_required_.exchange(false)) {
        RCLCPP_INFO(this->get_logger(), "Re-initializing camera (%s | %s @ %d FPS | %s | %s)...", path.c_str(), spec.label, fps, stereo ? "Stereo" : "Single", auto_exp ? "Auto Exposure" : "Manual Exposure");

        if (cap_.isOpened()) {
          cap_.release();
        }

        while (CameraSetup(cap_, path, stereo, spec.width, spec.height, fps, auto_exp, manual_exp) && rclcpp::ok() && is_running_) {
          RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 2000, "ERROR : Camera Re-init Failed! Waiting for Camera...");
          std::lock_guard<std::mutex> lock(param_mutex_);
          path = camera_path_;
          spec = initial_spec_;
          fps = frame_fps_;
          stereo = stereo_camera_;
          auto_exp = auto_exposure_;
          manual_exp = manual_exposure_unit_;
        }

        if (!rclcpp::ok() || !is_running_) break;
        RCLCPP_INFO(this->get_logger(), "Camera Re-initialized Successfully!!!");
      }

      int frameLoss = 0;

      while (ExtractCameraFeed(cap_, stereo, singleCamera, leftCamera, rightCamera, initial_spec_.width, initial_spec_.height, flip) && rclcpp::ok() && is_running_ && !reinit_required_.load()) {
        RCLCPP_ERROR(this->get_logger(), "ERROR : Frame is Empty!!!"); 
        rclcpp::sleep_for(100ms);
        if (++frameLoss > 10) {
          cap_.release();
          while (CameraSetup(cap_, path, stereo, initial_spec_.width, initial_spec_.height, fps, auto_exp, manual_exp) && rclcpp::ok() && is_running_ && !reinit_required_.load()) {
            RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 2000, "ERROR : Camera Loss!!! Waiting for Camera...");
          }
          if (cap_.isOpened()) RCLCPP_INFO(this->get_logger(), "Camera Found!!! FD-USB-02B10 Camera Publisher Continued!!!");
        }
      }

      if (reinit_required_.load()) {
        continue;
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