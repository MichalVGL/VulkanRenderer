#ifndef DELETIONQUEUE_H
#define DELETIONQUEUE_H

#include <deque>
#include <functional>
#include <ranges>
#include <algorithm>

namespace Renderer
{
	class DeletionQueue final
	{
	public:

		void PushDeleter(std::function<void()>&& deletorFunc)
		{
			m_Deletors.push_back(deletorFunc);
		}

		void Flush()
		{
			std::ranges::for_each(std::views::reverse(m_Deletors), [](std::function<void()>& deletorFunc)
				{
					deletorFunc();
				});

			m_Deletors.clear();
		}

	private:

		std::deque<std::function<void()>> m_Deletors;

	};
}

#endif // !DELETIONQUEUE_H
