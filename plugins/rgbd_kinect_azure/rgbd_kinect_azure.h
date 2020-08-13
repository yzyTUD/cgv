#pragma once

#include <rgbd_input.h>

#include "lib_begin.h"

#include <memory>
#include <thread>
#include <mutex>
#include <k4a/k4a.hpp>

namespace rgbd {
	/// interface for kinect devices provided by a driver (only to be used by driver implementors)
	class CGV_API rgbd_kinect_azure : public rgbd_device
	{
	public:
		/// create a detached kinect CLNUI device object
		rgbd_kinect_azure();
		~rgbd_kinect_azure();
		/// attach to the kinect device of the given serial
		bool attach(const std::string& serial);
		/// return whether device object is attached to a kinect device
		bool is_attached() const;
		/// detach from serial (done automatically in constructor
		bool detach();
		/// return whether rgbd device has support for view finding actuator
		bool has_view_finder() const;
		/// return a view finder info structure
		const view_finder_info& get_view_finder_info() const;

		/// check whether rgbd device has inertia measurement unit
		bool has_IMU() const;
		/// return additional information on inertia measurement unit
		const IMU_info& get_IMU_info() const;
		/// query the current measurement of the acceleration sensors within the given time_out in milliseconds; return whether a measurement has been retrieved
		bool put_IMU_measurement(IMU_measurement& m, unsigned time_out) const;
		/// check whether the device supports the given combination of input streams
		bool check_input_stream_configuration(InputStreams is) const;
		/// query the stream formats available for a given stream configuration
		void query_stream_formats(InputStreams is, std::vector<stream_format>& stream_formats) const;
		/// start the camera
		bool start_device(InputStreams is, std::vector<stream_format>& stream_formats);
		/// start the rgbd device with given stream formats 
		virtual bool start_device(const std::vector<stream_format>& stream_formats);
		/// stop the camera
		bool stop_device();
		/// return whether device has been started
		bool is_running() const;
		/// query a frame of the given input stream
		bool get_frame(InputStreams is, frame_type& frame, int timeOut);
		/// map a color frame to the image coordinates of the depth image
		void map_color_to_depth(const frame_type& depth_frame, const frame_type& color_frame,
			frame_type& warped_color_frame) const;
		/// map a depth value together with pixel indices to a 3D point with coordinates in meters; point_ptr needs to provide space for 3 floats
		bool map_depth_to_point(int x, int y, int depth, float* point_ptr) const;

	protected:
		k4a::device device;
		std::string device_serial;
		volatile bool device_started;

		stream_format color_format, ir_format, depth_format;
		bool near_mode;

		std::unique_ptr<std::thread> capture_thread;
		std::mutex capture_lock;
		volatile bool has_new_color_frame, has_new_depth_frame, has_new_ir_frame;
		std::shared_ptr<rgbd::frame_type> color_frame, depth_frame, ir_frame;
		std::chrono::time_point<std::chrono::steady_clock,std::chrono::microseconds> last_capture;
	private:
		void capture(int is);
	};

	/// interface for kinect drivers (implement only as driver implementor)
	class CGV_API rgbd_kinect_azure_driver : public rgbd_driver
	{
	public:
		/// construct driver
		rgbd_kinect_azure_driver();
		/// destructor
		~rgbd_kinect_azure_driver();
		/// return the number of kinect devices found by driver
		unsigned get_nr_devices();
		/// return the serial of the i-th kinect devices
		std::string get_serial(int i);
		/// create a kinect device
		rgbd_device* create_rgbd_device();
	};
}
#include <cgv/config/lib_end.h>