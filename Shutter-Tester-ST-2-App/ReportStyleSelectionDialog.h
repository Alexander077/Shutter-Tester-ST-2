/*
 * Copyright (C) 2026 Alexander Litvinov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <QDialog>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QJsonObject>
#include <QJsonArray>

class ReportStyleSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReportStyleSelectionDialog(bool showCurtainOptions = false, QWidget *parent = nullptr);
    ~ReportStyleSelectionDialog();

    // Returns the selected profile name, or empty string if cancelled
    QString selectedProfileName() const;
    // Returns the selected profile JSON object, or empty object if not found
    QJsonObject selectedProfile() const;

    // Curtain table options (only relevant when showCurtainOptions is true)
    bool printCurtainTable() const;
    bool curtainTableOnSecondPage() const;

    // Speed row rendering options
    bool renderUntestedSpeedRows() const;

private:
    void loadConfigs();

    QComboBox *m_profileCombo;
    QPushButton *m_okBtn;
    QPushButton *m_cancelBtn;
    QCheckBox *m_printCurtainTableCheckbox = nullptr;
    QCheckBox *m_curtainTableSecondPageCheckbox = nullptr;
    QCheckBox *m_renderUntestedSpeedRowsCheckbox = nullptr;
    QJsonArray m_configs;
};
