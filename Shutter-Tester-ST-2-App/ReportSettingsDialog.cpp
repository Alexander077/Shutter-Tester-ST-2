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

#include "ReportSettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QScrollArea>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QPixmap>
#include <QBuffer>
#include <QByteArray>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QFileInfo>
#include <QSizePolicy>
#include <QFrame>
#include <QPainter>
#include <QLineEdit>
#include <QDialogButtonBox>

ReportSettingsDialog::ReportSettingsDialog(QWidget *parent)
    : QDialog(parent)
    , m_currentIndex(0)
    , m_currentFontSize(8)
    , m_currentPadding(5)
{
    setWindowTitle(tr("Report Settings"));
    setMinimumSize(1100, 650);

    loadConfigs();

    // Main horizontal layout: left = settings, right = preview
    QHBoxLayout *mainLayout = new QHBoxLayout(this);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(splitter);

    // ===== LEFT PANEL: Profile settings =====
    QWidget *leftWidget = new QWidget(this);
    leftWidget->setMinimumWidth(420);
    leftWidget->setMaximumWidth(540);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(8, 8, 8, 8);

    // Warning label for default profile
    m_warningLabel = new QLabel(
        "You can't edit the default profile. Please create your own profile for that", this);
    m_warningLabel->setStyleSheet("color: #e6c200; font-weight: bold; padding: 6px; "
                                  "background-color: #3a3a1a; border-radius: 3px;");
    m_warningLabel->setWordWrap(true);
    m_warningLabel->hide();
    leftLayout->addWidget(m_warningLabel);

    // Profile selector row: combo + add + delete
    QHBoxLayout *profileRow = new QHBoxLayout();
    leftLayout->addLayout(profileRow);

    profileRow->addWidget(new QLabel(tr("Profile:"), this));

    m_profileCombo = new QComboBox(this);
    m_profileCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    profileRow->addWidget(m_profileCombo);

    m_addProfileBtn = new QPushButton(QIcon(":/assets/add-document.png"), "", this);
    m_addProfileBtn->setToolTip(tr("Create new profile"));
    m_addProfileBtn->setFixedSize(28, 28);
    profileRow->addWidget(m_addProfileBtn);

    m_deleteProfileBtn = new QPushButton(QIcon(":/assets/cross.png"), "", this);
    m_deleteProfileBtn->setToolTip(tr("Delete profile"));
    m_deleteProfileBtn->setFixedSize(28, 28);
    profileRow->addWidget(m_deleteProfileBtn);

    // Scroll area for settings controls
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget *settingsWidget = new QWidget(scrollArea);
    QVBoxLayout *settingsLayout = new QVBoxLayout(settingsWidget);
    settingsLayout->setContentsMargins(0, 4, 0, 4);

    // --- Image section ---
    QGroupBox *imageGroup = new QGroupBox(tr("Header Image"), settingsWidget);
    QVBoxLayout *imageLayout = new QVBoxLayout(imageGroup);

    QHBoxLayout *imageRow = new QHBoxLayout();
    m_imagePreview = new QLabel(imageGroup);
    m_imagePreview->setFixedSize(64, 64);
    m_imagePreview->setAlignment(Qt::AlignCenter);
    m_imagePreview->setStyleSheet("border: 1px solid #555; background-color: #2a2a2a;");
    imageRow->addWidget(m_imagePreview);

    m_loadImageBtn = new QPushButton(tr("Load Image..."), imageGroup);
    imageRow->addWidget(m_loadImageBtn);
    imageRow->addStretch();
    imageLayout->addLayout(imageRow);

    settingsLayout->addWidget(imageGroup);

    // --- Colors section ---
    QGroupBox *colorsGroup = new QGroupBox(tr("Colors"), settingsWidget);
    QGridLayout *colorsLayout = new QGridLayout(colorsGroup);
    colorsLayout->setColumnStretch(1, 1);

    // Create color buttons with reset buttons
    colorsLayout->addWidget(new QLabel(tr("Primary Font Color:"), colorsGroup), 0, 0);
    colorsLayout->addWidget(makeColorButtonWithReset("PRIMARY_FONT_COLOR", m_colorBtnPrimaryFont, m_resetBtnPrimaryFont), 0, 1);

    colorsLayout->addWidget(new QLabel(tr("Secondary Font Color:"), colorsGroup), 1, 0);
    colorsLayout->addWidget(makeColorButtonWithReset("SECONDARY_FONT_COLOR", m_colorBtnSecondaryFont, m_resetBtnSecondaryFont), 1, 1);

    colorsLayout->addWidget(new QLabel(tr("Instrument ID Label Font Color:"), colorsGroup), 2, 0);
    colorsLayout->addWidget(makeColorButtonWithReset("INSTRUMENT_ID_LABEL_FONT_COLOR", m_colorBtnInstrumentIdLabel, m_resetBtnInstrumentIdLabel), 2, 1);

    colorsLayout->addWidget(new QLabel(tr("Values Font Color:"), colorsGroup), 3, 0);
    colorsLayout->addWidget(makeColorButtonWithReset("VALUES_1_FONT_COLOR", m_colorBtnValues1, m_resetBtnValues1), 3, 1);

    colorsLayout->addWidget(new QLabel(tr("Table Background Color:"), colorsGroup), 4, 0);
    colorsLayout->addWidget(makeColorButtonWithReset("TABLE_BG_COLOR", m_colorBtnTableBg, m_resetBtnTableBg), 4, 1);

    colorsLayout->addWidget(new QLabel(tr("Curtains Data Table Background:"), colorsGroup), 5, 0);
    colorsLayout->addWidget(makeColorButtonWithReset("CURTAINS_DATA_TABLE_BG_COLOR", m_colorBtnCurtainsBg, m_resetBtnCurtainsBg), 5, 1);

    settingsLayout->addWidget(colorsGroup);

    // --- Font & Padding section ---
    QGroupBox *fontGroup = new QGroupBox(tr("Font & Padding"), settingsWidget);
    QGridLayout *fontLayout = new QGridLayout(fontGroup);
    fontLayout->setColumnStretch(1, 1);

    fontLayout->addWidget(new QLabel(tr("Font Family:"), fontGroup), 0, 0);
    m_fontFamilyCombo = new QComboBox(fontGroup);
    // Populate with PDF-safe fonts
    QStringList pdfFonts = {
        "Arial",
        "Helvetica",
        "Times New Roman",
        "Times",
        "Courier New",
        "Courier",
        "Verdana",
        "Georgia",
        "Trebuchet MS",
        "Tahoma",
        "Segoe UI",
        "Calibri",
        "Cambria",
        "Impact",
        "Comic Sans MS",
        "Palatino Linotype",
        "Garamond",
        "Bookman Old Style",
        "Lucida Console",
        "Lucida Sans Unicode"
    };
    m_fontFamilyCombo->addItems(pdfFonts);
    m_fontFamilyCombo->setEditable(false);
    fontLayout->addWidget(m_fontFamilyCombo, 0, 1);

    fontLayout->addWidget(new QLabel(tr("Tables Font Size (pt):"), fontGroup), 1, 0);
    m_fontSizeSpin = new QSpinBox(fontGroup);
    m_fontSizeSpin->setRange(1, 50);
    m_fontSizeSpin->setSingleStep(1);
    fontLayout->addWidget(m_fontSizeSpin, 1, 1);

    fontLayout->addWidget(new QLabel(tr("Table Cell Padding (px):"), fontGroup), 2, 0);
    m_paddingSpin = new QSpinBox(fontGroup);
    m_paddingSpin->setRange(0, 50);
    m_paddingSpin->setSingleStep(1);
    fontLayout->addWidget(m_paddingSpin, 2, 1);

    settingsLayout->addWidget(fontGroup);
    settingsLayout->addStretch();

    scrollArea->setWidget(settingsWidget);
    leftLayout->addWidget(scrollArea, 1);

    splitter->addWidget(leftWidget);

    // ===== RIGHT PANEL: Preview =====
    QWidget *rightWidget = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(8, 8, 8, 8);

    QLabel *previewTitle = new QLabel(tr("Report Preview"), rightWidget);
    previewTitle->setStyleSheet("font-weight: bold; font-size: 13px; margin-bottom: 4px;");
    rightLayout->addWidget(previewTitle);

    m_previewBrowser = new QTextBrowser(rightWidget);
    m_previewBrowser->setOpenExternalLinks(false);
    rightLayout->addWidget(m_previewBrowser, 1);

    splitter->addWidget(rightWidget);
    splitter->setSizes({460, 640});

    // ===== Connections =====
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ReportSettingsDialog::onProfileComboChanged);
    connect(m_addProfileBtn, &QPushButton::clicked, this, &ReportSettingsDialog::onAddProfileClicked);
    connect(m_deleteProfileBtn, &QPushButton::clicked, this, &ReportSettingsDialog::onDeleteProfileClicked);
    connect(m_loadImageBtn, &QPushButton::clicked, this, &ReportSettingsDialog::onLoadImageClicked);
    connect(m_fontFamilyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ReportSettingsDialog::onFontFamilyComboChanged);
    connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ReportSettingsDialog::onFontSizeChanged);
    connect(m_paddingSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ReportSettingsDialog::onPaddingChanged);

    // Populate combo and load first profile
    populateProfileCombo();
    if (m_configs.size() > 0) {
        m_profileCombo->setCurrentIndex(0);
        onProfileComboChanged(0);
    }
}

