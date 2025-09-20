#include <iostream>
#include <vector>
#include <atomic>
#include <functional>
#include <chrono>
#include <cassert>
#include <stdexcept>
#include <cmath>
#include <async.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/global_control.h>

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
#ifndef TEST_SIZE
	#define TEST_SIZE 2048
#endif 

using namespace async;

// Original async_for_each tests
double test_dynamic_dispatch() 
{
	std::vector<size_t> numbers(TEST_SIZE, 0);
	std::atomic<size_t> counter{0};

	async_for_each(numbers.begin(), numbers.end(),
		[&counter](size_t& val, size_t idx)
		{
			val = idx;
			counter++;
		});

	assert(counter.load() == TEST_SIZE);
	return 0;
}

double test_compile_time_dispatch() 
{
	std::atomic<size_t> counter{0};

	async_for_each<size_t, 0, TEST_SIZE, 1>(
		[&counter](size_t idx)
		{
			counter++;
		});

	assert(counter.load() == TEST_SIZE);
	return 0;
}

double test_exception_handling() 
{
	std::vector<size_t> numbers(TEST_SIZE, 0);

	try
	{
		async_for_each(numbers.begin(), numbers.end(),
			[](size_t& val, size_t idx)
			{
				if (idx == TEST_SIZE / 2)
				{
					throw std::runtime_error("test exception");
				}
				val = idx;
			});
		assert(false && "Expected exception was not thrown");
	}
	catch (const std::runtime_error& e)
	{
		std::cout << "Caught expected exception: " << e.what() << std::endl;
	}

	return 0.0;
}

// TBB equivalent tests
double test_tbb_blocked_range() 
{
	std::vector<size_t> numbers(TEST_SIZE, 0);
	std::atomic<size_t> counter{0};

	tbb::parallel_for(
		tbb::blocked_range<size_t>(0, TEST_SIZE),
		[&](const tbb::blocked_range<size_t>& range) {
			for (size_t i = range.begin(); i < range.end(); ++i) {
				numbers[i] = i;
				counter++;
			}
		}
	);

	assert(counter.load() == TEST_SIZE);
	return 0;
}

double test_tbb_simple_range() 
{
	std::atomic<size_t> counter{0};

	tbb::parallel_for(size_t(0), size_t(TEST_SIZE), [&counter](size_t idx) {
		counter++;
	});

	assert(counter.load() == TEST_SIZE);
	return 0;
}

double test_tbb_exception_handling() 
{
	std::vector<size_t> numbers(TEST_SIZE, 0);

	try
	{
		tbb::parallel_for(
			tbb::blocked_range<size_t>(0, TEST_SIZE),
			[&](const tbb::blocked_range<size_t>& range) {
				for (size_t i = range.begin(); i < range.end(); ++i) {
					if (i == TEST_SIZE / 2) {
						throw std::runtime_error("test exception");
					}
					numbers[i] = i;
				}
			}
		);
		assert(false && "Expected exception was not thrown");
	}
	catch (const std::runtime_error& e)
	{
		std::cout << "Caught expected exception: " << e.what() << std::endl;
	}

	return 0.0;
}

// Computational workload tests for better performance comparison
double test_async_computational_work()
{
	std::vector<double> data(TEST_SIZE);
	std::atomic<size_t> counter{0};

	async_for_each(data.begin(), data.end(),
		[&counter](double& val, size_t idx) {
			// Simulate some computational work
			val = 0.0;
			for (int i = 0; i < 100; ++i) {
				val += std::sin(static_cast<double>(idx + i));
			}
			counter++;
		});

	assert(counter.load() == TEST_SIZE);
	return 0;
}

double test_tbb_computational_work()
{
	std::vector<double> data(TEST_SIZE);
	std::atomic<size_t> counter{0};

	tbb::parallel_for(
		tbb::blocked_range<size_t>(0, TEST_SIZE),
		[&](const tbb::blocked_range<size_t>& range) {
			for (size_t idx = range.begin(); idx < range.end(); ++idx) {
				// Simulate some computational work
				data[idx] = 0.0;
				for (int i = 0; i < 100; ++i) {
					data[idx] += std::sin(static_cast<double>(idx + i));
				}
				counter++;
			}
		}
	);

	assert(counter.load() == TEST_SIZE);
	return 0;
}

