#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>

struct ConsoleLogEntry {
    std::string message;
    spdlog::level::level_enum level;
    std::chrono::system_clock::time_point timestamp;
};

class ConsoleLogStore {
public:
    static constexpr size_t kDefaultCapacity = 2000;

    explicit ConsoleLogStore(size_t capacity = kDefaultCapacity) : head_(0), count_(0) {
        entries_.resize(capacity);
    }

    ConsoleLogStore(const ConsoleLogStore&) = delete;
    ConsoleLogStore& operator=(const ConsoleLogStore&) = delete;

    void addEntry(std::string message, spdlog::level::level_enum level) {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_[head_] = ConsoleLogEntry{std::move(message), level, std::chrono::system_clock::now()};
        head_ = (head_ + 1) % entries_.size();
        if (count_ < entries_.size()) {
            ++count_;
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        head_ = 0;
        count_ = 0;
    }

    size_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

    ConsoleLogEntry getEntry(size_t index) const {
        std::lock_guard<std::mutex> lock(mutex_);
        const size_t capacity = entries_.size();
        const size_t pos = (head_ - count_ + index + capacity) % capacity;
        return entries_[pos];
    }

    size_t capacity() const { return entries_.size(); }

private:
    std::vector<ConsoleLogEntry> entries_;
    size_t head_;
    size_t count_;
    mutable std::mutex mutex_;
};

template<typename Mutex>
class EditorConsoleSink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit EditorConsoleSink(std::shared_ptr<ConsoleLogStore> store) : store_(std::move(store)) {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);
        std::string text(formatted.data(), formatted.size());
        if (!text.empty() && text.back() == '\n') {
            text.pop_back();
        }
        store_->addEntry(std::move(text), msg.level);
    }

    void flush_() override {}

private:
    std::shared_ptr<ConsoleLogStore> store_;
};
