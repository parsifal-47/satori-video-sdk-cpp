#include <cstring>
#include <gsl/gsl>
#include <memory>
#include <string>
#include <thread>

extern "C" {
#include <libavutil/error.h>
}

#include "asio_streams.h"
#include "avutils.h"
#include "error.h"
#include "logging.h"
#include "video_streams.h"

namespace rtm {
namespace video {

struct file_source_impl {
  file_source_impl(const std::string &filename, const bool loop)
      : _filename(filename), _loop(loop), _start(std::chrono::system_clock::now()) {}

  ~file_source_impl() {
    if (_dec_ctx) avcodec_free_context(&_dec_ctx);
    if (_fmt_ctx) avformat_close_input(&_fmt_ctx);
  }

  int init() {
    int ret = 0;

    LOG(1) << "Opening file " << _filename;
    if ((ret = avformat_open_input(&_fmt_ctx, _filename.c_str(), nullptr, nullptr)) < 0) {
      LOG(ERROR) << "Could not open file: " << _filename << " "
                 << avutils::error_msg(ret);
      return ret;
    }
    LOG(1) << "File " << _filename << " is open";

    LOG(1) << "Looking for stream info...";
    if ((ret = avformat_find_stream_info(_fmt_ctx, nullptr)) < 0) {
      LOG(ERROR) << "Could not find stream information:" << avutils::error_msg(ret);
      return ret;
    }
    LOG(1) << "Stream info found";

    LOG(1) << "Number of streams " << _fmt_ctx->nb_streams;

    LOG(1) << "Looking for best stream...";
    if ((ret = av_find_best_stream(_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &_dec, 0)) < 0) {
      LOG(ERROR) << "Could not find video stream:" << avutils::error_msg(ret);
      return ret;
    }
    LOG(1) << "Best stream found";

    _stream_idx = ret;
    _stream = _fmt_ctx->streams[_stream_idx];

    LOG(1) << "Allocating codec context...";
    _dec_ctx = avcodec_alloc_context3(_dec);
    if (!_dec_ctx) {
      LOG(ERROR) << "Failed to allocate codec context";
      return -1;
    }
    LOG(1) << "Codec context is allocated";

    LOG(1) << "Copying codec parameters to codec context...";
    if ((ret = avcodec_parameters_to_context(_dec_ctx, _stream->codecpar)) < 0) {
      LOG(ERROR) << "Failed to copy codec parameters to codec context:"
                 << avutils::error_msg(ret);
      return ret;
    }
    LOG(1) << "Codec parameters were copied to codec context";

    LOG(1) << "Opening video codec...";
    AVDictionary *opts = nullptr;
    if ((ret = avcodec_open2(_dec_ctx, _dec, &opts)) < 0) {
      LOG(ERROR) << "*** Failed to open video codec:" << avutils::error_msg(ret);
      return ret;
    }
    LOG(1) << "Video codec is open";

    return 0;
  }

  void generate(int count, streams::observer<encoded_packet> &observer) {
    if (_fmt_ctx == nullptr) {
      if (int ret = init()) {
        observer.on_error(video_error::StreamInitializationError);
        return;
      }
    }

    int packets = 0;
    while (packets < count) {
      if (!_metadata_sent) {
        send_metadata(observer);
        ++packets;
        continue;
      }

      av_init_packet(&_pkt);
      auto release = gsl::finally([this]() { av_packet_unref(&_pkt); });

      int ret = av_read_frame(_fmt_ctx, &_pkt);
      if (ret < 0) {
        if (ret == AVERROR_EOF) {
          if (_loop) {
            LOG(4) << "restarting " << _filename;
            av_seek_frame(_fmt_ctx, _stream_idx, _fmt_ctx->start_time,
                          AVSEEK_FLAG_BACKWARD);
            continue;
          } else {
            LOG(4) << "eof in " << _filename;
            observer.on_complete();
            return;
          }
        } else {
          observer.on_error(video_error::FrameGenerationError);
          return;
        }
      }

      if (_pkt.stream_index == _stream_idx) {
        LOG(4) << "packet from file " << _filename;
        encoded_frame frame;
        frame.data = std::string{_pkt.data, _pkt.data + _pkt.size};
        frame.id = {_last_pos, _pkt.pos};
        auto ts = 1000 * _pkt.pts * _stream->time_base.num / _stream->time_base.den;
        frame.timestamp =
            std::chrono::system_clock::time_point{_start + std::chrono::milliseconds(ts)};
        frame.key_frame = static_cast<bool>(_pkt.flags & AV_PKT_FLAG_KEY);
        observer.on_next(frame);
        _last_pos = _pkt.pos + 1 /* because our intervals are [i1, i2] */;
        packets++;
      }
    }
  }

  void send_metadata(streams::observer<encoded_packet> &observer) {
    observer.on_next(encoded_metadata{
        _dec->name,
        {_dec_ctx->extradata, _dec_ctx->extradata + _dec_ctx->extradata_size}});
    _metadata_sent = true;
  }

  const std::string _filename;
  const bool _loop{false};

  std::chrono::system_clock::time_point _start;
  AVFormatContext *_fmt_ctx{nullptr};
  int _stream_idx{-1};
  AVStream *_stream{nullptr};
  AVCodec *_dec{nullptr};  // TODO: check if possible to destroy it
  AVCodecContext *_dec_ctx{nullptr};
  AVPacket _pkt{0};
  int64_t _last_pos{0};
  bool _metadata_sent{false};
};

streams::publisher<encoded_packet> file_source(boost::asio::io_service &io,
                                               std::string filename, bool loop,
                                               bool batch) {
  avutils::init();
  streams::publisher<encoded_packet> result =
      streams::generators<encoded_packet>::stateful(
          [filename, loop]() { return new file_source_impl(filename, loop); },
          [](file_source_impl *impl, int count, streams::observer<encoded_packet> &sink) {
            return impl->generate(count, sink);
          });

  if (!batch) {
    // todo: fps
    int fps = 25;
    result = std::move(result) >> streams::asio::interval<encoded_packet>(
                                      io, std::chrono::milliseconds(1000 / fps));
  }

  return result;
}

}  // namespace video
}  // namespace rtm
