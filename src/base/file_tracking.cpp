#pragma once

#include <base/file_tracking.hpp>

#include <base/unicode_utils.hpp>
#include <sys/utime.h>

#include <fstream>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace {

using fn = void (*)();

fn s_suspend;
fn s_resume;

}  // namespace

namespace bcache {
namespace file {

void find_tracking_module() {
  for (const auto& name : {L"FileTracker64.dll", L"FileTracker32.dll", L"FileTracker.dll"}) {
    auto handle = GetModuleHandleW(name);
    if (handle) {
      s_suspend = (fn)GetProcAddress(handle, "SuspendTracking");
      s_resume = (fn)GetProcAddress(handle, "ResumeTracking");
      return;
    }
  }
}

void suspend_tracking() {
  if (s_suspend) {
    s_suspend();
  }
}

void resume_tracking() {
  if (s_resume) {
    s_resume();
  }
}

void force_register_write(const std::string& path) {
  auto handle = CreateFileW(utf8_to_ucs2(path).c_str(), GENERIC_WRITE, 0, 0, OPEN_ALWAYS, 0, 0);
  CloseHandle(handle);
}

}  // namespace file
}  // namespace bcache
