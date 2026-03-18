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
#include <array>
#include <QColor>

struct MarkerDefinition {
      QColor fg; // text color
      QColor bg; // text background
      bool bold;
      bool italic;
      };

enum class Marker { Normal, Flow, Type, Comment, String, Search, SearchHit };

class MarkerDefinitions : public std::array<MarkerDefinition, 7>
      {
    public:
      void setDarkMode(bool dark) {
            (*this)[0] = {dark ? QColor(200, 200, 200) : QColor(0, 0, 0), QColor(), false, false}; // normal
            (*this)[1] = {dark ? QColor(100, 150, 255) : QColor(0, 0, 150), QColor(), true, false}; // flow
            (*this)[2] = {dark ? QColor(200, 200, 100) : QColor(0, 0, 150), QColor(), false, false}; // type
            (*this)[3] = {dark ? QColor(100, 200, 100) : QColor(0, 100, 0), QColor(), false, true}; // comment
            (*this)[4] = {dark ? QColor(200, 100, 100) : QColor(150, 0, 0), QColor(), false, false}; // string
            (*this)[5] = {dark ? QColor(0, 0, 0) : QColor(255, 255, 255), dark ? QColor(150, 150, 150) : QColor(120, 120, 120), false, false}; // search
            (*this)[6] = {dark ? QColor(0, 0, 0) : QColor(255, 255, 255), dark ? QColor(150, 150, 220) : QColor(120, 120, 220), false, false}; // search hit
            }
      const MarkerDefinition* md(const Marker m) const { return &(*this)[size_type(m)]; }
      };

extern MarkerDefinitions markerDefinitions;
