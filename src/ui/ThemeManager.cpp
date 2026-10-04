#include "ThemeManager.hpp"
#include <QApplication>
#include <QFile>
#include <QDir>
#include <QFileInfo>

ThemeManager* ThemeManager::s_instance = nullptr;

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
    , m_currentIndex(0)
{
    initializeThemes();

    QString savedId = savedThemeId();
    if (savedId == QStringLiteral("apple_glass")) {
        savedId = QStringLiteral("light_theme");
        saveThemeId(savedId);
    }
    if (!savedId.isEmpty()) {
        for (int i = 0; i < m_themes.size(); ++i) {
            if (m_themes[i].id == savedId) {
                m_currentIndex = i;
                return;
            }
        }
    }

    // Use Emerald Matrix unless the user has previously selected a theme.
    for (int i = 0; i < m_themes.size(); ++i) {
        if (m_themes[i].id == QStringLiteral("emerald_matrix")) {
            m_currentIndex = i;
            break;
        }
    }
}

ThemeManager* ThemeManager::instance()
{
    if (!s_instance) {
        s_instance = new ThemeManager();
    }
    return s_instance;
}

void ThemeManager::initializeThemes()
{
    // =========================================================================
    // 1. Light Theme (Apple-inspired translucent light surfaces)
    // =========================================================================
    const QString lightTheme = QStringLiteral(R"(
QMainWindow {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 rgba(245,247,251,0.88),
        stop:0.45 rgba(237,242,248,0.84),
        stop:1 rgba(232,237,245,0.88));
}
QWidget {
    font-family: "SF Pro Display", "SF Pro Text", "Inter", "Segoe UI", sans-serif;
    color: #1f2937;
}
QWidget#headerWidget {
    background: rgba(255,255,255,0.46);
    border: 1px solid rgba(255,255,255,0.55);
    border-radius: 18px;
    padding: 12px 20px;
    box-shadow: 0 12px 32px rgba(15, 23, 42, 0.08);
}
QFrame#machinePanel {
    background: rgba(255,255,255,0.34);
    border: 1px solid rgba(255,255,255,0.68);
    border-radius: 20px;
    padding: 16px;
    box-shadow: 0 12px 28px rgba(15, 23, 42, 0.06);
}
QFrame#machinePanel:hover {
    border-color: rgba(59, 130, 246, 0.45);
    background: rgba(255,255,255,0.42);
}
QFrame#machinePanel[machineState="CONNECTED"] {
    border: 1px solid rgba(16, 185, 129, 0.45);
    background: rgba(230, 255, 245, 0.45);
}
QFrame#machinePanel[machineState="ONLINE_SSH_UNAVAILABLE"] {
    border: 1px dashed rgba(245, 158, 11, 0.58);
    background: rgba(255, 248, 220, 0.36);
}
QFrame#machinePanel[machineState="AUTH_REQUIRED"] {
    border: 1px solid rgba(251, 146, 60, 0.55);
    background: rgba(255, 244, 214, 0.35);
}
QFrame#machinePanel[machineState="ERROR"] {
    border: 1px solid rgba(239, 68, 68, 0.45);
    background: rgba(255, 236, 236, 0.32);
}
QLabel#machineHeading {
    color: #111827;
    font-size: 16px;
    font-weight: 700;
    letter-spacing: 0.2px;
}
QLabel#statusLabel { font-size: 12px; font-weight: 700; }
QLabel#ipHeading {
    color: #64748b;
    font-size: 10px;
    font-weight: 700;
    letter-spacing: 0.12em;
    text-transform: uppercase;
}
QLabel#ipAddress {
    background: rgba(255,255,255,0.48);
    color: #1d4ed8;
    border: 1px solid rgba(148,163,184,0.28);
    border-radius: 9px;
    font-family: "SF Mono", "Monaco", "JetBrains Mono", monospace;
    font-size: 12px;
    font-weight: 600;
    padding: 5px 10px;
}
QFrame#authFrame {
    background: rgba(255,255,255,0.38);
    border: 1px solid rgba(148, 163, 184, 0.4);
    border-radius: 14px;
    padding: 10px;
}
QLineEdit {
    background: rgba(255,255,255,0.62);
    color: #111827;
    border: 1px solid rgba(148,163,184,0.35);
    border-radius: 10px;
    padding: 7px 11px;
    font-size: 12px;
    min-height: 30px;
}
QLineEdit:focus {
    background: rgba(255,255,255,0.78);
    border-color: rgba(59,130,246,0.55);
}
QPushButton {
    background: rgba(255,255,255,0.52);
    color: #0f172a;
    border: 1px solid rgba(148,163,184,0.32);
    border-radius: 10px;
    padding: 6px 12px;
    min-height: 30px;
    font-size: 12px;
    font-weight: 600;
}
QPushButton:hover {
    background: rgba(255,255,255,0.72);
    border-color: rgba(59,130,246,0.42);
}
QPushButton:pressed {
    background: rgba(219,234,254,0.9);
}
QPushButton:disabled {
    background: rgba(226,232,240,0.5);
    color: rgba(71, 85, 105, 0.8);
}
QPushButton#disconnect {
    background: rgba(254, 226, 226, 0.75);
    color: #991b1b;
    border: 1px solid rgba(239, 68, 68, 0.35);
}
QPushButton#disconnect:hover {
    background: rgba(254, 202, 202, 0.92);
    color: #7f1d1d;
}
QTreeView {
    background: rgba(255,255,255,0.34);
    color: #1f2937;
    border: 1px solid rgba(148,163,184,0.35);
    border-radius: 12px;
    padding: 4px;
    outline: none;
}
QTreeView::item {
    padding: 6px 8px;
    border-radius: 7px;
}
QTreeView::item:hover {
    background: rgba(191,219,254,0.24);
}
QTreeView::item:selected {
    background: rgba(96,165,250,0.28);
    color: #0f172a;
}
QHeaderView::section {
    background: rgba(255,255,255,0.45);
    color: #475569;
    padding: 7px 8px;
    border: none;
    border-right: 1px solid rgba(148,163,184,0.25);
    border-bottom: 1px solid rgba(148,163,184,0.25);
    font-size: 11px;
    font-weight: 700;
}
QFrame#progressDock {
    background: rgba(255,255,255,0.52);
    border: 1px solid rgba(148,163,184,0.34);
    border-radius: 16px;
    padding: 8px;
    box-shadow: 0 12px 28px rgba(15, 23, 42, 0.06);
}
QProgressBar {
    background: rgba(255,255,255,0.65);
    border: 1px solid rgba(148,163,184,0.32);
    border-radius: 8px;
    text-align: center;
    color: #0f172a;
    font-size: 11px;
    font-weight: 700;
    height: 18px;
}
QProgressBar::chunk {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #60a5fa, stop:1 #2dd4bf);
    border-radius: 7px;
}
QMenu {
    background: rgba(255,255,255,0.92);
    border: 1px solid rgba(148,163,184,0.4);
    border-radius: 12px;
    padding: 6px;
}
QMenu::item {
    padding: 8px 18px;
    border-radius: 8px;
    color: #0f172a;
}
QMenu::item:selected {
    background: rgba(191,219,254,0.72);
    color: #0f172a;
}
QScrollBar:vertical {
    background: rgba(148,163,184,0.12);
    width: 10px;
    border-radius: 5px;
}
QScrollBar::handle:vertical {
    background: rgba(148,163,184,0.45);
    min-height: 24px;
    border-radius: 5px;
}
QStatusBar {
    background: rgba(255,255,255,0.42);
    color: #475569;
    border-top: 1px solid rgba(148,163,184,0.25);
    font-size: 12px;
}
)" );

    // =========================================================================
    // 2. Cyber Glass Dark (Flagship Glassmorphic Aesthetic)
    // =========================================================================
    const QString cyberGlassDark = QStringLiteral(R"(
/* DirZero - Cyber Glass Dark Theme */
QMainWindow {
    background: #090d14;
}

QWidget {
    font-family: "SF Pro Display", "Inter", "Segoe UI", "Ubuntu", sans-serif;
    color: #e2e8f0;
}

/* Glass Header Bar */
QWidget#headerWidget {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 rgba(22, 31, 48, 0.95), stop:1 rgba(15, 22, 34, 0.95));
    border: 1px solid rgba(56, 189, 248, 0.2);
    border-radius: 12px;
    padding: 10px 18px;
}

