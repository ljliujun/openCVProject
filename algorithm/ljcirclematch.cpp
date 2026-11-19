#include "ljcirclematch.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

LJCircleMatch::LJCircleMatch()
	: m_threshold(0.50), m_maxResults(20), m_nmsRadiusFactor(1.0)
	, m_minRadius(10), m_maxRadius(500), m_smoothKernel(0)
	, m_edgeThreshold(0), m_subpixel(true), m_samplePoints(360)
{
	setName("CircleMatch");
}

void LJCircleMatch::setParams(double threshold, int maxResults, double nmsRadiusFactor)
{
	m_threshold = threshold;
	m_maxResults = maxResults;
	m_nmsRadiusFactor = nmsRadiusFactor;
}

void LJCircleMatch::setRadiusRange(int minRadius, int maxRadius)
{
	m_minRadius = std::max(2, minRadius);
	m_maxRadius = std::max(m_minRadius + 1, maxRadius);
}

void LJCircleMatch::setSmoothing(int kernelSize) { m_smoothKernel = kernelSize; }
void LJCircleMatch::setEdgeThreshold(double highThreshold) { m_edgeThreshold = highThreshold; }
void LJCircleMatch::setSubpixel(bool enable) { m_subpixel = enable; }
void LJCircleMatch::setSamplePoints(int count) { m_samplePoints = std::max(24, count); }

static cv::Mat toGray8U(const cv::Mat& s)
{
	if (s.empty()) return cv::Mat();
	cv::Mat g;
	if (s.channels() == 1) { s.convertTo(g, CV_8U); return g; }
	cv::Mat c;
	if (s.channels() == 4) cv::cvtColor(s, c, cv::COLOR_BGRA2BGR);
	else if (s.depth() == CV_8U) c = s;
	else s.convertTo(c, CV_8U);
	cv::cvtColor(c, g, cv::COLOR_BGR2GRAY);
	return g;
}

static double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

static double medianOfMat(const cv::Mat& m)
{
	cv::Mat flat = m.reshape(1, 1);
	if (flat.empty()) return 0.0;
	std::vector<float> vals(flat.begin<float>(), flat.end<float>());
	size_t n = vals.size();
	std::nth_element(vals.begin(), vals.begin() + n / 2, vals.end());
	return vals[n / 2];
}

// 鲁棒圆拟合：Gauss-Newton + Tukey bisquare 迭代重加权，返回归一化残差
static double fitCircleIRLS(const std::vector<cv::Point2f>& pts, cv::Point2f& c, double& r)
{
	if (pts.size() < 3) return 1e9;
	double a = c.x, b = c.y, T = 4.685;
	for (int it = 0; it < 30; ++it)
	{
		std::vector<double> f(pts.size()), d(pts.size());
		for (size_t i = 0; i < pts.size(); ++i)
		{
			double dx = pts[i].x - a, dy = pts[i].y - b;
			d[i] = std::sqrt(dx * dx + dy * dy);
			f[i] = d[i] - r;
		}
		std::vector<double> t = f;
		std::nth_element(t.begin(), t.begin() + t.size() / 2, t.end());
		double med = t[t.size() / 2];
		for (size_t i = 0; i < f.size(); ++i) t[i] = std::fabs(f[i] - med);
		std::nth_element(t.begin(), t.begin() + t.size() / 2, t.end());
		double sc = 1.4826 * t[t.size() / 2] + 1e-9;

		double S00 = 0, S01 = 0, S02 = 0, S11 = 0, S12 = 0, S22 = 0, g0 = 0, g1 = 0, g2 = 0;
		for (size_t i = 0; i < pts.size(); ++i)
		{
			if (d[i] < 1e-9) continue;
			double u = f[i] / sc;
			double w = (std::fabs(u) <= T) ? (1.0 - u * u / (T * T)) : 0.0;
			w *= w;
			double jx = -(pts[i].x - a) / d[i], jy = -(pts[i].y - b) / d[i];
			S00 += w * jx * jx; S01 += w * jx * jy; S02 -= w * jx;
			S11 += w * jy * jy; S12 -= w * jy; S22 += w;
			g0 += w * jx * f[i]; g1 += w * jy * f[i]; g2 -= w * f[i];
		}
		cv::Mat J = (cv::Mat_<double>(3, 3) << S00, S01, S02, S01, S11, S12, S02, S12, S22);
		cv::Mat G = (cv::Mat_<double>(3, 1) << -g0, -g1, -g2);
		cv::Mat dl;
		if (!cv::solve(J, G, dl, cv::DECOMP_SVD)) break;
		double step = std::max(1.0, r * 0.5);
		a += clampd(dl.at<double>(0), -step, step);
		b += clampd(dl.at<double>(1), -step, step);
		r += clampd(dl.at<double>(2), -step, step);
		if (r < 1e-3) r = 1e-3;
		if (std::fabs(dl.at<double>(0)) + std::fabs(dl.at<double>(1)) + std::fabs(dl.at<double>(2)) < 1e-4) break;
	}
	c = cv::Point2f(static_cast<float>(a), static_cast<float>(b));
	double sum = 0; int cnt = 0;
	for (const auto& p : pts)
	{
		double dx = p.x - a, dy = p.y - b;
		double di = std::sqrt(dx * dx + dy * dy);
		sum += (di - r) * (di - r); ++cnt;
	}
	return cnt ? std::sqrt(sum / cnt) / (r + 1e-6) : 1e9;
}

