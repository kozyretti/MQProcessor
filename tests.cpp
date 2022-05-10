#include <gtest/gtest.h>
#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>
#include "MQProcessor.h"

using namespace mqp;
using namespace std::chrono_literals;
using std::this_thread::sleep_for;

static std::mt19937 gen{ std::random_device{}() };


template<typename Key, typename Value>
class Consumer : public IConsumer<Key, Value> {

    using container = std::vector<Value>;

    const Key id_;
    const container vals_;
    typename container::const_iterator it_;
    std::atomic_bool done_{ false };

public:
    Consumer(const Key& id)
      : id_{ id }, it_{ vals_.begin() } { }

    Consumer(const Key& id, container&& vals)
      : id_{ id }, vals_{ std::move(vals) }, it_{ vals_.begin() } { }

    const Key& get_id() const {
        return id_;
    }

    const container& get_vals() const {
        return vals_;
    }

    void Consume(const Key& id, const Value& value) noexcept override {
        //std::cout << "Consume (" << id << "): " << value << std::endl;
        ASSERT_EQ(id_, id);
        ASSERT_EQ(*it_++, value);
        sleep_for(20ms);
        if (it_ == vals_.end()) {
            done_ = true;
        }
    }

    void wait_until_done() {
        while (!done_) {
            std::this_thread::yield();
        }
    }
};

using ConsumerInt = Consumer<int, int>;

TEST(MultiQueueProcessorTest, ejecting_strategy) {
    MQProcessor<int, int> processor{ 5 };
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(processor.Enqueue(0, i));
    }
}

TEST(MultiQueueProcessorTest, non_ejecting_strategy) {
    MQProcessor<int, int, false> processor{ 5 };
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(processor.Enqueue(0, i));
    }
    for (int i = 0; i < 5; ++i) {
        EXPECT_FALSE(processor.Enqueue(0, i));
    }
}

TEST(MultiQueueProcessorTest, wo_consumers) {
    MQProcessor<int, int> processor;
    processor.Enqueue(1, 1);
    processor.Enqueue(2, 2);
    processor.Enqueue(3, 3);
    sleep_for(1s);
}

TEST(MultiQueueProcessorTest, subscribe_unsubscribe_consumers) {
    MQProcessor<int, int> processor;

    int consumers_num = 20;
    std::vector<std::shared_ptr<ConsumerInt>> consumers(consumers_num);
    for (int id = 0; id < consumers.size(); ++id) {
        consumers[id] = std::make_shared<ConsumerInt>(id, std::vector<int>{});
        processor.Subscribe(id, consumers[id]);
    }

    // unsubscribe in random order
    std::shuffle(consumers.begin(), consumers.end(), gen);
    for (auto& c : consumers) {
        // not existed Unsubscribe
        processor.Unsubscribe(c->get_id() + consumers_num * 2);
        // normal Unsubscribe
        processor.Unsubscribe(c->get_id());
        // double Unsubscribe
        processor.Unsubscribe(c->get_id());
    }

    sleep_for(1s);
}

TEST(MultiQueueProcessorTest, one_consumer) {
    MQProcessor<int, int> processor;

    auto consumer = std::make_shared<ConsumerInt>(1, std::vector<int>{ 1,2,3,5,4,8 });
    processor.Subscribe(1, consumer);

    for (auto& i : consumer->get_vals()) {
        processor.Enqueue(1, i);
        sleep_for(100ms);
    }

    consumer->wait_until_done();
    processor.Unsubscribe(1);
}

TEST(MultiQueueProcessorTest, enqueue_before_one_consumer_) {
    MQProcessor<int, int> processor;

    auto consumer = std::make_shared<ConsumerInt>(1, std::vector<int>{ 1,2,3,5,4,8 });

    auto it = consumer->get_vals().begin();
    processor.Enqueue(1, *it++);
    processor.Enqueue(1, *it++);

    processor.Subscribe(1, consumer);

    while (it != consumer->get_vals().end()) {
        processor.Enqueue(1, *it++);
        sleep_for(100ms);
    }

    consumer->wait_until_done();
    processor.Unsubscribe(1);
}

TEST(MultiQueueProcessorTest, unsubscribe_before_done_one_consumer_) {
    MQProcessor<int, int> processor;

    auto consumer = std::make_shared<ConsumerInt>(1, std::vector<int>{ 1, 2, 3, 5, 4, 8 });
    processor.Subscribe(1, consumer);

    auto& vals = consumer->get_vals();
    int part = vals.size() * 0.4;
    for (int i = 0; i < part; ++i) {
        processor.Enqueue(1, vals[i]);
    }

    processor.Unsubscribe(1);

    for (int i = part; i < vals.size(); ++i) {
        processor.Enqueue(1, vals[i]);
    }

    sleep_for(1s);
}

TEST(MultiQueueProcessorTest, several_consumers) {
    int consumers_num = 20;
    int values_num = 50;

    // prepare consumers with random test data
    std::vector<std::shared_ptr<ConsumerInt>> consumers(consumers_num);
    for (int id = 0; id < consumers.size(); ++id) {
        std::vector<int> vals(values_num);
        std::generate(vals.begin(), vals.end(), gen);
        consumers[id] = std::make_shared<ConsumerInt>(id, std::move(vals));
    }
    std::vector<std::shared_ptr<ConsumerInt>> consumers_for_enqueue = consumers;

    MQProcessor<int, int> processor;

    // subscribe part of consumers
    int part = consumers_num * 0.4;
    for (int i = 0; i < part; ++i) {
        auto& consumer = consumers[i];
        processor.Subscribe(consumer->get_id(), consumer);
    }

    // enqueue first part of data
    int values_part = values_num * 0.4;
    for (int i = 0; i < values_part; ++i) {
        std::shuffle(consumers_for_enqueue.begin(), consumers_for_enqueue.end(), gen);
        for (auto& c : consumers_for_enqueue) {
            processor.Enqueue(c->get_id(), c->get_vals()[i]);
        }
    }

    // subscribe remaining consumers
    for (int i = part; i < consumers_num; ++i) {
        auto& consumer = consumers[i];
        processor.Subscribe(consumer->get_id(), consumer);
    }

    // enqueue remaining part of data
    for (int i = values_part; i < values_num; ++i) {
        std::shuffle(consumers_for_enqueue.begin(), consumers_for_enqueue.end(), gen);
        for (auto& c : consumers_for_enqueue) {
            processor.Enqueue(c->get_id(), c->get_vals()[i]);
        }
    }

    for (auto& c : consumers) {
        c->wait_until_done();
    }
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
