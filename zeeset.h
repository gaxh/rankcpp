#ifndef __ZEESET_H__
#define __ZEESET_H__

// copy from redis zset

#include <map>
#include <random>
#include <ctime>
#include <sstream>
#include <cassert>
#include <vector>

// KeyType and ValueType must be comparable
template<typename KeyType, typename ValueType, int MaxLevel = 32, int BranchProbPercent = 25>
class ZeeSkiplist {
public:
    using KEY_TYPE = KeyType;
    using VALUE_TYPE = ValueType;

    static constexpr int MAX_LEVEL = MaxLevel;
    static constexpr int BRANCH_PROB_PERCENT = BranchProbPercent;
    static constexpr float BRANCH_PROB = BranchProbPercent / 100.f;

private:
    struct Node {
        KEY_TYPE KEY;
        VALUE_TYPE VALUE;
        Node *BACKWARD = NULL;
        
        struct Level {
            Node *FORWARD = NULL;
            unsigned long SPAN = 0;
        };

        Level LEVEL[MAX_LEVEL];

        Node(const KEY_TYPE &key, const VALUE_TYPE &value) :
            KEY(key), VALUE(value) {}

        Node() = default;
        ~Node() = default;

        void Reset() {
            BACKWARD = NULL;

            for(int i = 0; i < MAX_LEVEL; ++i) {
                LEVEL[i].FORWARD = NULL;
                LEVEL[i].SPAN = 0;
            }
        }
    };

    Node *m_header = NULL;
    Node *m_tail = NULL;
    unsigned long m_length = 0;
    int m_level = 1;

    std::mt19937 m_rng;
public:
    ZeeSkiplist() {
        m_header = CreateNode();
        m_rng.seed(time(NULL));
    }

    ~ZeeSkiplist() {
        Clear();
        FreeNode(m_header);
    }

    ZeeSkiplist(const ZeeSkiplist &) = delete;
    ZeeSkiplist(ZeeSkiplist &&) = delete;
    ZeeSkiplist &operator=(const ZeeSkiplist &) = delete;
    ZeeSkiplist &operator=(ZeeSkiplist &&) = delete;

    void Clear() {
        Node *x = m_header->LEVEL[0].FORWARD;

        while(x) {
            Node *next = x->LEVEL[0].FORWARD;
            FreeNode(x);
            x = next;
        }

        m_header->Reset();
        m_tail = NULL;
        m_length = 0;
        m_level = 1;
    }

    unsigned long Length() {
        return m_length;
    }

    unsigned long MaxRank() {
        return m_length;
    }

private:
    Node *CreateNode(const KEY_TYPE &key, const VALUE_TYPE &value) {
        return new Node(key, value);
    }

    Node *CreateNode() {
        return new Node();
    }

    void FreeNode(Node *n) {
        delete n;
    }

    int RandomLevel() {
        int level = 1;
        while( level < MAX_LEVEL && (((unsigned)m_rng() & (unsigned)0xffff) < (unsigned)(BRANCH_PROB * 0xffff)) ) {
            ++level;
        }
        return level < MAX_LEVEL ? level : MAX_LEVEL;
    }

    bool key_compare_less(const KEY_TYPE &k1, const KEY_TYPE &k2) {
        return k1 < k2;
    }

    bool key_compare_equal(const KEY_TYPE &k1, const KEY_TYPE &k2) {
        return k1 == k2;
    }

    bool value_compare_less(const VALUE_TYPE &v1, const VALUE_TYPE &v2) {
        return v1 < v2;
    }

    bool value_compare_equal(const VALUE_TYPE &v1, const VALUE_TYPE &v2) {
        return v1 == v2;
    }

    Node *InsertNode(const KEY_TYPE &key, const VALUE_TYPE &value) {
        return InsertNodeOnly(CreateNode(key, value));
    }

