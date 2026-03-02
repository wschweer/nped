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

#include "configdialog.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QDialogButtonBox>
#include "logger.h"

//---------------------------------------------------------
//   ConfigDialog
//---------------------------------------------------------

ConfigDialog::ConfigDialog(Agent* a, QWidget* parent) : QDialog(parent), m_agent(a) {
      auto* mainLayout = new QHBoxLayout(this);
      setModal(true);

      // Linke Seite: Liste und Buttons
      auto* leftLayout = new QVBoxLayout();
      m_modelList      = new QListWidget(this);
      connect(m_modelList, &QListWidget::itemSelectionChanged, this, &ConfigDialog::onModelSelectionChanged);

      auto* btnLayout = new QHBoxLayout();
      auto* addBtn    = new QPushButton("+", this);
      auto* delBtn    = new QPushButton("-", this);
      connect(addBtn, &QPushButton::clicked, this, &ConfigDialog::addNewModel);
      connect(delBtn, &QPushButton::clicked, this, &ConfigDialog::deleteSelectedModel);

      btnLayout->addWidget(addBtn);
      btnLayout->addWidget(delBtn);
      leftLayout->addWidget(m_modelList);
      leftLayout->addLayout(btnLayout);

      // Rechte Seite: Formular
      auto* formLayout = new QFormLayout();
      m_nameEdit       = new QLineEdit(this);
      m_urlEdit        = new QLineEdit(this);
      m_keyEdit        = new QLineEdit(this);
      m_keyEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
      m_idEdit = new QLineEdit(this);

      formLayout->addRow("Anzeigename:", m_nameEdit);
      formLayout->addRow("Base URL:", m_urlEdit);
      formLayout->addRow("API Key:", m_keyEdit);
      formLayout->addRow("Modell ID:", m_idEdit);

      m_modelList->clear();
      for (const auto& m : m_agent->models())
            m_modelList->addItem(m.name);

#if 0
      // Verbindung zum Resultat
      connect(m_agent, &Agent::modelsDiscovered, this, [this](const QStringList& models) {
            if (!models.isEmpty()) {
                  // Setze das erste gefundene Modell als Standard in das Feld
                  m_idEdit->setText(models.first());
                  // Optional: Zeige eine Auswahlbox mit allen Namen (z.B. QInputDialog)
                  //  m_chatDisplay->append("Gefundene Modelle: " + models.join(", "));
                        }
                  });
#endif
      // Speichern/Abbrechen
      auto* dialogBtns = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
      connect(dialogBtns, &QDialogButtonBox::accepted, m_agent, &Agent::saveSettings);
      connect(dialogBtns, &QDialogButtonBox::rejected, this, &QDialog::reject);

      auto* rightLayout = new QVBoxLayout();
      rightLayout->addLayout(formLayout);
      rightLayout->addStretch();
      rightLayout->addWidget(dialogBtns);

      mainLayout->addLayout(leftLayout, 1);
      mainLayout->addLayout(rightLayout, 2);
Debug("{} {}", parent->width(), parent->height());
      move ((parent->width()-width())/2, (parent->height()-height())/2);
Debug("move {} {}", (parent->width()-width())/2, (parent->height()-height())/2);
      }

//---------------------------------------------------------
//   saveSettings
//---------------------------------------------------------

void ConfigDialog::saveSettings() {
      // Aktuelle Feldeingaben für das selektierte Item übernehmen
      int idx         = m_modelList->currentRow();
      auto& m_configs = m_agent->models();
      if (idx >= 0 && idx < m_configs.size()) {
            m_configs[idx].name            = m_nameEdit->text();
            m_configs[idx].baseUrl         = m_urlEdit->text();
            m_configs[idx].apiKey          = m_keyEdit->text();
            m_configs[idx].modelIdentifier = m_idEdit->text();
            }
      m_agent->saveSettings();
      accept();
      }

//---------------------------------------------------------
//   onModelSelectionChanged
//---------------------------------------------------------

void ConfigDialog::onModelSelectionChanged() {
      int idx = m_modelList->currentRow();
      if (idx < 0)
            return;
      const auto& cfg = m_agent->models()[idx];
      m_nameEdit->setText(cfg.name);
      m_urlEdit->setText(cfg.baseUrl);
      m_keyEdit->setText(cfg.apiKey);
      m_idEdit->setText(cfg.modelIdentifier);
      }

//---------------------------------------------------------
//   addNewModel
//---------------------------------------------------------

void ConfigDialog::addNewModel() {
      Model newCfg;
      newCfg.name            = "Ollama";
      newCfg.baseUrl         = "http://localhost:11434"; // Default für Ollama
      newCfg.modelIdentifier = "";

      m_agent->models().append(newCfg);

      auto* item = new QListWidgetItem(newCfg.name);
      m_modelList->addItem(item);
      m_modelList->setCurrentItem(item);
      }

//---------------------------------------------------------
//   deleteSelectedModel
//---------------------------------------------------------

void ConfigDialog::deleteSelectedModel() {
      int row = m_modelList->currentRow();
      if (row >= 0) {
            // Aus Liste und internem Vector entfernen
            delete m_modelList->takeItem(row);
            auto& configs = m_agent->models();
            configs.removeAt(row);

            // Formular leeren wenn nichts mehr selektiert ist
            if (configs.isEmpty()) {
                  m_nameEdit->clear();
                  m_urlEdit->clear();
                  m_keyEdit->clear();
                  m_idEdit->clear();
                  }
            }
      }

//---------------------------------------------------------
//   updateModelData
//---------------------------------------------------------

void ConfigDialog::updateModelData() {
      int idx = m_modelList->currentRow();

      auto& configs = m_agent->models();
      if (idx < 0 || idx >= configs.size())
            return;

      // Daten aus den LineEdits zurück in das Config-Objekt schreiben
      configs[idx].name            = m_nameEdit->text();
      configs[idx].baseUrl         = m_urlEdit->text();
      configs[idx].apiKey          = m_keyEdit->text();
      configs[idx].modelIdentifier = m_idEdit->text();

      // Namen in der Liste links ebenfalls aktualisieren
      m_modelList->item(idx)->setText(m_nameEdit->text());
      }
