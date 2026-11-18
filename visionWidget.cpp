#include "visionWidget.h"
#include <QPixmap>
#include <QLabel>
#include <QHBoxLayout>
#include <cmath>
#include <QMessageBox>
#include <QDebug>
#include "algorithm/ljgraymatch.h"

#pragma execution_character_set("utf-8")

VisionWidget::VisionWidget(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	initUI();
	initConnect();
	
}

VisionWidget::~VisionWidget()
{}

void VisionWidget::initConnect()
{
    connect(ui.btnLearn, &QPushButton::clicked,
        this, &VisionWidget::onBtnLearnClicked);
    connect(ui.btnTest, &QPushButton::clicked,
        this, &VisionWidget::onBtnTestClicked);
    connect(ui.comboBoxAlgorithm, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &VisionWidget::onAlgorithmChanged);
}

void VisionWidget::initUI()
{
    cv::Mat imgMat = cv::imread("F:/openCVProject/image/testImage/3.bmp");
    if (!imgMat.empty())
    {
        // 关键：保存一份 Mat 缓存，后续学习/测试都用这个原图
        m_sourceMat = imgMat.clone();
        ui.graphicsWidget->setImage(matToQImage(imgMat));
    }
}

QImage VisionWidget::matToQImage(const cv::Mat& mat)
{
    if (mat.empty())
        return QImage();

    switch (mat.type())
    {
        case CV_8UC1:   // 灰度图
        {
            QImage img(mat.data, mat.cols, mat.rows,
                static_cast<int>(mat.step), QImage::Format_Grayscale8);
            return img.copy();
        }
        case CV_8UC3:   // BGR 三通道
        {
            // Qt 5.14+ 提供 Format_BGR888，可直接映射 BGR，省去 cvtColor
            QImage img(mat.data, mat.cols, mat.rows,
                static_cast<int>(mat.step), QImage::Format_BGR888);
            return img.copy();
        }
        case CV_8UC4:   // BGRA 四通道
        {
            // 小端机器上 ARGB32 的内存布局就是 B,G,R,A，与 OpenCV BGRA 一致
            QImage img(mat.data, mat.cols, mat.rows,
                static_cast<int>(mat.step), QImage::Format_ARGB32);
            return img.copy();
        }
        default:
        {
            // 其他类型（16 位、浮点、多通道等）先统一转成 8 位
            cv::Mat tmp;
            if (mat.depth() == CV_16U)
                mat.convertTo(tmp, CV_8U, 1.0 / 256.0);
            else if (mat.depth() == CV_32F || mat.depth() == CV_64F)
                mat.convertTo(tmp, CV_8U, 255.0);
            else
                mat.convertTo(tmp, CV_8U);

            return matToQImage(tmp);  // 递归处理
        }
    }
}

// ==============================================================
// 槽函数实现
// ==============================================================
void VisionWidget::onBtnLearnClicked()
{
	// 1) 拿矩形框选区域（场景坐标 = 原图像素坐标）
	QRectF rectF = ui.graphicsWidget->getRectSelection();
	if (rectF.isNull())
	{
		QMessageBox::warning(this, tr("提示"),
			tr("没有框选矩形，请在图上拖动绿色矩形后再点学习"));
		return;
	}

	if (m_sourceMat.empty())
	{
		QMessageBox::warning(this, tr("提示"),
			tr("源图为空，请先加载图片"));
		return;
	}

	// 2) 从源图中截出 ROI 作为模板
	QRect rect = rectF.toRect().intersected(
		QRect(0, 0, m_sourceMat.cols, m_sourceMat.rows)).normalized();
	if (rect.width() < 4 || rect.height() < 4)
	{
		QMessageBox::warning(this, tr("提示"),tr("框选区域太小"));
		return;
	}

	m_templateMat = m_sourceMat(cv::Rect(rect.x(), rect.y(), rect.width(), rect.height())).clone();

	QMessageBox::information(this,tr("提示"),tr("学习完成!"));

	qDebug() << "[Learn] template size:" << m_templateMat.cols << "x" << m_templateMat.rows;
	qDebug() << "[Learn] rect in source:" << rect;
}

void VisionWidget::onBtnTestClicked()
{
	if (m_sourceMat.empty() || m_templateMat.empty())
	{
		QMessageBox::warning(this, tr("提示"),tr("请先点击【学习】按钮框选模板"));
		return;
	}

	double threshold = ui.lineEdit_Score->text().toDouble() / 100.0;
	if (threshold < 0.01) threshold = 0.01;
	if (threshold > 1.0)   threshold = 1.0;

	m_matcher = std::make_shared<LJGrayMatch>();
	// 2) 配置 matcher 并执行
	m_matcher->setParams(threshold,20,0.5);
	m_matcher->prepare(m_sourceMat, m_templateMat);

	if (!m_matcher->match())
	{
		QMessageBox::warning(this, tr("匹配失败"),
			QString::fromStdString(m_matcher->lastError()));
		return;
	}

	// 3) 把结果交给 View 画红框
	const auto& results = m_matcher->results();

	ui.graphicsWidget->drawMatchResults(
		QSize(m_templateMat.cols, m_templateMat.rows),
		results);

	if (results.empty())
	{
		// 一个都没找到，提示用户考虑降低阈值
		QMessageBox::information(this, tr("匹配完成"),tr("未找到匹配项，尝试降低【适配分数】阈值（当前 %1）")
			.arg(ui.lineEdit_Score->text()));
	}
}

void VisionWidget::onAlgorithmChanged(int index)
{
	// 目前只有灰度匹配，预留扩展位：
	// index == 0 → LJGrayMatch  (当前)
	// index == 1 → LJCircleMatch (未来)
	// ...
	qDebug() << "[Algorithm] switched to index:" << index;
}