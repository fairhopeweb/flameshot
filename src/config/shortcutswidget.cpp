// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Yurii Puchkov at Namecheap & Contributors

#include "shortcutswidget.h"
#include "capturetool.h"
#include "setshortcutwidget.h"
#include "src/core/qguiappcurrentscreen.h"
#include <QHeaderView>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVector>

#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
#include <QCursor>
#include <QRect>
#include <QScreen>
#endif

ShortcutsWidget::ShortcutsWidget(QWidget* parent)
  : QWidget(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowIcon(QIcon(":img/app/flameshot.svg"));
    setWindowTitle(tr("Hot Keys"));

#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
    QRect position = frameGeometry();
    QScreen* screen = QGuiAppCurrentScreen().currentScreen();
    position.moveCenter(screen->availableGeometry().center());
    move(position.topLeft());
#endif

    m_layout = new QVBoxLayout(this);
    m_layout->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    initShortcuts();
    initInfoTable();
    show();
}

const QList<QStringList>& ShortcutsWidget::shortcuts()
{
    return m_shortcuts;
}

void ShortcutsWidget::initInfoTable()
{
    m_table = new QTableWidget(this);
    m_table->setToolTip(tr("Available shortcuts in the screen capture mode."));

    m_layout->addWidget(m_table);

    m_table->setColumnCount(2);
    m_table->setRowCount(m_shortcuts.size());
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->verticalHeader()->hide();

    // header creation
    QStringList names;
    names << tr("Description") << tr("Key");
    m_table->setHorizontalHeaderLabels(names);
    connect(m_table,
            SIGNAL(cellClicked(int, int)),
            this,
            SLOT(slotShortcutCellClicked(int, int)));

    // add content
    for (int i = 0; i < shortcuts().size(); ++i) {
        const auto current_shortcut = m_shortcuts.at(i);
        const auto identifier = current_shortcut.at(0);
        const auto description = current_shortcut.at(1);
        const auto default_key_sequence = current_shortcut.at(2);
        m_table->setItem(i, 0, new QTableWidgetItem(description));

        const auto key_sequence = identifier.isEmpty()
                                    ? default_key_sequence
                                    : m_config.shortcut(identifier);
#if defined(Q_OS_MACOS)
        QTableWidgetItem* item =
          new QTableWidgetItem(nativeOSHotKeyText(key_sequence));
#else
        QTableWidgetItem* item = new QTableWidgetItem(key_sequence);
#endif
        item->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(i, 1, item);

        if (identifier.isEmpty()) {
            QFont font;
            font.setBold(true);
            item->setFont(font);
            item->setFlags(item->flags() ^ Qt::ItemIsEnabled);
            m_table->item(i, 1)->setFont(font);
        }
    }

    // Read-only table items
    for (int x = 0; x < m_table->rowCount(); ++x) {
        for (int y = 0; y < m_table->columnCount(); ++y) {
            QTableWidgetItem* item = m_table->item(x, y);
            item->setFlags(item->flags() ^ Qt::ItemIsEditable);
        }
    }

    // adjust size
    m_table->resizeColumnsToContents();
    m_table->resizeRowsToContents();
    m_table->horizontalHeader()->setMinimumSectionSize(200);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSizePolicy(QSizePolicy::Expanding,
                                               QSizePolicy::Expanding);
}

void ShortcutsWidget::slotShortcutCellClicked(int row, int col)
{
    if (col == 1) {
        // Ignore non-changable shortcuts
        if (Qt::ItemIsEnabled !=
            (Qt::ItemIsEnabled & m_table->item(row, col)->flags())) {
            return;
        }

        SetShortcutDialog* setShortcutDialog = new SetShortcutDialog();
        if (0 != setShortcutDialog->exec()) {
            QString shortcutName = m_shortcuts.at(row).at(0);
            QKeySequence shortcutValue = setShortcutDialog->shortcut();

            // set no shortcut is Backspace
#if defined(Q_OS_MACOS)
            if (shortcutValue == QKeySequence(Qt::CTRL + Qt::Key_Backspace)) {
                shortcutValue = QKeySequence("");
            }
#else
            if (shortcutValue == QKeySequence(Qt::Key_Backspace)) {
                shortcutValue = QKeySequence("");
            }
#endif

            if (m_config.setShortcut(shortcutName, shortcutValue.toString())) {
#if defined(Q_OS_MACOS)
                QTableWidgetItem* item = new QTableWidgetItem(
                  nativeOSHotKeyText(shortcutValue.toString()));
#else
                QTableWidgetItem* item =
                  new QTableWidgetItem(shortcutValue.toString());
#endif
                item->setTextAlignment(Qt::AlignCenter);
                item->setFlags(item->flags() ^ Qt::ItemIsEditable);
                m_table->setItem(row, col, item);
            }
        }
        delete setShortcutDialog;
    }
}

