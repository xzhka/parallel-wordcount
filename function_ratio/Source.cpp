// function_ratio: evaluates f(a) and f(b) on two threads and prints f(a)/f(b).
//
// f(x) = (sqrt(x) + factorial(x)) / x
//   sqrt(x)      - Newton's method
//   factorial(x) - n * (n-1) * ... until the factor drops to <= 1
//                  (a product for non-integer x, NOT the gamma function)
//
// The two calls are independent, so they need no mutex: each thread writes its
// own variable, and main reads them only after join().
// The workload is tiny (microseconds), so this demonstrates the
// start-both / join-both / then-read pattern, not a speedup.
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stack>
#include <thread>

typedef std::chrono::duration<double, std::micro> USEC;
typedef std::chrono::steady_clock HRC;

long double findSqrt(long double x)
{
	if (x < 0)
		return NAN;               // no real square root
	if (x == 0 || x == 1)
		return x;                 // also avoids dividing by zero below

	long double y = x;
	long double z = (y + (x / y)) / 2;

	while (std::fabs(y - z) >= 0.00001L) {
		y = z;
		z = (y + (x / y)) / 2;
	}
	return z;
}

long double factorial(long double n)
{
	std::stack<long double> factorial;
	long double res{ 1 };
	while (true)
	{
		if (n <= 1.0L)
		{
			if (factorial.empty())
			{
				break;
			}
			res *= factorial.top();
			factorial.pop();
		}
		else
		{
			factorial.push(n);
			n--;
		}
	}
	return res;
}


long double f(long double x)
{
	return (findSqrt(x) + factorial(x)) / x;
}


int main()
{
	long double a = 75.6L, b = 50.96L;

	long double res_a{ 0 }, res_b{ 0 };
	auto startTime = HRC::now();
	std::thread tA([&]() { res_a = f(a); });   // writes only res_a
	std::thread tB([&]() { res_b = f(b); });   // writes only res_b
	tA.join();                                 // after join() the results are safe to read
	tB.join();
	auto endTime = HRC::now();

	std::cout << std::setprecision(12);
	std::cout << "f(a) = " << res_a << std::endl;
	std::cout << "f(b) = " << res_b << std::endl;
	std::cout << "f(a) / f(b) = " << res_a / res_b << std::endl;

	// Check: the same calls on one thread must give identical results.
	const bool ok = (res_a == f(a)) && (res_b == f(b));
	std::cout << "Matches sequential: " << (ok ? "yes" : "NO") << std::endl;

	std::cout << "Execution time " << std::chrono::duration_cast<USEC>(endTime - startTime).count() << " usec" << std::endl;
	return ok ? 0 : 1;
}
