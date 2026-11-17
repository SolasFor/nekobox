#include <QApplication>
#include <QColor>
#include <QFile>
#include <QGuiApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QTextStream>
#include <QtGlobal>

#include <nekobox/dataStore/Utils.hpp>
#include <nekobox/ui/setting/ThemeManager.hpp>

ThemeManager *themeManager = new ThemeManager;

namespace {

QPalette buildModernPalette(bool dark) {
    QPalette pal;
    if (dark) {
        const QColor windowBg(0x21, 0x20, 0x1E);
        const QColor baseBg(0x19, 0x18, 0x16);
        const QColor altBase(0x1E, 0x1D, 0x1A);
        const QColor buttonBg(0x30, 0x2E, 0x2A);
        const QColor textPrimary(0xEC, 0xE9, 0xE0);
        const QColor textMuted(0x87, 0x86, 0x7F);
        const QColor textDisabled(0x5E, 0x5D, 0x59);
        const QColor accent(0xD9, 0x77, 0x57);

        pal.setColor(QPalette::Window, windowBg);
        pal.setColor(QPalette::WindowText, textPrimary);
        pal.setColor(QPalette::Base, baseBg);
        pal.setColor(QPalette::AlternateBase, altBase);
        pal.setColor(QPalette::ToolTipBase, QColor(0x2A, 0x29, 0x26));
        pal.setColor(QPalette::ToolTipText, textPrimary);
        pal.setColor(QPalette::PlaceholderText, textMuted);
        pal.setColor(QPalette::Text, textPrimary);
        pal.setColor(QPalette::Button, buttonBg);
        pal.setColor(QPalette::ButtonText, textPrimary);
        pal.setColor(QPalette::BrightText, Qt::white);
        pal.setColor(QPalette::Link, QColor(0xE2, 0x88, 0x6B));
        pal.setColor(QPalette::Highlight, accent);
        pal.setColor(QPalette::HighlightedText, Qt::white);
        pal.setColor(QPalette::Midlight, QColor(0x35, 0x33, 0x2E));
        pal.setColor(QPalette::Mid, QColor(0x46, 0x43, 0x3D));
        pal.setColor(QPalette::Dark, QColor(0x14, 0x13, 0x11));

        pal.setColor(QPalette::Disabled, QPalette::WindowText, textDisabled);
        pal.setColor(QPalette::Disabled, QPalette::Text, textDisabled);
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, textDisabled);
    } else {
        const QColor windowBg(0xFA, 0xF9, 0xF5);
        const QColor baseBg(0xFF, 0xFF, 0xFF);
        const QColor altBase(0xFA, 0xF9, 0xF5);
        const QColor buttonBg(0xFF, 0xFF, 0xFF);
        const QColor textPrimary(0x14, 0x14, 0x13);
        const QColor textMuted(0x87, 0x86, 0x7F);
        const QColor textDisabled(0xB0, 0xAE, 0xA5);
        const QColor accent(0xC9, 0x64, 0x42);

        pal.setColor(QPalette::Window, windowBg);
        pal.setColor(QPalette::WindowText, textPrimary);
        pal.setColor(QPalette::Base, baseBg);
        pal.setColor(QPalette::AlternateBase, altBase);
        pal.setColor(QPalette::ToolTipBase, baseBg);
        pal.setColor(QPalette::ToolTipText, textPrimary);
        pal.setColor(QPalette::PlaceholderText, textMuted);
        pal.setColor(QPalette::Text, textPrimary);
        pal.setColor(QPalette::Button, buttonBg);
        pal.setColor(QPalette::ButtonText, textPrimary);
        pal.setColor(QPalette::BrightText, Qt::white);
        pal.setColor(QPalette::Link, accent);
        pal.setColor(QPalette::Highlight, accent);
        pal.setColor(QPalette::HighlightedText, Qt::white);
        pal.setColor(QPalette::Midlight, QColor(0xF0, 0xEE, 0xE6));
        pal.setColor(QPalette::Mid, QColor(0xD5, 0xD1, 0xC7));
        pal.setColor(QPalette::Dark, QColor(0x87, 0x86, 0x7F));

        pal.setColor(QPalette::Disabled, QPalette::WindowText, textDisabled);
        pal.setColor(QPalette::Disabled, QPalette::Text, textDisabled);
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, textDisabled);
    }
    return pal;
}

} // namespace

QMap<QString, QString> &ThemeManager::getThemes() {
    static QMap<QString, QString> map;
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
    }
    return map;
}

