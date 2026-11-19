#pragma once
#include <QGraphicsView>
#include <QList>
#include <opencv2/core.hpp>
#include <vector>

#include "ui_ljgraphicsView.h"

QT_BEGIN_NAMESPACE
namespace Ui { class LJGraphicsViewClass; };
QT_END_NAMESPACE

class QWheelEvent;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QGraphicsItem;
class LJResizableRectItem;
struct MatchResult;

struct DrawMatchResult
{
	cv::Point topLeft;       // 模板匹配到的左上角坐标（原图坐标系）
	double    score;         // 匹配分数，越大越好（不同算法值域不同，子类文档说明）

	DrawMatchResult() : topLeft(0, 0), score(0.0) {}
	DrawMatchResult(const cv::Point& p, double s) : topLeft(p), score(s) {}
};

class LJGraphicsView : public QGraphicsView
{
	Q_OBJECT
public:
	LJGraphicsView(QWidget *parent = nullptr);
	~LJGraphicsView();
	
	void setImage(const QImage& img);
	// ===== 对外接口（供 VisionWidget/算法调度层调用）=====
	// 1) 获取当前矩形框选区域的场景坐标
	QRectF getRectSelection() const;

	// 2) 在场景上画一组匹配结果（红框 + 分数）
	//    templateSize: 模板尺寸（用于画框宽高）
	//    results:      匹配结果列表（topLeft 是模板左上角原图坐标）
	void drawMatchResults(const QSize& templateSize,const std::vector<MatchResult*>& results);

	// 2.1) 画一组圆检测结果（红虚线圆 + 分数）
	void drawCircleResults(const std::vector<MatchResult*>& results);

	// 3) 清除所有红色匹配标记
	void clearMatchResults();
protected:
	void wheelEvent(QWheelEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event)override;
private:
	Ui::LJGraphicsViewClass *ui;

	QGraphicsScene* m_scene;
	QGraphicsPixmapItem* m_pixItem;
	LJResizableRectItem* m_rectItem;

	QList<QGraphicsItem*> m_matchMarkers;  // 红色匹配框，便于清除
};

