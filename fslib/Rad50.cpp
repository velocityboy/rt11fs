// Copyright 2017 Jim Geist. This software is licensed under the 
// MIT license as described in the file LICENSE.txt.

#include "Rad50.h"

#include <sstream>
#include <stdexcept>

using std::ostringstream;
using std::runtime_error;
using std::string;

namespace RT11FS {

namespace {
const auto charset = string {" ABCDEFGHIJKLMNOPQRSTUVWXYZ$.%0123456789"};
const auto BASE = 050;
}

auto Rad50::fromRad50(uint16_t rad50) -> string
{
  auto c0 = charset.substr(rad50 / (BASE * BASE), 1);
  auto c1 = charset.substr((rad50 / BASE) % BASE, 1);
  auto c2 = charset.substr(rad50 % BASE, 1);

  return c0 + c1 + c2;
}

auto Rad50::toRad50(const string &str, uint16_t &out) -> bool
{
  if (str.size() != 3) {
    return false;
  }

  auto result = uint16_t {0};
  
  for (auto ch : str) {
    auto index = charset.find(ch);
    if (index == string::npos) {
      return false;
    }
    result = result * BASE + index;
  }
  
  out = result;
  return true;
}

}
