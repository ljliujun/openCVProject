#include "ljgraymatch.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

LJGrayMatch::LJGrayMatch()
	: m_threshold(0.80)
	, m_maxResults(20)
	, m_nmsRadiusFactor(0.5)
	, m_scaleMin(0.90)
	, m_scaleMax(1.10)
	, m_scaleSteps(3)
	, m_smoothKernel(0)
	, m_subpixel(true)
{
	setName("GrayMatch");
}

void LJGrayMatch::setParams(double threshold, int maxResults, double nmsRadiusFactor)
{
	m_threshold        = threshold;
	m_maxResults       = maxResults;
	m_nmsRadiusFactor  = nmsRadiusFactor;
}

void LJGrayMatch::setScaleRange(double scaleMin, double scaleMax, int scaleSteps)
{
	m_scaleMin   = scaleMin;
	m_scaleMax   = scaleMax;
	m_scaleSteps = scaleSteps < 1 ? 1 : scaleSteps;
}

void LJGrayMatch::setSmoothing(int kernelSize)
{
	m_smoothKernel = kernelSize;
}

void LJGrayMatch::setSubpixel(bool enable)
{
	m_subpixel = enable;
}

// ==============================================================
// 辅助：把任意 CV_8UC* Mat 转成 CV_8UC1 灰度
// ==============================================================
static cv::Mat toGray8U(const cv::Mat& src)
{
	if (src.empty()) return cv::Mat();

	cv::Mat work;
	// 统一先弄成 CV_8UC3
	switch (src.channels())
	{
	case 1:
		src.convertTo(work, CV_8U);
		return work.clone();
	case 3:
		if (src.depth() == CV_8U) work = src.clone();
		else                     src.convertTo(work, CV_8U);
		break;
	case 4:
		cv::cvtColor(src, work, cv::COLOR_BGRA2BGR);
		break;
	default:
		// 其他通道：先转 8U 再缩到 3 通道
		src.convertTo(work, CV_8U);
		break;
	}

	cv::Mat gray;
	cv::cvtColor(work, gray, cv::COLOR_BGR2GRAY);
	return gray;
}

namespace 
{

// 亚像素峰值精化：在响应图峰值处用一维抛物线拟合，得到亚像素偏移
cv::Point2f refinePeak(const cv::Mat& resp, const cv::Point& p)
{
	const int x = p.x, y = p.y;
	if (x <= 0 || y <= 0 || x >= resp.cols - 1 || y >= resp.rows - 1)
		return cv::Point2f(static_cast<float>(x), static_cast<float>(y));

	const float c  = resp.at<float>(y, x);
	const float xl = resp.at<float>(y, x - 1);
	const float xr = resp.at<float>(y, x + 1);
	const float yu = resp.at<float>(y - 1, x);
	const float yd = resp.at<float>(y + 1, x);

	const double denomX = static_cast<double>(xl) - 2.0 * c + xr;
	const double denomY = static_cast<double>(yu) - 2.0 * c + yd;

	double dx = 0.0, dy = 0.0;
	if (std::abs(denomX) > 1e-6)
		dx = (static_cast<double>(xl) - xr) / (2.0 * denomX);
	if (std::abs(denomY) > 1e-6)
		dy = (static_cast<double>(yu) - yd) / (2.0 * denomY);

	dx = std::max(-1.0, std::min(1.0, dx));
	dy = std::max(-1.0, std::min(1.0, dy));

	return cv::Point2f(static_cast<float>(x + dx), static_cast<float>(y + dy));
}

// 生成 [lo, hi] 内均匀分布的缩放比例列表
std::vector<double> buildScales(double lo, double hi, int steps)
{
	std::vector<double> scales;
	if (steps <= 1)
	{
		scales.push_back((lo + hi) * 0.5);
		return scales;
	}
	for (int i = 0; i < steps; ++i)
		scales.push_back(lo + (hi - lo) * static_cast<double>(i) / (steps - 1));
	return scales;
}

} // namespace

