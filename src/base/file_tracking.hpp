#pragma once

#include <string>

namespace bcache {
namespace file {

void find_tracking_module();
void suspend_tracking();
void resume_tracking();
void force_register_write(const std::string& path);

}  // namespace file
}  // namespace bcache
