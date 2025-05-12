#include <iostream>
#include <vector>
#include <atomic>
#include <functional>
#include <chrono>
#include <cassert>
#include <stdexcept>
#include <async.h>

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

#define TEST_SIZE 2048
using namespace async;

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
    constexpr int NUM_RUNS = 100;

    double dynamic_result = 0.0, compile_result = 0.0, exception_result = 0.0;
    double dynamic_time = 0.0, compile_time = 0.0, exception_time = 0.0;

    try
    {
        dynamic_time = dispatch(test_dynamic_dispatch, dynamic_result, NUM_RUNS);
        std::cout << "[OK] dynamic_dispatch (avg over " << NUM_RUNS << " runs): " << dynamic_time << " s\n" << std::endl;

        compile_time = dispatch(test_compile_time_dispatch, compile_result, NUM_RUNS);
        std::cout << "[OK] compile_time_dispatch (avg over " << NUM_RUNS << " runs): " << compile_time << " s\n" << std::endl;

        // Exception test is run only once
        exception_time = dispatch(test_exception_handling, exception_result, 1);
        std::cout << "[OK] exception_handling (single run): " << exception_time << " s\n" << std::endl;

        double ratio = dynamic_time / compile_time;
        std::cout << "[INFO] Performance Ratio (dynamic / compile-time): " << ratio << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FAIL] A benchmark threw an exception: " << e.what() << "\n" << std::endl;
    }

    return 0;
}
