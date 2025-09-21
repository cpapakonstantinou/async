#ifndef __ASYNC_H__
#define __ASYNC_H__

/**
 * \file async.h
 * \brief Header-only support for task-based parallelism using C++ `std::async`.
 * \author Constantine Papakonstantinou
 * \date 2025
 */

// Copyright (c) 2025  Constantine Papakonstantinou
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <future>
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>
#include <concepts>
#include <ranges>
#include <algorithm>
#include <exception>
#include <type_traits>
#include <iterator>

#ifdef __linux__
	#include <pthread.h>
	#include <sched.h>
#endif

namespace async
{
	// Recommend the number of real cores, for thread pinning.
	#ifndef ASYNC_NUM_THREADS
		#define ASYNC_NUM_THREADS 4  // default value
	#endif
	
	#ifndef ASYNC_MIN_THREADS
		#define ASYNC_MIN_THREADS 1  // default value
	#endif

	#ifndef ASYNC_MAX_THREADS
		#define ASYNC_MAX_THREADS 64  // default value
	#endif

	static constexpr size_t threads = ASYNC_NUM_THREADS; ///< Number of threads to use.

	/** 
	 * \brief runtime support for setting threads with environment variable
	 * \note this only applies .
	 * \note  
	 */
	inline size_t runtime_threads() 
	{
		static size_t _threads = 0;
		if ( _threads == 0) 
		{
			const char* env_async = std::getenv("ASYNC_NUM_THREADS");    
			if (env_async) 
			{

				_threads = 
					std::min(ASYNC_MAX_THREADS, 
						std::max(ASYNC_MIN_THREADS, std::atoi(env_async)));
			} 
			else 
			{
				_threads = threads;
			}
		}
		return _threads;
	}

	/**
	 * \brief Launch an asynchronous task using std::async with forwarded parameters.
	 * \tparam F Callable type
	 * \tparam Ts Argument types
	 * \param f Callable to execute
	 * \param params Arguments to pass to callable
	 * \return std::future holding the result of the async operation
	 */
	template<typename F, typename... Ts> 
	inline std::future<typename std::result_of<F(Ts...)>::type>
	call_async(F&& f, Ts&&... params) 
	{
		return std::async(std::launch::async, 
			std::forward<F>(f), 
			std::forward<Ts>(params)...);
	}

	/// Trait for checking if an iterator is random access
	template<typename I>
	constexpr bool is_random_access_iterator_v = std::random_access_iterator<I>;

