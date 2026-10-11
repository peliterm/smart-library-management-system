// ============================================================================
// DueDateHeap.cpp
// Smart Library Management System - CPSC 315
// ============================================================================
#include "DueDateHeap.h"
#include <algorithm>

// ---------------------------------------------------------------------------
// swapNodes: the only place array slots move. Both the array and the index
// map are updated here so they cannot drift apart.
// ---------------------------------------------------------------------------
void DueDateHeap::swapNodes(size_t a, size_t b) {
    if (a == b) return;
    std::swap(data[a], data[b]);
    indexOf[data[a].loanId] = a;
    indexOf[data[b].loanId] = b;
}

// ---------------------------------------------------------------------------
// siftUp: moves a node toward the root while it is due earlier than its
// parent. Used after an insertion at the end of the array.
// ---------------------------------------------------------------------------
void DueDateHeap::siftUp(size_t i) {
    while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (data[i].dueDay < data[parent].dueDay) {
            swapNodes(i, parent);
            i = parent;
        } else {
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// siftDown: moves a node toward the leaves while either child is due
// earlier. Used after a removal pulls the last element to that position.
// ---------------------------------------------------------------------------
void DueDateHeap::siftDown(size_t i) {
    const size_t n = data.size();
    while (true) {
        size_t left     = 2 * i + 1;
        size_t right    = 2 * i + 2;
        size_t smallest = i;

        if (left  < n && data[left].dueDay  < data[smallest].dueDay) smallest = left;
        if (right < n && data[right].dueDay < data[smallest].dueDay) smallest = right;

        if (smallest == i) break;
        swapNodes(i, smallest);
        i = smallest;
    }
}

bool DueDateHeap::push(const LoanEntry& entry) {
    if (indexOf.find(entry.loanId) != indexOf.end()) {
        return false;                       // duplicate loan ID
    }
    data.push_back(entry);
    indexOf[entry.loanId] = data.size() - 1;
    siftUp(data.size() - 1);
    return true;
}

const LoanEntry* DueDateHeap::peekMin() const {
    return data.empty() ? nullptr : &data[0];
}

bool DueDateHeap::popMin(LoanEntry& out) {
    if (data.empty()) return false;

    out = data[0];
    indexOf.erase(data[0].loanId);

    if (data.size() == 1) {
        data.pop_back();
        return true;
    }

    // Move the last element to the root, shrink, then restore the invariant.
    data[0] = data.back();
    data.pop_back();
    indexOf[data[0].loanId] = 0;
    siftDown(0);
    return true;
}

// ---------------------------------------------------------------------------
// removeLoan: the operation the index map exists for.
//
// Without the map, locating the loan is an O(n) scan. With it, the position
// is known immediately. The removed slot is filled by the last element,
// which then sifts either direction depending on how its due date compares
// to its new neighbors.
// ---------------------------------------------------------------------------
bool DueDateHeap::removeLoan(int loanId) {
    auto it = indexOf.find(loanId);
    if (it == indexOf.end()) return false;

    size_t idx  = it->second;
    size_t last = data.size() - 1;

    indexOf.erase(it);

    if (idx == last) {                      // removing the final element
        data.pop_back();
        return true;
    }

    data[idx] = data[last];                 // fill the gap
    data.pop_back();
    indexOf[data[idx].loanId] = idx;

    // The replacement may belong higher or lower than where it landed.
    if (idx > 0 && data[idx].dueDay < data[(idx - 1) / 2].dueDay) {
        siftUp(idx);
    } else {
        siftDown(idx);
    }
    return true;
}

const LoanEntry* DueDateHeap::find(int loanId) const {
    auto it = indexOf.find(loanId);
    return (it == indexOf.end()) ? nullptr : &data[it->second];
}

// ---------------------------------------------------------------------------
// overdueAsOf: pops from a copy so the live heap is untouched. Stops at the
// first loan that is not yet due, since the heap guarantees everything after
// it is due later.
// ---------------------------------------------------------------------------
std::vector<LoanEntry> DueDateHeap::overdueAsOf(int today) const {
    DueDateHeap scratch = *this;            // copy: vector and map both copy
    std::vector<LoanEntry> out;
    LoanEntry entry;

    while (scratch.peekMin() != nullptr && scratch.peekMin()->dueDay <= today) {
        scratch.popMin(entry);
        out.push_back(entry);
    }
    return out;
}

bool DueDateHeap::validate() const {
    // 1. Heap property: no parent is due later than a child.
    for (size_t i = 0; i < data.size(); ++i) {
        size_t left  = 2 * i + 1;
        size_t right = 2 * i + 2;
        if (left  < data.size() && data[left].dueDay  < data[i].dueDay) return false;
        if (right < data.size() && data[right].dueDay < data[i].dueDay) return false;
    }

    // 2. Index map agrees with the array, in both directions.
    if (indexOf.size() != data.size()) return false;
    for (size_t i = 0; i < data.size(); ++i) {
        auto it = indexOf.find(data[i].loanId);
        if (it == indexOf.end() || it->second != i) return false;
    }
    return true;
}