void ShortcutsWidget::initShortcuts()
{
    auto buttons = CaptureToolButton::getIterableButtonTypes();

    // get shortcuts names from capture buttons
    for (const CaptureToolButton::ButtonType& t : buttons) {
        CaptureToolButton* b = new CaptureToolButton(t, nullptr);
        QString shortcutName = QVariant::fromValue(t).toString();
        if (shortcutName != "TYPE_IMAGEUPLOADER") {
            appendShortcut(shortcutName, b->tool()->description());
        }
        delete b;
    }

    // additional tools that don't have their own buttons
    appendShortcut("TYPE_TOGGLE_PANEL", "Toggle side panel");
    appendShortcut("TYPE_RESIZE_LEFT", "Resize selection left 1px");
    appendShortcut("TYPE_RESIZE_RIGHT", "Resize selection right 1px");
    appendShortcut("TYPE_RESIZE_UP", "Resize selection up 1px");
    appendShortcut("TYPE_RESIZE_DOWN", "Resize selection down 1px");
    appendShortcut("TYPE_SELECT_ALL", "Select entire screen");
    appendShortcut("TYPE_MOVE_LEFT", "Move selection left 1px");
    appendShortcut("TYPE_MOVE_RIGHT", "Move selection right 1px");
    appendShortcut("TYPE_MOVE_UP", "Move selection up 1px");
    appendShortcut("TYPE_MOVE_DOWN", "Move selection down 1px");
    appendShortcut("TYPE_COMMIT_CURRENT_TOOL", "Commit text in text area");
    appendShortcut("TYPE_DELETE_CURRENT_TOOL", "Delete current tool");

    // non-editable shortcuts have an empty shortcut name

    m_shortcuts << (QStringList() << "" << QObject::tr("Quit capture")
                                  << QKeySequence(Qt::Key_Escape).toString());

    // Global hotkeys
#if defined(Q_OS_MACOS)
    m_shortcuts << (QStringList()
                    << "" << QObject::tr("Screenshot history") << "⇧⌘⌥H");
    m_shortcuts << (QStringList()
                    << "" << QObject::tr("Capture screen") << "⇧⌘⌥4");
#elif defined(Q_OS_WIN)
    m_shortcuts << (QStringList() << "" << QObject::tr("Screenshot history")
                                  << "Shift+Print Screen");
    m_shortcuts << (QStringList()
                    << "" << QObject::tr("Capture screen") << "Print Screen");
#else
    // TODO - Linux doesn't support global shortcuts for (XServer and Wayland),
    // possibly it will be solved in the QHotKey library later. So it is
    // disabled for now.
#endif
    m_shortcuts << (QStringList()
                    << "" << QObject::tr("Show color picker") << "Right Click");
    m_shortcuts << (QStringList()
                    << "" << QObject::tr("Change the tool's thickness")
                    << "Mouse Wheel");
}

void ShortcutsWidget::appendShortcut(const QString& shortcutName,
                                     const QString& description)
{
    m_shortcuts << (QStringList()
                    << shortcutName
                    << QObject::tr(description.toStdString().c_str())
                    << ConfigHandler().shortcut(shortcutName));
}

#if defined(Q_OS_MACOS)
const QString& ShortcutsWidget::nativeOSHotKeyText(const QString& text)
{
    m_res = text;
    m_res.replace("Ctrl+", "⌘");
    m_res.replace("Alt+", "⌥");
    m_res.replace("Meta+", "⌃");
    m_res.replace("Shift+", "⇧");
    return m_res;
}
#endif