double dispatch(std::function<double(void)> f, double& arg_out, int num_runs)
{
	using clock = std::chrono::high_resolution_clock;

	double total_time = 0.0;
	volatile double sink = 0.0;

	for (int i = 0; i < num_runs; ++i)
	{
		auto start = clock::now();
		double result = f();  // Call benchmark
		auto end = clock::now();

		sink += result;      // Prevent optimization
		arg_out = result;    // Store last result

		total_time += std::chrono::duration<double>(end - start).count();
	}

	return total_time / num_runs;
}

int main(int argc, char* argv[])
{
	constexpr int NUM_RUNS = 1000;
	tbb::global_control global_limit(tbb::global_control::max_allowed_parallelism, async::threads);

	std::cout << "Threading Performance Comparison" << std::endl;
	std::cout << "--------------------------------" << std::endl;
	std::cout << "Threads: " << async::threads << std::endl;
	std::cout << "Test size: " << TEST_SIZE << " elements" << std::endl;
	std::cout << "Runs per test: " << NUM_RUNS << std::endl << std::endl;
	std::cout << std::endl;

	double result = 0.0;
	double time = 0.0;

	try
	{
		// async_for_each tests
		std::cout << "async_for_each Tests" << std::endl;
		std::cout << "--------------------" << std::endl;
		
		time = dispatch(test_dynamic_dispatch, result, NUM_RUNS);
		std::cout << "[ASYNC] dynamic_dispatch: " << time << " s (avg)" << std::endl;
		double async_dynamic_time = time;

		time = dispatch(test_compile_time_dispatch, result, NUM_RUNS);
		std::cout << "[ASYNC] compile_time_dispatch: " << time << " s (avg)" << std::endl;
		double async_compile_time = time;

		time = dispatch(test_async_computational_work, result, NUM_RUNS);
		std::cout << "[ASYNC] computational_work: " << time << " s (avg)" << std::endl;
		double async_compute_time = time;
		std::cout << std::endl;

		// TBB tests
		std::cout << "TBB Tests" << std::endl;
		std::cout << "---------" << std::endl;
		
		time = dispatch(test_tbb_blocked_range, result, NUM_RUNS);
		std::cout << "[TBB] blocked_range: " << time << " s (avg)" << std::endl;
		double tbb_blocked_time = time;

		time = dispatch(test_tbb_simple_range, result, NUM_RUNS);
		std::cout << "[TBB] simple_range: " << time << " s (avg)" << std::endl;
		double tbb_simple_time = time;

		time = dispatch(test_tbb_computational_work, result, NUM_RUNS);
		std::cout << "[TBB] computational_work: " << time << " s (avg)" << std::endl;
		double tbb_compute_time = time;
		std::cout << std::endl;

		// Exception handling tests (single run)
		std::cout << "Exception Handling Tests" << std::endl;
		std::cout << "------------------------" << std::endl;
		
		time = dispatch(test_exception_handling, result, 1);
		std::cout << "[ASYNC] exception_handling: " << time << " s" << std::endl;

		time = dispatch(test_tbb_exception_handling, result, 1);
		std::cout << "[TBB] exception_handling: " << time << " s" << std::endl;
		std::cout << std::endl;
		// Performance comparison
		std::cout << "Performance Ratios (async/TBB) " << std::endl;
		std::cout << "----------------------------------" << std::endl;
		std::cout << "Dynamic vs Blocked Range: " << (async_dynamic_time / tbb_blocked_time) << std::endl;
		std::cout << "Compile-time vs Simple Range: " << (async_compile_time / tbb_simple_time) << std::endl;
		std::cout << "Computational Work: " << (async_compute_time / tbb_compute_time) << std::endl;
		std::cout << std::endl;

		std::cout << "Summary" << std::endl;
		std::cout << "-------" << std::endl;
		if (async_compute_time < tbb_compute_time) 
		{
			std::cout << "async_for_each is faster: " << ((tbb_compute_time - async_compute_time) / tbb_compute_time * 100) << "%" << std::endl;
		} 
		else 
		{
			std::cout << "TBB is faster:" << ((async_compute_time - tbb_compute_time) / async_compute_time * 100) << "%" << std::endl;
		}
	  
	}
	catch (const std::exception& e)
	{
		std::cerr << "[FAIL] A benchmark threw an exception: " << e.what() << std::endl;
	}

	return 0;
}