	/**
	 * \brief Launches a parallel for-each operation across a range using asynchronous tasks.
	 * 
	 * Optimized with atomic operations for better performance.
	 * 
	 * \tparam I Iterator type
	 * \tparam F Callable type
	 * \tparam P Progress callback type
	 * \param begin Iterator to start of range
	 * \param end Iterator to end of range
	 * \param f Function to invoke on each element (can optionally take an index)
	 * \param threads Number of threads to use
	 * \param progress Optional progress callback (receives count of completed threads)
	 */
	template<typename I, typename F, typename P>
	inline void 
	async_for_each(I begin, I end, F&& f,
						size_t threads = runtime_threads(),
						P&& progress = [](size_t) {})
	{
		using difference_type = typename std::iterator_traits<I>::difference_type;
		auto size = std::ranges::distance(begin, end);
		if (size == 0) return;

		threads = std::min(threads, static_cast<size_t>(size));
		auto chunk_size = size / threads;
		std::vector<std::future<void>> futures(threads);

		alignas(64) std::atomic<bool> abort{false};
		alignas(64) std::atomic<size_t> completed{0};

		std::exception_ptr ex_ptr = nullptr;
		std::mutex ex_mutex;

		I chunk_begin = begin;

		for (size_t i = 0; i < threads; ++i)
		{
			I chunk_end;

			if constexpr (is_random_access_iterator_v<I>)
			{
				chunk_end = (i == threads - 1) ? end : std::ranges::next(chunk_begin, chunk_size);
			}
			else
			{
				chunk_end = chunk_begin;
				difference_type steps = (i == threads - 1) ? size : chunk_size;
				while (steps-- && chunk_end != end) ++chunk_end;
			}

			auto local_chunk_begin = chunk_begin;
			auto local_chunk_end = chunk_end;
			auto chunk_len = std::ranges::distance(local_chunk_begin, local_chunk_end);
			auto idx_offset = i * chunk_len;

			futures[i] = call_async([=, &f, &abort, &ex_ptr, &ex_mutex, &completed, &progress]() mutable 
			{
				#ifdef __linux__
					cpu_set_t cpuset;
					CPU_ZERO(&cpuset);
					CPU_SET(i % std::thread::hardware_concurrency(), &cpuset);
					pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);					
				#endif

				try
				{
					size_t idx = idx_offset;
					for (auto it = local_chunk_begin; it != local_chunk_end; ++it)
					{
						[[assume(!abort.load(std::memory_order_relaxed))]];
						if (abort.load(std::memory_order_relaxed)) break;

						if constexpr (std::is_invocable_v<F, decltype(*it), size_t, size_t>)
							f(*it, idx++, i); // i is the thread id
						else if constexpr (std::is_invocable_v<F, decltype(*it), size_t>)
							f(*it, idx++);
						else if constexpr (std::is_invocable_v<F, decltype(*it)>)
							f(*it);
						else
							static_assert(false, "f must be invocable with (T), (T, size_t) or (T, size_t, size_t)");
					}
					
					auto prev_completed = completed.fetch_add(1, std::memory_order_relaxed);
					progress(prev_completed + 1);
				}
				catch (...)
				{
					[[assume(!abort.load(std::memory_order_relaxed))]];
					if (!abort.load(std::memory_order_relaxed))
					{
						std::lock_guard<std::mutex> lock(ex_mutex);
						if (!ex_ptr)
						{
							ex_ptr = std::current_exception();
							abort.store(true, std::memory_order_relaxed);
						}
					}
				}
			});

			chunk_begin = chunk_end;
		}

		for (auto& fut : futures)
		{
			try 
			{ 
				fut.get(); 
			}
			catch (...)
			{
				[[assume(!abort.load(std::memory_order_relaxed))]];
				if (!abort.load(std::memory_order_relaxed))
				{
					std::lock_guard<std::mutex> lock(ex_mutex);
					if (!ex_ptr)
					{
						ex_ptr = std::current_exception();
						abort.store(true, std::memory_order_relaxed);
					}
				}
			}
		}

