#include "ljgraphicsView.h"
#include "ljresizableRectItem.h"
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QWheelEvent>
#include <QPixmap>
#include "algorithm/ljmatchbase.h"
#include "algorithm/ljcirclematch.h"

LJGraphicsView::LJGraphicsView(QWidget* parent)
	: QGraphicsView(parent)
	, ui(new Ui::LJGraphicsViewClass())
	, m_scene(nullptr)
	, m_pixItem(nullptr)
	, m_rectItem(nullptr)
{
	ui->setupUi(this);

	// ====== 隐藏水平和垂直滚动条 ======
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	// ====== 启用内置的鼠标拖动手势 ======
	// ScrollHandDrag 会利用滚动条来平移视口，即使滚动条被隐藏也能正常工作
	setDragMode(QGraphicsView::ScrollHandDrag);
	//// 默认光标设为张开的手型，提示用户可以拖动
	//setCursor(Qt::OpenHandCursor);

	// 关键设置：缩放锚点固定为鼠标下方
	setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	// 缩放时不限制在 sceneRect 内，允许无限漫游
	setSceneRect(QRectF());
	// 推荐：开启平滑渲染与抗锯齿，提升图片缩放后的视觉质量
	setRenderHint(QPainter::Antialiasing);
	setRenderHint(QPainter::SmoothPixmapTransform);

	m_scene = new QGraphicsScene(this);
	this->setScene(m_scene);
}

LJGraphicsView::~LJGraphicsView()
{
	delete ui;
}

void LJGraphicsView::setImage(const QImage& img)
{
	if (img.isNull())
		return;
	if (!m_scene)
		return;
	QPixmap pixmap = QPixmap::fromImage(img);
	if (!m_pixItem)
	{
		m_pixItem = new QGraphicsPixmapItem(pixmap);
		m_scene->addItem(m_pixItem);
		m_scene->setSceneRect(m_pixItem->boundingRect());  // 首次设置
	}
	else
	{
		// 已存在 → 直接更新像素内容
		m_pixItem->setPixmap(pixmap);
		// 图片尺寸变化时需要更新 sceneRect，否则可能出现滚动条异常
		m_scene->setSceneRect(m_pixItem->boundingRect());
	}

	//叠加一个绿色的可交互的矩形
	if (!m_rectItem)
	{
		m_rectItem = new LJResizableRectItem(QRectF(50, 50, 200, 120));
		m_scene->addItem(m_rectItem);
	}
}

void LJGraphicsView::wheelEvent(QWheelEvent* event)
{
	// 缩放因子，1.15 意味着每格滚轮放大 15%
	const double zoomFactor = 1.15;
	// 获取滚轮的滚动量（通常上滚 >0，下滚 <0）
	const int delta = event->angleDelta().y();

	if (delta > 0) 
	{
		scale(zoomFactor, zoomFactor);
	}
	else if (delta < 0)
	{
		scale(1.0 / zoomFactor, 1.0 / zoomFactor);
	}
}

void LJGraphicsView::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
		setCursor(Qt::ClosedHandCursor);
	QGraphicsView::mousePressEvent(event);
}

void LJGraphicsView::mouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
		setCursor(Qt::OpenHandCursor);
	QGraphicsView::mouseReleaseEvent(event);
}

QRectF LJGraphicsView::getRectSelection() const
{
	if (!m_rectItem) return QRectF();
	return m_rectItem->sceneRectF();  // 场景坐标 = 原图像素坐标
}

void LJGraphicsView::clearMatchResults()
{
	for (QGraphicsItem* item : m_matchMarkers)
	{
		if (item && item->scene())
			item->scene()->removeItem(item);
		delete item;
	}
	m_matchMarkers.clear();
}

void LJGraphicsView::drawMatchResults(const QSize& templateSize,
	const std::vector<MatchResult*>& results)
{
	clearMatchResults();   // 先清上次的标记

	if (results.empty() || templateSize.isEmpty())
		return;

	const int tw = templateSize.width();
	const int th = templateSize.height();

	QPen redPen(QColor(255, 0, 0));
	redPen.setStyle(Qt::DashLine);
	redPen.setWidthF(2.0);
	redPen.setCosmetic(true);  // 线宽不随缩放改变

	for (const auto r : results)
	{
		const float scale = (r->scale > 0.01) ? static_cast<float>(r->scale) : 1.0f;
		const int w = cvRound(tw * scale);
		const int h = cvRound(th * scale);
		QRectF matchRect(r->topLeftF.x, r->topLeftF.y, w, h);

		// 画红色虚线框
		QGraphicsRectItem* box = m_scene->addRect(
			matchRect.adjusted(-1, -1, 1, 1), redPen, Qt::NoBrush);
		box->setZValue(50);
		m_matchMarkers.append(box);

		// 画黄色分数文字
		QGraphicsSimpleTextItem* text = m_scene->addSimpleText(
			QStringLiteral("%1").arg(r->score, 0, 'f', 3));
		text->setBrush(QBrush(QColor(255, 255, 0)));
		text->setPos(matchRect.topLeft()
			+ QPointF(2, -text->boundingRect().height() - 2));
		text->setZValue(51);
		m_matchMarkers.append(text);
	}
}

void LJGraphicsView::drawCircleResults(const std::vector<MatchResult*>& results)
{
	clearMatchResults();   // 先清上次的标记

	if (results.empty())
		return;

	QPen redPen(QColor(255, 0, 0));
	redPen.setStyle(Qt::DashLine);
	redPen.setWidthF(2.0);
	redPen.setCosmetic(true);

	for (const auto* r : results)
	{
		const auto* cr = dynamic_cast<const CircleResult*>(r);
		if (!cr || cr->radius <= 0.0)
			continue;

		const QRectF circleRect(
			cr->center.x - cr->radius, cr->center.y - cr->radius,
			cr->radius * 2.0, cr->radius * 2.0);

		// 画红色虚线圆
		QGraphicsEllipseItem* circle = m_scene->addEllipse(circleRect, redPen, Qt::NoBrush);
		circle->setZValue(50);
		m_matchMarkers.append(circle);

		// 画黄色分数文字（放在圆上方）
		QGraphicsSimpleTextItem* text = m_scene->addSimpleText(
			QStringLiteral("%1").arg(cr->score, 0, 'f', 3));
		text->setBrush(QBrush(QColor(255, 255, 0)));
		text->setPos(QPointF(cr->center.x, cr->center.y - cr->radius)
			+ QPointF(2, -text->boundingRect().height() - 2));
		text->setZValue(51);
		m_matchMarkers.append(text);
	}
}
