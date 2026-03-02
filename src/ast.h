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

#include <QString>
#include <nlohmann/json.hpp>

#include "types.h"

// Alias für bessere Lesbarkeit
using json     = nlohmann::json;

//---------------------------------------------------------
//   ASTNode
//---------------------------------------------------------

struct ASTNode {
      QString role;
      QString kind;
      QString detail;
      QString arcana;
      Pos p1;
      Pos p2;
      std::vector<ASTNode> children;
      void read(const json&);
      };
