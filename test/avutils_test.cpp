#define BOOST_TEST_MODULE AVUtilsTest
#include <boost/test/included/unit_test.hpp>

#include "avutils.h"

BOOST_AUTO_TEST_CASE(av_error_messages) {
  BOOST_CHECK_EQUAL("Resource temporarily unavailable",
                    rtm::video::avutils::error_msg(AVERROR(EAGAIN)));
}

BOOST_AUTO_TEST_CASE(av_pixel_format) {
  BOOST_CHECK_EQUAL(AV_PIX_FMT_BGR24,
                    rtm::video::avutils::to_av_pixel_format(image_pixel_format::BGR));

  BOOST_CHECK_EQUAL(AV_PIX_FMT_RGB0,
                    rtm::video::avutils::to_av_pixel_format(image_pixel_format::RGB0));
}

BOOST_AUTO_TEST_CASE(encoder_context) {
  rtm::video::avutils::init();
  const AVCodecID encoder_id = AV_CODEC_ID_VP9;
  const AVCodec *encoder = avcodec_find_encoder(encoder_id);
  BOOST_TEST(encoder != nullptr);

  std::shared_ptr<AVCodecContext> ctx = rtm::video::avutils::encoder_context(encoder_id);
  BOOST_CHECK_EQUAL(AVMEDIA_TYPE_VIDEO, ctx->codec_type);
  BOOST_CHECK_EQUAL(encoder_id, ctx->codec_id);
  BOOST_CHECK_EQUAL(encoder->pix_fmts[0], ctx->pix_fmt);
  BOOST_CHECK_EQUAL(4, ctx->gop_size);
  BOOST_CHECK_EQUAL(1, ctx->time_base.num);
  BOOST_CHECK_EQUAL(1000, ctx->time_base.den);
}

BOOST_AUTO_TEST_CASE(av_frame) {
  const int width = 100;
  const int height = 50;
  const int align = 32;
  const AVPixelFormat pixel_format = AV_PIX_FMT_BGR24;
  std::shared_ptr<AVFrame> frame =
      rtm::video::avutils::av_frame(width, height, align, pixel_format);

  BOOST_CHECK_EQUAL(width, frame->width);
  BOOST_CHECK_EQUAL(height, frame->height);
  BOOST_CHECK_EQUAL(pixel_format, frame->format);
  BOOST_CHECK_EQUAL(FFALIGN(width, align) * 3, frame->linesize[0]);
}

BOOST_AUTO_TEST_CASE(sws) {
  const int src_width = 100;
  const int src_height = 50;
  const int src_align = 32;
  const AVPixelFormat src_pixel_format = AV_PIX_FMT_BGR0;
  std::shared_ptr<AVFrame> src_frame =
      rtm::video::avutils::av_frame(src_width, src_height, src_align, src_pixel_format);

  const int dst_width = 100;
  const int dst_height = 50;
  const int dst_align = 32;
  const AVPixelFormat dst_pixel_format = AV_PIX_FMT_RGB0;
  std::shared_ptr<AVFrame> dst_frame =
      rtm::video::avutils::av_frame(dst_width, dst_height, dst_align, dst_pixel_format);

  std::shared_ptr<SwsContext> ctx =
      rtm::video::avutils::sws_context(src_frame, dst_frame);

  const uint8_t r = 10;
  const uint8_t g = 16;
  const uint8_t b = 19;

  // updating topleft pixel
  src_frame->data[0][0] = b;
  src_frame->data[0][1] = g;
  src_frame->data[0][2] = r;

  rtm::video::avutils::sws_scale(ctx, src_frame, dst_frame);

  BOOST_CHECK_EQUAL(r, dst_frame->data[0][0]);
  BOOST_CHECK_EQUAL(g, dst_frame->data[0][1]);
  BOOST_CHECK_EQUAL(b, dst_frame->data[0][2]);
}