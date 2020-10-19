#ifndef PAGINATOR_H
#define PAGINATOR_H

#include <iostream>
#include <vector>

template <typename Iterator>
class IteratorRange {
public:
    explicit IteratorRange(Iterator begin, Iterator end)
        : begin_(begin)
        , end_(end)
        , size_(end - begin) {

    }

    Iterator begin() const {
        return begin_;
    }

    Iterator end() const {
        return end_;
    }

    size_t size() const {
        return size_;
    }

private:
    Iterator begin_;
    Iterator end_;
    size_t size_;
};

template <typename Iterator>
std::ostream& operator<< (std::ostream& out, const IteratorRange<Iterator>& range) {
    for (auto it : range) {
        out << it;
    }
    return out;
}

template <typename Iterator>
class Paginator {
public:
    explicit Paginator(Iterator begin, Iterator end, int size) {
        while (begin != end) {
            int dist = end - begin;
            int shift = (dist < size) ? dist : size;
            pages_.push_back(IteratorRange(begin, begin + shift));
            begin = begin + shift;
        }
    }

    auto begin() const {
        return pages_.begin();
    }

    auto end() const {
        return pages_.end();
    }

    size_t size() const {
        return pages_.size();
    }

private:
    std::vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

#endif // PAGINATOR_H
