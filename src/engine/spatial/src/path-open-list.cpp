#include <algorithm>
#include <engine/spatial/path-open-list.h>
#include <functional>

namespace eng::spatial {

void PathOpenList::clear(uint32_t first_total) {
  for (std::vector<uint64_t>& bucket : buckets_) {
    bucket.clear();
  }
  total_ = first_total;
  size_ = 0;
}

void PathOpenList::push(uint32_t total, uint32_t rest, uint32_t index) {
  std::vector<uint64_t>& bucket = buckets_[total % PATH_OPEN_SPAN];
  bucket.push_back((static_cast<uint64_t>(rest) << 32U) | index);
  std::ranges::push_heap(bucket, std::greater{});
  ++size_;
}

bool PathOpenList::empty() const {
  return size_ == 0;
}

uint32_t PathOpenList::take() {
  std::vector<uint64_t>& bucket = firstBucket();
  std::ranges::pop_heap(bucket, std::greater{});
  const auto index = static_cast<uint32_t>(bucket.back());
  bucket.pop_back();
  --size_;
  return index;
}

std::vector<uint64_t>& PathOpenList::firstBucket() {
  while (buckets_[total_ % PATH_OPEN_SPAN].empty()) {
    ++total_;
  }
  return buckets_[total_ % PATH_OPEN_SPAN];
}

}  // namespace eng::spatial
