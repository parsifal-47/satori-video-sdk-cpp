#pragma once

#include <cbor.h>
#include <boost/variant.hpp>
#include <chrono>
#include <string>
#include <vector>

#include "rtmvideo.h"

namespace rtm {
namespace video {

static constexpr size_t max_payload_size = 65000;

// frame id is an integer interval [i1, i2),
// it was implemented this way because one of the sources is RTP
struct frame_id {
  int64_t i1;
  int64_t i2;
};

bool operator==(const frame_id &lhs, const frame_id &rhs);
bool operator!=(const frame_id &lhs, const frame_id &rhs);

// network representation of codec parameters, e.g. in binary data
// is converted into base64, because RTM supports only text/json data
struct network_metadata {
  std::string codec_name;
  std::string base64_data;

  cbor_item_t *to_cbor() const;
};

// network representation of encoded video frame, e.g. binary data
// is converted into base64, because RTM supports only text/json data
struct network_frame {
  std::string base64_data;
  frame_id id{0, 0};
  std::chrono::system_clock::time_point t;
  uint32_t chunk{1};
  uint32_t chunks{1};

  cbor_item_t *to_cbor() const;
};

// algebraic type to support flow of network data using streams API
using network_packet = boost::variant<network_metadata, network_frame>;

// codec parameters to decode encoded frames
struct encoded_metadata {
  std::string codec_name;
  std::string codec_data;

  network_metadata to_network() const;
};

// encoded frame
struct encoded_frame {
  std::string data;
  frame_id id;

  std::vector<network_frame> to_network(std::chrono::system_clock::time_point t) const;
};

// algebraic type to support flow of encoded frame data using streams API
using encoded_packet = boost::variant<encoded_metadata, encoded_frame>;

// TODO: may contain some data like FPS, etc.
struct image_metadata {};

// If an image uses packed pixel format like packed RGB or packed YUV,
// then it has only a single plane, e.g. all it's data is within plane_data[0].
// If an image uses planar pixel format like planar YUV or HSV,
// then every component is stored as a separate array (e.g. separate plane),
// for example, for YUV  Y is plane_data[0], U is plane_data[1] and V is
// plane_data[2]. A stride is a plane size with alignment.
struct image_frame {
  frame_id id;

  image_pixel_format pixel_format;
  uint16_t width;
  uint16_t height;

  std::string plane_data[MAX_IMAGE_PLANES];
  uint32_t plane_strides[MAX_IMAGE_PLANES];
};

using image_packet = boost::variant<image_metadata, image_frame>;

}  // namespace video
}  // namespace rtm