    Node *InsertNodeOnly(Node *n) {
        Node *update[MAX_LEVEL];
        Node *x;
        unsigned long rank[MAX_LEVEL];
        int level;

        x = m_header;

        for(int i = m_level - 1; i >= 0; --i) {
            rank[i] = i == (m_level - 1) ? 0 : rank[i + 1];
            while( x->LEVEL[i].FORWARD && ( value_compare_less(x->LEVEL[i].FORWARD->VALUE, n->VALUE) ||
                        ( value_compare_equal(x->LEVEL[i].FORWARD->VALUE, n->VALUE) &&
                          key_compare_less(x->LEVEL[i].FORWARD->KEY, n->KEY))) ) {
                rank[i] += x->LEVEL[i].SPAN;
                x = x->LEVEL[i].FORWARD;
            }
            update[i] = x;
        }

        level = RandomLevel();

        if(level > m_level) {
            for(int i = m_level; i < level; ++i) {
                rank[i] = 0;
                update[i] = m_header;
                update[i]->LEVEL[i].SPAN = m_length;
            }
            m_level = level;
        }

        x = n;
        for(int i = 0; i < level; ++i) {
            x->LEVEL[i].FORWARD = update[i]->LEVEL[i].FORWARD;
            update[i]->LEVEL[i].FORWARD = x;

            x->LEVEL[i].SPAN = update[i]->LEVEL[i].SPAN - (rank[0] - rank[i]);
            update[i]->LEVEL[i].SPAN = (rank[0] - rank[i]) + 1;
        }

        for(int i = level; i < m_level; ++i) {
            update[i]->LEVEL[i].SPAN++;
        }

        x->BACKWARD = (update[0] == m_header) ? NULL : update[0];
        if(x->LEVEL[0].FORWARD) {
            x->LEVEL[0].FORWARD->BACKWARD = x;
        } else {
            m_tail = x;
        }
        m_length++;
        return x;
    }

    void RemoveNodeOnly(Node *x, Node *update[MAX_LEVEL]) {
        for(int i = 0; i < m_level; ++i) {
            if( update[i]->LEVEL[i].FORWARD == x ) {
                update[i]->LEVEL[i].SPAN += x->LEVEL[i].SPAN - 1;
                update[i]->LEVEL[i].FORWARD = x->LEVEL[i].FORWARD;
            } else {
                update[i]->LEVEL[i].SPAN -= 1;
            }
        }

        if(x->LEVEL[0].FORWARD) {
            x->LEVEL[0].FORWARD->BACKWARD = x->BACKWARD;
        } else {
            m_tail = x->BACKWARD;
        }
        while(m_level > 1 && m_header->LEVEL[m_level - 1].FORWARD == NULL) {
            m_level--;
        }
        m_length--;
    }

    bool DeleteNode(const KEY_TYPE &key, const VALUE_TYPE &value, Node **out) {
        Node *update[MAX_LEVEL];
        Node *x;

        x = m_header;
        for(int i = m_level - 1; i >= 0; --i) {
            while( x->LEVEL[i].FORWARD && ( value_compare_less(x->LEVEL[i].FORWARD->VALUE, value) ||
                        ( value_compare_equal(x->LEVEL[i].FORWARD->VALUE, value) &&
                          key_compare_less(x->LEVEL[i].FORWARD->KEY, key))) ) {
                x = x->LEVEL[i].FORWARD;
            }
            update[i] = x;
        }

        x = x->LEVEL[0].FORWARD;
        if(x && key_compare_equal(x->KEY, key) && value_compare_equal(x->VALUE, value)) {
            RemoveNodeOnly(x, update);
            if(!out) {
                FreeNode(x);
            } else {
                *out = x;
            }
            return true;
        }

        return false;
    }

    Node *UpdateNode(const KEY_TYPE &key, const VALUE_TYPE &value, const VALUE_TYPE &new_value) {
        Node *update[MAX_LEVEL];
        Node *x;

        x = m_header;
        for(int i = m_level - 1; i >= 0; --i) {
            while( x->LEVEL[i].FORWARD && ( value_compare_less(x->LEVEL[i].FORWARD->VALUE, value) ||
                        ( value_compare_equal(x->LEVEL[i].FORWARD->VALUE, value) &&
                          key_compare_less(x->LEVEL[i].FORWARD->KEY, key))) ) {
                x = x->LEVEL[i].FORWARD;
            }
            update[i] = x;
        }

        x = x->LEVEL[0].FORWARD;

        if(!x || !key_compare_equal(x->KEY, key) || !value_compare_equal(x->VALUE, value)) {
            return NULL;
        }

        if( (x->BACKWARD == NULL || value_compare_less(x->BACKWARD->VALUE, new_value)) &&
                (x->LEVEL[0].FORWARD == NULL || value_compare_less(new_value, x->LEVEL[0].FORWARD->VALUE))) {
            x->VALUE = new_value;
            return x;
        }

        RemoveNodeOnly(x, update);
        x->Reset();
        x->VALUE = new_value;

        return InsertNodeOnly(x);
    }

