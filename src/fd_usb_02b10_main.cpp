#include "rclcpp/rclcpp.hpp"
#include "fd_usb_02b10_ros2/fd_usb_02b10_pub.hpp"

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<fd_usb_02b10_ros2::FDUSB02B10Pub>());
    rclcpp::shutdown();
    return 0;
}