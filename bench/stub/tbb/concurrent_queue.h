/* SPDX-License-Identifier: MIT */
/* Заглушка tbb::concurrent_queue для сборки ips4o без TBB (последовательный режим). */
#pragma once
#include <deque>
#include <mutex>
namespace tbb {
template <class T> class concurrent_queue {
  std::deque<T> q; std::mutex m;
 public:
  void push(const T &v) { std::lock_guard<std::mutex> g(m); q.push_back(v); }
  bool try_pop(T &v) { std::lock_guard<std::mutex> g(m); if (q.empty()) return false; v = q.front(); q.pop_front(); return true; }
  bool empty() const { return q.empty(); }
  void clear() { q.clear(); }
};
}
