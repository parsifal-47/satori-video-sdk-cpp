#include "stream_error.h"

#include <string>

namespace satori {
namespace video {
namespace streams {

struct stream_error_category : std::error_category {
  const char *name() const noexcept override { return "video_error"; }
  std::string message(int ev) const override {
    switch (static_cast<stream_error>(ev)) {
      case stream_error::ValueWasMoved:
        return "value was moved";
      case stream_error::NotInitialized:
        return "not initialized";
    }
  }
};

std::error_condition make_error_condition(stream_error e) {
  static stream_error_category category;
  return {static_cast<int>(e), category};
}

}  // namespace streams
}  // namespace video
}  // namespace satori