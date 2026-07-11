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

#include "ReportStyleSelectionDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFile>
#include <QJsonDocument>
#include <QCoreApplication>
#include <QDir>

ReportStyleSelectionDialog::ReportStyleSelectionDialog(bool showCurtainOptions, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Select Report Style"));
    setMinimumWidth(350);

    loadConfigs();

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    mainLayout->addWidget(new QLabel(tr("Select report style profile:"), this));

    m_profileCombo = new QComboBox(this);
    m_profileCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    for (int i = 0; i < m_configs.size(); ++i) {
        QJsonObject obj = m_configs[i].toObject();
        m_profileCombo->addItem(obj["reportStyleName"].toString());
    }
    // Select default profile if exists
    for (int i = 0; i < m_profileCombo->count(); ++i) {
        if (m_profileCombo->itemText(i) == "Default Report Style") {
            m_profileCombo->setCurrentIndex(i);
            break;
        }
    }
    mainLayout->addWidget(m_profileCombo);

    // Curtain table options (shown only for focal plane shutters)
    if (showCurtainOptions) {
        m_printCurtainTableCheckbox = new QCheckBox(tr("Print curtain table"), this);
        m_printCurtainTableCheckbox->setChecked(true);
        mainLayout->addWidget(m_printCurtainTableCheckbox);

        m_curtainTableSecondPageCheckbox = new QCheckBox(tr("Place curtain table on the second page"), this);
        m_curtainTableSecondPageCheckbox->setChecked(false);
        m_curtainTableSecondPageCheckbox->setEnabled(true);
        mainLayout->addWidget(m_curtainTableSecondPageCheckbox);

        // Disable second checkbox when first is unchecked
        connect(m_printCurtainTableCheckbox, &QCheckBox::toggled, m_curtainTableSecondPageCheckbox, &QCheckBox::setEnabled);
    }

    // Speed row rendering option (always shown)
    m_renderUntestedSpeedRowsCheckbox = new QCheckBox(tr("Render untested speed rows"), this);
    m_renderUntestedSpeedRowsCheckbox->setChecked(false);
    mainLayout->addWidget(m_renderUntestedSpeedRowsCheckbox);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_okBtn = new QPushButton(tr("OK"), this);
    m_okBtn->setDefault(true);
    m_cancelBtn = new QPushButton(tr("Cancel"), this);

    buttonLayout->addWidget(m_okBtn);
    buttonLayout->addWidget(m_cancelBtn);
    mainLayout->addLayout(buttonLayout);

    connect(m_okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

ReportStyleSelectionDialog::~ReportStyleSelectionDialog()
{
}

void ReportStyleSelectionDialog::loadConfigs()
{
    QString configPath = QCoreApplication::applicationDirPath() + QDir::separator() + "report_style_configs.json";
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (doc.isArray()) {
        m_configs = doc.array();
    }
}

QString ReportStyleSelectionDialog::selectedProfileName() const
{
    if (m_profileCombo && m_profileCombo->currentIndex() >= 0) {
        return m_profileCombo->currentText();
    }
    return QString();
}

QJsonObject ReportStyleSelectionDialog::selectedProfile() const
{
    if (!m_profileCombo || m_profileCombo->currentIndex() < 0) {
        return QJsonObject();
    }

    int index = m_profileCombo->currentIndex();
    if (index >= 0 && index < m_configs.size()) {
        return m_configs[index].toObject();
    }
    return QJsonObject();
}

bool ReportStyleSelectionDialog::printCurtainTable() const
{
    return m_printCurtainTableCheckbox ? m_printCurtainTableCheckbox->isChecked() : false;
}

bool ReportStyleSelectionDialog::curtainTableOnSecondPage() const
{
    return m_curtainTableSecondPageCheckbox ? m_curtainTableSecondPageCheckbox->isChecked() : false;
}

bool ReportStyleSelectionDialog::renderUntestedSpeedRows() const
{
    return m_renderUntestedSpeedRowsCheckbox ? m_renderUntestedSpeedRowsCheckbox->isChecked() : false;
}