ReportSettingsDialog::~ReportSettingsDialog()
{
}

void ReportSettingsDialog::loadConfigs()
{
    QString configPath = QCoreApplication::applicationDirPath() + QDir::separator() + "report_style_configs.json";
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // If file doesn't exist, create a default config
        m_configs = QJsonArray();
        QJsonObject defaultProfile;
        defaultProfile["reportStyleName"] = "Default Report Style";
        defaultProfile["HEADER_IMAGE_BASE64"] = "";
        defaultProfile["TABLES_FONT_SIZE"] = "8pt";
        defaultProfile["TABLE_CELL_PADDING"] = "5px";
        QJsonObject colors;
        colors["PRIMARY_FONT_COLOR"] = "#f0ead8";
        colors["SECONDARY_FONT_COLOR"] = "#c8a96e";
        colors["INSTRUMENT_ID_LABEL_FONT_COLOR"] = "#777770";
        colors["FONT_FAMILY"] = "Arial";
        colors["VALUES_1_FONT_COLOR"] = "#1a1a1e";
        colors["TABLE_BG_COLOR"] = "#faf8f4";
        colors["CURTAINS_DATA_TABLE_BG_COLOR"] = "#faf8f4";
        defaultProfile["colors"] = colors;
        m_configs.append(defaultProfile);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (doc.isArray()) {
        m_configs = doc.array();
    } else {
        m_configs = QJsonArray();
    }
}