bool ThemeManager::isModernTheme(const QString &theme) {
    const auto lower = theme.trimmed().toLower();
    return lower.isEmpty() || lower == "system" || lower == "claude" ||
           lower == "claude dark" || lower == "claude light" ||
           lower == "modern" || lower == "modern dark" || lower == "modern light";
}

bool ThemeManager::isDarkEffective(const QString &theme) {
    const int mode = getMode(theme);
    if (mode == 1) {
        return true;
    }
    if (mode == 2) {
        return false;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (qApp != nullptr && QGuiApplication::styleHints() != nullptr) {
        const auto scheme = QGuiApplication::styleHints()->colorScheme();
        if (scheme == Qt::ColorScheme::Dark) {
            return true;
        }
        if (scheme == Qt::ColorScheme::Light) {
            return false;
        }
    }
#endif
    if (qApp != nullptr) {
        return qApp->palette().color(QPalette::Window).lightness() < 128;
    }
    return true;
}

std::tuple<QString, bool> ThemeManager::getPath(const QString &theme) {
    auto lowerTheme = theme.toLower();
    if (isModernTheme(lowerTheme)) {
        return std::make_tuple(qApp->style()->name(), false);
    } else if (lowerTheme == "dark") {
        return std::make_tuple(":nekobox/qss/MaterialDark.qss", true);
    } else if (lowerTheme == "amoled") {
        return std::make_tuple(":nekobox/qss/AMOLED.qss", true);
    } else if (lowerTheme == "aqua") {
        return std::make_tuple(":nekobox/qss/Aqua.qss", true);
    } else if (lowerTheme == "kawaii") {
        return std::make_tuple(":nekobox/qss/Kawaii.qss", true);
    } else if (lowerTheme == "flatgray") {
        return std::make_tuple(":nekobox/qss/flatgray.css", true);
    } else if (lowerTheme == "lightblue") {
        return std::make_tuple(":nekobox/qss/lightblue.css", true);
    } else if (lowerTheme == "blacksoft") {
        return std::make_tuple(":nekobox/qss/blacksoft.css", true);
    } else {
        return std::make_tuple(theme, false);
    }
}

int ThemeManager::getMode(const QString &theme) {
    auto lowerTheme = theme.trimmed().toLower();
    if (lowerTheme.isEmpty() || lowerTheme == "system" || lowerTheme == "claude" ||
        lowerTheme == "modern") {
        return 0;
    } else if (lowerTheme == "claude dark" || lowerTheme == "modern dark" ||
               lowerTheme == "dark" || lowerTheme == "amoled" ||
               lowerTheme == "aqua" || lowerTheme == "blacksoft") {
        return 1;
    } else if (lowerTheme == "claude light" || lowerTheme == "modern light" ||
               lowerTheme == "kawaii" || lowerTheme == "flatgray" ||
               lowerTheme == "lightblue" || lowerTheme == "windowsvista") {
        return 2;
    } else {
        return 0;
    }
}

QString ThemeManager::buildModernStylesheet(bool dark) {
    if (dark) {
        return QStringLiteral(
            "QMainWindow, QDialog, QMessageBox {"
            "  background-color: #21201E;"
            "  color: #ECE9E0;"
            "}"
            "QWidget {"
            "  color: #ECE9E0;"
            "  selection-background-color: #D97757;"
            "  selection-color: #FFFFFF;"
            "}"
            "QWidget:disabled {"
            "  color: #5E5D59;"
            "}"
            "QToolTip {"
            "  background-color: #2A2926;"
            "  color: #ECE9E0;"
            "  border: 1px solid rgba(217, 119, 87, 145);"
            "  border-radius: 8px;"
            "  padding: 5px 9px;"
            "}"
            "QMenuBar {"
            "  background-color: #21201E;"
            "  color: #ECE9E0;"
            "  border-bottom: 1px solid rgba(236, 233, 224, 24);"
            "}"
            "QMenuBar::item {"
            "  background: transparent;"
            "  padding: 5px 10px;"
            "  border-radius: 6px;"
            "}"
            "QMenuBar::item:selected {"
            "  background-color: rgba(217, 119, 87, 52);"
            "  color: #FFFFFF;"
            "}"
            "QMenu {"
            "  background-color: #2A2926;"
            "  color: #ECE9E0;"
            "  border: 1px solid rgba(236, 233, 224, 34);"
            "  border-radius: 10px;"
            "  padding: 6px;"
            "}"
            "QMenu::item {"
            "  padding: 6px 26px 6px 26px;"
            "  border-radius: 6px;"
            "}"
            "QMenu::item:selected {"
            "  background-color: rgba(217, 119, 87, 72);"
            "  color: #FFFFFF;"
            "}"
            "QMenu::item:disabled {"
            "  color: #5E5D59;"
            "}"
            "QMenu::separator {"
            "  height: 1px;"
            "  background: rgba(236, 233, 224, 24);"
            "  margin: 4px 8px;"
            "}"
            "QTabWidget::pane {"
            "  background-color: #2A2926;"
            "  border: 1px solid rgba(236, 233, 224, 24);"
            "  border-radius: 10px;"
            "  top: -1px;"
            "}"
            "QTabBar {"
            "  background: transparent;"
            "}"
            "QTabBar::tab {"
            "  background-color: rgba(236, 233, 224, 10);"
            "  color: #B0AEA5;"
            "  border: 1px solid rgba(236, 233, 224, 20);"
            "  border-radius: 7px;"
            "  padding: 6px 14px;"
            "  margin-right: 4px;"
            "  margin-bottom: 3px;"
            "}"
            "QTabBar::tab:hover {"
            "  background-color: #35332E;"
            "  color: #ECE9E0;"
            "  border-color: rgba(217, 119, 87, 110);"
            "}"
            "QTabBar::tab:selected {"
            "  background-color: #302E2A;"
            "  color: #FFFFFF;"
            "  border: 1px solid #D97757;"
            "  font-weight: 600;"
            "}"
            "QTableView, QTableWidget, QListView, QListWidget, QTreeView, QTreeWidget {"
            "  background-color: #191816;"
            "  alternate-background-color: #1E1D1A;"
            "  border: 1px solid rgba(236, 233, 224, 24);"
            "  border-radius: 10px;"
            "  gridline-color: rgba(236, 233, 224, 14);"
            "  outline: none;"
            "}"
            "QTableView::item, QTableWidget::item, QListView::item, QListWidget::item, QTreeView::item, QTreeWidget::item {"
            "  padding: 4px 8px;"
            "  border-bottom: 1px solid rgba(236, 233, 224, 10);"
            "}"
            "QTableView::item:hover, QTableWidget::item:hover, QListView::item:hover, QListWidget::item:hover, QTreeView::item:hover, QTreeWidget::item:hover {"
            "  background-color: rgba(236, 233, 224, 14);"
            "}"
            "QTableView::item:selected, QTableView::item:selected:!active,"
            "QTableWidget::item:selected, QTableWidget::item:selected:!active {"
            "  background-color: rgba(217, 119, 87, 52);"
            "}"
            "QListView::item:selected, QListView::item:selected:!active,"
            "QListWidget::item:selected, QListWidget::item:selected:!active,"
            "QTreeView::item:selected, QTreeView::item:selected:!active,"
            "QTreeWidget::item:selected, QTreeWidget::item:selected:!active {"
            "  background-color: rgba(217, 119, 87, 72);"
            "  color: #FFFFFF;"
            "}"
            "QHeaderView {"
            "  background: transparent;"
            "  border: none;"
            "}"
            "QHeaderView::section:horizontal {"
            "  background-color: #252421;"
            "  color: #ECE9E0;"
            "  font-weight: 600;"
            "  padding: 6px 10px;"
            "  border: none;"
            "  border-bottom: 1px solid rgba(236, 233, 224, 24);"
            "  border-right: 1px solid rgba(236, 233, 224, 14);"
            "}"
            "QHeaderView::section:vertical {"
            "  background-color: #21201E;"
            "  color: #B0AEA5;"
            "  font-weight: 600;"
            "  padding: 0px 6px;"
            "  border: none;"
            "  border-bottom: 1px solid rgba(236, 233, 224, 12);"
            "  border-right: 1px solid rgba(236, 233, 224, 18);"
            "}"
            "QHeaderView::section:hover {"
            "  background-color: #302E2A;"
            "  color: #FFFFFF;"
            "}"
            "QTableCornerButton::section {"
            "  background-color: #252421;"
            "  border: none;"
            "  border-bottom: 1px solid rgba(236, 233, 224, 24);"
            "  border-right: 1px solid rgba(236, 233, 224, 18);"
            "}"
            "QPushButton {"
            "  background-color: #302E2A;"
            "  color: #ECE9E0;"
            "  border: 1px solid rgba(236, 233, 224, 38);"
            "  border-radius: 8px;"
            "  padding: 5px 14px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #393732;"
            "  border: 1px solid rgba(217, 119, 87, 145);"
            "  color: #FFFFFF;"
            "}"
            "QPushButton:pressed {"
            "  background-color: rgba(217, 119, 87, 72);"
            "  border: 1px solid #D97757;"
            "}"
            "QPushButton:checked {"
            "  background-color: rgba(217, 119, 87, 68);"
            "  border: 1px solid #D97757;"
            "  color: #FFFFFF;"
            "}"
            "QPushButton:default {"
            "  background-color: #D97757;"
            "  border: 1px solid #E2886B;"
            "  color: #FFFFFF;"
            "  font-weight: 600;"
            "}"
            "QPushButton:default:hover {"
            "  background-color: #E2886B;"
            "}"
            "QPushButton:default:pressed {"
            "  background-color: #C96442;"
            "}"
            "QPushButton:disabled {"
            "  background-color: rgba(236, 233, 224, 8);"
            "  color: #5E5D59;"
            "  border: 1px solid rgba(236, 233, 224, 16);"
            "}"
            "QToolButton {"
            "  background-color: #302E2A;"
            "  color: #ECE9E0;"
            "  border: 1px solid rgba(236, 233, 224, 30);"
            "  border-radius: 8px;"
            "  padding: 4px 8px;"
            "}"
            "QToolButton:hover {"
            "  background-color: #393732;"
            "  border: 1px solid rgba(217, 119, 87, 145);"
            "  color: #FFFFFF;"
            "}"
            "QToolButton:pressed {"
            "  background-color: rgba(217, 119, 87, 72);"
            "  border: 1px solid #D97757;"
            "}"
            "QToolButton:checked {"
            "  background-color: rgba(217, 119, 87, 64);"
            "  border: 1px solid #D97757;"
            "  color: #FFFFFF;"
            "}"
            "QToolButton:disabled {"
            "  background-color: rgba(236, 233, 224, 8);"
            "  color: #5E5D59;"
            "  border: 1px solid rgba(236, 233, 224, 16);"
            "}"
            "QLineEdit, QPlainTextEdit, QTextEdit {"
            "  background-color: #191816;"
            "  color: #ECE9E0;"
            "  border: 1px solid rgba(236, 233, 224, 38);"
            "  border-radius: 8px;"
            "  padding: 5px 9px;"
            "  selection-background-color: #D97757;"
            "  selection-color: #FFFFFF;"
            "}"
            "QLineEdit:hover, QPlainTextEdit:hover, QTextEdit:hover {"
            "  border: 1px solid rgba(217, 119, 87, 125);"
            "}"
            "QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus {"
            "  background-color: #24221F;"
            "  border: 1px solid #D97757;"
            "}"
            "QSpinBox, QDoubleSpinBox {"
            "  background-color: #191816;"
            "  color: #ECE9E0;"
            "  border: 1px solid rgba(236, 233, 224, 38);"
            "  border-radius: 8px;"
            "  padding: 4px 8px;"
            "}"
            "QSpinBox:hover, QDoubleSpinBox:hover {"
            "  border: 1px solid rgba(217, 119, 87, 125);"
            "}"
            "QSpinBox:focus, QDoubleSpinBox:focus {"
            "  background-color: #24221F;"
            "  border: 1px solid #D97757;"
            "}"
            "QSpinBox::up-button, QSpinBox::down-button,"
            "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
            "  background: transparent;"
            "  border: none;"
            "  width: 16px;"
            "}"
            "QComboBox {"
            "  background-color: #302E2A;"
            "  color: #ECE9E0;"
            "  border: 1px solid rgba(236, 233, 224, 38);"
            "  border-radius: 8px;"
            "  padding: 5px 28px 5px 10px;"
            "}"
            "QComboBox:hover {"
            "  background-color: #393732;"
            "  border: 1px solid rgba(217, 119, 87, 140);"
            "}"
            "QComboBox:focus, QComboBox:on {"
            "  border: 1px solid #D97757;"
            "}"
            "QComboBox::drop-down {"
            "  subcontrol-origin: padding;"
            "  subcontrol-position: top right;"
            "  width: 26px;"
            "  border: none;"
            "  background: transparent;"
            "}"
            "QComboBox::down-arrow {"
            "  image: url(:/icon/chevron-down.svg);"
            "  width: 15px;"
            "  height: 15px;"
            "}"
            "QComboBox QAbstractItemView {"
            "  background-color: #2A2926;"
            "  color: #ECE9E0;"
            "  border: 1px solid rgba(236, 233, 224, 36);"
            "  border-radius: 8px;"
            "  padding: 4px;"
            "  outline: none;"
            "  selection-background-color: rgba(217, 119, 87, 72);"
            "  selection-color: #FFFFFF;"
            "}"
            "QAbstractSpinBox QLineEdit, QComboBox QLineEdit {"
            "  background: transparent;"
            "  border: none;"
            "  padding: 0px;"
            "}"
            "QCheckBox, QRadioButton {"
            "  spacing: 7px;"
            "}"
            "QCheckBox::indicator {"
            "  width: 15px;"
            "  height: 15px;"
            "  border-radius: 4px;"
            "  border: 1px solid rgba(236, 233, 224, 65);"
            "  background-color: #191816;"
            "}"
            "QCheckBox::indicator:hover {"
            "  border: 1px solid #D97757;"
            "}"
            "QCheckBox::indicator:checked {"
            "  background-color: #D97757;"
            "  border: 1px solid #D97757;"
            "  image: url(:/icon/check.svg);"
            "}"
            "QCheckBox::indicator:disabled {"
            "  background-color: rgba(236, 233, 224, 10);"
            "  border: 1px solid rgba(236, 233, 224, 24);"
            "}"
            "QRadioButton::indicator {"
            "  width: 12px;"
            "  height: 12px;"
            "  border-radius: 8px;"
            "  border: 2px solid rgba(236, 233, 224, 65);"
            "  background-color: #191816;"
            "}"
            "QRadioButton::indicator:hover {"
            "  border: 2px solid #D97757;"
            "}"
            "QRadioButton::indicator:checked {"
            "  width: 8px;"
            "  height: 8px;"
            "  border-radius: 8px;"
            "  border: 4px solid #D97757;"
            "  background-color: #ECE9E0;"
            "}"
            "QTextBrowser#masterLogBrowser {"
            "  background-color: #191816;"
            "  border: 1px solid rgba(236, 233, 224, 24);"
            "  border-radius: 10px;"
            "  padding: 8px;"
            "}"
            "QGroupBox {"
            "  background-color: #2A2926;"
            "  border: 1px solid rgba(236, 233, 224, 24);"
            "  border-radius: 10px;"
            "  margin-top: 14px;"
            "  padding-top: 12px;"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin;"
            "  subcontrol-position: top left;"
            "  left: 12px;"
            "  padding: 0 6px;"
            "  color: #E2886B;"
            "  font-weight: 600;"
            "}"
            "QProgressBar {"
            "  background-color: #191816;"
            "  color: #ECE9E0;"
            "  border: 1px solid rgba(236, 233, 224, 24);"
            "  border-radius: 6px;"
            "  text-align: center;"
            "}"
            "QProgressBar::chunk {"
            "  background-color: #D97757;"
            "  border-radius: 5px;"
            "}"
            "QScrollArea {"
            "  background: transparent;"
            "  border: none;"
            "}"
            "QScrollBar:vertical {"
            "  background: transparent;"
            "  width: 9px;"
            "  margin: 2px 1px 2px 1px;"
            "}"
            "QScrollBar::handle:vertical {"
            "  background: rgba(236, 233, 224, 48);"
            "  min-height: 28px;"
            "  border-radius: 4px;"
            "}"
            "QScrollBar::handle:vertical:hover {"
            "  background: rgba(217, 119, 87, 190);"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
            "  background: none;"
            "  height: 0px;"
            "}"
            "QScrollBar:horizontal {"
            "  background: transparent;"
            "  height: 9px;"
            "  margin: 1px 2px 1px 2px;"
            "}"
            "QScrollBar::handle:horizontal {"
            "  background: rgba(236, 233, 224, 48);"
            "  min-width: 28px;"
            "  border-radius: 4px;"
            "}"
            "QScrollBar::handle:horizontal:hover {"
            "  background: rgba(217, 119, 87, 190);"
            "}"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,"
            "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
            "  background: none;"
            "  width: 0px;"
            "}");
    }

    return QStringLiteral(
        "QMainWindow, QDialog, QMessageBox {"
        "  background-color: #FAF9F5;"
        "  color: #141413;"
        "}"
        "QWidget {"
        "  color: #141413;"
        "  selection-background-color: #C96442;"
        "  selection-color: #FFFFFF;"
        "}"
        "QWidget:disabled {"
        "  color: #B0AEA5;"
        "}"
        "QToolTip {"
        "  background-color: #FFFFFF;"
        "  color: #141413;"
        "  border: 1px solid #D97757;"
        "  border-radius: 8px;"
        "  padding: 5px 9px;"
        "}"
        "QMenuBar {"
        "  background-color: #FAF9F5;"
        "  color: #141413;"
        "  border-bottom: 1px solid #E5E2D9;"
        "}"
        "QMenuBar::item {"
        "  background: transparent;"
        "  padding: 5px 10px;"
        "  border-radius: 6px;"
        "}"
        "QMenuBar::item:selected {"
        "  background-color: #F6E5DF;"
        "  color: #9E4426;"
        "}"
        "QMenu {"
        "  background-color: #FFFFFF;"
        "  color: #141413;"
        "  border: 1px solid #E5E2D9;"
        "  border-radius: 10px;"
        "  padding: 6px;"
        "}"
        "QMenu::item {"
        "  padding: 6px 26px 6px 26px;"
        "  border-radius: 6px;"
        "}"
        "QMenu::item:selected {"
        "  background-color: #F6E5DF;"
        "  color: #9E4426;"
        "}"
        "QMenu::item:disabled {"
        "  color: #B0AEA5;"
        "}"
        "QMenu::separator {"
        "  height: 1px;"
        "  background: #E5E2D9;"
        "  margin: 4px 8px;"
        "}"
        "QTabWidget::pane {"
        "  background-color: #FFFFFF;"
        "  border: 1px solid #E5E2D9;"
        "  border-radius: 10px;"
        "  top: -1px;"
        "}"
        "QTabBar {"
        "  background: transparent;"
        "}"
        "QTabBar::tab {"
        "  background-color: #F0EEE6;"
        "  color: #5E5D59;"
        "  border: 1px solid #E5E2D9;"
        "  border-radius: 7px;"
        "  padding: 6px 14px;"
        "  margin-right: 4px;"
        "  margin-bottom: 3px;"
        "}"
        "QTabBar::tab:hover {"
        "  background-color: #F6E5DF;"
        "  color: #9E4426;"
        "  border-color: #E5A992;"
        "}"
        "QTabBar::tab:selected {"
        "  background-color: #FFFFFF;"
        "  color: #141413;"
        "  border: 1px solid #C96442;"
        "  font-weight: 600;"
        "}"
        "QTableView, QTableWidget, QListView, QListWidget, QTreeView, QTreeWidget {"
        "  background-color: #FFFFFF;"
        "  alternate-background-color: #FAF9F5;"
        "  border: 1px solid #E5E2D9;"
        "  border-radius: 10px;"
        "  gridline-color: #F0EEE6;"
        "  outline: none;"
        "}"
        "QTableView::item, QTableWidget::item, QListView::item, QListWidget::item, QTreeView::item, QTreeWidget::item {"
        "  padding: 4px 8px;"
        "  border-bottom: 1px solid #F0EEE6;"
        "}"
        "QTableView::item:hover, QTableWidget::item:hover, QListView::item:hover, QListWidget::item:hover, QTreeView::item:hover, QTreeWidget::item:hover {"
        "  background-color: #F5F3EC;"
        "}"
        "QTableView::item:selected, QTableView::item:selected:!active,"
        "QTableWidget::item:selected, QTableWidget::item:selected:!active {"
        "  background-color: rgba(201, 100, 66, 34);"
        "  color: #141413;"
        "}"
        "QListView::item:selected, QListView::item:selected:!active,"
        "QListWidget::item:selected, QListWidget::item:selected:!active,"
        "QTreeView::item:selected, QTreeView::item:selected:!active,"
        "QTreeWidget::item:selected, QTreeWidget::item:selected:!active {"
        "  background-color: #F6E5DF;"
        "  color: #141413;"
        "}"
        "QHeaderView {"
        "  background: transparent;"
        "  border: none;"
        "}"
        "QHeaderView::section:horizontal {"
        "  background-color: #F3F1E9;"
        "  color: #141413;"
        "  font-weight: 600;"
        "  padding: 6px 10px;"
        "  border: none;"
        "  border-bottom: 1px solid #E5E2D9;"
        "  border-right: 1px solid #ECE9DF;"
        "}"
        "QHeaderView::section:vertical {"
        "  background-color: #F7F5EE;"
        "  color: #5E5D59;"
        "  font-weight: 600;"
        "  padding: 0px 6px;"
        "  border: none;"
        "  border-bottom: 1px solid #ECE9DF;"
        "  border-right: 1px solid #E5E2D9;"
        "}"
        "QHeaderView::section:hover {"
        "  background-color: #EAE7DD;"
        "  color: #141413;"
        "}"
        "QTableCornerButton::section {"
        "  background-color: #F3F1E9;"
        "  border: none;"
        "  border-bottom: 1px solid #E5E2D9;"
        "  border-right: 1px solid #E5E2D9;"
        "}"
        "QPushButton {"
        "  background-color: #FFFFFF;"
        "  color: #141413;"
        "  border: 1px solid #D5D1C7;"
        "  border-radius: 8px;"
        "  padding: 5px 14px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #F6E5DF;"
        "  border: 1px solid #C96442;"
        "  color: #9E4426;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #EDD3C9;"
        "  border: 1px solid #B85737;"
        "}"
        "QPushButton:checked {"
        "  background-color: #F6E5DF;"
        "  border: 1px solid #C96442;"
        "  color: #9E4426;"
        "}"
        "QPushButton:default {"
        "  background-color: #C96442;"
        "  border: 1px solid #B85737;"
        "  color: #FFFFFF;"
        "  font-weight: 600;"
        "}"
        "QPushButton:default:hover {"
        "  background-color: #B85737;"
        "}"
        "QPushButton:default:pressed {"
        "  background-color: #A84C2C;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #F3F1E9;"
        "  color: #B0AEA5;"
        "  border: 1px solid #E5E2D9;"
        "}"
        "QToolButton {"
        "  background-color: #FFFFFF;"
        "  color: #141413;"
        "  border: 1px solid #D5D1C7;"
        "  border-radius: 8px;"
        "  padding: 4px 8px;"
        "}"
        "QToolButton:hover {"
        "  background-color: #F6E5DF;"
        "  border: 1px solid #C96442;"
        "  color: #9E4426;"
        "}"
        "QToolButton:pressed {"
        "  background-color: #EDD3C9;"
        "  border: 1px solid #B85737;"
        "}"
        "QToolButton:checked {"
        "  background-color: #F6E5DF;"
        "  border: 1px solid #C96442;"
        "  color: #9E4426;"
        "}"
        "QToolButton:disabled {"
        "  background-color: #F3F1E9;"
        "  color: #B0AEA5;"
        "  border: 1px solid #E5E2D9;"
        "}"
        "QLineEdit, QPlainTextEdit, QTextEdit {"
        "  background-color: #FFFFFF;"
        "  color: #141413;"
        "  border: 1px solid #D5D1C7;"
        "  border-radius: 8px;"
        "  padding: 5px 9px;"
        "  selection-background-color: #C96442;"
        "  selection-color: #FFFFFF;"
        "}"
        "QLineEdit:hover, QPlainTextEdit:hover, QTextEdit:hover {"
        "  border: 1px solid #D97757;"
        "}"
        "QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus {"
        "  border: 1px solid #C96442;"
        "}"
        "QSpinBox, QDoubleSpinBox {"
        "  background-color: #FFFFFF;"
        "  color: #141413;"
        "  border: 1px solid #D5D1C7;"
        "  border-radius: 8px;"
        "  padding: 4px 8px;"
        "}"
        "QSpinBox:hover, QDoubleSpinBox:hover {"
        "  border: 1px solid #D97757;"
        "}"
        "QSpinBox:focus, QDoubleSpinBox:focus {"
        "  border: 1px solid #C96442;"
        "}"
        "QSpinBox::up-button, QSpinBox::down-button,"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
        "  background: transparent;"
        "  border: none;"
        "  width: 16px;"
        "}"
        "QComboBox {"
        "  background-color: #FFFFFF;"
        "  color: #141413;"
        "  border: 1px solid #D5D1C7;"
        "  border-radius: 8px;"
        "  padding: 5px 28px 5px 10px;"
        "}"
        "QComboBox:hover {"
        "  background-color: #F5F3EC;"
        "  border: 1px solid #D97757;"
        "}"
        "QComboBox:focus, QComboBox:on {"
        "  border: 1px solid #C96442;"
        "}"
        "QComboBox::drop-down {"
        "  subcontrol-origin: padding;"
        "  subcontrol-position: top right;"
        "  width: 26px;"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QComboBox::down-arrow {"
        "  image: url(:/icon/chevron-down.svg);"
        "  width: 15px;"
        "  height: 15px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background-color: #FFFFFF;"
        "  color: #141413;"
        "  border: 1px solid #D5D1C7;"
        "  border-radius: 8px;"
        "  padding: 4px;"
        "  outline: none;"
        "  selection-background-color: #F6E5DF;"
        "  selection-color: #9E4426;"
        "}"
        "QAbstractSpinBox QLineEdit, QComboBox QLineEdit {"
        "  background: transparent;"
        "  border: none;"
        "  padding: 0px;"
        "}"
        "QCheckBox, QRadioButton {"
        "  spacing: 7px;"
        "}"
        "QCheckBox::indicator {"
        "  width: 15px;"
        "  height: 15px;"
        "  border-radius: 4px;"
        "  border: 1px solid #C8C3B8;"
        "  background-color: #FFFFFF;"
        "}"
        "QCheckBox::indicator:hover {"
        "  border: 1px solid #C96442;"
        "}"
        "QCheckBox::indicator:checked {"
        "  background-color: #C96442;"
        "  border: 1px solid #C96442;"
        "  image: url(:/icon/check.svg);"
        "}"
        "QCheckBox::indicator:disabled {"
        "  background-color: #F3F1E9;"
        "  border: 1px solid #E5E2D9;"
        "}"
        "QRadioButton::indicator {"
        "  width: 12px;"
        "  height: 12px;"
        "  border-radius: 8px;"
        "  border: 2px solid #C8C3B8;"
        "  background-color: #FFFFFF;"
        "}"
        "QRadioButton::indicator:hover {"
        "  border: 2px solid #C96442;"
        "}"
        "QRadioButton::indicator:checked {"
        "  width: 8px;"
        "  height: 8px;"
        "  border-radius: 8px;"
        "  border: 4px solid #C96442;"
        "  background-color: #FFFFFF;"
        "}"
        "QTextBrowser#masterLogBrowser {"
        "  background-color: #FFFFFF;"
        "  border: 1px solid #E5E2D9;"
        "  border-radius: 10px;"
        "  padding: 8px;"
        "}"
        "QGroupBox {"
        "  background-color: #FFFFFF;"
        "  border: 1px solid #E5E2D9;"
        "  border-radius: 10px;"
        "  margin-top: 14px;"
        "  padding-top: 12px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  left: 12px;"
        "  padding: 0 6px;"
        "  color: #B85737;"
        "  font-weight: 600;"
        "}"
        "QProgressBar {"
        "  background-color: #F0EEE6;"
        "  color: #141413;"
        "  border: 1px solid #E5E2D9;"
        "  border-radius: 6px;"
        "  text-align: center;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: #C96442;"
        "  border-radius: 5px;"
        "}"
        "QScrollArea {"
        "  background: transparent;"
        "  border: none;"
        "}"
        "QScrollBar:vertical {"
        "  background: transparent;"
        "  width: 9px;"
        "  margin: 2px 1px 2px 1px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #D5D1C7;"
        "  min-height: 28px;"
        "  border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: #C96442;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "  height: 0px;"
        "}"
        "QScrollBar:horizontal {"
        "  background: transparent;"
        "  height: 9px;"
        "  margin: 1px 2px 1px 2px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "  background: #D5D1C7;"
        "  min-width: 28px;"
        "  border-radius: 4px;"
        "}"
        "QScrollBar::handle:horizontal:hover {"
        "  background: #C96442;"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
        "  background: none;"
        "  width: 0px;"
        "}");
}

void ThemeManager::ApplyTheme(const QString &theme, bool force) {
    static QString nativeStyleName;
    if (nativeStyleName.isEmpty() && qApp != nullptr && qApp->style() != nullptr) {
        nativeStyleName = qApp->style()->name();
    }

    if (!force && theme == current_theme) {
        return;
    }

    static const QString nekoCss = QStringLiteral(
        "\nQMessageBox {\n"
        "    messagebox-text-interaction-flags: 5;\n"
        "}\n");

    if (isModernTheme(theme)) {
        qApp->setStyleSheet(QString());
        if (!nativeStyleName.isEmpty()) {
            qApp->setStyle(nativeStyleName);
        }
        const bool dark = isDarkEffective(theme);
        qApp->setPalette(buildModernPalette(dark));
        qApp->setStyleSheet(buildModernStylesheet(dark) + nekoCss);
    } else {
        QString themePath;
        bool isFile = false;
        std::tie(themePath, isFile) = getPath(theme);

        if (!isFile) {
            qApp->setStyleSheet(QString());
            qApp->setStyle(themePath);
            if (qApp->style() != nullptr) {
                qApp->setPalette(qApp->style()->standardPalette());
            }
            qApp->setStyleSheet(nekoCss);
        } else {
            QFile f(themePath);
            if (f.open(QFile::ReadOnly | QFile::Text)) {
                QTextStream ts(&f);
                QString qss = ts.readAll();
                f.close();

                auto lowerTheme = theme.toLower();
                if (lowerTheme == "flatgray" || lowerTheme == "lightblue" ||
                    lowerTheme == "blacksoft") {
                    qss.replace(":/qss/", ":/nekobox/qss/");
                }

                qApp->setStyleSheet(qss + nekoCss);
            }
        }
    }

    current_theme = theme;
    emit themeChanged(theme);
}
