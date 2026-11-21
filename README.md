# FD-USB-02B10 Underwater Binocular USB Camera ROS2 Wrapper

ROS 2 C++ driver and image publisher for the **FD-USB-02B10 Underwater Binocular USB Camera**. Captures hardware-compressed MJPEG streams via V4L2, splits side-by-side stereo frames into synchronized left and right topics, and supports live parameter reconfiguration and automatic USB reconnection at runtime.

---

## Features

* **Stereo & Mono Modes:** Automatically configures double-width V4L2 capture in stereo mode and splits frames into synchronized left and right image streams.
* **`image_transport` Support:** Publishes using `image_transport` with `SensorDataQoS` (`best_effort`), enabling compressed/theora transport plugins out of the box.
* **Dynamic Runtime Reconfiguration:** Change resolution, FPS, exposure mode, manual exposure time, or image flip on the fly via `ros2 param set` or `rqt_reconfigure` without restarting the node.
* **Automatic Camera Recovery:** Detects empty frames or USB disconnects and automatically re-establishes the V4L2 stream once the device reconnects.

---

## Dependencies

* **ROS 2** (Humble, Jazzy, etc)
* `rclcpp`
* `sensor_msgs`
* `cv_bridge`
* `image_transport`
* `OpenCV` (`libopencv-dev`)

---

## Installation & Build

1. **Clone the repository into your ROS 2 workspace:**
   ```bash
   cd ~/ros2_ws/src
   git clone https://github.com/DEEP-UNDER-ROV/fd_usb_02b10_ros2.git
   ```

2. **Install ROS dependencies:**
   ```bash
   cd ~/ros2_ws
   rosdep install --from-paths src --ignore-src -r -y
   ```

3. **Build the package:**
   ```bash
   colcon build --packages-select fd_usb_02b10_ros2
   source install/setup.bash
   ```

---

## Camera udev Rules Setup

To grant non-root `video` group permissions and bind the camera (`1bcf:0b15`) to a persistent `/dev/fd-usb-02b10` symlink:

**Option 1: Using ROS 2 (after `colcon build`)**
```bash
ros2 run fd_usb_02b10_ros2 install_udev.sh
```

**Option 2: Directly from the repository root (before building or on a Docker host)**
```bash
chmod +x scripts/install_udev.sh && ./scripts/install_udev.sh
```

Unplug and replug the camera after running the script. If the script added your user to the `video` group for the first time, log out and log back in (or reboot) for group permissions to take effect.

---

## Supported Resolutions

The table below lists the supported resolutions for the FD-USB-02B10 sensor in **Mono** (`stereo_camera:=false`) and **Stereo** (`stereo_camera:=true`) modes:

| # | Resolution | Mono | Stereo | Aspect Ratio / Notes |
| :- | :--- | :---: | :---: | :--- |
| 1 | **1920x1520** | `[x]` | `[x]` | $\sim 5:4$ (Full-height) |
| 2 | **1920x1512** | `[x]` | `[ ]` | 8px-aligned full-height |
| 3 | **1920x1080** | `[x]` | `[x]` | 16:9 (Full HD) |
| 4 | **1920x1072** | `[x]` | `[ ]` | 16px-aligned 1080p |
| 5 | **1520x1520** | `[ ]` | `[x]` | 1:1 (Square / VR180) |
| 6 | **1080x1080** | `[ ]` | `[x]` | 1:1 (Square / VR180) |
| 7 | **1280x720** | `[x]` | `[x]` | 16:9 (HD) |
| 8 | **1280x712** | `[x]` | `[ ]` | 8px-aligned 720p |
| 9 | **640x480** | `[x]` | `[x]` | 4:3 (VGA) |
| 10 | **640x472** | `[x]` | `[ ]` | 8px-aligned VGA |

> **Note on Stereo Width:** `camera_width` and `camera_height` always specify the **single-eye (per-sensor)** resolution. When `stereo_camera` is `true`, the driver automatically requests $2 \times \text{camera\_width}$ from V4L2 and crops the left and right halves. For example, setting `camera_width:=1280` and `camera_height:=720` in stereo mode captures a `2560x720` combined frame and publishes two `1280x720` images.

---

## Usage

### Run with Default Parameters (`1280x720` Stereo @ 30 FPS)
```bash
ros2 run fd_usb_02b10_ros2 fd_usb_02b10_ros2
```

### Run with Custom Startup Parameters
```bash
ros2 run fd_usb_02b10_ros2 fd_usb_02b10_ros2 --ros-args \
  -p camera_path:=/dev/fd-usb-02b10 \
  -p camera_width:=640 \
  -p camera_height:=480 \
  -p camera_fps:=30 \
  -p stereo_camera:=true \
  -p auto_exposure:=false \
  -p manual_exposure_unit:=100
```

