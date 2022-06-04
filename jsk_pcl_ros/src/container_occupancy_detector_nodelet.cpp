// -*- mode: C++ -*-
/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2022, JSK Lab
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
 *   * Neither the name of the JSK Lab nor the names of its
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
/*
 * container_occupancy_detector_nodelet.cpp
 * Author: Yoshiki Obinata <obinata@jsk.imi.i.u-tokyo.ac.jp>
 */

#include "jsk_pcl_ros/container_occupancy_detector.h"

namespace jsk_pcl_ros{

    void ContainerOccupancyDetector::onInit(){
        jsk_topic_tools::ConnectionBasedNodelet::onInit();

        occupancy_rate_pub_
            = advertise<std_msgs::Float64MultiArray>(*pnh_, "occupancy_rate", 1);
        tf_listener_ = new tf2_ros::TransformListener(tf_buffer_);
        onInitPostProcess();
    }

    void  ContainerOccupancyDetector::subscribe(){
        sub_box_.subscribe(*pnh_, "containers/boxes", 1);
        sub_points_.subscribe(*pnh_, "containers/points", 1);
        sync_ = std::make_shared<message_filters::Synchronizer<SyncPolicy> >(100);
        sync_->connectInput(sub_box_, sub_points_);
        sync_->registerCallback(boost::bind(&ContainerOccupancyDetector::calculate, this, _1, _2));
    }

    void ContainerOccupancyDetector::unsubscribe(){
        sub_box_.unsubscribe();
        sub_points_.unsubscribe();
        delete tf_listener_;
    }

    void ContainerOccupancyDetector::calculate(
        const jsk_recognition_msgs::BoundingBoxArray::ConstPtr& box_array_msg,
        const sensor_msgs::PointCloud2::ConstPtr& points_msg
    ){
        boost::mutex::scoped_lock lock(mutex_);
        pcl::PCLPointCloud2Ptr pcl_pc2_ptr_ =
            boost::shared_ptr<pcl::PCLPointCloud2>(new pcl::PCLPointCloud2);
        pcl::PointCloud<pcl::PointXYZ>::Ptr pcl_xyz_ptr_ =
            boost::shared_ptr<pcl::PointCloud<pcl::PointXYZ>>(new pcl::PointCloud<pcl::PointXYZ>);

        if(!box_array_msg->boxes.empty()){
            // if containers were subscribed
            if(pointsTransform(box_array_msg, points_msg)){
                // when point cloud transform succeeded
                std_msgs::Float64MultiArray occupancy_rates_;

                pcl_conversions::toPCL(*transformed_points_msg_, *pcl_pc2_ptr_); // convert to pcl
                pcl::fromPCLPointCloud2(*pcl_pc2_ptr_, *pcl_xyz_ptr_);

                for(auto box = box_array_msg->boxes.begin();
                    box != box_array_msg->boxes.end();
                    ++box){
                    // calculate occupancy for each bounding boxes
                    float sum_occupancy_ = 0.0, occupancy_;
                    float h_rate_;
                    const float box_top_h_ = box->pose.position.z + box->dimensions.z / 2.0;
                    const float box_buttom_h_ = box->pose.position.z - box->dimensions.z / 2.0;
                    uint64_t n_point_ = 0;

                    for(auto point = pcl_xyz_ptr_->points.begin();
                        point != pcl_xyz_ptr_->points.end();
                        ++point){
                        // NOTE do we have much better solution?
                        // The author found cluster_point_indices_decomposer's ~output%02d topic
                        // it is good if it can be subscribed
                        if(((point->x > (box->pose.position.x - box->dimensions.x / 2.0)) &&
                            (point->x < (box->pose.position.x + box->dimensions.x / 2.0))) &&
                           ((point->y > (box->pose.position.y - box->dimensions.y / 2.0)) &&
                            (point->y < (box->pose.position.y + box->dimensions.y / 2.0))) &&
                           ((point->z >= box_buttom_h_))){
                            // if point in the box
                            n_point_++;
                            sum_occupancy_ += (point->z - box_buttom_h_) / (box_top_h_ - box_buttom_h_);
                        }
                    }
                    occupancy_ = sum_occupancy_ / float(n_point_);
                    occupancy_rates_.data.push_back(occupancy_);
                }

                occupancy_rate_pub_.publish(occupancy_rates_);
            }else{
                // when point cloud transfrom not succeeded
                NODELET_WARN("Failed to transform pointcloud\n");
                return;
            }
        }else{
            // if no containers
            NODELET_DEBUG_ONCE("No containers subscribed\n");
            return;
        }
    }

    bool ContainerOccupancyDetector::pointsTransform(
        const jsk_recognition_msgs::BoundingBoxArray::ConstPtr& box_array_msg,
        const sensor_msgs::PointCloud2::ConstPtr& points_msg
    ){
        // convert pc2's frame to bounding box's one
        geometry_msgs::TransformStamped transform_stamped_;
        Eigen::Matrix4f mat;
        try{
            transform_stamped_ = tf_buffer_.lookupTransform(
                box_array_msg->header.frame_id,
                points_msg->header.frame_id,
                points_msg->header.stamp,
                ros::Duration(10.0));
            mat = tf2::transformToEigen(transform_stamped_.transform).matrix().cast<float>();
            pcl_ros::transformPointCloud(mat, *points_msg, *transformed_points_msg_);
            return true;
        }catch(tf2::TransformException &ex){
            NODELET_WARN("Failed to transform tf: %s\n", ex.what());
            return false;
        }
    }

    void ContainerOccupancyDetector::updateDiagnostic(
        diagnostic_updater::DiagnosticStatusWrapper &stat){
        if(vital_checker_ -> isAlive()){
            stat.summary(diagnostic_msgs::DiagnosticStatus::OK,
                         "ContainerOccupancyDetector running\n");
        }
        DiagnosticNodelet::updateDiagnostic(stat);
    }
}

#include <pluginlib/class_list_macros.h>
PLUGINLIB_EXPORT_CLASS(jsk_pcl_ros::ContainerOccupancyDetector, nodelet::Nodelet);
