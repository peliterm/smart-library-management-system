// ============================================================================
// Recommender.h
// Smart Library Management System - CPSC 315
//
// Two structures working together:
//
//   1. A co-borrowing GRAPH. Each vertex is a book. An edge between two books
//      carries a weight equal to the number of users who borrowed both. The
//      graph is stored as an adjacency list, since a catalog is sparse: most
//      pairs of books share no borrower at all, and a matrix would allocate
//      n^2 slots to store mostly zeros.
//
//   2. A score BST. Candidate books from the graph walk are inserted keyed on
//      (score, ISBN). The score is what the user cares about; the ISBN breaks
//      ties so two books with the same score still get one deterministic
//      position instead of depending on insertion order.
//
// Why a BST rather than sorting a vector:
//      A reverse in-order traversal reads the tree highest score first, so
//      the top N recommendations come off the front of the traversal and the
//      walk can stop early. Sorting the whole candidate list would order
//      results the user never scrolls to.
//
// Neighbor cap:
//      Each book keeps at most MAX_NEIGHBORS edges, retaining the heaviest.
//      This bounds the graph walk regardless of how popular a title becomes.
// ============================================================================
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