### Modify Parameters at Runtime
You can update any parameter while the node is running:

```bash
# Switch to manual exposure (10.0 ms shutter time)
ros2 param set /fd_usb_02b10_pub auto_exposure false
ros2 param set /fd_usb_02b10_pub manual_exposure_unit 100

# Flip the image 180 degrees immediately (no stream interruption)
ros2 param set /fd_usb_02b10_pub camera_flip true

# Change the camera resolution (two ros parameter simultaneously)
ros2 param load /fd_usb_02b10_pub <(printf "/fd_usb_02b10_pub:\n  ros__parameters:\n    camera_width: 640\n    camera_height: 480\n")
```

Or adjust parameters interactively using `rqt_reconfigure`:
```bash
ros2 run rqt_reconfigure rqt_reconfigure
```

---

## ROS 2 Parameters

| Parameter | Type | Default | Runtime Behavior | Description |
| :--- | :--- | :--- | :--- | :--- |
| `camera_path` | `string` | `"/dev/fd-usb-02b10"` | Re-initializes camera | V4L2 device path or udev symlink (e.g., `/dev/video0`). |
| `camera_width` | `int` | `1280` | Re-initializes camera | Horizontal resolution in pixels (per eye in stereo mode). |
| `camera_height` | `int` | `720` | Re-initializes camera | Vertical resolution in pixels. |
| `camera_fps` | `int` | `30` | Re-initializes camera | Requested capture frame rate in frames per second. |
| `stereo_camera` | `bool` | `true` | Re-initializes camera | `true` for side-by-side stereo splitting; `false` for mono mode. |
| `auto_exposure` | `bool` | `true` | Re-initializes camera | `true` for V4L2 aperture-priority auto exposure; `false` for manual exposure. |
| `manual_exposure_unit` | `int` | `333` | Re-initializes camera | Manual exposure time in units of $10^{-4}\text{ s}$ (used when `auto_exposure` is `false`). |
| `camera_flip` | `bool` | `false` | Immediate (no re-init) | Rotates output frames $180^\circ$ for inverted camera mounts. |

### Understanding `manual_exposure_unit`

When `auto_exposure` is set to `false`, the driver configures V4L2 manual exposure (`CAP_PROP_AUTO_EXPOSURE = 1`) using `manual_exposure_unit`. Following the V4L2 absolute exposure specification (`V4L2_CID_EXPOSURE_ABSOLUTE`), **1 unit equals $10^{-4}\text{ seconds}$ ($0.1\text{ ms}$ or $100\ \mu\text{s}$)**:

$$t_{\text{exposure}} = \text{manual\_exposure\_unit} \times 10^{-4}\text{ s} = \frac{\text{manual\_exposure\_unit}}{10}\text{ ms}$$

| `manual_exposure_unit` | Exposure Time | Equivalent Shutter Speed | Typical Use Case |
| :--- | :--- | :--- | :--- |
| **`333`** *(default)* | $33.3\text{ ms}$ | $\approx 1/30\text{ s}$ | Matches the maximum frame interval at 30 FPS (brightest image at 30 FPS). |
| **`166`** | $16.6\text{ ms}$ | $\approx 1/60\text{ s}$ | Balanced indoor lighting / 60 FPS frame interval. |
| **`100`** | $10.0\text{ ms}$ | $1/100\text{ s}$ | Moderate motion reduction in well-lit environments. |
| **`50`** | $5.0\text{ ms}$ | $1/200\text{ s}$ | Fast-moving robotics / underwater or outdoor scenes to minimize motion blur. |

> **Tip:** Keep $t_{\text{exposure}} \le \frac{1}{\text{camera\_fps}}$ (for example, `manual_exposure_unit <= 333` at 30 FPS). Setting an exposure time longer than the frame period forces the sensor to drop below the target frame rate.

---

## Published Topics

All image topics are published via `image_transport` using `SensorDataQoS` (`Best Effort`, `Volatile`) with synchronized timestamps:

| Topic | Message Type | `frame_id` | Description |
| :--- | :--- | :--- | :--- |
| `stereo_cam/left_cam` | `sensor_msgs/msg/Image` | `left_camera` | Left eye image (`bgr8`) in stereo mode (mirrors `single_cam/feed` in mono mode). |
| `stereo_cam/right_cam` | `sensor_msgs/msg/Image` | `right_camera` | Right eye image (`bgr8`) in stereo mode (mirrors `single_cam/feed` in mono mode). |
| `single_cam/feed` | `sensor_msgs/msg/Image` | `single_camera` | Full unsplit frame (`bgr8`): side-by-side stereo image or full mono frame. |

---