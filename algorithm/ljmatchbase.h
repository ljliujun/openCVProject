#pragma once
#include <vector>
#include <string>
#include <opencv2/core.hpp>

// ==============================================================
// 匹配结果：单次命中 = 左上角位置 + 分数
// 所有匹配算法子类统一输出这个结构
// ==============================================================
struct MatchResult
{
	cv::Point   topLeft;     // 模板匹配到的左上角坐标（原图坐标系，整数，兼容旧接口）
	cv::Point2f topLeftF;    // 亚像素级左上角坐标（原图坐标系）
	double      score;       // 匹配分数，越大越好（不同算法值域不同，子类文档说明）
	double      scale;       // 匹配时模板相对原始模板的缩放比（1.0 = 原始大小）

	MatchResult() : topLeft(0, 0), topLeftF(0.f, 0.f), score(0.0), scale(1.0) {}
	MatchResult(const cv::Point& p, double s)
		: topLeft(p), topLeftF(static_cast<float>(p.x), static_cast<float>(p.y)), score(s), scale(1.0) {}
};

// ==============================================================
// 算法基类 —— 统一所有匹配算法的接口
// 使用方式：prepare() → run() → results()
// 子类必须实现 run()
// ==============================================================
class LJMatchBase
{
public:
	virtual ~LJMatchBase() = default;

	virtual void setParams(double threshold, int maxResults, double nmsRadiusFactor) = 0;
	// 类别名，用于日志/调试（子类在构造时设置）
	const std::string& name() const { return m_name; }

	// 设置源图（要在其中寻找模板的大图）
	void setSource(const cv::Mat& source) { m_source = source.clone(); }
	// 设置模板图（要找的小图）
	void setTemplate(const cv::Mat& templ) { m_template = templ.clone(); }

	// 便捷：一次设置两者
	void prepare(const cv::Mat& source, const cv::Mat& templ)
	{
		setSource(source);
		setTemplate(templ);
	}

	// 执行匹配，纯虚函数，子类必须实现
	// 返回 true 表示匹配成功（哪怕一个结果都没找到也算成功执行），false 表示出错
	virtual bool match() = 0;

	// 匹配完成后通过这里取结果
	const std::vector<MatchResult*>& results() const { return m_results; }
	// 子类也可能需要清空后自己重新填充
	void clearResults();

	// 执行过程中出错的信息（run 返回 false 时调用这个看原因）
	const std::string& lastError() const { return m_lastError; }

protected:
	// 子类在构造时调用，设置算法名
	void setName(const std::string& n) { m_name = n; }

	// 子类可调用这个记录错误信息
	void setError(const std::string& msg) { m_lastError = msg; }

	// 子类可直接访问这几个成员
	cv::Mat                  m_source;     // 源图
	cv::Mat                  m_template;   // 模板
	std::vector<MatchResult*> m_results;    // 匹配结果

private:
	std::string m_name;
	std::string m_lastError;
};
