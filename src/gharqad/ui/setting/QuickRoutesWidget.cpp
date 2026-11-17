#include <nekobox/ui/setting/QuickRoutesWidget.h>

#include <nekobox/ui/setting/ProcessPickerDialog.h>
#include <nekobox/ui/setting/ThemeManager.hpp>

#include <nekobox/dataStore/DataStore.hpp>
#include <nekobox/dataStore/Database.hpp>
#include <nekobox/dataStore/ResourceEntity.hpp>
#include <nekobox/dataStore/RouteEntity.h>
#include <nekobox/global/GuiUtils.hpp>

#include <QAbstractItemView>
#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QDir>
#include <QTabWidget>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFocusEvent>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QMenu>
#include <QPalette>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {

void setStyledFlag(QWidget *widget, const char *name, bool value) {
    if (widget == nullptr || widget->property(name).toBool() == value) {
        return;
    }
    widget->setProperty(name, value);
    if (QStyle *st = widget->style()) {
        st->unpolish(widget);
        st->polish(widget);
    }
    widget->update();
}

QComboBox *findCellCombo(QTableWidget *table, int row, int column, const QString &objectName) {
    if (table == nullptr) {
        return nullptr;
    }
    QWidget *cell = table->cellWidget(row, column);
    if (cell == nullptr) {
        return nullptr;
    }
    if (auto *direct = qobject_cast<QComboBox *>(cell)) {
        return direct;
    }
    return cell->findChild<QComboBox *>(objectName);
}

QLineEdit *findCellLineEdit(QTableWidget *table, int row, int column, const QString &objectName) {
    if (table == nullptr) {
        return nullptr;
    }
    QWidget *cell = table->cellWidget(row, column);
    if (cell == nullptr) {
        return nullptr;
    }
    if (auto *direct = qobject_cast<QLineEdit *>(cell)) {
        return direct;
    }
    return cell->findChild<QLineEdit *>(objectName);
}

} // namespace

QuickRoutesWidget::QuickRoutesWidget(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("quickRoutesRoot"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    auto *heading = new QLabel(tr("Routing workspace"), this);
    heading->setObjectName(QStringLiteral("routesWorkspaceTitle"));
    root->addWidget(heading);
    auto *tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("routesWorkspaceTabs"));
    tabs->addTab(makeSection(tr("Applications"), &processTable, &processCountBadge,
                            &processRemoveButton, tr("Application"), true), tr("Processes"));
    tabs->addTab(makeSection(tr("Domains"), &domainTable, &domainCountBadge,
                            &domainRemoveButton, tr("Domain"), false, true), tr("Domains"));
    root->addWidget(tabs, 1);

    auto *saveBar = new QFrame(this);
    saveBar->setObjectName(QStringLiteral("routesSaveBar"));
    auto *saveRow = new QHBoxLayout(saveBar);
    saveRow->setContentsMargins(10, 6, 10, 6);
    saveRow->setSpacing(12);

    auto *hint = new QLabel(
        tr("Application routes match the .exe name. Use Tun mode to capture apps that do not use the system proxy. Saving restarts the core."),
        saveBar);
    hint->setObjectName(QStringLiteral("routesHintLabel"));
    hint->setWordWrap(true);
    saveRow->addWidget(hint, 1);

    saveButton = new QPushButton(tr("Save"), saveBar);
    saveButton->setObjectName(QStringLiteral("routesSaveButton"));
    saveButton->setCursor(Qt::PointingHandCursor);
    saveButton->setMinimumHeight(32);
    saveRow->addWidget(saveButton);
    root->addWidget(saveBar);

    connect(saveButton, &QPushButton::clicked, this, [this] { saveToStore(); });
    if (themeManager != nullptr) {
        connect(themeManager, &ThemeManager::themeChanged, this, [this] { applyVisualStyle(); });
    }

    applyVisualStyle();
    loadFromStore();
}