    unsigned long GetRankOfNode(const KEY_TYPE &key, const VALUE_TYPE &value) {
        Node *x;
        unsigned long rank = 0;

        x = m_header;
        for(int i = m_level - 1; i >= 0; --i) {
            while( x->LEVEL[i].FORWARD && ( value_compare_less(x->LEVEL[i].FORWARD->VALUE, value) ||
                        ( value_compare_equal(x->LEVEL[i].FORWARD->VALUE, value) &&
                          !key_compare_less( key, x->LEVEL[i].FORWARD->KEY ))) ) {
                rank += x->LEVEL[i].SPAN;
                x = x->LEVEL[i].FORWARD;
            }

            if(x && key_compare_equal(x->KEY, key) && value_compare_equal(x->VALUE, value)) {
                return rank;
            }
        }

        return 0;
    }

    Node *GetNodeByRank(unsigned long rank) {
        if(rank == 0 || rank > m_length) {
            return NULL;
        }

        Node *x;
        unsigned long traversed = 0;

        x = m_header;
        for(int i = m_level - 1; i >= 0; --i) {
            while( x->LEVEL[i].FORWARD && (traversed + x->LEVEL[i].SPAN) <= rank ) {
                traversed += x->LEVEL[i].SPAN;
                x = x->LEVEL[i].FORWARD;
            }

            if(traversed == rank) {
                return x;
            }
        }

        return NULL;
    }

    template<typename Function> /* std::function<void(unsigned long rank, Node *)> */
    void GetNodeByRangedRank(unsigned long rank_low, unsigned long rank_high, Function cb) {
        if(rank_low > rank_high) {
            return;
        }

        Node *x = GetNodeByRank(rank_low);

        unsigned long count = rank_high - rank_low + 1;
        unsigned long n = 0;

        while(x && (n < count)) {
            cb(rank_low + n, x);

            x = x->LEVEL[0].FORWARD;

            n++;
        }
    }

    template<typename Function> /* std::function<void(unsigned long rank, Node *)> */
    void ForeachNode(Function cb) {
        Node *x = m_header->LEVEL[0].FORWARD;
        unsigned long n = 0;

        while(x) {
            cb(++n, x);

            x = x->LEVEL[0].FORWARD;
        }
    }

    template<typename Function> /* std::function<void(unsigned long rank, Node *)> */
    void ForeachNodeReverse(Function cb) {
        if(!m_tail) {
            return;
        }

        Node *x = m_tail;
        unsigned long n = m_length;

        while(x) {
            cb(n--, x);

            x = x->BACKWARD;
        }
    }

    template<typename Function> /* std::function<void(unsigned long rank, Node *)> */
    void DeleteNodeByRangedRank(unsigned long rank_low, unsigned long rank_high, Function cb) {
        if(rank_low > rank_high) {
            return;
        }

        Node *update[MAX_LEVEL];
        Node *x;

        unsigned long traversed = 0;

        x = m_header;

        for(int i = m_level - 1; i >= 0; --i) {
            while( x->LEVEL[i].FORWARD && (traversed + x->LEVEL[i].SPAN) < rank_low ) {
                traversed += x->LEVEL[i].SPAN;
                x = x->LEVEL[i].FORWARD;
            }
            update[i] = x;
        }

        unsigned long count = rank_high - rank_low + 1;
        unsigned long n = 0;

        x = x->LEVEL[0].FORWARD;
        while(x && n < count) {
            Node *next = x->LEVEL[0].FORWARD;
            RemoveNodeOnly(x, update);
            cb( rank_low + n, x );
            FreeNode(x);
            x = next;
            n++;
        }
    }

