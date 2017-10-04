/******************************************************************************
 * Copyright 2017 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "modules/prediction/common/prediction_util.h"

#include <cmath>
#include <limits>

#include "modules/prediction/common/prediction_gflags.h"
#include "modules/common/log.h"

namespace apollo {
namespace prediction {
namespace math_util {

double Normalize(const double value, const double mean, const double std) {
  constexpr double eps = 1e-10;
  return (value - mean) / (std + eps);
}

double Sigmoid(const double value) { return 1 / (1 + std::exp(-1.0 * value)); }

double Relu(const double value) { return (value > 0.0) ? value : 0.0; }

int SolveQuadraticEquation(const std::vector<double>& coefficients,
                           std::pair<double, double>* roots) {
  if (coefficients.size() != 3) {
    return -1;
  }
  const double a = coefficients[0];
  const double b = coefficients[1];
  const double c = coefficients[2];
  if (std::fabs(a) <= std::numeric_limits<double>::epsilon()) {
    return -1;
  }

  double delta = b * b - 4.0 * a * c;
  if (delta < 0.0) {
    return -1;
  }

  roots->first = (0.0 - b + std::sqrt(delta)) / (2.0 * a);
  roots->second = (0.0 - b - std::sqrt(delta)) / (2.0 * a);
  return 0;
}

}  // namespace math_util

namespace predictor_util {

using ::apollo::common::PathPoint;
using ::apollo::common::TrajectoryPoint;

void TranslatePoint(const double translate_x, const double translate_y,
                    TrajectoryPoint* point) {
  if (point == nullptr || !point->has_path_point()) {
    AERROR << "Point is nullptr or has NO path_point.";
  }
  const double original_x = point->path_point().x();
  const double original_y = point->path_point().y();
  point->mutable_path_point()->set_x(original_x + translate_x);
  point->mutable_path_point()->set_y(original_y + translate_y);
}

void GenerateFreeMoveTrajectoryPoints(
    Eigen::Matrix<double, 6, 1> *state,
    const Eigen::Matrix<double, 6, 6>& transition,
    const size_t num,
    const double freq,
    std::vector<TrajectoryPoint> *points) {
  double x = (*state)(0, 0);
  double y = (*state)(1, 0);
  double v_x = (*state)(2, 0);
  double v_y = (*state)(3, 0);
  double acc_x = (*state)(4, 0);
  double acc_y = (*state)(5, 0);
  double theta = std::atan2(v_y, v_x);

  for (size_t i = 0; i < num; ++i) {
    double speed = std::hypot(v_x, v_y);
    if (speed <= std::numeric_limits<double>::epsilon()) {
      speed = 0.0;
      v_x = 0.0;
      v_y = 0.0;
      acc_x = 0.0;
      acc_y = 0.0;
    } else if (speed > FLAGS_max_speed) {
      speed = FLAGS_max_speed;
    }

    // update theta
    if (speed > std::numeric_limits<double>::epsilon()) {
      if (points->size() > 0) {
        PathPoint* prev_point = points->back().mutable_path_point();
        theta = std::atan2(y - prev_point->y(), x - prev_point->x());
        prev_point->set_theta(theta);
      }
    } else {
      if (points->size() > 0) {
        theta = points->back().path_point().theta();
      }
    }

    // update state
    (*state)(2, 0) = v_x;
    (*state)(3, 0) = v_y;
    (*state)(4, 0) = acc_x;
    (*state)(5, 0) = acc_y;

    // obtain position
    x = (*state)(0, 0);
    y = (*state)(1, 0);

    // Generate trajectory point
    TrajectoryPoint trajectory_point;
    PathPoint path_point;
    path_point.set_x(x);
    path_point.set_y(y);
    path_point.set_z(0.0);
    path_point.set_theta(theta);
    trajectory_point.mutable_path_point()->CopyFrom(path_point);
    trajectory_point.set_v(speed);
    trajectory_point.set_a(std::hypot(acc_x, acc_y));
    trajectory_point.set_relative_time(static_cast<double>(i) * freq);
    points->emplace_back(std::move(trajectory_point));

    // Update position, velocity and acceleration
    (*state) = transition * (*state);
    x = (*state)(0, 0);
    y = (*state)(1, 0);
    v_x = (*state)(2, 0);
    v_y = (*state)(3, 0);
    acc_x = (*state)(4, 0);
    acc_y = (*state)(5, 0);
  }
}

}  // namespace predictor_util
}  // namespace prediction
}  // namespace apollo