void ReportSettingsDialog::saveConfigs()
{
    QString configPath = QCoreApplication::applicationDirPath() + QDir::separator() + "report_style_configs.json";
    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"), tr("Cannot save report style configs:\n%1").arg(configPath));
        return;
    }

    QJsonDocument doc(m_configs);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}

void ReportSettingsDialog::populateProfileCombo()
{
    m_profileCombo->blockSignals(true);
    m_profileCombo->clear();
    for (int i = 0; i < m_configs.size(); ++i) {
        QJsonObject obj = m_configs[i].toObject();
        m_profileCombo->addItem(obj["reportStyleName"].toString());
    }
    m_profileCombo->blockSignals(false);
}

void ReportSettingsDialog::updateColorFromProfile(const QString &key, const QString &color)
{
    if (key == "PRIMARY_FONT_COLOR") { m_colorPrimaryFont = color; m_prevColorPrimaryFont = color; updateColorButtonColor(m_colorBtnPrimaryFont, color); }
    else if (key == "SECONDARY_FONT_COLOR") { m_colorSecondaryFont = color; m_prevColorSecondaryFont = color; updateColorButtonColor(m_colorBtnSecondaryFont, color); }
    else if (key == "INSTRUMENT_ID_LABEL_FONT_COLOR") { m_colorInstrumentIdLabel = color; m_prevColorInstrumentIdLabel = color; updateColorButtonColor(m_colorBtnInstrumentIdLabel, color); }
    else if (key == "VALUES_1_FONT_COLOR") { m_colorValues1 = color; m_prevColorValues1 = color; updateColorButtonColor(m_colorBtnValues1, color); }
    else if (key == "TABLE_BG_COLOR") { m_colorTableBg = color; m_prevColorTableBg = color; updateColorButtonColor(m_colorBtnTableBg, color); }
    else if (key == "CURTAINS_DATA_TABLE_BG_COLOR") { m_colorCurtainsBg = color; m_prevColorCurtainsBg = color; updateColorButtonColor(m_colorBtnCurtainsBg, color); }
}

void ReportSettingsDialog::saveColorToProfile(const QString &key, const QString &color)
{
    if (m_currentIndex < 0 || m_currentIndex >= m_configs.size()) return;
    QJsonObject profile = m_configs[m_currentIndex].toObject();
    QJsonObject colors = profile["colors"].toObject();
    colors[key] = color;
    profile["colors"] = colors;
    m_configs[m_currentIndex] = profile;
    saveConfigs();
}

void ReportSettingsDialog::loadProfileToControls(int index)
{
    if (index < 0 || index >= m_configs.size()) {
        clearControls();
        return;
    }

    QJsonObject profile = m_configs[index].toObject();

    // Image
    m_currentImageBase64 = profile["HEADER_IMAGE_BASE64"].toString();
    updateImagePreview(m_currentImageBase64);

    // Font size (integer)
    QString fontSizeStr = profile["TABLES_FONT_SIZE"].toString("8pt");
    fontSizeStr.remove("pt", Qt::CaseInsensitive);
    m_currentFontSize = qBound(1, (int)fontSizeStr.toDouble(), 50);
    m_fontSizeSpin->blockSignals(true);
    m_fontSizeSpin->setValue(m_currentFontSize);
    m_fontSizeSpin->blockSignals(false);

    // Padding (integer)
    QString paddingStr = profile["TABLE_CELL_PADDING"].toString("5px");
    paddingStr.remove("px", Qt::CaseInsensitive);
    m_currentPadding = qBound(0, (int)paddingStr.toDouble(), 50);
    m_paddingSpin->blockSignals(true);
    m_paddingSpin->setValue(m_currentPadding);
    m_paddingSpin->blockSignals(false);

    // Colors
    QJsonObject colors = profile["colors"].toObject();
    updateColorFromProfile("PRIMARY_FONT_COLOR", colors["PRIMARY_FONT_COLOR"].toString("#f0ead8"));
    updateColorFromProfile("SECONDARY_FONT_COLOR", colors["SECONDARY_FONT_COLOR"].toString("#c8a96e"));
    updateColorFromProfile("INSTRUMENT_ID_LABEL_FONT_COLOR", colors["INSTRUMENT_ID_LABEL_FONT_COLOR"].toString("#777770"));
    updateColorFromProfile("VALUES_1_FONT_COLOR", colors["VALUES_1_FONT_COLOR"].toString("#1a1a1e"));
    updateColorFromProfile("TABLE_BG_COLOR", colors["TABLE_BG_COLOR"].toString("#faf8f4"));
    updateColorFromProfile("CURTAINS_DATA_TABLE_BG_COLOR", colors["CURTAINS_DATA_TABLE_BG_COLOR"].toString("#faf8f4"));

    // Font family (combo)
    m_currentFontFamily = colors["FONT_FAMILY"].toString("Arial");
    m_fontFamilyCombo->blockSignals(true);
    int comboIdx = m_fontFamilyCombo->findText(m_currentFontFamily);
    if (comboIdx >= 0) {
        m_fontFamilyCombo->setCurrentIndex(comboIdx);
    } else {
        // If the stored font is not in the list, add it temporarily
        m_fontFamilyCombo->addItem(m_currentFontFamily);
        m_fontFamilyCombo->setCurrentIndex(m_fontFamilyCombo->count() - 1);
    }
    m_fontFamilyCombo->blockSignals(false);
}

