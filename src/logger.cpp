//=============================================================================
//  nped Program Editor
//
//  Copyright (C) 2025-2026 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

#include <format>
#include <ostream>
#include <string>
#include "logger.h"
namespace Logger {

bool verbose = true;

//---------------------------------------------------------
//   Logger::write
//---------------------------------------------------------

void Logger::write(std::ostream& f, MsgType t, const MsgLogContext& c, const std::string& msg) {
      std::string type;

      switch (t) {
            case MsgType::Printf: f << format("{}\n", msg); return;
            case MsgType::Log: type = "Log"; break;
            case MsgType::Debug: type = "Debug"; break;
            case MsgType::Info: type = "Info"; break;
            case MsgType::Warning: type = "Warning"; break;
            case MsgType::Critical: type = "Critical"; break;
            case MsgType::Fatal: type = "Fatal"; break;
            }

//      std::lock_guard<std::mutex> lock(_mutex);

      if (&f == &std::cerr) {
            // color messages
            if (t == MsgType::Critical)
                  f << std::format("\033[31m{}({}:{}, {}): {}\033[0m\n", type, c.file, c.line, c.function, msg);
            else if (t == MsgType::Warning)
                  f << std::format("\033[33m{}({}:{}, {}): {}\033[0m\n", type, c.file, c.line, c.function, msg);
            else
                  f << std::format("{}({}:{}, {}): {}\n", type, c.file, c.line, c.function, msg);
            }
      else {
            f << std::format("{}({}:{}, {}): {}\n", type, c.file, c.line, c.function, msg);
            }
      }

//---------------------------------------------------------
//   open
//    If open is not called, then no trace files are written
//    <cwd>/.<appName>-log-<pid>
//---------------------------------------------------------

void Logger::open(const char* appName) {
      // lets create lots of tracefiles:
      // std::string s = std::format(".{}-log-{}", appName, getpid());
      std::string s = std::format(".{}.log", appName);
      f.open(s.c_str(), std::ios_base::out);

      if (!f.is_open()) {
            std::cerr << "cannot open logfile <" << s << ">: " << strerror(errno) << "\n";
            }
      }

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void Logger::write(MsgType t, const MsgLogContext& c, const std::string& msg) {
      switch (t) {
            case MsgType::Log:      // log only into file
                  break;
            case MsgType::Debug:
            case MsgType::Warning:
            case MsgType::Info:
                  if (!verbose)     // verbose==true logs into file
                        break;
                  [[fallthrough]]; // always show
            case MsgType::Critical:
            case MsgType::Fatal:
            case MsgType::Printf:
                  //
                  write(std::cerr, t, c, msg);
                  break;
            }
      if (f.is_open()) {
            write(f, t, c, msg);
            //  flush(f); // this slows down things a bit but saves last messages before a crash
            }
      }

Logger logger;

      }; // namespace Logger
