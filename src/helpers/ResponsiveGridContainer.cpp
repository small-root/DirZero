#include "ResponsiveGridContainer.hpp"
#include <QResizeEvent>
#include <algorithm>

ResponsiveGridContainer::ResponsiveGridContainer(QWidget* parent, int minColWidth)
    : QWidget(parent)
    , m_minColWidth(minColWidth)
    , m_currentCols(-1)
{
    m_gridLayout = new QGridLayout(this);
    m_gridLayout->setContentsMargins(12, 12, 12, 12);
    m_gridLayout->setSpacing(16);
    setLayout(m_gridLayout);
}

void ResponsiveGridContainer::addPanel(QWidget* panel)
{
    if (panel && !m_panels.contains(panel)) {
        m_panels.append(panel);
        rearrangeLayout(true);
    }
}

void ResponsiveGridContainer::removePanel(QWidget* panel)
{
    if (panel && m_panels.contains(panel)) {
        m_panels.removeOne(panel);
        m_gridLayout->removeWidget(panel);
        panel->setParent(nullptr);
        rearrangeLayout(true);
    }
}

void ResponsiveGridContainer::clearPanels()
{
    while (m_gridLayout->count() > 0) {
        QLayoutItem* item = m_gridLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->setParent(nullptr);
        }
        delete item;
    }
    m_panels.clear();
    m_currentCols = -1;
}

void ResponsiveGridContainer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    rearrangeLayout(false);
}

int ResponsiveGridContainer::calculateColumns() const
{
    int w = width();
    if (w <= 0) {
        return 1;
    }
    int cols = std::max(1, w / m_minColWidth);
    return std::min(cols, 4); // Cap at 4 columns for clean presentation
}

void ResponsiveGridContainer::rearrangeLayout(bool force)
{
    if (m_panels.isEmpty()) {
        return;
    }

    int cols = calculateColumns();
    if (!force && cols == m_currentCols && m_gridLayout->count() == m_panels.size()) {
        return;
    }

    m_currentCols = cols;

    // Detach all widgets temporarily
    while (m_gridLayout->count() > 0) {
        m_gridLayout->takeAt(0);
    }

    // Re-place into grid rows & columns
    for (int i = 0; i < m_panels.size(); ++i) {
        int row = i / cols;
        int col = i % cols;
        m_gridLayout->addWidget(m_panels.at(i), row, col);
    }

    for (int c = 0; c < cols; ++c) {
        m_gridLayout->setColumnStretch(c, 1);
    }
}
