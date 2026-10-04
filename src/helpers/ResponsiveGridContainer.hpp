#pragma once

#include <QWidget>
#include <QGridLayout>
#include <QList>

class ResponsiveGridContainer : public QWidget {
    Q_OBJECT

public:
    explicit ResponsiveGridContainer(QWidget* parent = nullptr, int minColWidth = 380);
    ~ResponsiveGridContainer() override = default;

    void addPanel(QWidget* panel);
    void removePanel(QWidget* panel);
    void clearPanels();

    const QList<QWidget*>& panels() const { return m_panels; }

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    int m_minColWidth;
    int m_currentCols;
    QList<QWidget*> m_panels;
    QGridLayout* m_gridLayout;

    int calculateColumns() const;
    void rearrangeLayout(bool force = false);
};