    Node *GetNodeOfFirstGreaterValue(const VALUE_TYPE &value, unsigned long *rank) {
        if( !m_tail || !value_compare_less(value, m_tail->VALUE) ) {
            return NULL;
        }

        Node *x;
        unsigned long traversed = 0;

        x = m_header;

        for(int i = m_level - 1; i >= 0; --i) {
            while(x->LEVEL[i].FORWARD && !value_compare_less(value, x->LEVEL[i].FORWARD->VALUE)) {
                traversed += x->LEVEL[i].SPAN;
                x = x->LEVEL[i].FORWARD;
            }
        }

        if(x->LEVEL[0].FORWARD && value_compare_less(value, x->LEVEL[0].FORWARD->VALUE)) {
            if(rank) {
                *rank = traversed + 1;
            }
            return x->LEVEL[0].FORWARD;
        }

        return NULL;
    }

    Node *GetNodeOfFirstGreaterEqualValue(const VALUE_TYPE &value, unsigned long *rank) {
        if( !m_tail || value_compare_less(m_tail->VALUE, value) ) {
            return NULL;
        }

        Node *x;
        unsigned long traversed = 0;

        x = m_header;

        for(int i = m_level - 1; i >= 0; --i) {
            while(x->LEVEL[i].FORWARD && value_compare_less(x->LEVEL[i].FORWARD->VALUE, value)) {
                traversed += x->LEVEL[i].SPAN;
                x = x->LEVEL[i].FORWARD;
            }
        }

        if(x->LEVEL[0].FORWARD && !value_compare_less(x->LEVEL[0].FORWARD->VALUE, value)) {
            if(rank) {
                *rank = traversed + 1;
            }
            return x->LEVEL[0].FORWARD;
        }

        return NULL;
    }

    Node *GetNodeOfLastLessValue(const VALUE_TYPE &value, unsigned long *rank) {
        if( !m_header->LEVEL[0].FORWARD || !value_compare_less(m_header->LEVEL[0].FORWARD->VALUE, value) ) {
            return NULL;
        }

        Node *x;
        unsigned long traversed = 0;

        x = m_header;

        for(int i = m_level - 1; i >= 0; --i) {
            while(x->LEVEL[i].FORWARD && value_compare_less(x->LEVEL[i].FORWARD->VALUE, value)) {
                traversed += x->LEVEL[i].SPAN;
                x = x->LEVEL[i].FORWARD;
            }
        }

        if(x && value_compare_less(x->VALUE, value)) {
            if(rank) {
                *rank = traversed;
            }
            return x;
        }

        return NULL;
    }

    Node *GetNodeOfLastLessEqualValue(const VALUE_TYPE &value, unsigned long *rank) {
        if( !m_header->LEVEL[0].FORWARD || value_compare_less(value, m_header->LEVEL[0].FORWARD->VALUE) ) {
            return NULL;
        }

        Node *x;
        unsigned long traversed = 0;

        x = m_header;

        for(int i = m_level - 1; i >= 0; --i) {
            while(x->LEVEL[i].FORWARD && !value_compare_less(value, x->LEVEL[i].FORWARD->VALUE)) {
                traversed += x->LEVEL[i].SPAN;
                x = x->LEVEL[i].FORWARD;
            }
        }

        if(x && !value_compare_less(value, x->VALUE)) {
            if(rank) {
                *rank = traversed;
            }
            return x;
        }

        return NULL;
    }

public:
    void Insert(const KEY_TYPE &key, const VALUE_TYPE &value) {
        InsertNode(key, value);
    }

    bool Delete(const KEY_TYPE &key, const VALUE_TYPE &value) {
        return DeleteNode(key, value, NULL);
    }

    bool Update(const KEY_TYPE &key, const VALUE_TYPE &value, const VALUE_TYPE &new_value) {
        return UpdateNode(key, value, new_value) != NULL;
    }