bool LJGrayMatch::match()
{
	clearResults();

	// ---- 输入校验 ----
	if (m_source.empty())
	{
		setError("source image is empty");
		return false;
	}
	if (m_template.empty())
	{
		setError("template image is empty");
		return false;
	}

	// ---- 灰度化 ----
	cv::Mat srcGray = toGray8U(m_source);
	cv::Mat tplGray = toGray8U(m_template);
	if (srcGray.empty() || tplGray.empty())
	{
		setError("gray conversion failed");
		return false;
	}

	const int tw = tplGray.cols;
	const int th = tplGray.rows;
	if (tw < 4 || th < 4)
	{
		setError("template too small");
		return false;
	}
	if (tw > srcGray.cols || th > srcGray.rows)
	{
		setError("template larger than source");
		return false;
	}

	// ---- 可选高斯平滑，抑制噪声 ----
	if (m_smoothKernel > 1)
	{
		const int k = (m_smoothKernel % 2 == 0) ? m_smoothKernel + 1 : m_smoothKernel;
		cv::GaussianBlur(srcGray, srcGray, cv::Size(k, k), 0);
		cv::GaussianBlur(tplGray, tplGray, cv::Size(k, k), 0);
	}

	// ---- 多尺度模板匹配：每个尺度跑一遍，汇总所有候选 ----
	std::vector<MatchResult> candidates;

	const std::vector<double> scales = buildScales(m_scaleMin, m_scaleMax, m_scaleSteps);
	for (double s : scales)
	{
		if (s <= 0.0) continue;
		const int stw = cvRound(tw * s);
		const int sth = cvRound(th * s);
		if (stw < 4 || sth < 4) continue;
		if (stw > srcGray.cols || sth > srcGray.rows) continue;

		cv::Mat tplScaled;
		cv::resize(tplGray, tplScaled, cv::Size(stw, sth), 0, 0, cv::INTER_LINEAR);

		cv::Mat result;
		cv::matchTemplate(srcGray, tplScaled, result, cv::TM_CCOEFF_NORMED);

		// 掩码：255=参与搜索，0=忽略（OpenCV 掩码约定：非零参与）
		cv::Mat suppressMask(result.size(), CV_8UC1, cv::Scalar(255));

		const int nmsRadius = static_cast<int>(std::max(stw, sth) * m_nmsRadiusFactor);

		for (int i = 0; i < m_maxResults; ++i)
		{
			double bestVal = -1.0;
			cv::Point bestLoc;
			cv::minMaxLoc(result, nullptr, &bestVal, nullptr, &bestLoc, suppressMask);
			if (bestVal < m_threshold) break;

			MatchResult mr;
			mr.topLeftF = m_subpixel
				? refinePeak(result, bestLoc)
				: cv::Point2f(static_cast<float>(bestLoc.x), static_cast<float>(bestLoc.y));
			mr.topLeft = cv::Point(cvRound(mr.topLeftF.x), cvRound(mr.topLeftF.y));
			mr.score   = bestVal;
			mr.scale   = s;
			candidates.push_back(mr);

			cv::Rect suppress(
				std::max(0, bestLoc.x - nmsRadius),
				std::max(0, bestLoc.y - nmsRadius),
				2 * nmsRadius + 1,
				2 * nmsRadius + 1);
			suppress &= cv::Rect(0, 0, result.cols, result.rows);
			if (suppress.width > 0 && suppress.height > 0)
				suppressMask(suppress) = 0;
		}
	}

	// ---- 全局 NMS：按分数降序，抑制与已保留结果重叠的候选 ----
	std::sort(candidates.begin(), candidates.end(),
		[](const MatchResult& a, const MatchResult& b) { return a.score > b.score; });

	for (const MatchResult& c : candidates)
	{
		if (static_cast<int>(m_results.size()) >= m_maxResults) break;

		const cv::Rect2f rc(c.topLeftF.x, c.topLeftF.y,
		                    static_cast<float>(tw * c.scale), static_cast<float>(th * c.scale));

		bool overlap = false;
		for (const MatchResult* k : m_results)
		{
			const cv::Rect2f rk(k->topLeftF.x, k->topLeftF.y,
			                    static_cast<float>(tw * k->scale), static_cast<float>(th * k->scale));
			if ((rc & rk).area() > 0.0f) { overlap = true; break; }
		}
		if (overlap) continue;

		m_results.push_back(new MatchResult(c));
	}

	return true;
}
