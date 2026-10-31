#pragma once
#include <functional>
#include <memory>

// One cooperative guest stack. The parent owns dispatch; yield retains every
// native frame. Cancellation resumes the stack to unwind C++ guards normally.
class RetainedEeStack {
public:
    enum class State { Ready, Running, Waiting, Completed, Failed, Cancelled };
    explicit RetainedEeStack(std::function<void()> entry);
    ~RetainedEeStack();
    RetainedEeStack(const RetainedEeStack&) = delete;
    RetainedEeStack& operator=(const RetainedEeStack&) = delete;
    void resume();
    void yield();
    State state() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
