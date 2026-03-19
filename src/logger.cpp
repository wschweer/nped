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
#include "logger.h"
namespace Logger {

bool verbose = true;

//---------------------------------------------------------
//   Logger::write
//---------------------------------------------------------

void Logger::write(std::ostream& f, MsgType t, const MsgLogContext& c, const std::string& msg) {
      string type;

      switch (t) {
            case MsgType::Log:
            case MsgType::Printf: f << format("{}\n", msg); return;
            case MsgType::Debug: type = "Debug"; break;
            case MsgType::Info: type = "Info"; break;
            case MsgType::Warning: type = "Warning"; break;
            case MsgType::Critical: type = "Critical"; break;
            case MsgType::Fatal: type = "Fatal"; break;
            }
      if (&f == &std::cerr) {
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

Logger::Logger() {
      const char* logfile = getenv("LOGFILE");
      if (!logfile)
            logfile = "/home/ws/LOG";
      f.open(logfile);
      if (!f) {
            cerr << "cannot open logfile <" << logfile << ">==\n";
            exit(-1);
            }
      //      else
      //            cerr << format("writing into logfile <{}>\n", logfile);
      }

void Logger::write(MsgType t, const MsgLogContext& c, const std::string& msg) {
      switch (t) {
            case MsgType::Log: // write only into log file
                  break;
            case MsgType::Debug:
            case MsgType::Warning:
            case MsgType::Info:
                  if (!verbose)
                        break;
                  [[fallthrough]];
            case MsgType::Critical:
            case MsgType::Fatal:
            case MsgType::Printf: write(std::cerr, t, c, msg); break;
            }
      if (f)
            write(f, t, c, msg);
      if (t == MsgType::Fatal)
            abort();
      }

Logger logger;

      }; // namespace Logger
