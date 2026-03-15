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

#pragma once

#include <format>
#include <fstream>
#include <iostream>
#include <string>
#include <ostream>

using namespace std;

//-------------------------------------------------------
// These fancy templates allow you to use
// enums bits as flags in a somewhat typesafe manner.
// The following operators for enums are enabled:
//    |
//    &
//    |=
//    &=

template <typename T>
      requires std::is_enum_v<T> and requires(std::underlying_type_t<T> x) {
                  { x | x } -> std::same_as<std::underlying_type_t<T>>;
            T(x);
            }
constexpr T operator|(T left, T right) {
      using U = std::underlying_type_t<T>;
      return T(U(left) | U(right));
      }

template <typename T>
      requires std::is_enum_v<T> and requires(std::underlying_type_t<T> x) {
                  { x | x } -> std::same_as<std::underlying_type_t<T>>;
            T(x);
            }
constexpr T operator&(T left, T right) {
      using U = std::underlying_type_t<T>;
      return T(U(left) & U(right));
      }

template <typename T>
      requires std::is_enum_v<T> and requires(T x) {
                  { x | x } -> std::same_as<T>;
            }
constexpr T& operator|=(T& left, T right) {
      return left = left | right;
      }

template <typename T>
      requires std::is_enum_v<T> and requires(T x) {
                  { x & x } -> std::same_as<T>;
            }
constexpr T& operator&=(T& left, T right) {
      return left = left & right;
      }

#include <QString>

//---------------------------------------------------------
//   formatter QString
//---------------------------------------------------------

template <> struct std::formatter<QString> {
      constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
      auto format(const QString& s, auto& ctx) const { return std::format_to(ctx.out(), "{}", s.toStdString()); }
      };

//---------------------------------------------------------
//   formatter QStringView
//---------------------------------------------------------

template <> struct std::formatter<QStringView> {
      constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
      auto format(const QStringView& s, auto& ctx) const { return std::format_to(ctx.out(), "{}", s.toLocal8Bit().data()); }
      };

namespace Logger {
enum class MsgType { Debug, Info, Log, Warning, Critical, Fatal, Printf };

//---------------------------------------------------------
//   MsgLogContext
//---------------------------------------------------------

struct MsgLogContext {
      std::string file;
      int line;
      std::string function;
      };

//---------------------------------------------------------
//   Logger
//---------------------------------------------------------

class Logger
      {
      std::ofstream f;

      void write(std::ostream& f, MsgType t, const MsgLogContext& c, const std::string& msg);

    public:
      Logger();
      void close() { f.close(); }
      void write(MsgType t, const MsgLogContext& c, const std::string& msg);
      };

extern Logger logger;
      } // namespace Logger
#ifdef NDEBUG
#define Debug(msg, ...) ;
#define Info(msg, ...)
#define Log(msg, ...)
#define Warning(msg, ...)
#define Critical(msg, ...)
#define CDebug(cond, msg, ...)
#else
#define Debug(msg, ...)                                                                                                                    \
      do {                                                                                                                                 \
            Logger::logger.write(Logger::MsgType::Debug, {__FILE__, __LINE__, __FUNCTION__}, std::format(msg __VA_OPT__(, ) __VA_ARGS__)); \
            } while (0)
#define CDebug(cond, msg, ...)                                                                                                             \
      do {                                                                                                                                 \
            if (cond)                                                                                                                      \
                  Logger::logger.write(Logger::MsgType::Debug, {__FILE__, __LINE__, __FUNCTION__},                                         \
                                       std::format(msg __VA_OPT__(, ) __VA_ARGS__));                                                       \
            } while (0)
#define Info(msg, ...)                                                                                                                     \
      do {                                                                                                                                 \
            Logger::logger.write(Logger::MsgType::Info, {__FILE__, __LINE__, __FUNCTION__}, std::format(msg __VA_OPT__(, ) __VA_ARGS__));  \
            } while (0)
#define Log(msg, ...)                                                                                                                      \
      do {                                                                                                                                 \
            Logger::logger.write(Logger::MsgType::Log, {__FILE__, __LINE__, __FUNCTION__}, std::format(msg __VA_OPT__(, ) __VA_ARGS__));   \
            } while (0)
#define Warning(msg, ...)                                                                                                                  \
      do {                                                                                                                                 \
            Logger::logger.write(Logger::MsgType::Warning, {__FILE__, __LINE__, __FUNCTION__},                                             \
                                 std::format(msg __VA_OPT__(, ) __VA_ARGS__));                                                             \
            } while (0)
#define Critical(msg, ...)                                                                                                                 \
      do {                                                                                                                                 \
            Logger::logger.write(Logger::MsgType::Critical, {__FILE__, __LINE__, __FUNCTION__},                                            \
                                 std::format(msg __VA_OPT__(, ) __VA_ARGS__));                                                             \
            } while (0)
#define Printf(msg, ...)                                                                                                                   \
      do {                                                                                                                                 \
            Logger::logger.write(Logger::MsgType::Printf, {}, std::format(msg __VA_OPT__(, ) __VA_ARGS__));                                \
            } while (0)
#endif

#define Fatal(msg, ...)                                                                                                                    \
      do {                                                                                                                                 \
            Logger::logger.write(Logger::MsgType::Fatal, {__FILE__, __LINE__, __FUNCTION__}, std::format(msg __VA_OPT__(, ) __VA_ARGS__)), \
                ::abort();                                                                                                                 \
            } while (0)

#define Assert(x)                                                                                                                          \
      do {                                                                                                                                 \
            if (!(x)) {                                                                                                                    \
                  Logger::logger.write(Logger::MsgType::Fatal, {__FILE__, __LINE__, __FUNCTION__}, "Assert <" #x "> failed");              \
                  ::abort();                                                                                                               \
                  }                                                                                                                              \
            } while (0)
