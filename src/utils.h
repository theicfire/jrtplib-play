#pragma once

#include <chrono>
#include <comdef.h>
#include <Windows.h>

// Could be useful later for debugging: http://alax.info/blog/1383
template< typename T >
void ThrowIfFailed(HRESULT hr, T&& msg)
{
	if (FAILED(hr)) {
		_com_error err = hr;
		LPCTSTR errMsg = err.ErrorMessage();
		std::wcout << errMsg << std::endl;

		throw std::system_error{ hr, std::system_category(), std::forward<T>(msg) };
	}
}

class Timer {
public:
    Timer() {
        this->reset();
    }

    void reset() {
        this->time  = std::chrono::high_resolution_clock::now();
    }

    double getElapsedSeconds() const {
        return 1.0e-6 * std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - this->time).count();
    }

    double getElapsedMilliseconds() const {
        return 1.0e-3 * std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - this->time).count();
    }

    long getElapsedMicroseconds() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - this->time).count();
    }

private:
    std::chrono::high_resolution_clock::time_point time;
};