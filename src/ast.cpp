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

#include <QJsonArray>
#include "ast.h"
#include "logger.h"
#include "lsclient.h"

//---------------------------------------------------------
//   dumpNode
//---------------------------------------------------------

void dumpNode(const ASTNode& node, int level) {
      std::string indent;
      for (int i = 0; i < level; ++i)
            indent += "    ";
      Debug("{}{}", indent, node.kind);
      for (auto c : node.children)
            dumpNode(c, ++level);
      }

//---------------------------------------------------------
//   read
//---------------------------------------------------------

void ASTNode::read(const json& v) {
      //      Debug("{}", v.dump(4));
      arcana = QString::fromStdString(v.value("arcana", ""));
      kind   = QString::fromStdString(v.value("kind", ""));
      role   = QString::fromStdString(v.value("role", ""));
      detail = QString::fromStdString(v.value("detail", ""));
      if (v.contains("range")) {
            Range range(v["range"]);
            p1 = range.start;
            p2 = range.end;
            }
      if (!v.contains("children"))
            return;
      for (auto& c : v["children"]) {
            ASTNode& node = children.emplace_back();
            node.read(c);
            }
      }
