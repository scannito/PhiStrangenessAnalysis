#pragma once

#include "Logger.h"

#include "RooGlobalFunc.h"
#include "RooMsgService.h"

#include <charconv>
#include <iostream>
#include <memory>
#include <ostream>
#include <streambuf>
#include <string>
#include <string_view>

// RooFit's own messages, kept out of the way and counted for the end-of-run summary.
//
// Two things, both process-wide. DynamicRooFitter, the only user of RooFit, calls Configure when
// the first one is built: not the entry points, because RooFit is not the whole application's
// and a workflow without fits - efficiency, correlation, comparison - builds no fitter, never
// touches RooFit's message service and has no RooFit section in its summary.
//
//  - everything below WARNING is dropped. The INFO lines ("activating const optimization",
//    "Creation of NLL object took ...") were 35 thousand lines in a purity log of 11 MB;
//  - what is left is counted by kind and handed to Logger::Count. RooFit's warnings run to
//    thousands of lines a run (a background polynomial dipping below zero, a p.d.f
//    evaluating to NaN during minimisation) and used to go by without anyone seeing them.
//
// The text is not moved: it still goes to std::cout exactly as before, so the log keeps
// it whole and can still say which fit it came from. Only a copy is counted.
namespace RooFitMessages
{

namespace Detail
{

// Passes every character on to the buffer it wraps, and watches the lines go by.
class TeeBuf : public std::streambuf
{
 public:
  explicit TeeBuf(std::streambuf* target) : target(target) {}

 protected:
  int overflow(int c) override
  {
    if (c == traits_type::eof())
      return traits_type::not_eof(c);
    const int result = target->sputc(static_cast<char>(c));
    if (c == '\n') {
      EndOfLine();
      line.clear();
    } else {
      line += static_cast<char>(c);
    }
    return result;
  }

  std::streamsize xsputn(const char* s, std::streamsize n) override
  {
    for (std::streamsize i = 0; i < n; ++i)
      overflow(static_cast<unsigned char>(s[i]));
    return n;
  }

  int sync() override { return target->pubsync(); }

 private:
  // RooFit prints an evaluation-error block as a header, then the same indented message up to a
  // dozen times, then "... (remaining 9 messages suppressed)": that last line stands for nine
  // more occurrences of the previous one, which is how many times it really happened.
  void EndOfLine()
  {
    if (line.find_first_not_of(" \t") == std::string::npos)
      return;
    constexpr std::string_view marker = "(remaining ";
    if (const auto at = line.find(marker); at != std::string::npos && line.find(" messages suppressed)") != std::string::npos) {
      std::size_t n = 0;
      std::from_chars(line.data() + at + marker.size(), line.data() + line.size(), n);
      if (n > 0 && !last.empty())
        Logger::Count("RooFit", last, n);
      return;
    }
    Logger::Count("RooFit", line);
    last = line;
  }

  std::streambuf* target;
  std::string line;
  std::string last;
};

} // namespace Detail

// Safe to call more than once, which is how DynamicRooFitter uses it (on every construction): the
// streams that already point at the copy are left alone, so nothing is counted twice.
inline void Configure()
{
  auto& service = RooMsgService::instance();
  service.setGlobalKillBelow(RooFit::WARNING);

  // Never destroyed, on purpose: RooMsgService keeps the pointer for as long as the process
  // lives and may still write while the static objects go.
  static std::ostream* copy = new std::ostream(new Detail::TeeBuf(std::cout.rdbuf()));
  for (int i = 0; i < service.numStreams(); ++i)
    if (service.getStream(i).os == &std::cout)
      service.getStream(i).os = copy;
}

} // namespace RooFitMessages