QWidget *QuickRoutesWidget::makeSection(const QString &title, QTableWidget **table,
                                        QLabel **countBadge, QPushButton **removeButtonOut,
                                        const QString &firstHeader, bool processSection,
                                        bool domainSection) {
    auto *section = new QWidget(this);
    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *headerFrame = new QFrame(section);
    headerFrame->setObjectName(QStringLiteral("routesSectionHeader"));
    auto *header = new QHBoxLayout(headerFrame);
    header->setContentsMargins(4, 2, 4, 2);
    header->setSpacing(8);

    auto *titleLabel = new QLabel(title, headerFrame);
    titleLabel->setObjectName(QStringLiteral("routesSectionTitle"));
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    header->addWidget(titleLabel);

    auto *badge = new QLabel(QStringLiteral("0"), headerFrame);
    badge->setObjectName(QStringLiteral("routesCountBadge"));
    badge->setAlignment(Qt::AlignCenter);
    badge->setMinimumWidth(24);
    header->addWidget(badge);
    if (countBadge != nullptr) {
        *countBadge = badge;
    }

    header->addStretch();

    auto *addButton = new QPushButton(tr("Add"), headerFrame);
    addButton->setObjectName(QStringLiteral("routesActionButton"));
    addButton->setCursor(Qt::PointingHandCursor);
    addButton->setMinimumHeight(28);
    header->addWidget(addButton);

    if (processSection) {
        auto *emptyButton = new QPushButton(tr("Empty process"), headerFrame);
        header->addWidget(emptyButton);
        connect(emptyButton, &QPushButton::clicked, this, [this] {
            addProcessRow({}, Configs::proxyID);
            const int row = processTable->rowCount() - 1;
            processTable->setRowHidden(row, false);
            processTable->scrollToItem(processTable->item(row, 0));
            if (auto *edit = findCellLineEdit(processTable, row, 0, QStringLiteral("process_path")))
                edit->setFocus();
        });
        auto *runningButton = new QPushButton(tr("Running processes..."), headerFrame);
        runningButton->setObjectName(QStringLiteral("routesActionButton"));
        runningButton->setCursor(Qt::PointingHandCursor);
        runningButton->setMinimumHeight(28);
        runningButton->setToolTip(
            tr("Pick programs that are already running instead of browsing for an .exe"));
        header->addWidget(runningButton);
        connect(runningButton, &QPushButton::clicked, this, [this] {
            ProcessPickerDialog dialog(this);
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }
            int lastAddedRow = -1;
            for (const QString &path : dialog.selectedPaths()) {
                addProcessRow(path, Configs::proxyID);
                lastAddedRow = processTable->rowCount() - 1;
            }
            if (lastAddedRow >= 0 && processTable->item(lastAddedRow, 0) != nullptr) {
                processTable->scrollToItem(processTable->item(lastAddedRow, 0));
            }
        });
    }

    if (removeButtonOut != nullptr) {
        *removeButtonOut = nullptr;
    }

    layout->addWidget(headerFrame);

    auto *created = new QTableWidget(0, domainSection ? 3 : 2, section);
    *table = created;
    if (domainSection) {
        setupTable(created, {firstHeader, tr("Method"), tr("Outbound")}, 2);
    } else {
        setupTable(created, {firstHeader, tr("Outbound")}, 1);
    }
    layout->addWidget(created, 1);

    auto *tools = new QHBoxLayout();
    auto *search = new QLineEdit(section);
    search->setPlaceholderText(tr("Search rules..."));
    search->setClearButtonEnabled(true);
    search->setMinimumHeight(30);
    tools->addWidget(search, 1);
    auto *bulk = new QPushButton(tr("Route selected..."), section);
    bulk->setMinimumHeight(30);
    bulk->setEnabled(false);
    tools->addWidget(bulk);
    layout->insertLayout(1, tools);
    connect(created->selectionModel(), &QItemSelectionModel::selectionChanged, bulk,
            [created, bulk] { bulk->setEnabled(!created->selectionModel()->selectedRows().isEmpty()); });
    connect(search, &QLineEdit::textChanged, created, [created, processSection](const QString &text) {
        created->clearSelection();
        for (int row = 0; row < created->rowCount(); ++row) {
            auto *edit = findCellLineEdit(created, row, 0, processSection
                ? QStringLiteral("process_path") : QStringLiteral("domain_edit"));
            created->setRowHidden(row, edit != nullptr &&
                !edit->text().contains(text.trimmed(), Qt::CaseInsensitive));
        }
    });
    connect(addButton, &QPushButton::clicked, search, &QLineEdit::clear);
    connect(bulk, &QPushButton::clicked, this, [this, created, bulk, domainSection] {
        QMenu menu(this);
        for (const auto &choice : collectOutbounds()) {
            auto *action = menu.addAction(choice.name);
            connect(action, &QAction::triggered, this, [created, domainSection, choice] {
                const auto rows = created->selectionModel()->selectedRows();
                for (const auto &index : rows) {
                    if (created->isRowHidden(index.row())) continue;
                    auto *combo = findCellCombo(created, index.row(), domainSection ? 2 : 1,
                                                QStringLiteral("outbound_combo"));
                    if (combo != nullptr) combo->setCurrentIndex(combo->findData(choice.id));
                }
            });
        }
        menu.exec(bulk->mapToGlobal(QPoint(0, bulk->height())));
    });

    connect(created->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this, created] { syncSectionState(created); });

    connect(addButton, &QPushButton::clicked, this, [this, processSection] {
        if (processSection) {
            // Cancelling the file dialog must not leave a blank row behind.
            const auto path = browseForExecutable();
            if (path.isEmpty()) {
                return;
            }
            addProcessRow(path, Configs::proxyID);
            const int last = processTable->rowCount() - 1;
            if (last >= 0 && processTable->item(last, 0) != nullptr) {
                processTable->scrollToItem(processTable->item(last, 0));
            }
        } else {
            addDomainRow({}, Configs::proxyID, QStringLiteral("suffix"));
            const int last = domainTable->rowCount() - 1;
            if (last >= 0) {
                if (domainTable->item(last, 0) != nullptr) {
                    domainTable->scrollToItem(domainTable->item(last, 0));
                }
                if (auto *edit = findCellLineEdit(domainTable, last, 0, QStringLiteral("domain_edit"))) {
                    edit->setFocus();
                }
            }
        }
    });

    return section;
}

void QuickRoutesWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    refreshOutboundCombos();
    syncAllSections();
}

void QuickRoutesWidget::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event != nullptr && event->type() == QEvent::PaletteChange) {
        applyVisualStyle();
    }
}

