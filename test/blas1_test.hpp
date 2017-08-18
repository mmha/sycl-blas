/***************************************************************************
 *
 *  @license
 *  Copyright (C) 2016 Codeplay Software Limited
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  For your convenience, a copy of the License has been included in this
 *  repository.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  SYCL-BLAS: BLAS implementation using SYCL
 *
 *  @filename blas1_test.hpp
 *
 **************************************************************************/

#ifndef BLAS1_TEST_HPP
#define BLAS1_TEST_HPP

#include <cmath>
#include <complex>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <interface/blas1_interface_sycl.hpp>

using namespace blas;

template <typename ClassName>
struct option_size;
namespace {
static const size_t RANDOM_SIZE = UINT_MAX;
static const size_t RANDOM_STRD = UINT_MAX;
}  // namespace special
#define REGISTER_SIZE(size, test_name)          \
  template <>                                   \
  struct option_size<class test_name> {         \
    static constexpr const size_t value = size; \
  };
template <typename ClassName>
struct option_strd;
#define REGISTER_STRD(strd, test_name)          \
  template <>                                   \
  struct option_strd<class test_name> {         \
    static constexpr const size_t value = strd; \
  };
template <typename ScalarT, typename ClassName>
struct option_prec;
#define REGISTER_PREC(type, val, test_name)   \
  template <>                                 \
  struct option_prec<type, class test_name> { \
    static constexpr const type value = val;  \
  };

// Wraps the above arguments into one template parameter.
// We will treat template-specialized blas_templ_struct as a single class
template <class ScalarT_, class Device_>
struct blas_templ_struct {
  using scalar_t = ScalarT_;
  using device_t = Device_;
};
// A "using" shortcut for the struct
template <class ScalarT_, class Device_ = SYCLDevice>
using blas1_test_args = blas_templ_struct<ScalarT_, Device_>;

// the test class itself
template <class B>
class BLAS1_Test;

template <class ScalarT_, class Device_>
class BLAS1_Test<blas1_test_args<ScalarT_, Device_>> : public ::testing::Test {
 public:
  using ScalarT = ScalarT_;
  using Device = Device_;

  BLAS1_Test() {}

  virtual ~BLAS1_Test() {}
  virtual void SetUp() {}
  virtual void TearDown() {}

  static size_t rand_size() {
    // make sure the generated number is not too big for a type
    // i.e. we do not want the sample size to be too big because of
    // precision/memory restrictions
    int max_size = 19 + 3 * std::log2(sizeof(ScalarT) / sizeof(float));
    int max_rand = std::log2(RAND_MAX);
    return rand() >> (max_rand - max_size);
  }

  // it is important that all tests are run with the same test size
  // so each time we access this function within the same program, we get the
  // same
  // randomly generated size
  size_t test_size() {
    static bool first = true;
    static size_t N;
    if (first) {
      first = false;
      N = rand_size();
    }
    return N;
  }

  // getting the stride in the same way as the size above
  size_t test_strd() {
    static bool first = true;
    static size_t N;
    if (first) {
      first = false;
      N = ((rand() & 1) * (rand() % 5)) + 1;
    }
    return N;
  }

  template <typename DataType,
            typename value_type = typename DataType::value_type>
  static void set_rand(DataType &vec, size_t _N) {
    value_type left(-1), right(1);
    for (size_t i = 0; i < _N; ++i) {
      vec[i] = value_type(rand() % int(int(right - left) * 16)) * .03125 - right;
    }
  }

  template <typename DataType,
            typename value_type = typename DataType::value_type>
  static void print_cont(const DataType &vec, size_t _N,
                         std::string name = "vector") {
    std::cout << name << ": ";
    for (size_t i = 0; i < _N; ++i) std::cout << vec[i] << " ";
    std::cout << std::endl;
  }

  template <typename DataType,
            typename value_type = typename DataType::value_type>
  static cl::sycl::buffer<value_type, 1> make_buffer(DataType &vec) {
    return cl::sycl::buffer<value_type, 1>(vec.data(), vec.size());
  }

  template <typename value_type>
  static vector_view<value_type, cl::sycl::buffer<value_type>> make_vview(
      cl::sycl::buffer<value_type, 1> &buf) {
    return vector_view<value_type, cl::sycl::buffer<value_type>>(buf);
  }

  template <typename DeviceSelector,
            typename = typename std::enable_if<
                std::is_same<Device, SYCLDevice>::value>::type>
  static cl::sycl::queue make_queue(DeviceSelector s) {
    return cl::sycl::queue(s, [=](cl::sycl::exception_list eL) {
      try {
        for (auto &e : eL) {
          std::rethrow_exception(e);
        }
      } catch (cl::sycl::exception &e) {
        std::cout << "E " << e.what() << std::endl;
      } catch (std::exception &e) {
        std::cout << "Standard Exception " << e.what() << std::endl;
      } catch (...) {
        std::cout << " An exception " << std::endl;
      }
    });
  }
};

// unpacking the parameters within the test function
// B is blas_templ_struct
// TestClass is BLAS1_Test<B>
// T is default (scalar) type for the test (e.g. float, double)
// C is the container type for the test (e.g. std::vector)
// E is the executor kind for the test (sequential, openmp, sycl)
#define B1_TEST(name) TYPED_TEST(BLAS1_Test, name)
#define UNPACK_PARAM(test_name)                 \
  using ScalarT = typename TypeParam::scalar_t; \
  using TestClass = BLAS1_Test<TypeParam>;      \
  using Device = typename TypeParam::device_t;  \
  using test = class test_name;
// TEST_SIZE determines the size based on the suggestion
#define TEST_SIZE                                                       \
  ((option_size<test>::value == ::RANDOM_SIZE) ? TestClass::test_size() \
                                               : option_size<test>::value)
#define TEST_STRD                                                       \
  ((option_strd<test>::value == ::RANDOM_STRD) ? TestClass::test_strd() \
                                               : option_strd<test>::value)
// TEST_PREC determines the precision for the test based on the suggestion for
// the type
#define TEST_PREC option_prec<ScalarT, test>::value

#endif /* end of include guard: BLAS1_TEST_HPP */