    unsigned long GetRankOfElement(const KEY_TYPE &key, const VALUE_TYPE &value) {
        return GetRankOfNode(key, value);
    }

    bool GetElementByRank(unsigned long rank, KEY_TYPE &key, VALUE_TYPE &value) {
        Node *n = GetNodeByRank(rank);

        if(n) {
            key = n->KEY;
            value = n->VALUE;

            return true;
        } else {
            return false;
        }
    }

    template<typename Function> /* std::function<void(unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value)> */
    void GetElementsByRangedRank(unsigned long rank_low, unsigned long rank_high, Function cb) {
        GetNodeByRangedRank(rank_low, rank_high, [cb](unsigned long rank, Node *n) {
                    cb(rank, n->KEY, n->VALUE);
                });
    }

    template<typename Function> /* std::function<void(unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value)> */
    void ForeachElements(Function cb) {
        ForeachNode([cb](unsigned long rank, Node *n){
                    cb(rank, n->KEY, n->VALUE);
                });
    }

    template<typename Function> /* std::function<void(unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value)> */
    void ForeachElementsReverse(Function cb) {
        ForeachNodeReverse([cb](unsigned long rank, Node *n){
                    cb(rank, n->KEY, n->VALUE);
                });
    }

    template<typename Function> /* std::function<void(unsigned long, const KEY_TYPE &key, const VALUE_TYPE &value)> */
    void DeleteByRangedRank(unsigned long rank_low, unsigned long rank_high, Function cb) {
        DeleteNodeByRangedRank(rank_low, rank_high, [cb](unsigned long rank, Node *n){
                    cb(rank, n->KEY, n->VALUE);
                });
    }

    bool GetElementOfFirstGreaterValue(const VALUE_TYPE &v, KEY_TYPE &key, VALUE_TYPE &value, unsigned long *rank) {
        Node *n = GetNodeOfFirstGreaterValue(v, rank);

        if(n) {
            key = n->KEY;
            value = n->VALUE;
            return true;
        } else {
            return false;
        }
    }

    bool GetElementOfFirstGreaterEqualValue(const VALUE_TYPE &v, KEY_TYPE &key, VALUE_TYPE &value, unsigned long *rank) {
        Node *n = GetNodeOfFirstGreaterEqualValue(v, rank);

        if(n) {
            key = n->KEY;
            value = n->VALUE;
            return true;
        } else {
            return false;
        }
    }

    bool GetElementOfLastLessValue(const VALUE_TYPE &v, KEY_TYPE &key, VALUE_TYPE &value, unsigned long *rank) {
        Node *n = GetNodeOfLastLessValue(v, rank);

        if(n) {
            key = n->KEY;
            value = n->VALUE;
            return true;
        } else {
            return false;
        }
    }

    bool GetElementOfLastLessEqualValue(const VALUE_TYPE &v, KEY_TYPE &key, VALUE_TYPE &value, unsigned long *rank) {
        Node *n = GetNodeOfLastLessEqualValue(v, rank);

        if(n) {
            key = n->KEY;
            value = n->VALUE;
            return true;
        } else {
            return false;
        }
    }

    template<typename Function> /* std::function<void(unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value)> */
    void GetElementsByRangedValue(const VALUE_TYPE &v_low, bool include_v_low, const VALUE_TYPE &v_high, bool include_v_high, Function cb) {
        unsigned long rank;
        Node *first = include_v_low ? GetNodeOfFirstGreaterEqualValue(v_low, &rank) :
            GetNodeOfFirstGreaterValue(v_low, &rank);

        if(!first) {
            return;
        }

        unsigned long rank2;
        Node *last = include_v_high ? GetNodeOfLastLessEqualValue(v_high, &rank2) :
            GetNodeOfLastLessValue(v_high, &rank2);

        if(!last) {
            return;
        }

        while(rank <= rank2) {
            cb(rank, first->KEY, first->VALUE);
            first = first->LEVEL[0].FORWARD;
            ++rank;
        }
    }