bool QuickRoutesWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == nullptr || event == nullptr) {
        return QWidget::eventFilter(watched, event);
    }

    if (watched == processTable || watched == domainTable) {
        auto *table = static_cast<QTableWidget *>(watched);
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if ((keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace) &&
                qobject_cast<QLineEdit *>(QApplication::focusWidget()) == nullptr) {
                removeSelectedRows(table);
                return true;
            }
        } else if (event->type() == QEvent::Leave) {
            int &hovered = (table == processTable) ? hoveredProcessRow : hoveredDomainRow;
            if (hovered != -1) {
                hovered = -1;
                syncSectionState(table);
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    if ((processTable != nullptr && watched == processTable->viewport()) ||
        (domainTable != nullptr && watched == domainTable->viewport())) {
        if (event->type() == QEvent::Leave) {
            QTableWidget *table = (processTable != nullptr && watched == processTable->viewport())
                                      ? processTable
                                      : domainTable;
            int &hovered = (table == processTable) ? hoveredProcessRow : hoveredDomainRow;
            if (hovered != -1) {
                hovered = -1;
                syncSectionState(table);
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    auto *widget = qobject_cast<QWidget *>(watched);
    if (widget == nullptr) {
        return QWidget::eventFilter(watched, event);
    }

    // Scrolling must not silently change the routing destination.
    if (event->type() == QEvent::Wheel && qobject_cast<QComboBox *>(widget) != nullptr &&
        !widget->hasFocus()) {
        event->ignore();
        return true;
    }

    if (event->type() == QEvent::Enter && widget->objectName() == QLatin1String("routeRowCell")) {
        if (QTableWidget *table = ownerTableFor(widget)) {
            const int row = rowForWidget(table, widget);
            int &hovered = (table == processTable) ? hoveredProcessRow : hoveredDomainRow;
            if (hovered != row) {
                hovered = row;
                syncSectionState(table);
            }
        }
    } else if (event->type() == QEvent::Leave &&
               widget->objectName() == QLatin1String("routeRowCell")) {
        if (QTableWidget *table = ownerTableFor(widget)) {
            const int row = rowForWidget(table, widget);
            int &hovered = (table == processTable) ? hoveredProcessRow : hoveredDomainRow;
            if (hovered == row) {
                hovered = -1;
                syncSectionState(table);
            }
        }
    } else if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            if (QTableWidget *table = ownerTableFor(widget)) {
                const int row = rowForWidget(table, widget);
                if (row >= 0) {
                    if (widget->property("rowSelectSurface").toBool()) {
                        selectRowFromClick(table, row, mouseEvent->modifiers(), true);
                        return true;
                    }
                    if (widget->property("rowEditorControl").toBool()) {
                        if (table->selectionModel() != nullptr &&
                            (!table->selectionModel()->isRowSelected(row, QModelIndex()) ||
                             table->selectionModel()->selectedRows().size() > 1)) {
                            selectRowFromClick(table, row, Qt::NoModifier, false);
                        }
                        return false;
                    }
                }
            }
        }
    } else if (event->type() == QEvent::FocusIn &&
               widget->property("rowEditorControl").toBool()) {
        auto *focusEvent = static_cast<QFocusEvent *>(event);
        if (focusEvent->reason() == Qt::TabFocusReason ||
            focusEvent->reason() == Qt::BacktabFocusReason ||
            focusEvent->reason() == Qt::ShortcutFocusReason) {
            if (QTableWidget *table = ownerTableFor(widget)) {
                const int row = rowForWidget(table, widget);
                if (row >= 0 && table->selectionModel() != nullptr &&
                    !table->selectionModel()->isRowSelected(row, QModelIndex())) {
                    selectRowFromClick(table, row, Qt::NoModifier, false);
                }
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}

void QuickRoutesWidget::setupTable(QTableWidget *table, const QStringList &headers,
                                   int outboundColumn) {
    table->setObjectName(QStringLiteral("routesTable"));
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setMinimumSectionSize(120);
    if (headers.size() > 2) {
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
        table->setColumnWidth(1, 235);
    }
    table->horizontalHeader()->setSectionResizeMode(outboundColumn, QHeaderView::Interactive);
    table->horizontalHeader()->setHighlightSections(false);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(44);
    table->setColumnWidth(outboundColumn, 195);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(false);
    table->setShowGrid(false);
    table->setFocusPolicy(Qt::StrongFocus);
    table->viewport()->setAttribute(Qt::WA_Hover, true);
    table->installEventFilter(this);
    table->viewport()->installEventFilter(this);
}

void QuickRoutesWidget::applyVisualStyle() {
    if (applyingStyle) {
        return;
    }
    applyingStyle = true;

    const bool dark = themeManager != nullptr
                          ? ThemeManager::isDarkEffective(themeManager->current_theme)
                          : (palette().color(QPalette::Window).lightness() < 128 ||
                             palette().color(QPalette::Base).lightness() < 128);

    QString qss = dark
        ? QStringLiteral(
              "QTableWidget#routesTable {"
              "  background-color: #191816;"
              "  border: 1px solid rgba(236, 233, 224, 24);"
              "  border-radius: 10px;"
              "  outline: none;"
              "}"
              "QTableWidget#routesTable QHeaderView::section {"
              "  background-color: #252421;"
              "  color: #ECE9E0;"
              "  font-weight: 600;"
              "  padding: 6px 10px;"
              "  border: none;"
              "  border-bottom: 1px solid rgba(236, 233, 224, 24);"
              "  border-right: 1px solid rgba(236, 233, 224, 14);"
              "}"
              "QTableWidget#routesTable::item {"
              "  padding: 0px;"
              "  border: none;"
              "  background: transparent;"
              "}"
              "QTableWidget#routesTable::item:selected,"
              "QTableWidget#routesTable::item:selected:!active {"
              "  background-color: rgba(217, 119, 87, 46);"
              "}"
              "QFrame#routeRowCell {"
              "  background-color: transparent;"
              "  border-top: none;"
              "  border-right: none;"
              "  border-bottom: 1px solid rgba(236, 233, 224, 14);"
              "  border-left: none;"
              "}"
              "QFrame#routeRowCell[firstCol=\"true\"] {"
              "  border-left: 3px solid transparent;"
              "}"
              "QFrame#routeRowCell[rowHovered=\"true\"][rowSelected=\"false\"] {"
              "  background-color: rgba(236, 233, 224, 12);"
              "}"
              "QFrame#routeRowCell[firstCol=\"true\"][rowHovered=\"true\"][rowSelected=\"false\"] {"
              "  border-left: 3px solid rgba(217, 119, 87, 130);"
              "}"
              "QFrame#routeRowCell[rowSelected=\"true\"] {"
              "  background-color: rgba(217, 119, 87, 46);"
              "  border-bottom: 1px solid rgba(217, 119, 87, 100);"
              "}"
              "QFrame#routeRowCell[firstCol=\"true\"][rowSelected=\"true\"] {"
              "  border-left: 3px solid #D97757;"
              "}"
              "QFrame#processSelectChip {"
              "  background-color: #2A2926;"
              "  border: 1px solid rgba(236, 233, 224, 28);"
              "  border-radius: 7px;"
              "}"
              "QFrame#processSelectChip:hover {"
              "  background-color: rgba(217, 119, 87, 42);"
              "  border: 1px solid rgba(217, 119, 87, 140);"
              "}"
              "QFrame#processSelectChip[rowSelected=\"true\"] {"
              "  background-color: rgba(217, 119, 87, 85);"
              "  border: 1px solid #D97757;"
              "}"
              "QLabel#processSelectDot {"
              "  color: #87867F;"
              "  font-size: 12px;"
              "  background: transparent;"
              "}"
              "QLabel#processSelectDot[rowSelected=\"true\"] {"
              "  color: #F5C4B0;"
              "}"
              "QLabel#processSelectName {"
              "  color: #ECE9E0;"
              "  background: transparent;"
              "}"
              "QLabel#domainRowGrip {"
              "  background-color: #2A2926;"
              "  border: 1px solid rgba(236, 233, 224, 26);"
              "  border-radius: 7px;"
              "  color: #87867F;"
              "  font-size: 12px;"
              "}"
              "QLabel#domainRowGrip:hover {"
              "  background-color: rgba(217, 119, 87, 42);"
              "  border: 1px solid rgba(217, 119, 87, 140);"
              "  color: #F5C4B0;"
              "}"
              "QLabel#domainRowGrip[rowSelected=\"true\"] {"
              "  background-color: rgba(217, 119, 87, 85);"
              "  border: 1px solid #D97757;"
              "  color: #FFFFFF;"
              "}"
              "QLabel#routesCountBadge {"
              "  background-color: rgba(217, 119, 87, 42);"
              "  color: #F2B8A2;"
              "  border: 1px solid rgba(217, 119, 87, 90);"
              "  border-radius: 9px;"
              "  padding: 1px 8px;"
              "  font-size: 11px;"
              "  font-weight: 600;"
              "}"
              "QPushButton#routesActionButton {"
              "  padding: 4px 12px;"
              "}"
              "QPushButton#routesRemoveButton {"
              "  padding: 4px 12px;"
              "}"
              "QPushButton#routesRemoveButton:enabled {"
              "  border: 1px solid rgba(214, 83, 71, 130);"
              "  color: #F5B5AE;"
              "}"
              "QPushButton#routesRemoveButton:enabled:hover {"
              "  background-color: rgba(214, 83, 71, 68);"
              "  border: 1px solid #D65347;"
              "  color: #FFFFFF;"
              "}"
              "QFrame#routesSaveBar {"
              "  background-color: #282724;"
              "  border: 1px solid rgba(236, 233, 224, 22);"
              "  border-radius: 10px;"
              "}"
              "QLabel#routesHintLabel {"
              "  color: #B0AEA5;"
              "}"
              "QPushButton#routesSaveButton {"
              "  background-color: #D97757;"
              "  color: #FFFFFF;"
              "  border: 1px solid #E2886B;"
              "  border-radius: 8px;"
              "  padding: 5px 18px;"
              "  font-weight: 600;"
              "}"
              "QPushButton#routesSaveButton:hover {"
              "  background-color: #E2886B;"
              "}"
              "QPushButton#routesSaveButton:pressed {"
              "  background-color: #C96442;"
              "}")
        : QStringLiteral(
              "QTableWidget#routesTable {"
              "  background-color: #FFFFFF;"
              "  border: 1px solid #E5E2D9;"
              "  border-radius: 10px;"
              "  outline: none;"
              "}"
              "QTableWidget#routesTable QHeaderView::section {"
              "  background-color: #F3F1E9;"
              "  color: #141413;"
              "  font-weight: 600;"
              "  padding: 6px 10px;"
              "  border: none;"
              "  border-bottom: 1px solid #E5E2D9;"
              "  border-right: 1px solid #ECE9DF;"
              "}"
              "QTableWidget#routesTable::item {"
              "  padding: 0px;"
              "  border: none;"
              "  background: transparent;"
              "}"
              "QTableWidget#routesTable::item:selected,"
              "QTableWidget#routesTable::item:selected:!active {"
              "  background-color: rgba(201, 100, 66, 26);"
              "}"
              "QFrame#routeRowCell {"
              "  background-color: transparent;"
              "  border-top: none;"
              "  border-right: none;"
              "  border-bottom: 1px solid #ECE9DF;"
              "  border-left: none;"
              "}"
              "QFrame#routeRowCell[firstCol=\"true\"] {"
              "  border-left: 3px solid transparent;"
              "}"
              "QFrame#routeRowCell[rowHovered=\"true\"][rowSelected=\"false\"] {"
              "  background-color: #F5F3EC;"
              "}"
              "QFrame#routeRowCell[firstCol=\"true\"][rowHovered=\"true\"][rowSelected=\"false\"] {"
              "  border-left: 3px solid #E5A992;"
              "}"
              "QFrame#routeRowCell[rowSelected=\"true\"] {"
              "  background-color: #F9EBE5;"
              "  border-bottom: 1px solid #E5A992;"
              "}"
              "QFrame#routeRowCell[firstCol=\"true\"][rowSelected=\"true\"] {"
              "  border-left: 3px solid #C96442;"
              "}"
              "QFrame#processSelectChip {"
              "  background-color: #F5F4EE;"
              "  border: 1px solid #D5D1C7;"
              "  border-radius: 7px;"
              "}"
              "QFrame#processSelectChip:hover {"
              "  background-color: #F6E5DF;"
              "  border: 1px solid #D97757;"
              "}"
              "QFrame#processSelectChip[rowSelected=\"true\"] {"
              "  background-color: #F6E5DF;"
              "  border: 1px solid #C96442;"
              "}"
              "QLabel#processSelectDot {"
              "  color: #87867F;"
              "  font-size: 12px;"
              "  background: transparent;"
              "}"
              "QLabel#processSelectDot[rowSelected=\"true\"] {"
              "  color: #A84C2C;"
              "}"
              "QLabel#processSelectName {"
              "  color: #141413;"
              "  background: transparent;"
              "}"
              "QLabel#domainRowGrip {"
              "  background-color: #F5F4EE;"
              "  border: 1px solid #D5D1C7;"
              "  border-radius: 7px;"
              "  color: #87867F;"
              "  font-size: 12px;"
              "}"
              "QLabel#domainRowGrip:hover {"
              "  background-color: #F6E5DF;"
              "  border: 1px solid #D97757;"
              "  color: #A84C2C;"
              "}"
              "QLabel#domainRowGrip[rowSelected=\"true\"] {"
              "  background-color: #F6E5DF;"
              "  border: 1px solid #C96442;"
              "  color: #A84C2C;"
              "}"
              "QLabel#routesCountBadge {"
              "  background-color: #F6E5DF;"
              "  color: #A84C2C;"
              "  border: 1px solid #E8BFA8;"
              "  border-radius: 9px;"
              "  padding: 1px 8px;"
              "  font-size: 11px;"
              "  font-weight: 600;"
              "}"
              "QPushButton#routesActionButton {"
              "  padding: 4px 12px;"
              "}"
              "QPushButton#routesRemoveButton {"
              "  padding: 4px 12px;"
              "}"
              "QPushButton#routesRemoveButton:enabled {"
              "  border: 1px solid #E5A29B;"
              "  color: #B43C31;"
              "}"
              "QPushButton#routesRemoveButton:enabled:hover {"
              "  background-color: #FCE9E7;"
              "  border: 1px solid #D65347;"
              "  color: #8F2C23;"
              "}"
              "QFrame#routesSaveBar {"
              "  background-color: #F3F1E9;"
              "  border: 1px solid #E5E2D9;"
              "  border-radius: 10px;"
              "}"
              "QLabel#routesHintLabel {"
              "  color: #5E5D59;"
              "}"
              "QPushButton#routesSaveButton {"
              "  background-color: #C96442;"
              "  color: #FFFFFF;"
              "  border: 1px solid #B85737;"
              "  border-radius: 8px;"
              "  padding: 5px 18px;"
              "  font-weight: 600;"
              "}"
              "QPushButton#routesSaveButton:hover {"
              "  background-color: #B85737;"
              "}"
              "QPushButton#routesSaveButton:pressed {"
              "  background-color: #A84C2C;"
              "}");

    qss += QStringLiteral(
        "QLabel#routesWorkspaceTitle { font-size: 20px; font-weight: 700; padding: 4px 8px; }"
        "QTabWidget#routesWorkspaceTabs::pane { border: none; background: transparent; }"
        "QTabWidget#routesWorkspaceTabs QTabBar::tab { padding: 8px 22px; min-width: 100px; }"
        "QFrame#routesSectionHeader { padding: 6px; }"
        "QLineEdit { padding: 5px 8px; border-radius: 8px; }"
        "QComboBox { padding: 4px 28px 4px 8px; border-radius: 8px; }"
        "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 26px; border: none; background: transparent; }"
        "QComboBox::down-arrow { image: url(:/icon/chevron-down.svg); width: 15px; height: 15px; }"
        "QPushButton { padding: 6px 12px; border-radius: 8px; }"
        "QToolButton#routeDeleteButton { font-size: 20px; color: #87867F; border: none; background: transparent; }"
        "QToolButton#routeDeleteButton:hover { color: #D65347; background: rgba(214, 83, 71, 28); border-radius: 7px; }"
    );
    if (styleSheet() != qss) {
        setStyleSheet(qss);
    }
    applyingStyle = false;
}

QFrame *QuickRoutesWidget::wrapCellContent(QTableWidget * /*table*/, QWidget *inner, bool firstCol,
                                           int leftMargin, int rightMargin) {
    auto *cellFrame = new QFrame();
    cellFrame->setObjectName(QStringLiteral("routeRowCell"));
    cellFrame->setAttribute(Qt::WA_StyledBackground, true);
    cellFrame->setAttribute(Qt::WA_Hover, true);
    cellFrame->setCursor(Qt::PointingHandCursor);
    cellFrame->setProperty("firstCol", firstCol);
    cellFrame->setProperty("rowSelected", false);
    cellFrame->setProperty("rowHovered", false);
    registerRowSurface(cellFrame);

    auto *layout = new QHBoxLayout(cellFrame);
    layout->setContentsMargins(leftMargin, 6, rightMargin, 6);
    layout->setSpacing(0);
    layout->addWidget(inner, 1);
    return cellFrame;
}

void QuickRoutesWidget::registerRowSurface(QWidget *widget) {
    if (widget == nullptr) {
        return;
    }
    widget->setProperty("rowSelectSurface", true);
    widget->installEventFilter(this);
}

void QuickRoutesWidget::registerRowEditor(QWidget *widget) {
    if (widget == nullptr) {
        return;
    }
    widget->setProperty("rowEditorControl", true);
    widget->setCursor(Qt::ArrowCursor);
    widget->installEventFilter(this);
}

int QuickRoutesWidget::rowForWidget(QTableWidget *table, QWidget *widget) {
    if (table == nullptr || widget == nullptr) {
        return -1;
    }
    for (int row = 0; row < table->rowCount(); ++row) {
        for (int col = 0; col < table->columnCount(); ++col) {
            QWidget *cell = table->cellWidget(row, col);
            if (cell != nullptr && (cell == widget || cell->isAncestorOf(widget))) {
                return row;
            }
        }
    }
    return -1;
}

QTableWidget *QuickRoutesWidget::ownerTableFor(QWidget *widget) const {
    if (widget == nullptr) {
        return nullptr;
    }
    if (processTable != nullptr && processTable->isAncestorOf(widget)) {
        return processTable;
    }
    if (domainTable != nullptr && domainTable->isAncestorOf(widget)) {
        return domainTable;
    }
    return nullptr;
}

void QuickRoutesWidget::selectRowFromClick(QTableWidget *table, int row,
                                           Qt::KeyboardModifiers modifiers, bool focusTable) {
    if (table == nullptr || table->selectionModel() == nullptr || row < 0 ||
        row >= table->rowCount()) {
        return;
    }
    auto *selModel = table->selectionModel();
    const QModelIndex target = table->model()->index(row, 0);

    if (modifiers.testFlag(Qt::ControlModifier)) {
        selModel->select(target, QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
        selModel->setCurrentIndex(target, QItemSelectionModel::NoUpdate);
    } else if (modifiers.testFlag(Qt::ShiftModifier)) {
        int anchor = table->currentRow();
        if (anchor < 0 || anchor >= table->rowCount()) {
            anchor = row;
        }
        const int top = std::min(anchor, row);
        const int bottom = std::max(anchor, row);
        const QItemSelection range(table->model()->index(top, 0),
                                   table->model()->index(bottom, table->columnCount() - 1));
        selModel->select(range, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    } else {
        selModel->select(target, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        selModel->setCurrentIndex(target, QItemSelectionModel::NoUpdate);
    }

    if (focusTable) {
        table->setFocus(Qt::MouseFocusReason);
    }
    syncSectionState(table);
}

void QuickRoutesWidget::syncSectionState(QTableWidget *table) {
    if (table == nullptr) {
        return;
    }

    const int hoveredRow = (table == processTable) ? hoveredProcessRow : hoveredDomainRow;
    auto *selModel = table->selectionModel();
    int selectedCount = 0;

    for (int row = 0; row < table->rowCount(); ++row) {
        const bool selected = selModel != nullptr && selModel->isRowSelected(row, QModelIndex());
        const bool hovered = (row == hoveredRow);
        if (selected) {
            ++selectedCount;
        }
        for (int col = 0; col < table->columnCount(); ++col) {
            QWidget *cell = table->cellWidget(row, col);
            if (cell == nullptr) {
                continue;
            }
            setStyledFlag(cell, "rowSelected", selected);
            setStyledFlag(cell, "rowHovered", hovered);
            if (col == 0) {
                if (auto *chip = cell->findChild<QWidget *>(QStringLiteral("processSelectChip"))) {
                    setStyledFlag(chip, "rowSelected", selected);
                }
                if (auto *dot = cell->findChild<QLabel *>(QStringLiteral("processSelectDot"))) {
                    dot->setText(selected ? QStringLiteral("●") : QStringLiteral("○"));
                    setStyledFlag(dot, "rowSelected", selected);
                }
                if (auto *grip = cell->findChild<QLabel *>(QStringLiteral("domainRowGrip"))) {
                    grip->setText(selected ? QStringLiteral("●") : QStringLiteral("○"));
                    setStyledFlag(grip, "rowSelected", selected);
                }
            }
        }
    }

    QLabel *badge = (table == processTable) ? processCountBadge : domainCountBadge;
    if (badge != nullptr) {
        badge->setText(QString::number(table->rowCount()));
    }

    QPushButton *removeBtn = (table == processTable) ? processRemoveButton : domainRemoveButton;
    if (removeBtn != nullptr) {
        removeBtn->setEnabled(selectedCount > 0);
        removeBtn->setText(selectedCount > 1 ? tr("Remove (%1)").arg(selectedCount) : tr("Remove"));
    }
}

void QuickRoutesWidget::syncAllSections() {
    syncSectionState(processTable);
    syncSectionState(domainTable);
}

QList<QuickRoutesWidget::OutboundChoice> QuickRoutesWidget::collectOutbounds() {
    QList<OutboundChoice> list;
    list.append({Configs::proxyID, tr("proxy")});
    list.append({Configs::directID, tr("direct")});
    list.append({Configs::blockID, tr("block")});

    return list;
}

void QuickRoutesWidget::fillOutboundCombo(QComboBox *combo, const QList<OutboundChoice> &choices,
                                          int selectedId) const {
    combo->clear();
    combo->setProperty("legacyOutbound", QVariant());
    int selectedIndex = 0;
    bool found = false;
    for (const auto &choice : choices) {
        combo->addItem(choice.name, choice.id);
        if (choice.id == selectedId) {
            selectedIndex = combo->count() - 1;
            found = true;
        }
    }
    if (!found) {
        // Preserve an existing profile route until the user explicitly changes it.
        combo->setProperty("legacyOutbound", selectedId);
        combo->setPlaceholderText(tr("Existing profile (%1)").arg(selectedId));
        selectedIndex = -1;
    }
    combo->setCurrentIndex(selectedIndex);
}

QComboBox *QuickRoutesWidget::makeOutboundCombo(int selectedId) {
    auto *combo = new QComboBox();
    combo->setObjectName(QStringLiteral("outbound_combo"));
    combo->setFocusPolicy(Qt::StrongFocus);
    combo->setMinimumHeight(30);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    fillOutboundCombo(combo, collectOutbounds(), selectedId);
    connect(combo, &QComboBox::activated, combo, [combo] {
        combo->setProperty("legacyOutbound", QVariant());
    });
    registerRowEditor(combo);
    combo->setCursor(Qt::PointingHandCursor);
    return combo;
}

QString QuickRoutesWidget::browseForExecutable() {
    QString startPath;
    if (Configs::resourceManager != nullptr) {
        startPath = Configs::resourceManager->getLatestPath();
    }
    const auto selected = QFileDialog::getOpenFileName(this, tr("Select an application"), startPath,
#ifdef Q_OS_WIN
                                                       tr("Applications (*.exe);;All files (*.*)")
#else
                                                       QString()
#endif
    );
    if (selected.isEmpty()) {
        return {};
    }
    if (Configs::resourceManager != nullptr) {
        Configs::resourceManager->latest_path = QFileInfo(selected).absolutePath();
    }
    return QDir::toNativeSeparators(selected);
}

QWidget *QuickRoutesWidget::makeProcessEditor(const QString &path) {
    auto *holder = new QWidget();
    registerRowSurface(holder);
    auto *layout = new QHBoxLayout(holder);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *chip = new QFrame(holder);
    chip->setObjectName(QStringLiteral("processSelectChip"));
    chip->setAttribute(Qt::WA_StyledBackground, true);
    chip->setCursor(Qt::PointingHandCursor);
    chip->setMinimumWidth(120);
    chip->setMaximumWidth(180);
    chip->setMinimumHeight(30);
    chip->setProperty("rowSelected", false);
    registerRowSurface(chip);

    auto *chipLayout = new QHBoxLayout(chip);
    chipLayout->setContentsMargins(8, 2, 10, 2);
    chipLayout->setSpacing(6);

    auto *dotLabel = new QLabel(QStringLiteral("○"), chip);
    dotLabel->setObjectName(QStringLiteral("processSelectDot"));
    dotLabel->setAlignment(Qt::AlignCenter);
    dotLabel->setFixedWidth(14);
    dotLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    auto *iconLabel = new QLabel(chip);
    iconLabel->setObjectName(QStringLiteral("processSelectIcon"));
    iconLabel->setFixedSize(18, 18);
    iconLabel->setScaledContents(true);
    iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    auto *nameLabel = new QLabel(chip);
    nameLabel->setObjectName(QStringLiteral("processSelectName"));
    nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    chipLayout->addWidget(dotLabel);
    chipLayout->addWidget(iconLabel);
    chipLayout->addWidget(nameLabel, 1);

    auto *edit = new QLineEdit(path, holder);
    edit->setObjectName(QStringLiteral("process_path"));
    edit->setMinimumHeight(30);
    edit->setPlaceholderText(tr("Full path to an .exe, or just its file name"));
    registerRowEditor(edit);
    edit->setCursor(Qt::IBeamCursor);

    auto *browse = new QToolButton(holder);
    browse->setObjectName(QStringLiteral("processBrowseButton"));
    browse->setText(QStringLiteral("…"));
    browse->setMinimumSize(32, 30);
    browse->setToolTip(tr("Select an application"));
    registerRowEditor(browse);
    browse->setCursor(Qt::PointingHandCursor);

    connect(edit, &QLineEdit::textChanged, chip,
            [this, chip](const QString &value) { updateProcessChip(chip, value); });

    connect(browse, &QToolButton::clicked, holder, [this, edit] {
        const auto selected = browseForExecutable();
        if (!selected.isEmpty()) {
            edit->setText(selected);
        }
    });

    updateProcessChip(chip, path);

    layout->addWidget(chip);
    layout->addWidget(edit, 1);
    layout->addWidget(browse);
    layout->addWidget(makeRemoveButton(holder));
    return holder;
}

void QuickRoutesWidget::updateProcessChip(QWidget *chip, const QString &rawPath) {
    if (chip == nullptr) {
        return;
    }
    const QString trimmed = rawPath.trimmed();
    const QString normalized = QDir::fromNativeSeparators(trimmed);
    QString shortName = QFileInfo(normalized).fileName();
    if (shortName.isEmpty()) {
        shortName = trimmed.isEmpty() ? tr("No path specified") : trimmed;
    }

    if (auto *nameLabel = chip->findChild<QLabel *>(QStringLiteral("processSelectName"))) {
        QFont f = nameLabel->font();
        f.setBold(!trimmed.isEmpty());
        nameLabel->setFont(f);
        const QFontMetrics fm(f);
        nameLabel->setText(fm.elidedText(shortName, Qt::ElideMiddle, 145));
    }

    if (auto *iconLabel = chip->findChild<QLabel *>(QStringLiteral("processSelectIcon"))) {
        QIcon icon;
        if (!trimmed.isEmpty() && !trimmed.startsWith(QLatin1String("\\\\")) &&
            QFileInfo::exists(trimmed)) {
            icon = iconProvider.icon(QFileInfo(trimmed));
        }
        if (icon.isNull()) {
            icon = style()->standardIcon(QStyle::SP_FileIcon);
        }
        iconLabel->setPixmap(icon.pixmap(18, 18));
    }

    const QString selectHint =
        tr("Click to select this row (Ctrl or Shift for multiple, Delete to remove)");
    chip->setToolTip(trimmed.isEmpty() ? selectHint : (trimmed + QLatin1Char('\n') + selectHint));
}

QWidget *QuickRoutesWidget::makeDomainEditor(const QString &domain, QComboBox *methodCombo) {
    auto *holder = new QWidget();
    registerRowSurface(holder);
    auto *layout = new QHBoxLayout(holder);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *grip = new QLabel(QStringLiteral("○"), holder);
    grip->setObjectName(QStringLiteral("domainRowGrip"));
    grip->setAlignment(Qt::AlignCenter);
    grip->setFixedSize(30, 30);
    grip->setCursor(Qt::PointingHandCursor);
    grip->setProperty("rowSelected", false);
    grip->setToolTip(tr("Click to select this row (Ctrl or Shift for multiple, Delete to remove)"));
    registerRowSurface(grip);

    auto *edit = new QLineEdit(domain, holder);
    edit->setObjectName(QStringLiteral("domain_edit"));
    edit->setMinimumHeight(30);
    registerRowEditor(edit);
    edit->setCursor(Qt::IBeamCursor);

    auto updatePlaceholder = [edit](QComboBox *combo) {
        if (combo != nullptr && combo->currentData().toString() == QLatin1String("suffix")) {
            edit->setPlaceholderText(tr("Domain suffix, for example: google.com"));
        } else {
            edit->setPlaceholderText(tr("Domain, for example: example.com"));
        }
    };
    if (methodCombo != nullptr) {
        connect(methodCombo, &QComboBox::currentIndexChanged, edit,
                [updatePlaceholder, methodCombo] { updatePlaceholder(methodCombo); });
    }
    updatePlaceholder(methodCombo);

    layout->addWidget(grip);
    layout->addWidget(edit, 1);
    layout->addWidget(makeRemoveButton(holder));
    return holder;
}

QToolButton *QuickRoutesWidget::makeRemoveButton(QWidget *parent) {
    auto *button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("routeDeleteButton"));
    button->setText(QStringLiteral("×"));
    button->setToolTip(tr("Remove rule"));
    button->setAccessibleName(tr("Remove rule"));
    button->setFixedSize(30, 30);
    button->setCursor(Qt::PointingHandCursor);
    connect(button, &QToolButton::clicked, this, [this, button] {
        if (auto *table = ownerTableFor(button)) {
            const int row = rowForWidget(table, button);
            if (row >= 0) {
                table->removeRow(row);
                hoveredProcessRow = hoveredDomainRow = -1;
                syncSectionState(table);
            }
        }
    });
    return button;
}

QString QuickRoutesWidget::processPathAt(int row) const {
    auto *edit = findCellLineEdit(processTable, row, 0, QStringLiteral("process_path"));
    return edit != nullptr ? edit->text().trimmed() : QString();
}

QComboBox *QuickRoutesWidget::makeMethodCombo(const QString &method) {
    auto *combo = new QComboBox();
    combo->setObjectName(QStringLiteral("method_combo"));
    combo->setFocusPolicy(Qt::StrongFocus);
    combo->setMinimumHeight(30);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->addItem(tr("Exact domain"), QStringLiteral("domain"));
    combo->addItem(tr("Domain and subdomains"), QStringLiteral("suffix"));
    if (method == QLatin1String("keyword")) {
        combo->addItem(tr("Substring (legacy)"), QStringLiteral("keyword"));
    }
    combo->setItemData(0, tr("Matches only this domain"), Qt::ToolTipRole);
    combo->setItemData(1, tr("Matches this domain and all of its subdomains"), Qt::ToolTipRole);
    const int index = combo->findData(method);
    combo->setCurrentIndex(index >= 0 ? index : 0);
    registerRowEditor(combo);
    combo->setCursor(Qt::PointingHandCursor);
    return combo;
}

int QuickRoutesWidget::outboundAt(QTableWidget *table, int row, int outboundColumn) {
    auto *combo = findCellCombo(table, row, outboundColumn, QStringLiteral("outbound_combo"));
    if (combo == nullptr) {
        return Configs::proxyID;
    }
    if (combo->currentIndex() < 0 && combo->property("legacyOutbound").isValid())
        return combo->property("legacyOutbound").toInt();
    return combo->currentData().toInt();
}

void QuickRoutesWidget::addProcessRow(const QString &path, int outboundId) {
    const int row = processTable->rowCount();
    processTable->insertRow(row);
    for (int col = 0; col < processTable->columnCount(); ++col) {
        auto *item = new QTableWidgetItem();
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        processTable->setItem(row, col, item);
    }
    processTable->setCellWidget(
        row, 0, wrapCellContent(processTable, makeProcessEditor(path), true, 10, 8));
    processTable->setCellWidget(
        row, 1, wrapCellContent(processTable, makeOutboundCombo(outboundId), false, 6, 10));
    syncSectionState(processTable);
}

void QuickRoutesWidget::addDomainRow(const QString &domain, int outboundId,
                                     const QString &method) {
    const int row = domainTable->rowCount();
    domainTable->insertRow(row);
    for (int col = 0; col < domainTable->columnCount(); ++col) {
        auto *item = new QTableWidgetItem();
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        domainTable->setItem(row, col, item);
    }

    auto *methodCombo = makeMethodCombo(method.isEmpty() ? QStringLiteral("keyword") : method);
    domainTable->setCellWidget(
        row, 0, wrapCellContent(domainTable, makeDomainEditor(domain, methodCombo), true, 10, 8));
    domainTable->setCellWidget(row, 1, wrapCellContent(domainTable, methodCombo, false, 6, 6));
    domainTable->setCellWidget(
        row, 2, wrapCellContent(domainTable, makeOutboundCombo(outboundId), false, 6, 10));
    syncSectionState(domainTable);
}

void QuickRoutesWidget::removeSelectedRows(QTableWidget *table) {
    if (table == nullptr || table->selectionModel() == nullptr) {
        return;
    }
    auto rows = table->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        return;
    }
    std::sort(rows.begin(), rows.end(),
              [](const QModelIndex &a, const QModelIndex &b) { return a.row() > b.row(); });
    for (const auto &index : rows) {
        table->removeRow(index.row());
    }
    if (table == processTable) {
        hoveredProcessRow = -1;
    } else if (table == domainTable) {
        hoveredDomainRow = -1;
    }
    table->clearSelection();
    syncSectionState(table);
}

void QuickRoutesWidget::refreshOutboundCombos() {
    const auto choices = collectOutbounds();
    auto updateTable = [&](QTableWidget *table, int outboundColumn) {
        if (table == nullptr) {
            return;
        }
        for (int row = 0; row < table->rowCount(); row++) {
            auto *combo =
                findCellCombo(table, row, outboundColumn, QStringLiteral("outbound_combo"));
            if (combo == nullptr) {
                continue;
            }
            const auto selected = outboundAt(table, row, outboundColumn);
            QSignalBlocker blocker(combo);
            fillOutboundCombo(combo, choices, selected);
        }
    };
    updateTable(processTable, 1);
    updateTable(domainTable, 2);
}

void QuickRoutesWidget::loadFromStore() {
    hoveredProcessRow = -1;
    hoveredDomainRow = -1;
    processTable->setRowCount(0);
    domainTable->setRowCount(0);
    if (Configs::dataStore == nullptr || Configs::dataStore->routing == nullptr ||
        Configs::dataStore->routing->quick_routes == nullptr) {
        syncAllSections();
        return;
    }
    auto routes = Configs::dataStore->routing->quick_routes;
    routes->normalize();
    for (int i = 0; i < routes->process_match.size(); i++) {
        addProcessRow(routes->process_match[i], routes->process_outbound[i]);
    }
    for (int i = 0; i < routes->domain_match.size(); i++) {
        addDomainRow(routes->domain_match[i], routes->domain_outbound[i],
                     i < routes->domain_method.size() ? routes->domain_method[i] : QString());
    }
    syncAllSections();
}

void QuickRoutesWidget::saveToStore() {
    if (Configs::dataStore == nullptr || Configs::dataStore->routing == nullptr ||
        Configs::dataStore->routing->quick_routes == nullptr) {
        return;
    }
    auto routes = Configs::dataStore->routing->quick_routes;
    routes->process_match.clear();
    routes->process_outbound.clear();
    routes->domain_match.clear();
    routes->domain_outbound.clear();
    routes->domain_method.clear();

    for (int row = 0; row < processTable->rowCount(); row++) {
        const auto path = processPathAt(row);
        if (path.isEmpty()) {
            continue;
        }
        routes->process_match.append(path);
        routes->process_outbound.append(outboundAt(processTable, row, 1));
    }
    for (int row = 0; row < domainTable->rowCount(); row++) {
        auto *edit = findCellLineEdit(domainTable, row, 0, QStringLiteral("domain_edit"));
        const auto domain = edit != nullptr ? edit->text().trimmed() : QString();
        if (domain.isEmpty()) {
            continue;
        }
        auto *methodCombo = findCellCombo(domainTable, row, 1, QStringLiteral("method_combo"));
        const auto method = methodCombo != nullptr ? methodCombo->currentData().toString()
                                                   : QStringLiteral("keyword");
        routes->domain_match.append(domain);
        routes->domain_method.append(method);
        routes->domain_outbound.append(outboundAt(domainTable, row, 2));
    }
    routes->normalize();

    Configs::dataStore->routing->Save();

    if (MW_show_log) {
        MW_show_log(tr("[Routes] Saved %1 application and %2 domain routes")
                        .arg(routes->process_match.size())
                        .arg(routes->domain_match.size()));
    }
    if (MW_dialog_message) {
        MW_dialog_message("", "UpdateDataStore,RouteChanged");
    }
}