void ReportSettingsDialog::clearControls()
{
    m_currentImageBase64.clear();
    updateImagePreview("");
    m_fontSizeSpin->blockSignals(true);
    m_fontSizeSpin->setValue(8);
    m_fontSizeSpin->blockSignals(false);
    m_paddingSpin->blockSignals(true);
    m_paddingSpin->setValue(5);
    m_paddingSpin->blockSignals(false);
    m_fontFamilyCombo->blockSignals(true);
    m_fontFamilyCombo->setCurrentIndex(0); // Arial
    m_fontFamilyCombo->blockSignals(false);
    updateColorFromProfile("PRIMARY_FONT_COLOR", "#000000");
    updateColorFromProfile("SECONDARY_FONT_COLOR", "#000000");
    updateColorFromProfile("INSTRUMENT_ID_LABEL_FONT_COLOR", "#000000");
    updateColorFromProfile("VALUES_1_FONT_COLOR", "#000000");
    updateColorFromProfile("TABLE_BG_COLOR", "#000000");
    updateColorFromProfile("CURTAINS_DATA_TABLE_BG_COLOR", "#000000");
}

void ReportSettingsDialog::setControlsEnabled(bool enabled)
{
    m_loadImageBtn->setEnabled(enabled);
    m_fontFamilyCombo->setEnabled(enabled);
    m_fontSizeSpin->setEnabled(enabled);
    m_paddingSpin->setEnabled(enabled);
    m_colorBtnPrimaryFont->setEnabled(enabled);
    m_colorBtnSecondaryFont->setEnabled(enabled);
    m_colorBtnInstrumentIdLabel->setEnabled(enabled);
    m_colorBtnValues1->setEnabled(enabled);
    m_colorBtnTableBg->setEnabled(enabled);
    m_colorBtnCurtainsBg->setEnabled(enabled);
    m_resetBtnPrimaryFont->setEnabled(enabled);
    m_resetBtnSecondaryFont->setEnabled(enabled);
    m_resetBtnInstrumentIdLabel->setEnabled(enabled);
    m_resetBtnValues1->setEnabled(enabled);
    m_resetBtnTableBg->setEnabled(enabled);
    m_resetBtnCurtainsBg->setEnabled(enabled);
    m_deleteProfileBtn->setEnabled(enabled);
}

void ReportSettingsDialog::onProfileComboChanged(int index)
{
    m_currentIndex = index;

    if (index < 0 || index >= m_configs.size()) return;

    QJsonObject profile = m_configs[index].toObject();
    QString name = profile["reportStyleName"].toString();

    bool isDefault = (name == "Default Report Style");

    // Show warning for default profile
    m_warningLabel->setVisible(isDefault);

    // Disable controls for default profile
    setControlsEnabled(!isDefault);

    // Load profile to controls
    loadProfileToControls(index);

    // Update preview
    updatePreview();
}

void ReportSettingsDialog::onAddProfileClicked()
{
    // Dialog with text box for new profile name
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Create New Profile"));
    dialog.setMinimumWidth(300);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    layout->addWidget(new QLabel(tr("Enter new profile name:"), &dialog));

    QLineEdit *nameEdit = new QLineEdit(&dialog);
    layout->addWidget(nameEdit);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) {
            QMessageBox::critical(this, tr("Error"), tr("Profile name cannot be empty."));
            return;
        }

        // Check for duplicate names
        for (int i = 0; i < m_configs.size(); ++i) {
            if (m_configs[i].toObject()["reportStyleName"].toString() == name) {
                QMessageBox::critical(this, tr("Error"), tr("A profile with this name already exists."));
                return;
            }
        }

        // Create new profile based on current one
        QJsonObject newProfile;
        if (m_currentIndex >= 0 && m_currentIndex < m_configs.size()) {
            newProfile = m_configs[m_currentIndex].toObject();
        }
        newProfile["reportStyleName"] = name;

        m_configs.append(newProfile);
        saveConfigs();

        populateProfileCombo();
        int newIndex = m_configs.size() - 1;
        m_profileCombo->setCurrentIndex(newIndex);
    }
}