    unsigned long GetElementsCountByRangedValue(const VALUE_TYPE &v_low, bool include_v_low, const VALUE_TYPE &v_high, bool include_v_high) {
        unsigned long rank;
        Node *first = include_v_low ? GetNodeOfFirstGreaterEqualValue(v_low, &rank) :
            GetNodeOfFirstGreaterValue(v_low, &rank);

        if(!first) {
            return 0;
        }

        unsigned long rank2;
        Node *last = include_v_high ? GetNodeOfLastLessEqualValue(v_high, &rank2) :
            GetNodeOfLastLessValue(v_high, &rank2);

        if(!last) {
            return 0;
        }

        return rank <= rank2 ? rank2 - rank + 1 : 0;
    }

    template<typename Function> /* std::function<void(unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value)> */
    void DeleteByRangedValue(const VALUE_TYPE &v_low, bool include_v_low, const VALUE_TYPE &v_high, bool include_v_high, Function cb) {
        unsigned long rank;
        Node *first = include_v_low ? GetNodeOfFirstGreaterEqualValue(v_low, &rank) :
            GetNodeOfFirstGreaterValue(v_low, &rank);

        if(!first) {
            return;
        }

        unsigned long rank2;
        Node *last = include_v_high ? GetNodeOfLastLessEqualValue(v_high, &rank2) :
            GetNodeOfLastLessValue(v_high, &rank2);

        if(!last) {
            return;
        }

        DeleteByRangedRank(rank, rank2, cb);
    }

    template<typename Function> /* std::function<bool(unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value)> */
    void ForeachElementsOfNearbyRank(unsigned long rank, unsigned long lower_count, unsigned long upper_count, Function pick_cb) {
        Node *x = GetNodeByRank(rank);

        if(!x) {
            return;
        }

        pick_cb(rank, x->KEY, x->VALUE);

        {
            Node *y = x->BACKWARD;
            unsigned long r = rank - 1;

            while(y && lower_count) {
                if(pick_cb(r, y->KEY, y->VALUE)) {
                    --lower_count;
                }

                y = y->BACKWARD;
                --r;
            }
        }

        {
            Node *y = x->LEVEL[0].FORWARD;
            unsigned long r = rank + 1;

            while(y && upper_count) {
                if(pick_cb(r, y->KEY, y->VALUE)) {
                    --upper_count;
                }

                y = y->LEVEL[0].FORWARD;
                ++r;
            }

        }
    }

    template<typename Function> /* std::function<bool(unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value)> */
    void ForeachElementsOfNearbyValue(const VALUE_TYPE &value, unsigned long lower_count, unsigned long upper_count, Function pick_cb)
    {
        unsigned long rank = 0;
        Node *x = GetNodeOfFirstGreaterEqualValue(value, &rank);

        if(!x)
        {
            x = GetNodeOfLastLessEqualValue(value, &rank);

            if(!x)
            {
                return;
            }
        }

        pick_cb(rank, x->KEY, x->VALUE);

        {
            Node *y = x->BACKWARD;
            unsigned long r = rank - 1;

            while(y && lower_count) {
                if(pick_cb(r, y->KEY, y->VALUE)) {
                    --lower_count;
                }

                y = y->BACKWARD;
                --r;
            }
        }

        {
            Node *y = x->LEVEL[0].FORWARD;
            unsigned long r = rank + 1;

            while(y && upper_count) {
                if(pick_cb(r, y->KEY, y->VALUE)) {
                    --upper_count;
                }

                y = y->LEVEL[0].FORWARD;
                ++r;
            }

        }

    }

    std::string DumpLevels() {
        std::ostringstream ss;
        Node *x = m_header->LEVEL[0].FORWARD;
        unsigned long i = 0;

        while(x) {
            ss << "(" << ++i << ") " << x << ":" << "[" << x->KEY << "]" << "=" << x->VALUE;

            for(size_t k = 0; k < MAX_LEVEL && (x->LEVEL[k].FORWARD || x->LEVEL[k].SPAN); ++k) {
                ss << " {" << k << ":" << x->LEVEL[k].SPAN << ":" << x->LEVEL[k].FORWARD << "}";
            }

            ss << "\n";
            
            x = x->LEVEL[0].FORWARD;
        }

        ss << "(sumary) " << "[level]=" << m_level << ", " << "[length]=" << m_length;

        return ss.str();
    }

