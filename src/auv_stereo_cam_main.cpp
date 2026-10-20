#include "rclcpp/rclcpp.hpp"
#include "auv_stereo_cam/auv_stereo_cam_pub.hpp"

int main (int argc, char * argv[]){
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<auv_stereo_cam::StereoCamPub>());
    rclcpp::shutdown();
    return 0;
}