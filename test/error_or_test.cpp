#define BOOST_TEST_MODULE ErrorOrTest
#include <boost/test/included/unit_test.hpp>

#include "error_or.h"

using namespace rtm::video;

BOOST_AUTO_TEST_CASE(error_or_conversions) {
  error_or<int> i1(10);
  i1.check_ok();

  error_or<int> i2 = std::error_condition(video_error::EndOfStreamError);
  i2.check_not_ok();

  const int i = -100;
  error_or<int> i3 = i;
  i3.check_ok();
}

BOOST_AUTO_TEST_CASE(error_or_move_test) {
  std::unique_ptr<int> ptr(new int);
  BOOST_TEST((bool)ptr);

  error_or<std::unique_ptr<int>> status(std::move(ptr));
  status.check_ok();
  BOOST_TEST(!(bool)ptr);

  ptr = status.move();
  BOOST_TEST((bool)ptr);
  status.check_not_ok();
}

BOOST_AUTO_TEST_CASE(error_or_constructor_destructor_test) {
  static int constructor = 0;
  static int destructor = 0;

  struct foo {
    bool active{true};

    foo() { constructor++; }
    foo(const foo &) = delete;
    foo(foo &&other) { other.active = false; }

    ~foo() {
      if (active) destructor++;
    }
  };

  BOOST_TEST(constructor == 0);
  BOOST_TEST(destructor == 0);

  std::unique_ptr<error_or<foo>> ptr;
  BOOST_TEST(constructor == 0);
  BOOST_TEST(destructor == 0);

  foo f;
  BOOST_TEST(constructor == 1);
  BOOST_TEST(destructor == 0);

  ptr.reset(new error_or<foo>(std::move(f)));
  BOOST_TEST(constructor == 1);
  BOOST_TEST(destructor == 0);

  ptr.reset();
  BOOST_TEST(constructor == 1);
  BOOST_TEST(destructor == 1);

  ptr.reset(new error_or<foo>(std::error_condition(video_error::FrameNotReadyError)));
  BOOST_TEST(constructor == 1);
  BOOST_TEST(destructor == 1);

  ptr.reset();
  BOOST_TEST(constructor == 1);
  BOOST_TEST(destructor == 1);
};