/* Glass Machine Card */
QFrame#machinePanel {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(25, 36, 56, 0.9), stop:1 rgba(17, 24, 38, 0.9));
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 14px;
    padding: 16px;
}

QFrame#machinePanel:hover {
    border: 1px solid rgba(56, 189, 248, 0.4);
}

/* Card States with Glow Borders */
QFrame#machinePanel[machineState="CONNECTED"] {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(20, 38, 48, 0.92), stop:1 rgba(14, 26, 36, 0.92));
    border: 1px solid #10b981;
}

QFrame#machinePanel[machineState="ONLINE_SSH_UNAVAILABLE"] {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(36, 26, 18, 0.92), stop:1 rgba(22, 17, 14, 0.92));
    border: 1px dashed #d97706;
}

QFrame#machinePanel[machineState="AUTH_REQUIRED"] {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(38, 28, 16, 0.95), stop:1 rgba(24, 18, 12, 0.95));
    border: 1px solid #f59e0b;
}

QFrame#machinePanel[machineState="ERROR"] {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(38, 18, 22, 0.92), stop:1 rgba(26, 13, 16, 0.92));
    border: 1px solid #ef4444;
}

/* Headings & Monospace Badges */
QLabel#machineHeading {
    color: #f8fafc;
    font-size: 16px;
    font-weight: 800;
    letter-spacing: 0.5px;
}

