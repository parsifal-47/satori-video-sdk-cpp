#include <iostream>

#include "data.h"
#include "metrics.h"
#include "satori_video.h"
#include "streams/streams.h"
#include "video_streams.h"

namespace satori {
namespace video {

namespace {

auto &frame_publish_delay_milliseconds =
    prometheus::BuildHistogram()
        .Name("frame_publish_delay_milliseconds")
        .Register(metrics_registry())
        .Add({}, std::vector<double>{0,    0.1,  0.2,  0.3,  0.4,  0.5,  0.6,  0.7,  0.8,
                                     0.9,  1,    2,    3,    4,    5,    6,    7,    8,
                                     9,    10,   15,   20,   25,   30,   40,   50,   60,
                                     70,   80,   90,   100,  200,  300,  400,  500,  600,
                                     700,  800,  900,  1000, 2000, 3000, 4000, 5000, 6000,
                                     7000, 8000, 9000, 10000});

class rtm_sink_impl : public streams::subscriber<encoded_packet>,
                      boost::static_visitor<void> {
 public:
  rtm_sink_impl(const std::shared_ptr<rtm::publisher> &client,
                boost::asio::io_service &io_service, const std::string &rtm_channel)
      : _client(client),
        _io_service(io_service),
        _frames_channel(rtm_channel),
        _metadata_channel(rtm_channel + metadata_channel_suffix) {}

  void operator()(const encoded_metadata &m) {
    cbor_item_t *packet = m.to_network().to_cbor();
    CHECK_EQ(cbor_refcount(packet), 0);

    _io_service.post([ client = _client, channel = _metadata_channel, packet ]() {
      client->publish(channel, packet, nullptr);
    });
  }

  void operator()(const encoded_frame &f) {
    std::vector<network_frame> network_frames = f.to_network();

    for (const network_frame &nf : network_frames) {
      cbor_item_t *packet = nf.to_cbor();
      CHECK_EQ(cbor_refcount(packet), 0);
      _io_service.post([
        client = _client, channel = _frames_channel, packet,
        creation_time = f.creation_time
      ]() {
        const auto before_publish = std::chrono::system_clock::now();
        frame_publish_delay_milliseconds.Observe(
            std::chrono::duration_cast<std::chrono::milliseconds>(before_publish
                                                                  - creation_time)
                .count());
        client->publish(channel, packet, nullptr);
      });
    }

    _frames_counter++;
    if (_frames_counter % 100 == 0) {
      LOG(INFO) << "published " << _frames_counter << " frames to " << _frames_channel;
    }
  }

 private:
  void on_next(encoded_packet &&packet) override {
    boost::apply_visitor(*this, packet);
    _src->request(1);
  }

  void on_error(std::error_condition ec) override { ABORT() << ec.message(); }

  void on_complete() override { delete this; }

  void on_subscribe(streams::subscription &s) override {
    _src = &s;
    _src->request(1);
  }

  const std::shared_ptr<rtm::publisher> _client;
  boost::asio::io_service &_io_service;
  const std::string _frames_channel;
  const std::string _metadata_channel;
  streams::subscription *_src;
  uint64_t _frames_counter{0};
};
}  // namespace

streams::subscriber<encoded_packet> &rtm_sink(
    const std::shared_ptr<rtm::publisher> &client, boost::asio::io_service &io_service,
    const std::string &rtm_channel) {
  return *(new rtm_sink_impl(client, io_service, rtm_channel));
}

}  // namespace video
}  // namespace satori
