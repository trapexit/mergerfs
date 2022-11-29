#pragma once

#include <mutex>
#include <queue>
#include <utility>
#include <stdexcept>
#include <condition_variable>



template<typename T>
class unbounded_queue
{
public:
	explicit unbounded_queue(bool block = true)
	: m_block{ block } {}

	void push(const T& item)
	{
		{
			std::scoped_lock guard(m_queue_lock);
			m_queue.push(item);
		}
		m_condition.notify_one();
	}

	void push(T&& item)
	{
		{
			std::scoped_lock guard(m_queue_lock);
			m_queue.push(std::move(item));
		}
		m_condition.notify_one();
	}

	template<typename... Args>
	void emplace(Args&&... args)
	{
		{
			std::scoped_lock guard(m_queue_lock);
			m_queue.emplace(std::forward<Args>(args)...);
		}
		m_condition.notify_one();
	}

	bool try_push(const T& item)
	{
		{
			std::unique_lock lock(m_queue_lock, std::try_to_lock);
			if(!lock)
				return false;
			m_queue.push(item);
		}
		m_condition.notify_one();
		return true;
	}

	bool try_push(T&& item)
	{
		{
			std::unique_lock lock(m_queue_lock, std::try_to_lock);
			if(!lock)
				return false;
			m_queue.push(std::move(item));
		}
		m_condition.notify_one();
		return true;
	}

	bool pop(T& item)
	{
		std::unique_lock guard(m_queue_lock);
		m_condition.wait(guard, [&]() { return !m_queue.empty() || !m_block; });
		if(m_queue.empty())
			return false;
		item = std::move(m_queue.front());
		m_queue.pop();
		return true;
	}

	bool try_pop(T& item)
	{
		std::unique_lock lock(m_queue_lock, std::try_to_lock);
		if(!lock || m_queue.empty())
			return false;
		item = std::move(m_queue.front());
		m_queue.pop();
		return true;
	}

	std::size_t size() const
	{
		std::scoped_lock guard(m_queue_lock);
		return m_queue.size();
	}

	bool empty() const
	{
		std::scoped_lock guard(m_queue_lock);
		return m_queue.empty();
	}

	void block()
	{
		std::scoped_lock guard(m_queue_lock);
		m_block = true;
	}

	void unblock()
	{
		{
			std::scoped_lock guard(m_queue_lock);
			m_block = false;
		}
		m_condition.notify_all();
	}

	bool blocking() const
	{
		std::scoped_lock guard(m_queue_lock);
		return m_block;
	}

private:
	using queue_t = std::queue<T>;
	queue_t m_queue;

	bool m_block;

	mutable std::mutex m_queue_lock;
	std::condition_variable m_condition;
};



template<typename T>
class bounded_queue
{
public:
	explicit bounded_queue(std::size_t max_size, bool block = true)
	: m_block{ block }, m_max_size{ max_size }
	{
		if(!m_max_size)
			throw std::invalid_argument("bad queue max-size! must be non-zero!");
	}

	bool push(const T& item)
	{
		{
			std::unique_lock guard(m_queue_lock);
			m_condition_push.wait(guard, [&]() { return m_queue.size() < m_max_size || !m_block; });
			if(m_queue.size() == m_max_size)
				return false;
			m_queue.push(item);
		}
		m_condition_pop.notify_one();
		return true;
	}

	bool push(T&& item)
	{
		{
			std::unique_lock guard(m_queue_lock);
			m_condition_push.wait(guard, [&]() { return m_queue.size() < m_max_size || !m_block; });
			if(m_queue.size() == m_max_size)
				return false;
			m_queue.push(std::move(item));
		}
		m_condition_pop.notify_one();
		return true;
	}

	template<typename... Args>
	bool emplace(Args&&... args)
	{
		{
			std::unique_lock guard(m_queue_lock);
			m_condition_push.wait(guard, [&]() { return m_queue.size() < m_max_size || !m_block; });
			if(m_queue.size() == m_max_size)
				return false;
			m_queue.emplace(std::forward<Args>(args)...);
		}
		m_condition_pop.notify_one();
		return true;
	}

	bool pop(T& item)
	{
		{
			std::unique_lock guard(m_queue_lock);
			m_condition_pop.wait(guard, [&]() { return !m_queue.empty() || !m_block; });
			if(m_queue.empty())
				return false;
			item = std::move(m_queue.front());
			m_queue.pop();
		}
		m_condition_push.notify_one();
		return true;
	}

	std::size_t size() const
	{
		std::scoped_lock guard(m_queue_lock);
		return m_queue.size();
	}

	std::size_t capacity() const
	{
		return m_max_size;
	}

	bool empty() const
	{
		std::scoped_lock guard(m_queue_lock);
		return m_queue.empty();
	}

	bool full() const
	{
		std::scoped_lock lock(m_queue_lock);
		return m_queue.size() == capacity();
	}

	void block()
	{
		std::scoped_lock guard(m_queue_lock);
		m_block = true;
	}

	void unblock()
	{
		{
			std::scoped_lock guard(m_queue_lock);
			m_block = false;
		}
		m_condition_push.notify_all();
		m_condition_pop.notify_all();
	}

	bool blocking() const
	{
		std::scoped_lock guard(m_queue_lock);
		return m_block;
	}

private:
	using queue_t = std::queue<T>;
	queue_t m_queue;

	bool m_block;
	const std::size_t m_max_size;

	mutable std::mutex m_queue_lock;
	std::condition_variable m_condition_push;
	std::condition_variable m_condition_pop;
};
