#include "ResourceSystem.hpp"
//
#include <sstream>

#include "world/core/Utils.hpp"
#include "zensim/vulkan/Vulkan.hpp"
#include "zensim/zpc_tpls/fmt/format.h"
#include "zensim/zpc_tpls/stb/stb_image.h"

namespace zs {

  StdStreamRedirector g_infoRedirector(1), g_errorRedirector(2);

  void ResourceSystem::start_cstream_capture() {
    g_infoRedirector.start();
    instance()._capturedInfoStr.clear();
    g_errorRedirector.start();
    instance()._capturedErrStr.clear();
  }

  void ResourceSystem::dump_cstream_capture() {
    g_infoRedirector.stop(instance()._capturedInfoStr);
    g_errorRedirector.stop(instance()._capturedErrStr);
  }

  bool ResourceSystem::retrieve_info_cstream(std::string &str) {
    str = instance()._capturedInfoStr;
    return !str.empty();
  }

  bool ResourceSystem::retrieve_error_cstream(std::string &str) {
    str = instance()._capturedErrStr;
    return !str.empty();
  }

  void ResourceSystem::push_assembled_log(std::string_view msg, ConsoleRecord::type_e type) {
    assert(!g_infoRedirector.active() && !g_errorRedirector.active()
           && "only valid to collect captured string after dumping stream!");

    const auto &infoStr = instance()._capturedInfoStr;
    const auto &errorStr = instance()._capturedErrStr;
    int num = (int)(!infoStr.empty()) + (int)(!errorStr.empty()) + (int)(!msg.empty());
    if (num == 1) {
      std::string_view str = msg;
      int cate = type;
      if (!infoStr.empty()) {
        str = infoStr;
        type = ConsoleRecord::info;
      } else if (!errorStr.empty()) {
        str = errorStr;
        type = ConsoleRecord::error;
      }
      std::istringstream ss(str.data());
      std::string line;
      while (std::getline(ss, line, '\n')) push_log(line, type);
    } else if (num > 1) {
      auto process = [](std::string_view tag, std::string_view msg, ConsoleRecord::type_e type) {
        push_log(fmt::format("{:-^60}", tag), type);
        std::istringstream ss(msg.data());
        std::string line;
        while (std::getline(ss, line, '\n')) push_log(line, type);
        push_log(fmt::format("{:-^60}", ""), type);
      };
      if (!msg.empty()) process("python output", msg, type);
      if (!infoStr.empty()) process("c info", infoStr, ConsoleRecord::info);
      if (!errorStr.empty()) process("c error", errorStr, ConsoleRecord::error);
    }
  }

  bool ResourceSystem::push_log(std::string_view msg, ConsoleRecord::type_e type) noexcept {
    auto &entries = instance()._logEntries;
    entries.emplace_back(std::string(msg), type);
    if (entries.size() >= instance()._logEntryLimit) {
      entries.pop_front();
      // is full
      return true;
    }
    // not full
    return false;
  }

}  // namespace zs