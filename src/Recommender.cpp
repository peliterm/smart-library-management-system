// ============================================================================
// Recommender.cpp
// Smart Library Management System - CPSC 315
// ============================================================================
#include "Recommender.h"
#include <algorithm>
#include <unordered_set>

Recommender::Recommender() : root(nullptr) {}

Recommender::~Recommender() {
    clearTree();
}

// ---------------------------------------------------------------------------
// recordCoBorrow: the graph is undirected, so the edge is stored on both
// vertices. Weight is the count of users who borrowed both titles.
// ---------------------------------------------------------------------------
void Recommender::recordCoBorrow(const std::string& isbnA,
                                 const std::string& isbnB) {
    if (isbnA == isbnB) return;             // no self-edges

    adjacency[isbnA][isbnB] += 1;
    adjacency[isbnB][isbnA] += 1;

    trimNeighbors(isbnA);
    trimNeighbors(isbnB);
}

// ---------------------------------------------------------------------------
// trimNeighbors: keeps only the heaviest MAX_NEIGHBORS edges. Without this,
// a popular title accumulates an edge to nearly every other book and the
// two-hop walk stops being bounded.
// ---------------------------------------------------------------------------
void Recommender::trimNeighbors(const std::string& isbn) {
    auto& neighbors = adjacency[isbn];
    if (neighbors.size() <= MAX_NEIGHBORS) return;

    std::vector<std::pair<std::string, int>> ranked(neighbors.begin(),
                                                    neighbors.end());
    // Heaviest first; ISBN breaks ties so trimming is deterministic.
    std::sort(ranked.begin(), ranked.end(),
        [](const std::pair<std::string, int>& a,
           const std::pair<std::string, int>& b) {
            if (a.second != b.second) return a.second > b.second;
            return a.first < b.first;
        });

    neighbors.clear();
    for (size_t i = 0; i < MAX_NEIGHBORS; ++i) {
        neighbors[ranked[i].first] = ranked[i].second;
    }
}

size_t Recommender::edgeCountFor(const std::string& isbn) const {
    auto it = adjacency.find(isbn);
    return (it == adjacency.end()) ? 0 : it->second.size();
}

// ---------------------------------------------------------------------------
// keyLess: (score, isbn) compared as a pair. Score dominates; ISBN only
// matters when scores are equal.
// ---------------------------------------------------------------------------
bool Recommender::keyLess(int scoreA, const std::string& isbnA,
                          int scoreB, const std::string& isbnB) {
    if (scoreA != scoreB) return scoreA < scoreB;
    return isbnA < isbnB;
}

void Recommender::insertNode(int score, const std::string& isbn) {
    if (root == nullptr) {
        root = new TreeNode(score, isbn);
        return;
    }

    TreeNode* cur = root;
    while (true) {
        if (keyLess(score, isbn, cur->score, cur->isbn)) {
            if (cur->left == nullptr) {
                cur->left = new TreeNode(score, isbn);
                return;
            }
            cur = cur->left;
        } else if (keyLess(cur->score, cur->isbn, score, isbn)) {
            if (cur->right == nullptr) {
                cur->right = new TreeNode(score, isbn);
                return;
            }
            cur = cur->right;
        } else {
            return;                         // exact duplicate key
        }
    }
}

// ---------------------------------------------------------------------------
// reverseInOrder: right, node, left. Produces descending score order.
// The limit check short-circuits the walk once enough results exist, so a
// large candidate tree is not traversed in full.
// ---------------------------------------------------------------------------
void Recommender::reverseInOrder(TreeNode* node, size_t limit,
                                 std::vector<Recommendation>& out) const {
    if (node == nullptr || out.size() >= limit) return;

    reverseInOrder(node->right, limit, out);
    if (out.size() >= limit) return;

    out.push_back(Recommendation(node->isbn, node->score));
    reverseInOrder(node->left, limit, out);
}

// ---------------------------------------------------------------------------
// recommend: two-hop walk over the graph, then rank through the BST.
//
// Hop 1: the books the user has borrowed.
// Hop 2: the neighbors of those books, with edge weights accumulated so a
//        title connected to several of the user's books scores higher than
//        one connected to a single book.
// ---------------------------------------------------------------------------
std::vector<Recommendation> Recommender::recommend(
        const std::vector<std::string>& userHistory, size_t limit) {

    clearTree();                            // fresh tree per request

    std::unordered_set<std::string> alreadyRead(userHistory.begin(),
                                                userHistory.end());
    std::unordered_map<std::string, int> candidateScores;

    for (const std::string& read : userHistory) {
        auto it = adjacency.find(read);
        if (it == adjacency.end()) continue;

        for (const auto& edge : it->second) {
            const std::string& neighbor = edge.first;
            if (alreadyRead.count(neighbor) > 0) continue;   // skip own books
            candidateScores[neighbor] += edge.second;
        }
    }

    for (const auto& candidate : candidateScores) {
        insertNode(candidate.second, candidate.first);
    }

    std::vector<Recommendation> out;
    out.reserve(limit);
    reverseInOrder(root, limit, out);
    return out;
}

// ---------------------------------------------------------------------------
// validateTree / checkBounds: the BST validity check.
//
// Bounds are passed down as ancestor nodes rather than fixed values. Every
// node must sort above its lower bound and below its upper bound, which
// catches the case where a node satisfies its parent but violates a
// grandparent. Comparing only against the immediate parent would miss that.
// ---------------------------------------------------------------------------
bool Recommender::checkBounds(TreeNode* node,
                              const TreeNode* lowerBound,
                              const TreeNode* upperBound) const {
    if (node == nullptr) return true;

    if (lowerBound != nullptr &&
        !keyLess(lowerBound->score, lowerBound->isbn, node->score, node->isbn)) {
        return false;
    }
    if (upperBound != nullptr &&
        !keyLess(node->score, node->isbn, upperBound->score, upperBound->isbn)) {
        return false;
    }

    return checkBounds(node->left,  lowerBound, node)
        && checkBounds(node->right, node, upperBound);
}

bool Recommender::validateTree() const {
    return checkBounds(root, nullptr, nullptr);
}

void Recommender::destroy(TreeNode* node) {
    if (node == nullptr) return;
    destroy(node->left);
    destroy(node->right);
    delete node;
}

void Recommender::clearTree() {
    destroy(root);
    root = nullptr;
}