void ReportSettingsDialog::onDeleteProfileClicked()
{
    int index = m_profileCombo->currentIndex();
    if (index < 0 || index >= m_configs.size()) return;

    QString name = m_configs[index].toObject()["reportStyleName"].toString();

    if (name == "Default Report Style") {
        QMessageBox::warning(this, tr("Cannot Delete"), tr("The default profile cannot be deleted."));
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Confirm Deletion"),
        tr("Are you sure you want to delete the profile \"%1\"?").arg(name),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_configs.removeAt(index);
        saveConfigs();
        populateProfileCombo();
        if (m_configs.size() > 0) {
            int newIndex = qMin(index, m_configs.size() - 1);
            m_profileCombo->setCurrentIndex(newIndex);
            // Force reload and preview update in case setCurrentIndex didn't change the index
            // (e.g., when deleting the last profile and combo stays at the same position)
            onProfileComboChanged(newIndex);
        }
    }
}

void ReportSettingsDialog::onLoadImageClicked()
{
    // Determine the starting directory: use last used dir from settings,
    // or default to the user's Documents location.
    QSettings settings;
    QString lastDir = settings.value("lastImageDir").toString();
    if (lastDir.isEmpty() || !QDir(lastDir).exists()) {
        lastDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }

    QString fileName = QFileDialog::getOpenFileName(
        this, tr("Load Header Image"), lastDir,
        tr("Image Files (*.png *.jpg *.jpeg *.bmp *.gif *.svg *.webp)"));

    if (fileName.isEmpty()) return;

    // Remember the directory for next time
    QFileInfo fi(fileName);
    settings.setValue("lastImageDir", fi.absolutePath());

    QPixmap pix(fileName);
    if (pix.isNull()) {
        QMessageBox::warning(this, tr("Error"), tr("Cannot load image file:\n%1").arg(fileName));
        return;
    }

    // Scale to 64x64
    QPixmap scaled = pix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // Create a 64x64 transparent pixmap and center the scaled image
    QPixmap finalPix(64, 64);
    finalPix.fill(Qt::transparent);
    QPainter painter(&finalPix);
    int x = (64 - scaled.width()) / 2;
    int y = (64 - scaled.height()) / 2;
    painter.drawPixmap(x, y, scaled);
    painter.end();

    // Convert to base64 PNG
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    finalPix.save(&buffer, "PNG");
    buffer.close();

    m_currentImageBase64 = QString::fromLatin1(byteArray.toBase64());

    // Save to current profile
    if (m_currentIndex >= 0 && m_currentIndex < m_configs.size()) {
        QJsonObject profile = m_configs[m_currentIndex].toObject();
        profile["HEADER_IMAGE_BASE64"] = m_currentImageBase64;
        m_configs[m_currentIndex] = profile;
        saveConfigs();
    }

    updateImagePreview(m_currentImageBase64);
    updatePreview();
}

