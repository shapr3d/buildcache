//--------------------------------------------------------------------------------------------------
// Copyright (c) 2018 Marcus Geelnard
//
// This software is provided 'as-is', without any express or implied warranty. In no event will the
// authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose, including commercial
// applications, and to alter it and redistribute it freely, subject to the following restrictions:
//
//  1. The origin of this software must not be misrepresented; you must not claim that you wrote
//     the original software. If you use this software in a product, an acknowledgment in the
//     product documentation would be appreciated but is not required.
//
//  2. Altered source versions must be plainly marked as such, and must not be misrepresented as
//     being the original software.
//
//  3. This notice may not be removed or altered from any source distribution.
//--------------------------------------------------------------------------------------------------

#ifndef BUILDCACHE_STRING_LIST_HPP_
#define BUILDCACHE_STRING_LIST_HPP_

#include <base/unicode_utils.hpp>

#include <initializer_list>
#include <string>
#include <vector>

#if _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#undef STRICT
#undef ERROR
#undef min
#undef max
#endif

namespace bcache {
class string_list_t {
public:
  typedef typename std::vector<std::string>::iterator iterator;
  typedef typename std::vector<std::string>::const_iterator const_iterator;

  /// @brief Construct an empty list.
  string_list_t() {
  }

  /// @brief Construct a list from an initializer list.
  /// @param list The strings to add to the string list object.
  string_list_t(std::initializer_list<std::string> list) : m_args(list) {
  }

  /// @brief Construct a list from command line arguments.
  /// @param argc Argument count.
  /// @param argv Argument array.
  /// @note This is useful for handling command line arguments from a program main() function.
  string_list_t(const int argc, const char** argv) {
    for (int i = 0; i < argc; ++i) {
      m_args.emplace_back(std::string(argv[i]));
    }
  }

  /// @brief Construct a list from a delimited string.
  /// @param str The string.
  /// @param delimiter The delimiter.
  /// @note This is useful for splitting a string into a list of strings (e.g. the PATH environment
  /// variable).
  string_list_t(const std::string& str, const std::string& delimiter) {
    std::string::size_type current_str_start = 0u;
    while (current_str_start < str.size()) {
      const auto pos = str.find(delimiter, current_str_start);
      if (pos == std::string::npos) {
        m_args.emplace_back(str.substr(current_str_start));
        current_str_start = str.size();
      } else {
        m_args.emplace_back(str.substr(current_str_start, pos - current_str_start));
        current_str_start = pos + 1;
      }
    }
  }

  /// @brief Construct a list of arguments from a string with a shell-like format.
  /// @param cmd The string.
  /// @note As far as possible this routine mimics the standard shell behaviour, e.g. w.r.t.
  /// escaping and quotation.
  static string_list_t split_args(const std::string& cmd) {
    string_list_t args;

#ifdef _WIN32
    std::wstring wcmd = utf8_to_ucs2(cmd);
    int argc;
    wchar_t** argv = CommandLineToArgvW(wcmd.data(), &argc);
    for (int i = 0; i < argc; ++i) {
      args += ucs2_to_utf8(argv[i]);
    }

#else
    std::string arg;
    auto is_inside_quote = false;
    auto has_arg = false;
    char last_char = 0;
    for (auto& chr : cmd) {
      const auto is_space = (chr == ' ');
      const auto is_quote = (chr == '\"') && (last_char != '\\');

      if (is_quote) {
        is_inside_quote = !is_inside_quote;
      }

      // Start of a new argument?
      if ((!has_arg) && (!is_space)) {
        has_arg = true;
      }

      // Append this char to the argument string?
      if ((is_inside_quote || !is_space) && !is_quote) {
        arg += chr;
      }

      // End of argument?
      if (has_arg && is_space && !is_inside_quote) {
        args += unescape_arg(arg);
        arg.clear();
        has_arg = false;
      }

      last_char = chr;
    }

    if (has_arg) {
      args += unescape_arg(arg);
    }

#endif
    return args;
  }

  /// @brief Join all elements into a single string.
  /// @param separator The separator to use between strings (e.g. " ").
  /// @param escape Set this to true to escape each string.
  /// @returns a string containing all the strings of the list.
  /// @note When string escaping is enabled, the strings are escaped in a way that preserves
  /// command line argument information. E.g. strings that contain spaces are surrounded by quotes.
  std::string join(const std::string& separator, const bool escape = false) const {
    std::string result;
    for (auto arg : m_args) {
      const auto escaped_arg = escape ? escape_arg(arg) : arg;
      if (result.empty()) {
        result = result + escaped_arg;
      } else {
        result = result + separator + escaped_arg;
      }
    }
    return result;
  }

  /// @brief Remove all the elements.
  void clear() {
    m_args.clear();
  }

  std::string& operator[](const size_t idx) {
    return m_args[idx];
  }

  const std::string& operator[](const size_t idx) const {
    return m_args[idx];
  }

  string_list_t& operator+=(const std::string& str) {
    m_args.emplace_back(str);
    return *this;
  }

  string_list_t& operator+=(const string_list_t& list) {
    for (const auto& str : list) {
      m_args.emplace_back(str);
    }
    return *this;
  }

  string_list_t operator+(const std::string& str) const {
    string_list_t result(*this);
    result += str;
    return result;
  }

  string_list_t operator+(const string_list_t& list) const {
    string_list_t result(*this);
    result += list;
    return result;
  }

  size_t size() const {
    return m_args.size();
  }

  iterator begin() {
    return m_args.begin();
  }

  const_iterator begin() const {
    return m_args.begin();
  }

  const_iterator cbegin() const {
    return m_args.cbegin();
  }

  iterator end() {
    return m_args.end();
  }

  const_iterator end() const {
    return m_args.end();
  }

  const_iterator cend() const {
    return m_args.cend();
  }

private:
  static std::string escape_arg(const std::string& arg) {
    std::string escaped_arg;

    // These escaping rules try to match the most common Un*x shell conventions, e.g. as outlined
    // here: http://faculty.salina.k-state.edu/tim/unix_sg/shell/metachar.html
    auto needs_quotes = false;
    for (auto c : arg) {
      if (c == '"') {
        escaped_arg += "\\\"";
      } else if (c == '\\') {
#ifdef _WIN32
        // On Windows a backslash has semantic meaning and does not require escaping.
        escaped_arg += c;
#else
        escaped_arg += "\\\\";
#endif
      } else if (c == '$') {
        escaped_arg += "\\$";
        needs_quotes = true;
      } else if (c == '`') {
        escaped_arg += "\\`";
        needs_quotes = true;
      } else {
        if (c == ' ' || c == '&' || c == ';' || c == '>' || c == '<' || c == '|' || c == '(' ||
            c == ')' || c == '*' || c == '#') {
          needs_quotes = true;
        }
        escaped_arg += c;
      }
    }

    // Do we need to surround with quotes?
    if (needs_quotes) {
      escaped_arg = std::string("\"") + escaped_arg + std::string("\"");
    }

    return escaped_arg;
  }

  static std::string unescape_arg(const std::string& arg) {
    std::string unescaped_arg;

    auto is_escaped = false;
    for (auto c : arg) {
      if ((c == '\\') && !is_escaped) {
        is_escaped = true;
      } else {
        // TODO(m): We should handle different escape codes. The current solution at least works for
        // converting \\ -> \ and \" -> ".
        unescaped_arg += c;
        is_escaped = false;
      }
    }

    return unescaped_arg;
  }

  std::vector<std::string> m_args;
};
}  // namespace bcache

#endif  // BUILDCACHE_STRING_LIST_HPP_
