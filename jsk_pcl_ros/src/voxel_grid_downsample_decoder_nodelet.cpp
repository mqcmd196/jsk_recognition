/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2013, Ryohei Ueda and JSK Lab
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/o2r other materials provided
 *     with the distribution.
 *   * Neither the name of the Willow Garage nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

#include "jsk_pcl_ros/voxel_grid_downsample_decoder.h"
#include <pluginlib/class_list_macros.h>
#include <pcl/point_types.h>
#include <pcl/filters/passthrough.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/io/io.h>
#include <pcl_ros/transforms.h>


#include <string>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>


namespace jsk_pcl_ros
{

  int VoxelGridDownsampleDecoder::getPointcloudID(const sensor_msgs::PointCloud2ConstPtr &input) {
    std::vector<std::string> v;
    boost::algorithm::split(v, input->header.frame_id, boost::is_any_of(" "));
    int id = boost::lexical_cast<int>(v[1]);
    return id;
  }

  std::string VoxelGridDownsampleDecoder::getPointcloudFrameId(const sensor_msgs::PointCloud2ConstPtr &input) {
    std::vector<std::string> v;
    boost::algorithm::split(v, input->header.frame_id, boost::is_any_of(" "));
    return v[0];
  }
  
  void VoxelGridDownsampleDecoder::pointCB(const sensor_msgs::PointCloud2ConstPtr &input)
  {                                 
    int id = getPointcloudID(input);
    std::string frame_id = getPointcloudFrameId(input);
    // if id = 0, clear the buffer
    if (id == 0) {
      pc_buffer_.clear();
    }
    // update the buffer
    pc_buffer_.push_back(input);
    
    // publush the buffer
    publishBuffer();
    
  }

  void VoxelGridDownsampleDecoder::publishBuffer(void)
  {
    if (pc_buffer_.size() == 0) {
      ROS_WARN("no pointcloud is subscribed yet");
      return;
    }
    std::string result_frame_id = getPointcloudFrameId(pc_buffer_[0]);
    pcl::PointCloud<pcl::PointXYZ>::Ptr concatenated_cloud (new pcl::PointCloud<pcl::PointXYZ>);
    for (size_t i = 0; i < pc_buffer_.size(); i++)
    {
      pcl::PointCloud<pcl::PointXYZ>::Ptr tmp_cloud (new pcl::PointCloud<pcl::PointXYZ>);
      pcl::PointCloud<pcl::PointXYZ>::Ptr transformed_tmp_cloud (new pcl::PointCloud<pcl::PointXYZ>);
      fromROSMsg(*pc_buffer_[i], *tmp_cloud);
      tmp_cloud->header.frame_id = getPointcloudFrameId(pc_buffer_[i]);
      // transform the pointcloud
      pcl_ros::transformPointCloud(result_frame_id,
                                   *tmp_cloud,
                                   *transformed_tmp_cloud,
                                   tf_listener);
      // concatenate the tmp_cloud into concatenated_cloud
      for (size_t j = 0; j < transformed_tmp_cloud->points.size(); j++) {
        concatenated_cloud->points.push_back(transformed_tmp_cloud->points[j]);
      }
    }
    sensor_msgs::PointCloud2 out;
    toROSMsg(*concatenated_cloud, out);
    out.header = pc_buffer_[0]->header;
    out.header.frame_id = getPointcloudFrameId(pc_buffer_[0]);
    pub_.publish(out);
  }
  
  void VoxelGridDownsampleDecoder::onInit(void)
  {
    PCLNodelet::onInit();
    // encoded input
    sub_ = pnh_->subscribe("input", 1, &VoxelGridDownsampleDecoder::pointCB,
                           this);
    // decoded output
    pub_ = pnh_->advertise<sensor_msgs::PointCloud2>("output", 1);
  }
    
}


typedef jsk_pcl_ros::VoxelGridDownsampleDecoder VoxelGridDownsampleDecoder;
PLUGINLIB_DECLARE_CLASS (jsk_pcl, VoxelGridDownsampleDecoder, VoxelGridDownsampleDecoder, nodelet::Nodelet);
