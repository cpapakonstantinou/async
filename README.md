# async.h

**Header-only parallel for-each library using `std::async` in C++**

> Lightweight task-based parallelism using modern C++ (C++20/23). Supports both dynamic runtime dispatch and compile-time loop unrolling.

* Parallel `for_each` dynamic dispatch over iterators
* Compile-time `for_each` dispatch over sequences
* Per-thread CPU pinning (Linux only)
* Optional progress reporting

## Dynamic `async_for_each` dispatch over containers
 
 > Basic usage 

```
	#include "async.h"

	std::vector<float> data(1000, 42);

	async::async_for_each(data.begin(), data.end(), [](float& x) { ... });  
```

### Supported signatures

> The lambda passed to `async_for_each` can have the following signatures:

| Signature                                      | Description                                |
| ---------------------------------------------- | ------------------------------------------ |
| f(T& value)                                    | The operand                                |
| f(T& value, size_t index)                      | Exposes the index of the operand           |
| f(T& value, size_t index, size_t thread_id)    | Exposes the thread id managing the operand |

> Exposing the thread id

```
	async::async_for_each(data.begin(), data.end(),
	[](int& x, size_t idx, size_t thread_id) 
	{
		std::cout << "Thread " << thread_id << " processing index " << idx << "\n";
	});
```

## Compile-Time `async_for_each` dispatch over sequences

> This version unrolls and partitions the index space at compile time.

```	
	async::async_for_each<size_t, 0, 100, 1>(
	[](size_t i) 
	{
		std::cout << "index: " << i << "\n";
	});
```

### Supported signatures

 The lambda passed to `async_for_each` can have the following signatures:

| Signature                    | Description                                |
| ---------------------------- | ------------------------------------------ |
| f(T index)                   | Basic index access                         |
| f(T index, size_t thread_id) | Access with thread ID                      |

Example with thread ID:

```
	async::async_for_each<size_t, 0, 100, 2>(
	[](size_t i, size_t thread_id) 
	{
		std::cout << "Thread " << thread_id << " processing index " << i << "\n";
	});
```

## Etc

### Thread Pinning (Linux only)

On Linux, threads are pinned to CPU cores to improve cache locality.

### Exception Safety

Exceptions in any thread are caught and rethrown in the calling thread.
Execution aborts early on first exception.

### Progress tracking
You can track how many threads have completed their chunk:

```
	std::atomic<size_t> progress = 0;

	async::async_for_each(data.begin(), data.end(), [](int& x) { x *= 2; }, async::threads,
	[&](size_t completed) 
	{
		std::cout << "Completed threads: " << completed << "\n";
	});

```