		std::lock_guard<std::mutex> final_lock(ex_mutex);
		if (ex_ptr) 
			std::rethrow_exception(ex_ptr);
	}

	/**
	 * \brief Overload of async_for_each without progress callback.
	 */
	template<typename I, typename F>
	inline void async_for_each(I begin, I end, F&& f, size_t threads = runtime_threads()) 
	{
		async_for_each(begin, end, std::forward<F>(f), threads, [](size_t) {});
	}

	/**
	 * \brief Create stepped integer sequence at compile-time
	 */
	template <typename T = std::size_t, T N0, T Nn, T Ns = 1, T... Is>
	constexpr auto make_stepped_sequence_t(std::integer_sequence<T, Is...>) 
		-> std::integer_sequence<T, (N0 + Is * Ns)...>;

	/**
	 * \brief SFINAE generator of stepped sequence
	 */
	template <typename T, size_t N0, size_t Nn, size_t Ns = 1>
	using make_stepped_sequence = decltype(make_stepped_sequence_t<T, N0, Nn, Ns>(
		std::make_integer_sequence<T, (Nn - N0 + Ns - 1) / Ns>{}));

	/**
	 * \brief Defines a compile-time chunk from a sequence
	 */
	template <typename T, size_t N0, size_t Nn, size_t Ns, size_t Ci, size_t Cn>
	struct chunk_of_sequence 
	{
		static constexpr size_t total = (Nn - N0 + Ns - 1) / Ns;
		static constexpr T chunk_size = (total + Cn - 1) / Cn;
		static constexpr T offset = Ci * chunk_size;
		static constexpr T count = (offset + chunk_size > total) ? (total - offset) : chunk_size;
		static constexpr T chunk_start = N0 + offset * Ns;
		static constexpr T chunk_end   = chunk_start + count * Ns;

		using type = make_stepped_sequence<T, chunk_start, chunk_end, Ns>;
	};

	/**
	 * \brief Apply callback at compile time for each element in a stepped sequence.
	 */
	template <typename T, T Ns, T... Is, typename F>
	inline void 
	for_each_index(std::integer_sequence<T, Is...>, F&& f, size_t thread_id) 
	{
		(
			[&]<T I>() 
			{
				if constexpr (std::is_invocable_v<F, T>)
					f(I + Ns);
				else if constexpr (std::is_invocable_v<F, T, size_t>)
					f(I + Ns, thread_id); 
				else static_assert(std::is_invocable_v<F, T> || std::is_invocable_v<F, T, size_t>, 
						"`f` must be invocable with either (T) or (T, size_t)");
			}.template operator()<Is>(), ...
		);
	}

	/**
	 * \brief async for_each kernel using compile-time indexing.
	 */
	template <typename T, T N0, T Nn, T Ns = 1, typename F, size_t... Is>
	inline void 
	async_for_each_index(F&& f, std::index_sequence<Is...>)
	{
		static constexpr size_t chunk_count = sizeof...(Is);
		
		alignas(64) std::atomic<bool> abort{false};

		std::exception_ptr ex_ptr = nullptr;
		std::mutex ex_mutex;

		auto futures = std::make_tuple(
			call_async([=, &f, &abort, &ex_ptr, &ex_mutex]() 
			{
				#ifdef __linux__
					cpu_set_t cpuset;
					CPU_ZERO(&cpuset);
					CPU_SET(Is % std::thread::hardware_concurrency(), &cpuset);
					pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
				#endif

				try
				{
					[[assume(!abort.load(std::memory_order_relaxed))]];
					if (abort.load(std::memory_order_relaxed)) return;
					
					using chunk = typename chunk_of_sequence<T, N0, Nn, Ns, Is, chunk_count>::type;
					constexpr T offset = chunk_of_sequence<T, N0, Nn, Ns, Is, chunk_count>::chunk_start;
					for_each_index<T, offset>(chunk{}, f, Is);
				}
				catch (...)
				{
					[[assume(!abort.load(std::memory_order_relaxed))]];
					if (!abort.load(std::memory_order_relaxed))
					{
						std::lock_guard<std::mutex> lock(ex_mutex);
						if (!ex_ptr)
						{
							ex_ptr = std::current_exception();
							abort.store(true, std::memory_order_relaxed);
						}
					}
				}
			})...
		);

		( [&] {
				try 
				{ 
					std::get<Is>(futures).get(); 
				}
				catch (...) 
				{
					[[assume(!abort.load(std::memory_order_relaxed))]];
					if (!abort.load(std::memory_order_relaxed))
					{
						std::lock_guard<std::mutex> lock(ex_mutex);
						if (!ex_ptr)
						{
							ex_ptr = std::current_exception();
							abort.store(true, std::memory_order_relaxed);
						}
					}
				}
		}(), ...);

		std::lock_guard<std::mutex> final_lock(ex_mutex);
		if (ex_ptr) 
			std::rethrow_exception(ex_ptr);
	}

	/**
	 * \brief Public interface for compile-time indexed async loop.
	 * \tparam T Index type (usually size_t)
	 * \tparam N0 Start index (inclusive)
	 * \tparam Nn End index (exclusive)
	 * \tparam Ns Step size
	 * \tparam threads Number of parallel threads
	 * \tparam F Callable type
	 * \param f Function to call for each index
	 */
	template <typename T, const T N0, const T Nn, const T Ns, const size_t threads = threads, typename F>
	inline void 
	async_for_each(F&& f)
	{
		async_for_each_index<T, N0, Nn, Ns>(
			std::forward<F>(f),
			std::make_index_sequence<threads>{}
		);
	}
}//namespace async

#endif // __ASYNC_H__