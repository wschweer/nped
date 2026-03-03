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

#include <functional>
#include <vector>
#include <QHash>
#include <QByteArray>
#include <QAbstractListModel>
#include <QObject>
#include <QVariant>
#include <QModelIndex>
#include "logger.h"

//---------------------------------------------------------
//   VectorModel
//    allows an arbitrary vector to be an Qt ListModel
//---------------------------------------------------------

template <typename T> class VectorModel : public QAbstractListModel
      {
      QHash<int, QByteArray> roles;
      using ffunc = std::function<QVariant(T*)>;

      std::vector<ffunc> functions;
      inline static int role = 0;

    protected:
      std::vector<T*> v;

    public:
      VectorModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}
      virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
            switch (role) {
                  case Qt::DisplayRole: return functions[0](v[index.row()]);
                  default:
                        if (roles.contains(role))
                              return functions[role - Qt::UserRole](v[index.row()]);
                        break;
                  }
            return QVariant();
            };
      int columnCount([[maybe_unused]] const QModelIndex& p = QModelIndex()) const override { return 1; }
      int rowCount([[maybe_unused]] const QModelIndex& p = QModelIndex()) const override { return v.size(); }
      QHash<int, QByteArray> roleNames() const override { return roles; }
      void addRole(QByteArray a, ffunc f) {
            roles[role + Qt::UserRole] = a;
            functions.push_back(f);
            ++role;
            }
      void set(std::vector<T*>& val) {
            beginResetModel();
            v = val;
            endResetModel();
            }
      void beginReset() { beginResetModel(); }
      void push_back(T* data) { v.push_back(data); }
      void endReset() { endResetModel(); }
      virtual const std::vector<T*>& data() const { return v; }
      virtual std::vector<T*>& data() { return v; }
      virtual const T* data(size_t idx) const {
            if (idx >= v.size())
                  return nullptr;
            return v[idx];
            }
      virtual T* data(size_t idx) {
            if (idx >= v.size())
                  return nullptr;
            return v[idx];
            }
      void clear() {
            beginResetModel();
            for (auto i : v)
                  delete i;
            v.clear();
            endResetModel();
            }
      void insertRow(int row, const T* l) {
            beginInsertRows(QModelIndex(), row, row);
            v.insert(v.begin() + row, l);
            endInsertRows();
            }
      bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override {
            beginRemoveRows(parent, row, row + count - 1);
            v.erase(v.begin() + row, v.begin() + row + count);
            endRemoveRows();
            return true;
            }
      };
