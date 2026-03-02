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

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QFormLayout>
#include "agent.h"

//---------------------------------------------------------
//   ConfigDialog
//---------------------------------------------------------

class ConfigDialog : public QDialog
      {
      Q_OBJECT

      QListWidget* m_modelList;
      QLineEdit* m_nameEdit;
      QLineEdit* m_urlEdit;
      QLineEdit* m_keyEdit;
      QLineEdit* m_idEdit; // Model Identifier (z.B. llama3)
      Agent* m_agent;

    private slots:
      void addNewModel();
      void deleteSelectedModel();
      void onModelSelectionChanged();
      void updateModelData(); // Speichert Feldänderungen in die Liste
      void saveSettings();

    public:
      explicit ConfigDialog(Agent* a, QWidget* parent = nullptr);
      };