void ReportSettingsDialog::onColorButtonClicked()
{
    QPushButton *btn = qobject_cast<QPushButton *>(sender());
    if (!btn) return;

    // Determine which color key and current value
    QString key;
    QString *currentColorPtr = nullptr;
    QString *prevColorPtr = nullptr;
    QPushButton *colorBtn = nullptr;

    if (btn == m_colorBtnPrimaryFont) {
        key = "PRIMARY_FONT_COLOR";
        currentColorPtr = &m_colorPrimaryFont;
        prevColorPtr = &m_prevColorPrimaryFont;
        colorBtn = m_colorBtnPrimaryFont;
    } else if (btn == m_colorBtnSecondaryFont) {
        key = "SECONDARY_FONT_COLOR";
        currentColorPtr = &m_colorSecondaryFont;
        prevColorPtr = &m_prevColorSecondaryFont;
        colorBtn = m_colorBtnSecondaryFont;
    } else if (btn == m_colorBtnInstrumentIdLabel) {
        key = "INSTRUMENT_ID_LABEL_FONT_COLOR";
        currentColorPtr = &m_colorInstrumentIdLabel;
        prevColorPtr = &m_prevColorInstrumentIdLabel;
        colorBtn = m_colorBtnInstrumentIdLabel;
    } else if (btn == m_colorBtnValues1) {
        key = "VALUES_1_FONT_COLOR";
        currentColorPtr = &m_colorValues1;
        prevColorPtr = &m_prevColorValues1;
        colorBtn = m_colorBtnValues1;
    } else if (btn == m_colorBtnTableBg) {
        key = "TABLE_BG_COLOR";
        currentColorPtr = &m_colorTableBg;
        prevColorPtr = &m_prevColorTableBg;
        colorBtn = m_colorBtnTableBg;
    } else if (btn == m_colorBtnCurtainsBg) {
        key = "CURTAINS_DATA_TABLE_BG_COLOR";
        currentColorPtr = &m_colorCurtainsBg;
        prevColorPtr = &m_prevColorCurtainsBg;
        colorBtn = m_colorBtnCurtainsBg;
    } else {
        return;
    }

    // Backup current color before opening dialog
    QString backupColor = *currentColorPtr;

    QColor initialColor(currentColorPtr->isEmpty() ? "#000000" : *currentColorPtr);
    QColorDialog *dialog = new QColorDialog(initialColor, this);
    dialog->setWindowTitle(tr("Select Color"));
    dialog->setOption(QColorDialog::DontUseNativeDialog, true);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    // Live update: change button color and preview as user moves in the color picker
    connect(dialog, &QColorDialog::currentColorChanged, this, [this, currentColorPtr, colorBtn](const QColor &color) {
        if (!color.isValid()) return;
        QString colorStr = color.name();
        *currentColorPtr = colorStr;
        updateColorButtonColor(colorBtn, colorStr);
        updatePreviewWithLiveColors();
    });

    // On accept: save to profile config, set previous to backup (value before dialog)
    connect(dialog, &QColorDialog::accepted, this, [this, currentColorPtr, prevColorPtr, key, backupColor]() {
        *prevColorPtr = backupColor;  // previous becomes the value before dialog opened
        saveColorToProfile(key, *currentColorPtr);
    });

    // On reject: restore backup
    connect(dialog, &QColorDialog::rejected, this, [this, currentColorPtr, colorBtn, backupColor]() {
        *currentColorPtr = backupColor;
        updateColorButtonColor(colorBtn, backupColor);
        updatePreview();
    });

    dialog->open();
}

void ReportSettingsDialog::onResetColorClicked()
{
    QPushButton *btn = qobject_cast<QPushButton *>(sender());
    if (!btn) return;

    QString key;
    QString *currentColorPtr = nullptr;
    QString *prevColorPtr = nullptr;
    QPushButton *colorBtn = nullptr;

    if (btn == m_resetBtnPrimaryFont) {
        key = "PRIMARY_FONT_COLOR";
        currentColorPtr = &m_colorPrimaryFont;
        prevColorPtr = &m_prevColorPrimaryFont;
        colorBtn = m_colorBtnPrimaryFont;
    } else if (btn == m_resetBtnSecondaryFont) {
        key = "SECONDARY_FONT_COLOR";
        currentColorPtr = &m_colorSecondaryFont;
        prevColorPtr = &m_prevColorSecondaryFont;
        colorBtn = m_colorBtnSecondaryFont;
    } else if (btn == m_resetBtnInstrumentIdLabel) {
        key = "INSTRUMENT_ID_LABEL_FONT_COLOR";
        currentColorPtr = &m_colorInstrumentIdLabel;
        prevColorPtr = &m_prevColorInstrumentIdLabel;
        colorBtn = m_colorBtnInstrumentIdLabel;
    } else if (btn == m_resetBtnValues1) {
        key = "VALUES_1_FONT_COLOR";
        currentColorPtr = &m_colorValues1;
        prevColorPtr = &m_prevColorValues1;
        colorBtn = m_colorBtnValues1;
    } else if (btn == m_resetBtnTableBg) {
        key = "TABLE_BG_COLOR";
        currentColorPtr = &m_colorTableBg;
        prevColorPtr = &m_prevColorTableBg;
        colorBtn = m_colorBtnTableBg;
    } else if (btn == m_resetBtnCurtainsBg) {
        key = "CURTAINS_DATA_TABLE_BG_COLOR";
        currentColorPtr = &m_colorCurtainsBg;
        prevColorPtr = &m_prevColorCurtainsBg;
        colorBtn = m_colorBtnCurtainsBg;
    } else {
        return;
    }

    *currentColorPtr = *prevColorPtr;
    updateColorButtonColor(colorBtn, *currentColorPtr);
    saveColorToProfile(key, *currentColorPtr);
    updatePreview();
}

void ReportSettingsDialog::onFontFamilyComboChanged(int /*index*/)
{
    m_currentFontFamily = m_fontFamilyCombo->currentText();

    // Save to current profile
    if (m_currentIndex >= 0 && m_currentIndex < m_configs.size()) {
        QJsonObject profile = m_configs[m_currentIndex].toObject();
        QJsonObject colors = profile["colors"].toObject();
        colors["FONT_FAMILY"] = m_currentFontFamily;
        profile["colors"] = colors;
        m_configs[m_currentIndex] = profile;
        saveConfigs();
    }

    updatePreview();
}

void ReportSettingsDialog::onFontSizeChanged(int value)
{
    m_currentFontSize = value;

    // Save to current profile
    if (m_currentIndex >= 0 && m_currentIndex < m_configs.size()) {
        QJsonObject profile = m_configs[m_currentIndex].toObject();
        profile["TABLES_FONT_SIZE"] = QString::number(m_currentFontSize) + "pt";
        m_configs[m_currentIndex] = profile;
        saveConfigs();
    }

    updatePreview();
}

