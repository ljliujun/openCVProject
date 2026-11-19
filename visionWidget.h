#pragma once

#include <QWidget>
#include <QImage>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "ui_visionWidget.h"

//class LJMatchBase;

class VisionWidget : public QWidget
{
	Q_OBJECT

public:
	VisionWidget(QWidget *parent = nullptr);
	~VisionWidget() override;
	static QImage matToQImage(const cv::Mat& mat);
private:
	void initUI();
	void initConnect();
private slots:
	// ===== UI 槽函数 =====
	void onBtnLearnClicked();            // "学习" —— 框选模板
	void onBtnTestClicked();             // "测试" —— 执行匹配
	void onAlgorithmChanged(int index) const;   // 算法下拉切换
private:
	Ui::VisionWidgetClass ui;
	// ===== 算法相关 =====
	//std::shared_ptr<LJMatchBase> m_matcher;          // 灰度匹配算法实例
	cv::Mat     m_sourceMat;        // 源图（学习和测试的输入）
	cv::Mat     m_templateMat;      // 学习得到的模板
};

