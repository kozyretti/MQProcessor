#pragma once
#include <list>
#include <mutex>
#include <thread>
#include <atomic>
#include <shared_mutex>
#include <unordered_map>
#include "semaphore.h"

namespace mqp {

    template <typename Key, typename Value>
    class IConsumer {
    public:
        virtual ~IConsumer() = default;
        virtual void Consume(const Key& id, const Value& value) noexcept = 0;
    };

    template <typename Key, typename Value, bool EjectingStrategy = true>
    class MQProcessor {
    public:
        MQProcessor(const std::size_t maxChannelSize = 1000)
          : maxChannelSize_{maxChannelSize} { }

        ~MQProcessor() {
            running_ = false;
            semaphore_.post();
            if (th_.joinable()) {
                th_.join();
            }
        }
        MQProcessor(const MQProcessor&) = delete;
        MQProcessor(MQProcessor&&) = delete;
        MQProcessor& operator=(const MQProcessor&) = delete;
        MQProcessor& operator=(MQProcessor&&) = delete;

        using IConsumer_ptr = std::shared_ptr<IConsumer<Key, Value>>;

        void Subscribe(const Key& id, const IConsumer_ptr& consumer) {
            do_operation_in_channel_(id, [&consumer](auto& chanel) {
                chanel.reset_consumer(consumer); });
        }

        void Unsubscribe(const Key& id) {
            do_operation_in_channel_<false>(id, [](auto& chanel) {
                chanel.reset_consumer(nullptr); });
        }

        bool Enqueue(const Key& id, const Value& value) {
            return do_operation_in_channel_(id, [this, &value](auto& channel) {
                bool res = channel.push(value);
                semaphore_.post();
                return res; });
        }

    private:
        class Channel {
            std::mutex mutex_;
            std::list<Value> queue_;
            IConsumer_ptr consumer_;
            const std::size_t maxChannelSize_;

        public:
            Channel(std::size_t maxChannelSize)
              : maxChannelSize_{maxChannelSize} { }

            void reset_consumer(const IConsumer_ptr& consumer) {
                std::scoped_lock lock{mutex_};
                consumer_ = consumer;
            }

            bool push(const Value& value) {
                std::scoped_lock lock{mutex_};
                if (queue_.size() >= maxChannelSize_) {
                    if constexpr (!EjectingStrategy) {
                        return false;
                    }
                    queue_.pop_front();
                }
                queue_.emplace_back(value);
                return true;
            }

            void consume(const Key& id) {
                if (!queue_.empty() && consumer_) {
                    std::scoped_lock lock{mutex_};
                    if (!queue_.empty() && consumer_) {
                        consumer_->Consume(id, queue_.front());
                        queue_.pop_front();
                    }
                }
            }
        };

        template <bool Create = true, class Func>
        auto do_operation_in_channel_(const Key& id, Func func) {
            {
                std::shared_lock lock{mutex_};
                auto it = channels_.find(id);
                if (it != channels_.end()) {
                    return func(it->second);
                }
            }
            if constexpr (Create)
            {
                std::scoped_lock lock{mutex_};
                auto [it, _] = channels_.try_emplace(id, maxChannelSize_);
                return func(it->second);
            }
        }

        void Process() {
            while (running_) {
                semaphore_.wait();
                std::shared_lock lock{mutex_};
                for (auto& node : channels_) {
                    node.second.consume(node.first);
                }
                std::this_thread::yield();
            }
        }

        const std::size_t maxChannelSize_;

        std::shared_mutex mutex_;
        std::unordered_map<Key, Channel> channels_;

        Semaphore semaphore_;
        std::atomic_bool running_{true};
        std::thread th_{ [this]{ Process(); } };
    };

} // namespace mqp