QLabel#statusLabel {
    font-size: 12px;
    font-weight: 700;
}

QLabel#ipHeading {
    color: #64748b;
    font-size: 10px;
    font-weight: 700;
    letter-spacing: 0.5px;
    text-transform: uppercase;
}

QLabel#ipAddress {
    background: rgba(10, 14, 22, 0.9);
    color: #38bdf8;
    border: 1px solid rgba(56, 189, 248, 0.25);
    border-radius: 6px;
    font-family: "JetBrains Mono", "Fira Code", monospace;
    font-size: 12px;
    font-weight: 600;
    padding: 5px 10px;
}

/* In-App Authentication Box */
QFrame#authFrame {
    background: rgba(15, 23, 42, 0.95);
    border: 1px solid rgba(56, 189, 248, 0.3);
    border-radius: 10px;
    padding: 10px;
    margin: 4px 0px;
}

/* Modern Input Controls */
QLineEdit {
    background: rgba(10, 15, 26, 0.85);
    color: #f8fafc;
    border: 1px solid rgba(51, 65, 85, 0.8);
    border-radius: 6px;
    padding: 6px 10px;
    font-size: 11px;
}

QLineEdit:focus {
    border-color: #38bdf8;
    background: rgba(14, 21, 37, 0.95);
}

/* Sleek Glass Buttons */
QPushButton {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(37, 51, 77, 0.9), stop:1 rgba(26, 36, 56, 0.9));
    color: #e2e8f0;
    border: 1px solid rgba(255, 255, 255, 0.14);
    border-radius: 6px;
    padding: 4px 10px;
    font-size: 12px;
    font-weight: 600;
}

QPushButton:hover {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(56, 189, 248, 0.3), stop:1 rgba(37, 99, 235, 0.4));
    border-color: #38bdf8;
    color: #ffffff;
}

QPushButton:pressed {
    background: #1d4ed8;
}

QPushButton:disabled {
    background: rgba(15, 20, 28, 0.5);
    color: #475569;
    border-color: rgba(30, 41, 59, 0.4);
}

QPushButton#disconnect {
    background: rgba(76, 29, 30, 0.7);
    color: #fca5a5;
    border: 1px solid #7f1d1d;
}

QPushButton#disconnect:hover {
    background: #991b1b;
    color: #ffffff;
    border-color: #ef4444;
}

/* TreeView Filesystem */
QTreeView {
    background: rgba(11, 16, 25, 0.85);
    color: #cbd5e1;
    border: 1px solid rgba(35, 45, 63, 0.9);
    border-radius: 8px;
    padding: 4px;
    outline: none;
    font-size: 12px;
}

QTreeView::item {
    padding: 5px 6px;
    border-radius: 4px;
}

QTreeView::item:hover {
    background: rgba(56, 189, 248, 0.15);
    color: #ffffff;
}

QTreeView::item:selected {
    background: rgba(30, 58, 138, 0.8);
    color: #93c5fd;
}

QHeaderView::section {
    background: rgba(18, 25, 38, 0.95);
    color: #94a3b8;
    padding: 6px 8px;
    border: none;
    border-right: 1px solid #232d3f;
    border-bottom: 1px solid #1e293b;
    font-size: 11px;
    font-weight: 700;
}

/* In-App Transfer Progress Dock */
QFrame#progressDock {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 rgba(19, 27, 41, 0.98), stop:1 rgba(15, 23, 35, 0.98));
    border: 1px solid rgba(56, 189, 248, 0.3);
    border-radius: 10px;
    padding: 10px 14px;
}

QProgressBar {
    background: rgba(11, 15, 22, 0.9);
    border: 1px solid #28354b;
    border-radius: 5px;
    text-align: center;
    color: #f8fafc;
    font-size: 11px;
    font-weight: 700;
    height: 18px;
}

