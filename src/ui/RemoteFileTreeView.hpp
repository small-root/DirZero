#pragma once

#include <QTreeView>

class MachinePanelWidget;

class RemoteFileTreeView : public QTreeView {
    Q_OBJECT

public:
    explicit RemoteFileTreeView(MachinePanelWidget* panel, QWidget* parent = nullptr);
    ~RemoteFileTreeView() override = default;

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    MachinePanelWidget* m_panel;
};
