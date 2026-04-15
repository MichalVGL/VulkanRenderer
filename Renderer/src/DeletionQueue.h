#ifndef DELETIONQUEUE_H
#define DELETIONQUEUE_H

#include "pch.h"

#include <deque>

namespace dae
{
	class DeletionQueue final
	{
	public:

		~DeletionQueue()
		{
			Flush();
		}

		void PushDeleter(std::function<void()>&& deletorFunc)
		{
			m_Deletors.push_back(deletorFunc);
		}

		void Flush()
		{
			for (auto& deletorFunc : m_Deletors | std::views::reverse)
				deletorFunc();

			m_Deletors.clear();
		}

	private:

		std::deque<std::function<void()>> m_Deletors;

	};
}

#endif // !DELETIONQUEUE_H
