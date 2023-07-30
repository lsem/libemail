/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FOLLY_URI_H_
#error This file may only be included from folly/Uri.h
#endif

#include <functional>
#include <tuple>

// #include <folly/Conv.h>
// #include <folly/hash/Hash.h>

namespace folly {

namespace uri_detail {

using UriTuple = std::tuple<
    const std::string&,
    const std::string&,
    const std::string&,
    const std::string&,
    uint16_t,
    const std::string&,
    const std::string&,
    const std::string&>;

inline UriTuple as_tuple(const folly::Uri& k) {
  return UriTuple(
      k.scheme(),
      k.username(),
      k.password(),
      k.host(),
      k.port(),
      k.path(),
      k.query(),
      k.fragment());
}

} // namespace uri_detail


template <class String>
String Uri::toString() const {
  String str;
  if (hasAuthority_) {
    str += scheme_;
    str += "://";
    if (!password_.empty()) {
      //toAppend(username_, ":", password_, "@", &str);
      str += username_;
      str += ":";
      str += password_;
      str += "@";
    } else if (!username_.empty()) {
      //toAppend(username_, "@", &str);
      str += username_;
      str += "@";
    }
    //toAppend(host_, &str);
    str += host_;
    if (port_ != 0) {
      str += ":";
      str += port_;
      //toAppend(":", port_, &str);
    }
  } else {
    //toAppend(scheme_, ":", &str);
    str += scheme_;
  }
  //toAppend(path_, &str);
  str += path_;
  if (!query_.empty()) {
    //toAppend("?", query_, &str);
    str += query_;    
  }
  if (!fragment_.empty()) {
    //toAppend("#", fragment_, &str);
    str += "#";
    str += fragment_;
  }
  return str;
}

} // namespace folly

namespace std {

// template <>
// struct hash<folly::Uri> {
//   std::size_t operator()(const folly::Uri& k) const {
//     return std::hash<folly::uri_detail::UriTuple>{}(
//         folly::uri_detail::as_tuple(k));
//   }
// };

// template <>
// struct equal_to<folly::Uri> {
//   bool operator()(const folly::Uri& a, const folly::Uri& b) const {
//     return folly::uri_detail::as_tuple(a) == folly::uri_detail::as_tuple(b);
//   }
// };

} // namespace std
