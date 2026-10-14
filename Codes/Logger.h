#pragma once

#include <cstddef>
#include <ctime>
#include <exception>
#include <format>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// A deliberately small logger: levels, one line format, and - the reason it exists -
// a count of the warnings of the run, printed once at the end.
//
// The chain has checks that throw and diagnostics that must not (see CLAUDE.md,
// "Checks versus documentation"). A diagnostic that only scrolls past in a log of a
// million bytes is as good as silent; the summary is what brings it back in front of
// whoever reads the end of the log. That is the whole feature. Nothing here is
// asynchronous, rotates files or has more than one destination, and the code is
// single-threaded, so none of it is thread-safe.
//
// INFO and DEBUG go to stdout, WARN and ERROR to stderr. runAnalysis.sh merges the
// two with `2>&1 | tee`, so the log file still holds everything, in order.
//
// The format string is checked when the line is printed and not when the code is
// compiled, unlike std::format. That is the price of std::vformat, which is what lets
// a numeric spec such as {:.2f} work under Cling, where std::format rejects it (see
// CLAUDE.md, "Cling constraints"). Text that is not a literal goes in as an argument,
// never as the format string: Logger::Warn("{}", message), because braces in `message`
// would otherwise be read as placeholders.
//
// Not a replacement for throwing: a failed check still throws, with its message.
namespace Logger
{

enum class Level { Debug = 0, Info = 1, Warn = 2, Error = 3 };

namespace Detail
{

struct State {
  Level threshold{Level::Info};
  // Off by default: with it on every line looks different from run to run, and two logs
  // can no longer be compared as they are. Worth turning on for a long run.
  bool timestamps{false};
  // Distinct warnings in the order first seen, with how often each occurred. The same
  // warning inside a loop over bins would otherwise fill the summary with copies.
  std::vector<std::pair<std::string, std::size_t>> warnings;
  std::map<std::string, std::size_t> warningIndex;
  std::size_t totalWarnings{0};
  std::size_t totalErrors{0};
};

// Function-local static: one instance however the header is loaded.
inline State& GetState()
{
  static State state;
  return state;
}

constexpr std::size_t kMaxDistinctWarnings = 500;

inline const char* Name(Level level)
{
  switch (level) {
    case Level::Debug:
      return "DEBUG";
    case Level::Info:
      return "INFO";
    case Level::Warn:
      return "WARNING";
    case Level::Error:
      return "ERROR";
  }
  return "?";
}

inline std::string Timestamp()
{
  // strftime and not std::format("{:%T}"): the chrono format specs are rejected by
  // Cling as well.
  const std::time_t now = std::time(nullptr);
  char buffer[16];
  std::strftime(buffer, sizeof(buffer), "%H:%M:%S", std::localtime(&now));
  return buffer;
}

inline void Emit(Level level, const std::string& message)
{
  State& state = GetState();

  if (level == Level::Warn) {
    state.totalWarnings++;
    if (auto it = state.warningIndex.find(message); it != state.warningIndex.end()) {
      state.warnings[it->second].second++;
    } else if (state.warnings.size() < kMaxDistinctWarnings) {
      state.warningIndex[message] = state.warnings.size();
      state.warnings.emplace_back(message, 1);
    }
  } else if (level == Level::Error) {
    state.totalErrors++;
  }

  if (level < state.threshold)
    return;

  std::ostream& out = (level >= Level::Warn) ? std::cerr : std::cout;
  const std::string prefix = state.timestamps ? std::format("{} [{}] ", Timestamp(), Name(level))
                                              : std::format("[{}] ", Name(level));
  out << prefix << message << std::endl;
}

template <class... Args>
void Dispatch(Level level, std::string_view fmt, Args&... args)
{
  std::string message;
  try {
    message = std::vformat(fmt, std::make_format_args(args...));
  } catch (const std::exception& e) {
    // A logger that throws would turn a diagnostic into a failure. The line is
    // printed as it was written, and says why it could not be formatted.
    message = std::format("{}  [LOGGER: could not format this line: {}]", fmt, e.what());
  }
  Emit(level, message);
}

} // namespace Detail

// Messages below the threshold are not printed, but WARN and ERROR are still counted:
// silencing the output must not hide from the summary that something was said.
inline void SetLevel(Level level) { Detail::GetState().threshold = level; }
inline void SetTimestamps(bool enabled) { Detail::GetState().timestamps = enabled; }

template <class... Args>
void Debug(std::string_view fmt, Args&&... args)
{
  Detail::Dispatch(Level::Debug, fmt, args...);
}

template <class... Args>
void Info(std::string_view fmt, Args&&... args)
{
  Detail::Dispatch(Level::Info, fmt, args...);
}

template <class... Args>
void Warn(std::string_view fmt, Args&&... args)
{
  Detail::Dispatch(Level::Warn, fmt, args...);
}

template <class... Args>
void Error(std::string_view fmt, Args&&... args)
{
  Detail::Dispatch(Level::Error, fmt, args...);
}

inline std::size_t WarningCount() { return Detail::GetState().totalWarnings; }
inline std::size_t ErrorCount() { return Detail::GetState().totalErrors; }

// Prints what was warned about during the run, once per distinct message, with its
// count. Always to stdout, whatever the threshold, and only when there is something
// to say: a run without warnings prints one line saying so, so that its absence in a
// log cannot be mistaken for a summary that was never reached.
inline void PrintSummary()
{
  const Detail::State& state = Detail::GetState();
  std::cout << "------------------------------------------------------" << std::endl;
  if (state.totalWarnings == 0 && state.totalErrors == 0) {
    std::cout << "[SUMMARY] No warnings." << std::endl;
    std::cout << "------------------------------------------------------" << std::endl;
    return;
  }
  std::string header = std::format("[SUMMARY] {} warning(s), {} error(s) logged", state.totalWarnings, state.totalErrors);
  if (state.totalWarnings > 0)
    header += std::format(", {} distinct warning(s):", state.warnings.size());
  std::cout << header << std::endl;
  for (const auto& [message, count] : state.warnings) {
    const std::string marker = count > 1 ? std::format("(x{}) ", count) : std::string("      ");
    std::cout << std::format("  {}{}", marker, message) << std::endl;
  }
  if (state.totalWarnings > 0 && state.warnings.size() == Detail::kMaxDistinctWarnings)
    std::cout << "  ... further distinct warnings were counted but not listed." << std::endl;
  std::cout << "------------------------------------------------------" << std::endl;
}

} // namespace Logger
