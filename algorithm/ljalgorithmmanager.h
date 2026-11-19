#pragma once
#include <memory>
#include "ljmatchbase.h"

// ==============================================================
// 算法类型枚举 —— 外部通过它选择要使用的图像算法
// ==============================================================
enum class LJAlgorithmType
{
	GrayMatch = 0,   // 灰度模板匹配（LJGrayMatch）
	CircleMatch = 1, // 圆检测（LJCircleMatch）
	// 后续新增算法在此追加，例如：
	// EdgeMatch, ...
};


// ==============================================================
// 图像算法统一管理类（外观模式 + 单例）
// 所有图像算法都由该类集中管理，外部只与它交互，
// 不再直接 new 具体算法类。
//
// 典型用法：
//   auto& mgr = LJAlgorithmManager::instance();
//   mgr.setAlgorithm(LJAlgorithmType::GrayMatch);
//   mgr.prepare(source, templ);
//   if (mgr.match()) {
//       for (auto* r : mgr.results()) { ... }
//   }
//
// 需要子类特有配置时（如灰度匹配的多尺度/平滑），
// 通过 as<T>() 取到具体子类再设置，例如：
//   if (auto* gray = mgr.as<LJGrayMatch>())
//       gray->setScaleRange(0.9, 1.1, 5);
// ==============================================================
class LJAlgorithmManager
{
public:
	// 单例入口
	static LJAlgorithmManager& instance();

	// 选择要使用的算法；返回 false 表示不支持的算法类型
	bool setAlgorithm(LJAlgorithmType type);

	// 当前算法类型
	LJAlgorithmType currentType() const { return m_type; }

	// 当前算法实例（基类指针，用于访问通用接口）
	LJMatchBase* current() const { return m_algorithm.get(); }

	// 将当前算法强转为具体子类，用于调用子类特有接口
	template <typename T>
	T* as() const { return dynamic_cast<T*>(m_algorithm.get()); }

	// ---- 透传 LJMatchBase 的通用接口 ----
	void setParams(double threshold, int maxResults, double nmsRadiusFactor);
	void setSource(const cv::Mat& source);
	void setTemplate(const cv::Mat& templ);
	void prepare(const cv::Mat& source, const cv::Mat& templ);
	bool match();
	const std::vector<MatchResult*>& results() const;
	const std::string& lastError() const;
	const std::string& name() const;

private:
	LJAlgorithmManager();
	~LJAlgorithmManager() = default;
	LJAlgorithmManager(const LJAlgorithmManager&) = delete;
	LJAlgorithmManager& operator=(const LJAlgorithmManager&) = delete;

	LJAlgorithmType               m_type;        // 当前算法类型
	std::unique_ptr<LJMatchBase>  m_algorithm;   // 当前算法实例
};