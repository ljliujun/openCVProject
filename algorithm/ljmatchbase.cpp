#include "ljmatchbase.h"


void LJMatchBase::clearResults()
{
	for (auto& match : m_results)
	{
		if (match)
		{
			delete match;
			match = nullptr;
		}
	}
	m_results.clear(); 
}