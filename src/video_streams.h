#pragma once

#include <boost/asio.hpp>
#include <functional>
#include <memory>

#include "data.h"
#include "mkv_options.h"
#include "rtm_client.h"
#include "streams/streams.h"

namespace satori {
namespace video {

streams::publisher<encoded_packet> file_source(boost::asio::io_service &io,
                                               const std::string &filename, bool loop,
                                               bool batch);

streams::publisher<encoded_packet> camera_source(boost::asio::io_service &io,
                                                 const std::string &dimensions);

// options are ffmpeg protocol options, 'k1=v1,k2=v2'
streams::publisher<encoded_packet> url_source(const std::string &url,
                                              const std::string &options = "");

streams::publisher<network_packet> network_replay_source(boost::asio::io_service &io,
                                                         const std::string &filename,
                                                         bool batch);

streams::publisher<network_packet> rtm_source(
    const std::shared_ptr<rtm::subscriber> &client, const std::string &channel_name);

streams::op<network_packet, encoded_packet> decode_network_stream();

streams::op<encoded_packet, owned_image_packet> decode_image_frames(
    int bounding_width, int bounding_height, image_pixel_format pixel_format,
    bool keep_proportions);

streams::subscriber<encoded_packet> &rtm_sink(
    const std::shared_ptr<rtm::publisher> &client, const std::string &rtm_channel);

streams::subscriber<encoded_packet> &mkv_sink(const std::string &filename,
                                              const mkv::format_options &format_options);

}  // namespace video
}  // namespace satori
