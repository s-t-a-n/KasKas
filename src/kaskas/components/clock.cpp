#include "kaskas/io/implementations/DS3231_RTC_EEPROM.hpp"

namespace kaskas::io::clock {

struct DS3231Clock::ds3231_singleton* DS3231Clock::ds3231_singleton = nullptr;

} // namespace kaskas::io::clock