    bool TestSelf() {
        Node *x = m_header->LEVEL[0].FORWARD;

        while(x && x->LEVEL[0].FORWARD) {
            if(x->LEVEL[0].FORWARD->VALUE < x->VALUE) {
                return false;
            }

            x = x->LEVEL[0].FORWARD;
        }

        return true;
    }

    // re-construct tree-like structure
    void Optimize() {
        std::vector<Node *> all_nodes;
        all_nodes.reserve(m_length);

        for(Node *x = m_header->LEVEL[0].FORWARD; x; ) {
            Node *next = x->LEVEL[0].FORWARD;

            x->Reset();
            all_nodes.emplace_back(x);

            x = next;
        }

        m_header->Reset();
        m_tail = NULL;
        m_length = 0;
        m_level = 1;

        for(Node *x: all_nodes) {
            InsertNodeOnly(x);
        }
    }
};

template<typename KeyType, typename ValueType, int MaxLevel = 32, int BranchProbPercent = 25>
class ZeeSet {
public:
    using KEY_TYPE = KeyType;
    using VALUE_TYPE = ValueType;

    ZeeSet() = default;
    ~ZeeSet() = default;

    ZeeSet(const ZeeSet &) = delete;
    ZeeSet(ZeeSet &&) = delete;
    ZeeSet &operator=(const ZeeSet &) = delete;
    ZeeSet &operator=(ZeeSet &&) = delete;

    unsigned long Length() {
        return m_skiplist.Length();
    }

    unsigned long MaxRank() {
        return m_skiplist.MaxRank();
    }

    size_t Count() {
        return m_dict.size();
    }

    void Clear() {
        m_dict.clear();
        m_skiplist.Clear();
    }

    void Update(const KEY_TYPE &key, const VALUE_TYPE &value) {
        auto iter = m_dict.find(key);

        if(iter == m_dict.end()) {
            m_skiplist.Insert(key, value);
            m_dict[key] = value;
        } else {
            m_skiplist.Update(key, iter->second, value);
            m_dict[key] = value;
        }
    }

    void Delete(const KEY_TYPE &key) {
        auto iter = m_dict.find(key);

        if(iter == m_dict.end()){
            return;
        }

        m_skiplist.Delete(key, iter->second);
        m_dict.erase(iter);
    }

    unsigned long GetRankOfElement(const KEY_TYPE &key) {
        auto iter = m_dict.find(key);

        if(iter == m_dict.end()) {
            return 0;
        }

        return m_skiplist.GetRankOfElement(key, iter->second);
    }

    bool GetElementByRank(unsigned long rank, KEY_TYPE &key, VALUE_TYPE &value) {
        return m_skiplist.GetElementByRank(rank, key, value);
    }

    template<typename Function> /* std::function<void(unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value)> */
    void GetElementsByRangedRank(unsigned long rank_low, unsigned long rank_high, Function cb) {
        m_skiplist.GetElementsByRangedRank(rank_low, rank_high, cb);
    }

    template<typename Function> /* std::function<void(unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value)> */
    void ForeachElements(Function cb) {
        m_skiplist.ForeachElements(cb);
    }

    template<typename Function> /* std::function<void(unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value)> */
    void ForeachElementsReverse(Function cb) {
        m_skiplist.ForeachElementsReverse(cb);
    }

    template<typename Function> /* std::function<void(unsigned long, const KEY_TYPE &key, const VALUE_TYPE &value)> */
    void DeleteByRangedRank(unsigned long rank_low, unsigned long rank_high, Function cb) {
        m_skiplist.DeleteByRangedRank(rank_low, rank_high, [this, cb](unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value){
                    this->m_dict.erase(key);

                    if(cb) {
                        cb( rank, key, value );
                    }
                });
    }

    bool GetElementOfFirstGreaterValue(const VALUE_TYPE &v, KEY_TYPE &key, VALUE_TYPE &value, unsigned long *rank) {
        return m_skiplist.GetElementOfFirstGreaterValue(v, key, value, rank);
    }

