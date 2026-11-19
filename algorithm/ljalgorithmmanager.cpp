#include "ljalgorithmmanager.h"
#include "ljgraymatch.h"
#include "ljcirclematch.h"

// ==============================================================
// 单例
// ==============================================================
LJAlgorithmManager& LJAlgorithmManager::instance()
{
	static LJAlgorithmManager s_instance;
	return s_instance;
}

LJAlgorithmManager::LJAlgorithmManager()
	: m_type(LJAlgorithmType::GrayMatch)
{
	// 默认启用灰度匹配，外部拿到即可用
	setAlgorithm(LJAlgorithmType::GrayMatch);
}

bool LJAlgorithmManager::setAlgorithm(LJAlgorithmType type)
{
	std::unique_ptr<LJMatchBase> algo;
	switch (type)
	{
	case LJAlgorithmType::GrayMatch:
		algo = std::make_unique<LJGrayMatch>();
		break;
	case LJAlgorithmType::CircleMatch:
		algo = std::make_unique<LJCircleMatch>();
		break;
	default:
		return false;   // 未支持的算法类型
	}

	m_algorithm = std::move(algo);
	m_type = type;
	return true;
}

// ---- 透传通用接口 ----
void LJAlgorithmManager::setParams(double threshold, int maxResults, double nmsRadiusFactor)
{
	if (m_algorithm) m_algorithm->setParams(threshold, maxResults, nmsRadiusFactor);
}

void LJAlgorithmManager::setSource(const cv::Mat& source)
{
	if (m_algorithm) m_algorithm->setSource(source);
}

void LJAlgorithmManager::setTemplate(const cv::Mat& templ)
{
	if (m_algorithm) m_algorithm->setTemplate(templ);
}

void LJAlgorithmManager::prepare(const cv::Mat& source, const cv::Mat& templ)
{
	if (m_algorithm) m_algorithm->prepare(source, templ);
}

bool LJAlgorithmManager::match()
{
	if (!m_algorithm) return false;
	return m_algorithm->match();
}

const std::vector<MatchResult*>& LJAlgorithmManager::results() const
{
	static const std::vector<MatchResult*> s_empty;
	return m_algorithm ? m_algorithm->results() : s_empty;
}

const std::string& LJAlgorithmManager::lastError() const
{
	static const std::string s_empty;
	return m_algorithm ? m_algorithm->lastError() : s_empty;
}

const std::string& LJAlgorithmManager::name() const
{
	static const std::string s_empty;
	return m_algorithm ? m_algorithm->name() : s_empty;
}
