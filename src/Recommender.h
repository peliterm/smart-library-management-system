// Recommender.h - co-borrowing graph plus a score BST.
//
// The graph finds candidates, because a graph can hold arbitrary connections
// between titles. The tree ranks them, because a graph has no ordering.
// Reverse in-order traversal yields highest score first.
//
// Edges are an adjacency list, not a matrix: borrowing is sparse. Each title
// keeps its 50 strongest edges so a popular book cannot link to the whole
// catalog. The tree is unbalanced; it is rebuilt per request from a bounded
// candidate set, so the depth stays small in practice.

#ifndef RECOMMENDER_H
#define RECOMMENDER_H

#include <string>
#include <vector>
#include <unordered_map>

struct Recommendation {
    std::string isbn;
    int         score;      // co-borrow weight accumulated over the walk

    Recommendation() : score(0) {}
    Recommendation(const std::string& isbn_, int score_)
        : isbn(isbn_), score(score_) {}
};

class Recommender {
public:
    Recommender();
    ~Recommender();

    Recommender(const Recommender&) = delete;
    Recommender& operator=(const Recommender&) = delete;

    static constexpr size_t MAX_NEIGHBORS = 50;

    // Records that one user borrowed both books, strengthening their edge.
    // Called once per pair when a checkout completes.
    void recordCoBorrow(const std::string& isbnA, const std::string& isbnB);

    // Two-hop walk from the books a user has already read. Books the user
    // already holds are excluded. Results come back highest score first.
    std::vector<Recommendation> recommend(
        const std::vector<std::string>& userHistory, size_t limit);

    size_t vertexCount() const { return adjacency.size(); }
    size_t edgeCountFor(const std::string& isbn) const;

    // BST validity checker, following the pattern from Lab 7.15. Confirms
    // every node's key falls inside the (min, max) bounds its position in the
    // tree implies. A per-node parent comparison is not enough: a node can
    // satisfy its immediate parent and still violate an ancestor.
    bool validateTree() const;

private:
    // ---- graph ----
    // ISBN -> (neighbor ISBN -> co-borrow weight)
    std::unordered_map<std::string, std::unordered_map<std::string, int>> adjacency;

    void trimNeighbors(const std::string& isbn);

    // ---- score BST ----
    struct TreeNode {
        int         score;      // primary key
        std::string isbn;       // tiebreaker, keeps ordering total
        TreeNode*   left;
        TreeNode*   right;
        TreeNode(int s, const std::string& i)
            : score(s), isbn(i), left(nullptr), right(nullptr) {}
    };

    TreeNode* root;

    // True when (scoreA, isbnA) sorts before (scoreB, isbnB).
    static bool keyLess(int scoreA, const std::string& isbnA,
                        int scoreB, const std::string& isbnB);

    void insertNode(int score, const std::string& isbn);

    // Reverse in-order: right subtree, node, left subtree. Yields descending
    // score order and stops once limit results are collected.
    void reverseInOrder(TreeNode* node, size_t limit,
                        std::vector<Recommendation>& out) const;

    bool checkBounds(TreeNode* node,
                     const TreeNode* lowerBound,
                     const TreeNode* upperBound) const;

    void destroy(TreeNode* node);
    void clearTree();
};

#endif // RECOMMENDER_H
