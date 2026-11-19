#pragma once
#include "ljmatchbase.h"

// ==============================================================
// 圆检测结果：在 MatchResult 基础上增加圆特有的几何信息。
// 兼容字段：topLeft / topLeftF 存外接矩形左上角（圆心 - 半径）。
// ==============================================================
struct CircleResult : public MatchResult
{
	cv::Point2f center;    // 圆心（亚像素，原图坐标）
	double      radius;    // 半径（亚像素，像素）
	double      roundness; // 圆度：拟合归一化残差，越小越圆

	CircleResult() : MatchResult(), center(0.f, 0.f), radius(0.0), roundness(0.0) {}
};

// ==============================================================
// 圆检测 —— 面向工业视觉的鲁棒圆检测
// 流程：灰度化 → 高斯去噪 → Sobel 梯度 → Hough 粗定位
//      → 径向亚像素采样 → 鲁棒圆拟合(IRLS) → 打分 → NMS
// 分数 score ∈ [0,1]：边缘支持率 × 圆度质量，越大越好。
// ==============================================================
class LJCircleMatch : public LJMatchBase
{
public:
	LJCircleMatch();
	~LJCircleMatch() override = default;

	// threshold=最小分数、maxResults=最多返回数、nmsRadiusFactor=抑制半径系数
	void setParams(double threshold = 0.50,
	               int    maxResults = 20,
	               double nmsRadiusFactor = 1.0) override;

	void setRadiusRange(int minRadius, int maxRadius); // 检测半径范围（像素）
	void setSmoothing(int kernelSize);                 // 高斯去噪核（奇数；0 关）
	void setEdgeThreshold(double highThreshold);       // Hough 边缘阈值；<=0 自适应
	void setSubpixel(bool enable);                     // 亚像素精化（默认开）
	void setSamplePoints(int count);                   // 径向采样点数（默认 360）

	bool match() override;

private:
	double m_threshold;
	int    m_maxResults;
	double m_nmsRadiusFactor;

	int    m_minRadius;
	int    m_maxRadius;
	int    m_smoothKernel;
	double m_edgeThreshold;
	bool   m_subpixel;
	int    m_samplePoints;
};