// 径向采样：在给定圆附近沿半径方向搜索梯度最强的边缘点
static void sampleEdgePoints(const cv::Mat& gm, double cx, double cy, double r,
	int samples, std::vector<cv::Point2f>& pts)
{
	pts.clear();
	int band = static_cast<int>(clampd(cvRound(r * 0.10), 3, 25));
	for (int i = 0; i < samples; ++i)
	{
		double ang = 2.0 * CV_PI * i / samples;
		double dx = std::cos(ang), dy = std::sin(ang);
		double best = 0; int bx = -1, by = -1;
		for (int off = -band; off <= band; ++off)
		{
			int x = cvRound(cx + dx * (r + off));
			int y = cvRound(cy + dy * (r + off));
			if (x < 0 || y < 0 || x >= gm.cols || y >= gm.rows) continue;
			float m = gm.at<float>(y, x);
			if (m > best) { best = m; bx = x; by = y; }
		}
		if (best > 0) pts.emplace_back(static_cast<float>(bx), static_cast<float>(by));
	}
}

bool LJCircleMatch::match()
{
	clearResults();
	if (m_source.empty()) { setError("source image is empty"); return false; }

	cv::Mat gray = toGray8U(m_source);
	if (gray.empty()) { setError("gray conversion failed"); return false; }

	if (m_smoothKernel > 1)
	{
		int k = (m_smoothKernel % 2 == 0) ? m_smoothKernel + 1 : m_smoothKernel;
		cv::GaussianBlur(gray, gray, cv::Size(k, k), 0);
	}

	cv::Mat gx, gy;
	cv::Sobel(gray, gx, CV_32F, 1, 0, 3);
	cv::Sobel(gray, gy, CV_32F, 0, 1, 3);
	cv::Mat gm;
	cv::magnitude(gx, gy, gm);

	double high = m_edgeThreshold;
	if (high <= 0) high = std::max(1.0, medianOfMat(gm));

	int minR = std::max(2, m_minRadius);
	int maxR = std::min(std::min(gray.cols, gray.rows) / 2, std::max(minR + 1, m_maxRadius));
	if (maxR <= minR) { setError("invalid radius range"); return false; }

	std::vector<cv::Vec3f> circles;
	cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT, 1.5, minR * 2, high, 30, minR, maxR);

	struct Cand { cv::Point2f c; double r; double score; double roundness; };
	std::vector<Cand> cands;
	for (const auto& cc : circles)
	{
		std::vector<cv::Point2f> pts;
		sampleEdgePoints(gm, cc[0], cc[1], cc[2], m_samplePoints, pts);
		if (static_cast<int>(pts.size()) < 12) continue;
		cv::Point2f fc(cc[0], cc[1]);
		double fr = cc[2], roundness = 0.0;
		if (m_subpixel)
			roundness = fitCircleIRLS(pts, fc, fr);
		double score = (static_cast<double>(pts.size()) / m_samplePoints)
			* std::max(0.0, 1.0 - roundness * 4.0);
		if (score < m_threshold) continue;
		cands.push_back({ fc, fr, score, roundness });
	}

	std::sort(cands.begin(), cands.end(),
		[](const Cand& a, const Cand& b) { return a.score > b.score; });

	for (const auto& c : cands)
	{
		if (static_cast<int>(m_results.size()) >= m_maxResults) break;
		bool dup = false;
		for (const auto* k : m_results)
		{
			const auto* kc = dynamic_cast<const CircleResult*>(k);
			if (kc && cv::norm(c.c - kc->center) < m_nmsRadiusFactor * std::min(c.r, kc->radius))
			{ dup = true; break; }
		}
		if (dup) continue;

		auto* res = new CircleResult();
		res->center = c.c;
		res->radius = c.r;
		res->roundness = c.roundness;
		res->score = c.score;
		res->topLeftF = cv::Point2f(c.c.x - static_cast<float>(c.r), c.c.y - static_cast<float>(c.r));
		res->topLeft = cv::Point(cvRound(res->topLeftF.x), cvRound(res->topLeftF.y));
		m_results.push_back(res);
	}
	return true;
}
