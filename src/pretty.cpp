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

#include <vector>
#include <QByteArray>
#include <QDateTime>

#include "logger.h"
#include "editor.h"
#include "file.h"
#include "kontext.h"

//---------------------------------------------------------
//   markToken
//---------------------------------------------------------

void Line::markCppToken(int col1, int col2, const QString& token) {
      static const std::vector<const char*> flowKeywords {
         "if",      "else", "for",   "while",  "do",      "continue",  "break",
         "return",  "try",  "catch", "class",  "struct",  "namespace", "switch",
         "default", "case", "enum",  "public", "private", "protected"};
      static const std::vector<const char*> typeKeywords {
         "void",  "bool",   "int",         "char",         "float",      "double",  "const",
         "auto",  "static", "static_cast", "dynamic_cast", "const_cast", "new",     "delete",
         "using", "true",   "false",       "nullptr",      "constexpr",  "virtual", "override"};

      for (const auto& key : flowKeywords) {
            if (key == token) {
                  addMark(col1, col2, TextStyle::Flow);
                  return;
                  }
            }
      for (const auto& key : typeKeywords) {
            if (key == token) {
                  addMark(col1, col2, TextStyle::Type);
                  return;
                  }
            }
      }

//---------------------------------------------------------
//   makePretty
//---------------------------------------------------------

void File::makePretty() {
      if (languageId() == "cpp") // we currently handle only cpp
            markCpp();
      }

//---------------------------------------------------------
//   markCpp
//---------------------------------------------------------

void File::markCpp() {
      bool inComment = false;
      bool inString  = false;
      bool inRString = false;
      QString rStringPattern;

      bool inChar = false;
      //TODO      for (auto& l : _viewMode == ViewMode::GitVersion ? _gitVersion : _fileText) {
      for (auto& l : _fileText) {
            l.clearPrettyMarks();
            int col1 = 0;
            for (int i = 0; i < l.size(); ++i) {
                  if (inComment) {
                        if ((l[i] == '*') && (i < (l.size() - 1)) && (l[i + 1] == '/')) {
                              l.addMark(col1, ++i, TextStyle::Comment);
                              inComment = false;
                              }
                        }
                  else if (inRString) {
                        if (i < l.size() - rStringPattern.size() &&
                            l.qstring().mid(i).startsWith(rStringPattern)) {
                              inRString  = false;
                              i         += rStringPattern.size();
                              l.addMark(col1, i, TextStyle::String);
                              }
                        }
                  else if (inString) {
                        if (l[i] == '\\' && i < (l.size() - 1))
                              ++i;
                        else if (l[i] == '"') {
                              l.addMark(col1, ++i, TextStyle::String);
                              inString = false;
                              }
                        }
                  else if (inChar) {
                        if (l[i] == '\\' && i < (l.size() - 1))
                              ++i;
                        else if (l[i] == '\'') {
                              l.addMark(col1, ++i, TextStyle::String);
                              inChar = false;
                              }
                        }
                  //  look for 'R"xxx(' where xxx is rStringPattern
                  else if (l[i] == 'R' && i < (l.size() - 2) && l[i + 1] == '"') {
                        inRString       = true;
                        col1            = i;
                        rStringPattern  = "";
                        i              += 2; // skip the 'R"'
                        while (i < l.size() && l[i] != '(')
                              rStringPattern += l[i++];
                        rStringPattern = ')' + rStringPattern + "\"";
                        }
                  else if (l[i].isLetter() || l[i] == '_') {
                        QString token;
                        int col1  = i;
                        token    += l[i++];
                        while (i < l.size() && (l[i].isLetterOrNumber() || l[i] == '_'))
                              token += l[i++];
                        l.markCppToken(col1, i, token);
                        }
                  else if (l[i] == '/' && i < (l.size() - 1)) {
                        if (l[i + 1] == '/') {
                              l.addMark(i, l.size(), TextStyle::Comment);
                              break;
                              }
                        else if (l[i + 1] == '*') {
                              inComment = true;
                              col1      = i;
                              }
                        }
                  else if (l[i] == '"') {
                        col1     = i;
                        inString = true;
                        }
                  else if (l[i] == '\'') {
                        col1   = i;
                        inChar = true;
                        }
                  }
            if ((l.size() - col1) > 0) {
                  if (inComment)
                        l.addMark(col1, l.size(), TextStyle::Comment);
                  else if (inRString)
                        l.addMark(col1, l.size(), TextStyle::String);
                  }
            }
      }
