#pragma once
#include "ljmatchbase.h"

// ==============================================================
// 灰度模板匹配 —— 基于 cv::matchTemplate(TM_CCOEFF_NORMED)
// 特性：
//   1) 输入（源图/模板）可以是彩色或灰度，内部统一转灰度匹配
//   2) TM_CCOEFF_NORMED 对亮度/对比度变化鲁棒，分数范围 [0, 1]
//   3) 支持多目标（迭代找峰 + 模板大小区域抑制）
//   4) 参数可配置：阈值、最多返回数、NMS 抑制半径
// ==============================================================
class LJGrayMatch : public LJMatchBase
{
public:
	LJGrayMatch();
	~LJGrayMatch() override = default;

	// 一次性设置所有参数
	//   threshold     : 最小相关系数，0~1，越大越严格，默认 0.80
	//   maxResults    : 最多返回多少个命中，默认 20
	//   nmsRadiusFactor: NMS 抑制半径 = 模板长边 * 这个系数，默认 0.5
	void setParams(double threshold = 0.80,
	               int    maxResults = 20,
	               double nmsRadiusFactor = 0.5);

	// 多尺度匹配：scaleMin/scaleMax 为相对原始模板的缩放范围，
	// scaleSteps 为在该范围内均匀采样的档数（默认 0.9~1.1 共 3 档）
	void setScaleRange(double scaleMin, double scaleMax, int scaleSteps);

	// 匹配前高斯平滑核大小（奇数；0 表示不平滑），用于抑制噪声
	void setSmoothing(int kernelSize);

	// 是否启用亚像素峰值精化（默认开启），提升定位精度
	void setSubpixel(bool enable);

	bool match() override;

private:
	double m_threshold;
	int    m_maxResults;
	double m_nmsRadiusFactor;

	double m_scaleMin;
	double m_scaleMax;
	int    m_scaleSteps;
	int    m_smoothKernel;
	bool   m_subpixel;
};