QProgressBar::chunk {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2563eb, stop:1 #38bdf8);
    border-radius: 4px;
}

/* Glass Context Menu */
QMenu {
    background: rgba(23, 30, 44, 0.98);
    border: 1px solid rgba(56, 189, 248, 0.3);
    border-radius: 8px;
    padding: 6px;
}

QMenu::item {
    padding: 7px 24px;
    border-radius: 4px;
    color: #e2e8f0;
    font-size: 12px;
}

QMenu::item:selected {
    background: #1e3a8a;
    color: #ffffff;
}

/* Scrollbars */
QScrollBar:vertical {
    background: rgba(11, 15, 22, 0.6);
    width: 8px;
    border-radius: 4px;
}

QScrollBar::handle:vertical {
    background: rgba(56, 189, 248, 0.3);
    min-height: 24px;
    border-radius: 4px;
}

QScrollBar::handle:vertical:hover {
    background: #38bdf8;
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0px;
}

QStatusBar {
    background: rgba(9, 13, 20, 0.95);
    color: #94a3b8;
    border-top: 1px solid #1a2230;
    font-size: 12px;
}
)");

    // =========================================================================
    // 2. Midnight Cyber Glass
    // =========================================================================
    const QString midnightCyber = QStringLiteral(R"(
QMainWindow { background: #07090e; }
QWidget { font-family: "SF Pro Display", "Inter", sans-serif; color: #e2e8f0; }
QWidget#headerWidget {
    background: #0f141f;
    border: 1px solid #1d2638;
    border-radius: 12px;
    padding: 10px 18px;
}
QFrame#machinePanel {
    background: #0d121c;
    border: 1px solid #1a2336;
    border-radius: 14px;
    padding: 16px;
}
QFrame#machinePanel:hover { border-color: #2563eb; }
QFrame#machinePanel[machineState="CONNECTED"] { border: 1px solid #10b981; }
QFrame#machinePanel[machineState="AUTH_REQUIRED"] { border: 1px solid #f59e0b; }
QFrame#machinePanel[machineState="ERROR"] { border: 1px solid #ef4444; }
QLabel#machineHeading { color: #f8fafc; font-size: 16px; font-weight: 800; }
QLabel#ipAddress {
    background: #080c14;
    color: #60a5fa;
    border: 1px solid #1e293b;
    border-radius: 6px;
    font-family: monospace;
    font-size: 12px;
    padding: 5px 10px;
}
QFrame#authFrame {
    background: #0b1019;
    border: 1px solid #1e293b;
    border-radius: 10px;
    padding: 10px;
}
QLineEdit {
    background: #080c14;
    color: #f8fafc;
    border: 1px solid #1e293b;
    border-radius: 6px;
    padding: 6px 10px;
}
QPushButton {
    background: #1e293b;
    color: #e2e8f0;
    border: 1px solid #334155;
    border-radius: 6px;
    padding: 4px 10px;
    font-size: 12px;
    font-weight: 600;
}
QPushButton:hover { background: #2563eb; color: #ffffff; }
QTreeView {
    background: #080c14;
    color: #cbd5e1;
    border: 1px solid #1a2336;
    border-radius: 8px;
}
QTreeView::item:selected { background: #1e3a8a; }
QProgressBar { background: #080c14; border: 1px solid #1e293b; border-radius: 5px; height: 18px; text-align: center; }
QProgressBar::chunk { background: #2563eb; border-radius: 4px; }
QMenu { background: #0d121c; border: 1px solid #1e293b; }
QMenu::item:selected { background: #1e3a8a; }
QStatusBar { background: #07090e; color: #94a3b8; border-top: 1px solid #161f30; }
)");

    // =========================================================================
    // 3. Emerald Matrix Glass
    // =========================================================================
    const QString emeraldGlass = QStringLiteral(R"(
QMainWindow { background: #060e0a; }
QWidget { font-family: "SF Pro Display", "Inter", sans-serif; color: #ecfdf5; }
QLabel#viewHeading { color: #ecfdf5; font-size: 26px; font-weight: 700; }
QLabel#viewDescription { color: #94a3b8; font-size: 13px; }
QLabel#localPathLabel { color: #a7f3d0; font-size: 12px; padding: 6px 10px; }
QPushButton#driveCard {
    background: rgba(16, 38, 28, 0.78);
    color: #ecfdf5;
    border: 1px solid rgba(52, 211, 153, 0.22);
    border-radius: 14px;
    text-align: left;
    padding: 18px;
    font-size: 13px;
    font-weight: 600;
}
QPushButton#driveCard:hover {
    background: rgba(20, 55, 39, 0.9);
    border-color: rgba(52, 211, 153, 0.55);
}
QLabel#sessionNotice { color: #94a3b8; font-size: 11px; }
QWidget#headerWidget {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #0d1f17, stop:1 #081610);
    border: 1px solid rgba(16, 185, 129, 0.3);
    border-radius: 12px;
    padding: 10px 18px;
}
QFrame#machinePanel {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0f241b, stop:1 #0a1913);
    border: 1px solid rgba(16, 185, 129, 0.2);
    border-radius: 14px;
    padding: 16px;
}
QFrame#machinePanel:hover { border: 1px solid #10b981; }
QFrame#machinePanel[machineState="CONNECTED"] { border: 1px solid #34d399; }
QLabel#machineHeading { color: #34d399; font-size: 16px; font-weight: 800; }
QLabel#ipAddress {
    background: #050f0b;
    color: #6ee7b7;
    border: 1px solid #064e3b;
    border-radius: 6px;
    font-family: monospace;
    font-size: 12px;
    padding: 5px 10px;
}
QFrame#authFrame { background: #091a13; border: 1px solid #065f46; border-radius: 10px; padding: 10px; }
QLineEdit { background: #050f0b; color: #ecfdf5; border: 1px solid #065f46; border-radius: 6px; padding: 6px 10px; }
QPushButton { background: #064e3b; color: #a7f3d0; border: 1px solid #059669; border-radius: 6px; padding: 4px 10px; font-size: 12px; font-weight: 600; }
QPushButton:hover { background: #059669; color: #ffffff; }
QTreeView { background: #050f0b; color: #a7f3d0; border: 1px solid #064e3b; border-radius: 8px; }
QTreeView::item:selected { background: #065f46; color: #ecfdf5; }
QProgressBar { background: #050f0b; border: 1px solid #065f46; border-radius: 5px; height: 18px; text-align: center; }
QProgressBar::chunk { background: #10b981; border-radius: 4px; }
QMenu { background: #091a13; border: 1px solid #059669; }
QMenu::item:selected { background: #065f46; }
QStatusBar { background: #060e0a; color: #6ee7b7; border-top: 1px solid #064e3b; }
)");

    // =========================================================================
    // 4. Glass Frost Light (Modern Clean Light Mode)
    // =========================================================================
    const QString glassFrostLight = QStringLiteral(R"(
QMainWindow { background: #f1f5f9; }
QWidget { font-family: "SF Pro Display", "Inter", sans-serif; color: #0f172a; }
QLabel#viewHeading { color: #0f172a; font-size: 26px; font-weight: 700; }
QLabel#viewDescription { color: #64748b; font-size: 13px; }
QLabel#localPathLabel { color: #334155; font-size: 12px; padding: 6px 10px; }
QPushButton#driveCard {
    background: rgba(255,255,255,0.72);
    color: #0f172a;
    border: 1px solid rgba(148,163,184,0.36);
    border-radius: 14px;
    text-align: left;
    padding: 18px;
    font-size: 13px;
    font-weight: 600;
}
QPushButton#driveCard:hover {
    background: rgba(255,255,255,0.94);
    border-color: rgba(59,130,246,0.45);
}
QLabel#sessionNotice { color: #64748b; font-size: 11px; }
QWidget#headerWidget {
    background: rgba(255, 255, 255, 0.95);
    border: 1px solid #cbd5e1;
    border-radius: 12px;
    padding: 10px 18px;
}
QFrame#machinePanel {
    background: rgba(255, 255, 255, 0.9);
    border: 1px solid #e2e8f0;
    border-radius: 14px;
    padding: 16px;
}
QFrame#machinePanel:hover { border-color: #3b82f6; }
QFrame#machinePanel[machineState="CONNECTED"] { border: 1px solid #10b981; background: #f0fdf4; }
QFrame#machinePanel[machineState="AUTH_REQUIRED"] { border: 1px solid #f59e0b; background: #fffbeb; }
QFrame#machinePanel[machineState="ERROR"] { border: 1px solid #ef4444; background: #fef2f2; }
QLabel#machineHeading { color: #0f172a; font-size: 16px; font-weight: 800; }
QLabel#ipHeading { color: #64748b; font-size: 10px; font-weight: 700; }
QLabel#ipAddress {
    background: #e2e8f0;
    color: #1e3a8a;
    border: 1px solid #cbd5e1;
    border-radius: 6px;
    font-family: monospace;
    font-size: 12px;
    font-weight: 600;
    padding: 5px 10px;
}
QFrame#authFrame { background: #f8fafc; border: 1px solid #cbd5e1; border-radius: 10px; padding: 10px; }
QLineEdit { background: #ffffff; color: #0f172a; border: 1px solid #cbd5e1; border-radius: 6px; padding: 6px 10px; }
QLineEdit:focus { border-color: #3b82f6; }
QPushButton {
    background: #e2e8f0;
    color: #1e293b;
    border: 1px solid #cbd5e1;
    border-radius: 6px;
    padding: 4px 10px;
    font-size: 12px;
    font-weight: 600;
}
QPushButton:hover { background: #3b82f6; color: #ffffff; border-color: #2563eb; }
QPushButton#disconnect { background: #fee2e2; color: #991b1b; border: 1px solid #f87171; }
QPushButton#disconnect:hover { background: #ef4444; color: #ffffff; }
QTreeView { background: #ffffff; color: #1e293b; border: 1px solid #cbd5e1; border-radius: 8px; }
QTreeView::item:hover { background: #f1f5f9; }
QTreeView::item:selected { background: #bfdbfe; color: #1e3a8a; }
QHeaderView::section { background: #f8fafc; color: #475569; border-right: 1px solid #e2e8f0; border-bottom: 1px solid #cbd5e1; }
QFrame#progressDock { background: #ffffff; border: 1px solid #cbd5e1; border-radius: 10px; }
QProgressBar { background: #e2e8f0; border: 1px solid #cbd5e1; border-radius: 5px; height: 18px; text-align: center; color: #1e293b; }
QProgressBar::chunk { background: #3b82f6; border-radius: 4px; }
QMenu { background: #ffffff; border: 1px solid #cbd5e1; }
QMenu::item:selected { background: #3b82f6; color: #ffffff; }
QStatusBar { background: #f1f5f9; color: #475569; border-top: 1px solid #e2e8f0; }
)");

    // Register themes
    m_themes.append({QStringLiteral("light_theme"), QStringLiteral("☀️ Light Theme"), lightTheme});
    m_themes.append({QStringLiteral("cyber_glass_dark"), QStringLiteral("💎 Cyber Glass Dark"), cyberGlassDark});
    m_themes.append({QStringLiteral("midnight_cyber"), QStringLiteral("🌙 Midnight Glass"), midnightCyber});
    m_themes.append({QStringLiteral("emerald_matrix"), QStringLiteral("🟩 Emerald Matrix"), emeraldGlass});
    m_themes.append({QStringLiteral("glass_frost_light"), QStringLiteral("❄️ Glass Frost Light"), glassFrostLight});
}

ThemeDefinition ThemeManager::currentTheme() const
{
    if (m_currentIndex >= 0 && m_currentIndex < m_themes.size()) {
        return m_themes.at(m_currentIndex);
    }
    return m_themes.first();
}

QString ThemeManager::currentThemeId() const
{
    return currentTheme().id;
}

bool ThemeManager::switchToTheme(const QString& themeId)
{
    for (int i = 0; i < m_themes.size(); ++i) {
        if (m_themes[i].id == themeId) {
            m_currentIndex = i;
            applyCurrentTheme();
            saveThemeId(themeId);
            emit themeChanged(m_themes[i].id, m_themes[i].name);
            return true;
        }
    }
    return false;
}

ThemeDefinition ThemeManager::cycleNextTheme()
{
    if (m_themes.isEmpty()) {
        return {};
    }
    m_currentIndex = (m_currentIndex + 1) % m_themes.size();
    applyCurrentTheme();
    ThemeDefinition curr = currentTheme();
    saveThemeId(curr.id);
    emit themeChanged(curr.id, curr.name);
    return curr;
}

void ThemeManager::applyCurrentTheme()
{
    if (qApp) {
        ThemeDefinition curr = currentTheme();
        qApp->setStyleSheet(curr.qssContent);
    }
}

QString ThemeManager::savedThemeId() const
{
    QFile file(QStringLiteral(".active_theme"));
    if (file.open(QIODevice::ReadOnly)) {
        return QString::fromUtf8(file.readAll()).trimmed();
    }
    return QString();
}

void ThemeManager::saveThemeId(const QString& id)
{
    QFile file(QStringLiteral(".active_theme"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(id.toUtf8());
    }
}