    bool GetElementOfFirstGreaterEqualValue(const VALUE_TYPE &v, KEY_TYPE &key, VALUE_TYPE &value, unsigned long *rank) {
        return m_skiplist.GetElementOfFirstGreaterEqualValue(v, key, value, rank);
    }

    bool GetElementOfLastLessValue(const VALUE_TYPE &v, KEY_TYPE &key, VALUE_TYPE &value, unsigned long *rank) {
        return m_skiplist.GetElementOfLastLessValue(v, key, value, rank);
    }

    bool GetElementOfLastLessEqualValue(const VALUE_TYPE &v, KEY_TYPE &key, VALUE_TYPE &value, unsigned long *rank) {
        return m_skiplist.GetElementOfLastLessEqualValue(v, key, value, rank);
    }

    template<typename Function> /* std::function<void(unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value)> */
    void GetElementsByRangedValue(const VALUE_TYPE &v_low, bool include_v_low, const VALUE_TYPE &v_high, bool include_v_high, Function cb) {
        m_skiplist.GetElementsByRangedValue(v_low, include_v_low, v_high, include_v_high, cb);
    }

    unsigned long GetElementsCountByRangedValue(const VALUE_TYPE &v_low, bool include_v_low, const VALUE_TYPE &v_high, bool include_v_high) {
        return m_skiplist.GetElementsCountByRangedValue(v_low, include_v_low, v_high, include_v_high);
    }

    std::string DumpLevels() {
        std::ostringstream ss;
        ss << m_skiplist.DumpLevels() << "\n";
        ss << "dictionary size=" << Count();
        return ss.str();
    }

    template<typename Function> /* std::function<void(unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value)> */
    void DeleteByRangedValue(const VALUE_TYPE &v_low, bool include_v_low, const VALUE_TYPE &v_high, bool include_v_high, Function cb) {
        m_skiplist.DeleteByRangedValue(v_low, include_v_low, v_high, include_v_high, [this, cb](unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value) {
                    this->m_dict.erase(key);

                    if(cb) {
                        cb(rank, key, value);
                    }
                });
    }

    template<typename Function> /* std::function<bool(unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value)> */
    void ForeachElementsOfNearbyRank(unsigned long rank, unsigned long lower_count, unsigned long upper_count, Function pick_cb) {
        m_skiplist.ForeachElementsOfNearbyRank(rank, lower_count, upper_count, pick_cb);
    }

    template<typename Function> /* std::function<bool(unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value)> */
    void ForeachElementsOfNearbyValue(const VALUE_TYPE &value, unsigned long lower_count, unsigned long upper_count, Function pick_cb)
    {
        m_skiplist.ForeachElementsOfNearbyValue(value, lower_count, upper_count, pick_cb);
    }

    bool GetValueByKey(const KEY_TYPE &key, VALUE_TYPE &value) {
        auto iter = m_dict.find(key);

        if(iter == m_dict.end()) {
            return false;
        }

        value = iter->second;
        return true;
    }

    bool HasKey(const KEY_TYPE &key) {
        return m_dict.count(key) != 0;
    }

    bool TestSelf() {
        if(m_dict.size() != m_skiplist.Length()) {
            return false;
        }

        if(!m_skiplist.TestSelf()) {
            return false;
        }

        std::map<KEY_TYPE, VALUE_TYPE> data;
        bool result = true;

        ForeachElements([&data, &result](unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value){
                    if(data.count(key)) {
                        result = result || false;
                    }

                    data[key] = value;
                });

        if(!result || m_dict.size() != data.size()) {
            return false;
        }

        auto i = m_dict.begin();
        auto j = data.begin();

        for(; i != m_dict.end() && j != data.end(); ++i, ++j) {
            if(!(i->first == j->first)) {
                return false;
            }

            if(!(i->second == j->second)) {
                return false;
            }
        }

        return i == m_dict.end() && j == data.end();
    }

    void Optimize() {
        return m_skiplist.Optimize();
    }

private:
    ZeeSkiplist<KeyType, ValueType, MaxLevel, BranchProbPercent> m_skiplist;
    std::map<KEY_TYPE, VALUE_TYPE> m_dict;
};

#endif

