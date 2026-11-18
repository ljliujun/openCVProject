#include "ljresizableRectItem.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QStyleOptionGraphicsItem>
#include <QPen>
#include <QColor>

namespace 
{
    constexpr qreal kHandlePx = 8.0;   // 手柄在屏幕上的视觉大小（像素）
    constexpr qreal kMinSize = 10.0;  // 矩形最小宽高（item 坐标）
}

LJResizableRectItem::LJResizableRectItem(const QRectF& rect, QGraphicsItem* parent)
    : QGraphicsItem(parent), m_rect(rect.normalized())
{
    // 关键 flags
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);     // 拖动整体 = 默认行为
    setFlag(ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);         // 需要 hover 事件改变光标
    setZValue(10);                      // 画在图片之上
}

void LJResizableRectItem::setRect(const QRectF& rect)
{
    prepareGeometryChange();
    m_rect = rect.normalized();
    update();
}

qreal LJResizableRectItem::handleSize() const
{
    // 视图放大时 lod 变大 → 手柄的 item 坐标尺寸变小 → 屏幕上看起来大小不变
    return qMin(kHandlePx / qMax(m_lastLod, 1e-4), 50.0);
}

QRectF LJResizableRectItem::boundingRect() const
{
    // 留足 padding 覆盖手柄区域，避免手柄超出被裁掉
    const qreal pad = 60.0;
    return m_rect.adjusted(-pad, -pad, pad, pad);
}

LJResizableRectItem::Handle LJResizableRectItem::handleAt(const QPointF& pos) const
{
    if (!isSelected()) return None;                 // 只有选中后才响应手柄
    const qreal hs = handleSize();
    const qreal r = hs / 2.0;
    auto hit = [&](const QPointF& c)
    {
        return QRectF(c.x() - r, c.y() - r, hs, hs).contains(pos);
    };
    if (hit(m_rect.topLeft()))     return TopLeft;
    if (hit(m_rect.topRight()))    return TopRight;
    if (hit(m_rect.bottomLeft()))  return BottomLeft;
    if (hit(m_rect.bottomRight())) return BottomRight;
    return None;
}

void LJResizableRectItem::paint(QPainter* painter,const QStyleOptionGraphicsItem* option,QWidget* /*widget*/)
{
    // 记录当前视图缩放，供手柄绘制与命中检测使用
    m_lastLod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(painter->worldTransform());

    painter->setRenderHint(QPainter::Antialiasing, true);

    // ---- 主体：绿色矩形（cosmetic pen：线宽不随缩放变化）----
    QPen pen(QColor(0, 200, 0));
    pen.setWidthF(2.0);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(m_rect);

    // ---- 选中状态：虚线外框 + 四个角手柄 ----
    if (option->state & QStyle::State_Selected)
    {
        const qreal hs = handleSize();
        const qreal hr = hs / 2.0;
        const qreal d = hr;

        QPen selPen(QColor(0, 255, 0));
        selPen.setStyle(Qt::DashLine);
        selPen.setCosmetic(true);
        selPen.setWidthF(1.0);
        painter->setPen(selPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(m_rect.adjusted(-d, -d, d, d));

        painter->setPen(QPen(Qt::white, 1.0));
        painter->setBrush(QColor(0, 200, 0));
        const QPointF corners[4] = 
        {
            m_rect.topLeft(), m_rect.topRight(),
            m_rect.bottomLeft(), m_rect.bottomRight()
        };
        for (const QPointF& c : corners)
            painter->drawRect(QRectF(c.x() - hr, c.y() - hr, hs, hs));
    }
}

// ------------------------------------------------------------------
// 鼠标事件
// ------------------------------------------------------------------
void LJResizableRectItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        Handle h = handleAt(event->pos());
        if (h != None)
        {                 // 命中手柄 → 进入缩放模式
            setSelected(true);
            m_activeHandle = h;
            event->accept();
            return;                      // 不调用基类，阻止 ItemIsMovable 触发整体拖动
        }
    }
    QGraphicsItem::mousePressEvent(event);   // 否则交给默认逻辑：选中 / 拖动
}

void LJResizableRectItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_activeHandle != None) 
    {                // 缩放中
        QRectF r = m_rect;
        const QPointF p = event->pos();          // item 本地坐标
        switch (m_activeHandle) 
        {
            case TopLeft:     r.setTopLeft(p);     break;
            case TopRight:    r.setTopRight(p);    break;
            case BottomLeft:  r.setBottomLeft(p);  break;
            case BottomRight: r.setBottomRight(p); break;
            default: break;
        }
        // 限制最小尺寸，避免矩形翻转
        if (r.width() >= kMinSize && r.height() >= kMinSize) 
        {
            prepareGeometryChange();
            m_rect = r;
            update();
        }
        event->accept();
        return;
    }
    QGraphicsItem::mouseMoveEvent(event);        // 整体拖动由 ItemIsMovable 处理
}

void LJResizableRectItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    m_activeHandle = None;
    QGraphicsItem::mouseReleaseEvent(event);
}

void LJResizableRectItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    switch (handleAt(event->pos()))
    {
        case TopLeft:
        case BottomRight: setCursor(Qt::SizeFDiagCursor);
            break;
        case TopRight:
        case BottomLeft:  setCursor(Qt::SizeBDiagCursor);
            break;
        case None:
            setCursor(m_rect.contains(event->pos()) ? Qt::SizeAllCursor : Qt::ArrowCursor);
            break;
    }
    QGraphicsItem::hoverMoveEvent(event);
}

void LJResizableRectItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    unsetCursor();
    QGraphicsItem::hoverLeaveEvent(event);
}