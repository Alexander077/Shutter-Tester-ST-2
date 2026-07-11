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
#include <QJsonArray>
#include <QJsonObject>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QTextBrowser>
#include <QScrollArea>

class ReportSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReportSettingsDialog(QWidget *parent = nullptr);
    ~ReportSettingsDialog();

private slots:
    void onProfileComboChanged(int index);
    void onAddProfileClicked();
    void onDeleteProfileClicked();
    void onLoadImageClicked();
    void onColorButtonClicked();
    void onFontSizeChanged(int value);
    void onPaddingChanged(int value);
    void onFontFamilyComboChanged(int index);
    void onResetColorClicked();

private:
    void loadConfigs();
    void saveConfigs();
    void populateProfileCombo();
    void loadProfileToControls(int index);
    void clearControls();
    void setControlsEnabled(bool enabled);
    void updatePreview();
    void updatePreviewWithLiveColors();
    QString applyStyleToTemplate(const QJsonObject &style);
    QString loadTemplateForPreview();
    QWidget *makeColorButtonWithReset(const QString &key, QPushButton *&colorBtnOut, QPushButton *&resetBtnOut);
    void updateColorButtonColor(QPushButton *btn, const QString &color);
    void updateImagePreview(const QString &base64);
    void updateColorFromProfile(const QString &key, const QString &color);
    void saveColorToProfile(const QString &key, const QString &color);

    QJsonArray m_configs;
    int m_currentIndex;

    // UI elements
    QComboBox *m_profileCombo;
    QPushButton *m_addProfileBtn;
    QPushButton *m_deleteProfileBtn;
    QLabel *m_warningLabel;
    QLabel *m_imagePreview;
    QPushButton *m_loadImageBtn;
    QComboBox *m_fontFamilyCombo;
    QSpinBox *m_fontSizeSpin;
    QSpinBox *m_paddingSpin;
    QTextBrowser *m_previewBrowser;

    // Individual color buttons
    QPushButton *m_colorBtnPrimaryFont;
    QPushButton *m_colorBtnSecondaryFont;
    QPushButton *m_colorBtnInstrumentIdLabel;
    QPushButton *m_colorBtnValues1;
    QPushButton *m_colorBtnTableBg;
    QPushButton *m_colorBtnCurtainsBg;

    // Reset buttons for each color
    QPushButton *m_resetBtnPrimaryFont;
    QPushButton *m_resetBtnSecondaryFont;
    QPushButton *m_resetBtnInstrumentIdLabel;
    QPushButton *m_resetBtnValues1;
    QPushButton *m_resetBtnTableBg;
    QPushButton *m_resetBtnCurtainsBg;

    // Current color values
    QString m_colorPrimaryFont;
    QString m_colorSecondaryFont;
    QString m_colorInstrumentIdLabel;
    QString m_colorValues1;
    QString m_colorTableBg;
    QString m_colorCurtainsBg;

    // Previous (saved) color values for reset
    QString m_prevColorPrimaryFont;
    QString m_prevColorSecondaryFont;
    QString m_prevColorInstrumentIdLabel;
    QString m_prevColorValues1;
    QString m_prevColorTableBg;
    QString m_prevColorCurtainsBg;

    QString m_currentImageBase64;
    QString m_currentFontFamily;
    int m_currentFontSize;
    int m_currentPadding;
};