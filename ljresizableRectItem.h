#pragma once
#include <QGraphicsItem>

class LJResizableRectItem : public QGraphicsItem
{
public:
    explicit LJResizableRectItem(const QRectF& rect, QGraphicsItem* parent = nullptr);

    QRectF  rect() const { return m_rect; }
    void    setRect(const QRectF& rect);
    QRectF  sceneRectF() const { return mapToScene(m_rect).boundingRect(); } // 场景坐标，便于读取最终结果

    QRectF  boundingRect() const override;
    void    paint(QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event)   override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event)    override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event)    override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event)   override;

private:
    enum Handle { None, TopLeft, TopRight, BottomLeft, BottomRight };

    Handle handleAt(const QPointF& pos) const;   // 命中检测，判断鼠标在哪一个角
    qreal  handleSize() const;                   // 手柄在 item 坐标下的尺寸（随视图缩放自适应）

    QRectF m_rect;
    Handle m_activeHandle = None;
    qreal  m_lastLod = 1.0;                 // 最近一次绘制时的视图缩放系数
};