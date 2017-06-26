#ifndef __RAD50_H_
#define __RAD50_H_

#include <cstdint>
#include <string>

namespace RT11FS {

class Rad50
{
public:
  static auto fromRad50(uint16_t rad50) -> std::string;
  static auto toRad50(const std::string &str, uint16_t &out) -> bool;
};

}

#endif