void ReportSettingsDialog::onPaddingChanged(int value)
{
    m_currentPadding = value;

    // Save to current profile
    if (m_currentIndex >= 0 && m_currentIndex < m_configs.size()) {
        QJsonObject profile = m_configs[m_currentIndex].toObject();
        profile["TABLE_CELL_PADDING"] = QString::number(m_currentPadding) + "px";
        m_configs[m_currentIndex] = profile;
        saveConfigs();
    }

    updatePreview();
}

void ReportSettingsDialog::updatePreview()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_configs.size()) {
        m_previewBrowser->clear();
        return;
    }

    QJsonObject profile = m_configs[m_currentIndex].toObject();
    QString html = applyStyleToTemplate(profile);
    m_previewBrowser->setHtml(html);
}

void ReportSettingsDialog::updatePreviewWithLiveColors()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_configs.size()) {
        m_previewBrowser->clear();
        return;
    }

    // Start with the saved profile, but override colors with current live values
    QJsonObject profile = m_configs[m_currentIndex].toObject();
    QJsonObject colors = profile["colors"].toObject();
    colors["PRIMARY_FONT_COLOR"] = m_colorPrimaryFont;
    colors["SECONDARY_FONT_COLOR"] = m_colorSecondaryFont;
    colors["INSTRUMENT_ID_LABEL_FONT_COLOR"] = m_colorInstrumentIdLabel;
    colors["VALUES_1_FONT_COLOR"] = m_colorValues1;
    colors["TABLE_BG_COLOR"] = m_colorTableBg;
    colors["CURTAINS_DATA_TABLE_BG_COLOR"] = m_colorCurtainsBg;
    colors["FONT_FAMILY"] = m_currentFontFamily;
    profile["colors"] = colors;
    profile["TABLES_FONT_SIZE"] = QString::number(m_currentFontSize) + "pt";
    profile["TABLE_CELL_PADDING"] = QString::number(m_currentPadding) + "px";
    profile["HEADER_IMAGE_BASE64"] = m_currentImageBase64;

    QString html = applyStyleToTemplate(profile);
    m_previewBrowser->setHtml(html);
}

