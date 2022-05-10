#pragma once
#include <mutex>
#include <condition_variable>

namespace mqp {

    class Semaphore {
        std::size_t count_;
        std::mutex mutex_;
        std::condition_variable cv_;

    public:
        Semaphore(std::size_t count = 0) noexcept
            : count_{ count } { }

        void post() noexcept {
            {
                std::unique_lock lock{ mutex_ };
                ++count_;
            }
            cv_.notify_one();
        }

        void wait() noexcept {
            std::unique_lock lock{ mutex_ };
            cv_.wait(lock, [&]{ return count_ != 0; });
            --count_;
        }
    }; 

} // namespace mqp