QString ReportSettingsDialog::loadTemplateForPreview()
{
#ifdef Q_OS_MAC
    QDir dir(QCoreApplication::applicationDirPath());
    dir.cdUp();
    dir.cd("Resources");
    QString templatePath = dir.absolutePath() + "/assets/st2_fp_report_template.html";
#else
    QString templatePath = QCoreApplication::applicationDirPath() + "/assets/st2_fp_report_template.html";
#endif

    QFile file(templatePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    QString html = QString::fromUtf8(file.readAll());
    file.close();
    return html;
}

QString ReportSettingsDialog::applyStyleToTemplate(const QJsonObject &style)
{
    QString html = loadTemplateForPreview();
    if (html.isEmpty()) return QString();

    // Remove template row comments (START/END) but keep EXAMPLE blocks
    // Remove FOcal_PLANE_SPEED_ROW_TEMPLATE_START/END (only one block)
    static const QRegularExpression speedRowRe(
        QStringLiteral("<!--FOCAL_PLANE_SPEED_ROW_TEMPLATE_START.*?FOCAL_PLANE_SPEED_ROW_TEMPLATE_END-->"),
        QRegularExpression::DotMatchesEverythingOption);
    html.remove(speedRowRe);

    // Remove the FOCAL_PLANE_CURTAIN_ROW_TEMPLATE_START/END block (the template)
    static const QRegularExpression curtainRowRe(
        QStringLiteral("<!--FOCAL_PLANE_CURTAIN_ROW_TEMPLATE_START.*?FOCAL_PLANE_CURTAIN_ROW_TEMPLATE_END-->"),
        QRegularExpression::DotMatchesEverythingOption);
    html.remove(curtainRowRe);

    // Remove the EXAMPLE comment markers but keep the content
    html.remove("<!--FOCAL_PLANE_SPEED_ROW_EXAMPLE_START");
    html.remove("FOCAL_PLANE_SPEED_ROW_EXAMPLE_END-->");
    // Remove remaining curtain template markers (from the example block)
    html.remove("<!--FOCAL_PLANE_CURTAIN_ROW_EXAMPLE_START");
    html.remove("FOCAL_PLANE_CURTAIN_ROW_EXAMPLE_END-->");

    // Apply style placeholders
    QJsonObject colors = style["colors"].toObject();

    // Colors
    html.replace("{{PRIMARY_FONT_COLOR}}", colors["PRIMARY_FONT_COLOR"].toString("#f0ead8"));
    html.replace("{{SECONDARY_FONT_COLOR}}", colors["SECONDARY_FONT_COLOR"].toString("#c8a96e"));
    html.replace("{{INSTRUMENT_ID_LABEL_FONT_COLOR}}", colors["INSTRUMENT_ID_LABEL_FONT_COLOR"].toString("#777770"));
    html.replace("{{VALUES_1_FONT_COLOR}}", colors["VALUES_1_FONT_COLOR"].toString("#1a1a1e"));
    html.replace("{{TABLE_BG_COLOR}}", colors["TABLE_BG_COLOR"].toString("#faf8f4"));
    html.replace("{{CURTAINS_DATA_TABLE_BG_COLOR}}", colors["CURTAINS_DATA_TABLE_BG_COLOR"].toString("#faf8f4"));
    html.replace("{{FONT_FAMILY}}", colors["FONT_FAMILY"].toString("Arial"));

    // Font size and padding
    html.replace("{{TABLES_FONT_SIZE}}", style["TABLES_FONT_SIZE"].toString("8pt"));
    html.replace("{{TABLES_CELL_PADDING}}", style["TABLE_CELL_PADDING"].toString("5px"));

    // Header image
    QString imageBase64 = style["HEADER_IMAGE_BASE64"].toString();
    if (imageBase64.isEmpty()) {
        html.replace("{{HEADER_IMAGE_BASE64}}", "");
    } else {
        html.replace("{{HEADER_IMAGE_BASE64}}", imageBase64);
    }

    // Fill in some example data for preview
    html.replace("{{DATE}}", "2026-01-15");
    html.replace("{{FIRMWARE}}", "ST-2 FW 1.0");
    html.replace("{{APP_VERSION}}", "v1.0");
    html.replace("{{CAMERA_MODEL}}", "Example Camera");
    html.replace("{{SERIAL_NUMBER}}", "EX12345");
    html.replace("{{SHUTTER_NAME}}", "Example Shutter");
    html.replace("{{SHUTTER_TYPE}}", "Focal Plane - Horizontal");
    html.replace("{{FRAME_FORMAT}}", "35 mm");
    html.replace("{{SPEED_SERIES}}", "New Style (ISO)");
    html.replace("{{TECHNICIAN}}", "Technician");
    html.replace("{{MEASUREMENT_RUNS}}", "3");
    html.replace("{{TOLERANCE}}", "&plusmn;0.1 EV");
    html.replace("{{NOTES}}", "This is a preview of the report style.");

    // Status placeholders
    html.replace("{{STATUS}}", "PASS");
    html.replace("{{STATUS_COLOR}}", "#2e7d32");
    html.replace("{{STATUS_BG_COLOR}}", "#e8f5e9");
    html.replace("{{STATUS_BORDER_COLOR}}", "#2e7d32");
    html.replace("{{SPEEDS_TESTED}}", "2");
    html.replace("{{PASSED_COUNT}}", "1");
    html.replace("{{FAILED_COUNT}}", "1");

    return html;
}

QWidget *ReportSettingsDialog::makeColorButtonWithReset(const QString &, QPushButton *&colorBtnOut, QPushButton *&resetBtnOut)
{
    QWidget *container = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Color button — no tooltip
    QPushButton *colorBtn = new QPushButton(container);
    colorBtn->setMinimumHeight(28);
    colorBtn->setMaximumWidth(120);
    colorBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(colorBtn, &QPushButton::clicked, this, &ReportSettingsDialog::onColorButtonClicked);
    layout->addWidget(colorBtn);

    // Reset button — small square
    QPushButton *resetBtn = new QPushButton(QIcon(":/assets/refresh.png"), "", container);
    resetBtn->setFixedSize(28, 28);
    resetBtn->setToolTip(tr("Reset to previous value"));
    resetBtn->setStyleSheet("QPushButton { font-size: 14px; padding: 0; }");
    connect(resetBtn, &QPushButton::clicked, this, &ReportSettingsDialog::onResetColorClicked);
    layout->addWidget(resetBtn);

    colorBtnOut = colorBtn;
    resetBtnOut = resetBtn;
    return container;
}

void ReportSettingsDialog::updateColorButtonColor(QPushButton *btn, const QString &color)
{
    if (!btn) return;
    btn->setStyleSheet(QString("background-color: %1; border: 1px solid #888; min-height: 28px; max-width: 120px;")
                           .arg(color));
}

void ReportSettingsDialog::updateImagePreview(const QString &base64)
{
    if (base64.isEmpty()) {
        m_imagePreview->setText(tr("No\nImage"));
        m_imagePreview->setStyleSheet("border: 1px solid #555; background-color: #2a2a2a; color: #aaa; font-size: 10px;");
        return;
    }

    QByteArray bytes = QByteArray::fromBase64(base64.toLatin1());
    QPixmap pix;
    pix.loadFromData(bytes);
    if (!pix.isNull()) {
        m_imagePreview->setText("");
        m_imagePreview->setStyleSheet("border: 1px solid #555; background-color: #2a2a2a;");
        m_imagePreview->setPixmap(pix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        m_imagePreview->setText(tr("No\nImage"));
        m_imagePreview->setStyleSheet("border: 1px solid #555; background-color: #2a2a2a; color: #aaa; font-size: 10px;");
    }
}