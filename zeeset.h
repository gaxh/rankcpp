#ifndef __ZEESET_H__
#define __ZEESET_H__

// copy from redis zset

#include <vector>
#include <map>
#include <random>
#include <ctime>
#include <functional>
#include <sstream>
#include <cassert>

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

        std::vector<Level> LEVEL;

        Node(int level, const KEY_TYPE &key, const VALUE_TYPE &value) :
            KEY(key), VALUE(value) {
                LEVEL.resize(level);
            }

        Node(int level) {
            LEVEL.resize(level);
        }

        ~Node() = default;
    };

    Node *m_header = NULL;
    Node *m_tail = NULL;
    unsigned long m_length = 0;
    int m_level = 1;

    std::mt19937 m_rng;
public:
    ZeeSkiplist() {
        m_header = CreateNode(MAX_LEVEL);
        m_rng.seed(time(NULL));
    }

    ~ZeeSkiplist() {
        Node *x = m_header->LEVEL[0].FORWARD;

        FreeNode(m_header);

        while(x) {
            Node *next = x->LEVEL[0].FORWARD;
            FreeNode(x);
            x = next;
        }
    }

    unsigned long Length() {
        return m_length;
    }

    unsigned long MaxRank() {
        return m_length;
    }

private:
    Node *CreateNode(int level, const KEY_TYPE &key, const VALUE_TYPE &value) {
        return new Node(level, key, value);
    }

    Node *CreateNode(int level) {
        return new Node(level);
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
        Node *update[MAX_LEVEL];
        Node *x;
        unsigned int rank[MAX_LEVEL];
        int level;

        x = m_header;

        for(int i = m_level - 1; i >= 0; --i) {
            rank[i] = i == (m_level - 1) ? 0 : rank[i + 1];
            while( x->LEVEL[i].FORWARD && ( value_compare_less(x->LEVEL[i].FORWARD->VALUE, value) ||
                        ( value_compare_equal(x->LEVEL[i].FORWARD->VALUE, value) &&
                          key_compare_less(x->LEVEL[i].FORWARD->KEY, key))) ) {
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

        x = CreateNode(level, key, value);
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
        FreeNode(x);

        return InsertNode(key, new_value);
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

            if(key_compare_equal(x->KEY, key)) {
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

    void GetNodeByRangedRank(unsigned long rank_low, unsigned long rank_high, std::function<void(unsigned long rank, Node *)> cb) {
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

    void ForeachNode(std::function<void(unsigned long rank, Node *)> cb) {
        Node *x = m_header->LEVEL[0].FORWARD;
        unsigned long n = 0;

        while(x) {
            cb(++n, x);

            x = x->LEVEL[0].FORWARD;
        }
    }

    void DeleteNodeByRangedRank(unsigned long rank_low, unsigned long rank_high, std::function<void(unsigned long rank, Node *)> cb) {
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

    Node *GetNodeOfFirstGreaterValue(const VALUE_TYPE &value) {
        if( !m_tail || !value_compare_less(value, m_tail->VALUE) ) {
            return NULL;
        }

        Node *x;

        x = m_header;

        for(int i = m_level - 1; i >= 0; --i) {
            while(x->LEVEL[i].FORWARD && !value_compare_less(value, x->LEVEL[i].FORWARD->VALUE)) {
                x = x->LEVEL[i].FORWARD;
            }
        }

        if(x->LEVEL[0].FORWARD && value_compare_less(value, x->LEVEL[0].FORWARD->VALUE)) {
            return x->LEVEL[0].FORWARD;
        }

        return NULL;
    }

    Node *GetNodeOfFirstGreaterEqualValue(const VALUE_TYPE &value) {
        if( !m_tail || value_compare_less(m_tail->VALUE, value) ) {
            return NULL;
        }

        Node *x;

        x = m_header;

        for(int i = m_level - 1; i >= 0; --i) {
            while(x->LEVEL[i].FORWARD && value_compare_less(x->LEVEL[i].FORWARD->VALUE, value)) {
                x = x->LEVEL[i].FORWARD;
            }
        }

        if(x->LEVEL[0].FORWARD && !value_compare_less(x->LEVEL[0].FORWARD->VALUE, value)) {
            return x->LEVEL[0].FORWARD;
        }

        return NULL;
    }

    Node *GetNodeOfLastLessValue(const VALUE_TYPE &value) {
        if( !m_header->LEVEL[0].FORWARD || !value_compare_less(m_header->LEVEL[0].FORWARD->VALUE, value) ) {
            return NULL;
        }

        Node *x;

        x = m_header;

        for(int i = m_level - 1; i >= 0; --i) {
            while(x->LEVEL[i].FORWARD && value_compare_less(x->LEVEL[i].FORWARD->VALUE, value)) {
                x = x->LEVEL[i].FORWARD;
            }
        }

        if(x && value_compare_less(x->VALUE, value)) {
            return x;
        }

        return NULL;
    }

    Node *GetNodeOfLastLessEqualValue(const VALUE_TYPE &value) {
        if( !m_header->LEVEL[0].FORWARD || value_compare_less(value, m_header->LEVEL[0].FORWARD->VALUE) ) {
            return NULL;
        }

        Node *x;

        x = m_header;

        for(int i = m_level - 1; i >= 0; --i) {
            while(x->LEVEL[i].FORWARD && !value_compare_less(value, x->LEVEL[i].FORWARD->VALUE)) {
                x = x->LEVEL[i].FORWARD;
            }
        }

        if(x && !value_compare_less(value, x->VALUE)) {
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

    void GetElementsByRangedRank(unsigned long rank_low, unsigned long rank_high, std::function<void(unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value)> cb) {
        GetNodeByRangedRank(rank_low, rank_high, [cb](unsigned long rank, Node *n) {
                    cb(rank, n->KEY, n->VALUE);
                });
    }

    void ForeachElements(std::function<void(unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value)> cb) {
        ForeachNode([cb](unsigned long rank, Node *n){
                    cb(rank, n->KEY, n->VALUE);
                });
    }

    void DeleteByRangedRank(unsigned long rank_low, unsigned long rank_high, std::function<void(unsigned long, const KEY_TYPE &key, const VALUE_TYPE &value)> cb) {
        DeleteNodeByRangedRank(rank_low, rank_high, [cb](unsigned long rank, Node *n){
                    cb(rank, n->KEY, n->VALUE);
                });
    }

    bool GetElementOfFirstGreaterValue(const VALUE_TYPE &v, KEY_TYPE &key, VALUE_TYPE &value) {
        Node *n = GetNodeOfFirstGreaterValue(v);

        if(n) {
            key = n->KEY;
            value = n->VALUE;
            return true;
        } else {
            return false;
        }
    }

    bool GetElementOfFirstGreaterEqualValue(const VALUE_TYPE &v, KEY_TYPE &key, VALUE_TYPE &value) {
        Node *n = GetNodeOfFirstGreaterEqualValue(v);

        if(n) {
            key = n->KEY;
            value = n->VALUE;
            return true;
        } else {
            return false;
        }
    }

    bool GetElementOfLastLessValue(const VALUE_TYPE &v, KEY_TYPE &key, VALUE_TYPE &value) {
        Node *n = GetNodeOfLastLessValue(v);

        if(n) {
            key = n->KEY;
            value = n->VALUE;
            return true;
        } else {
            return false;
        }
    }

    bool GetElementOfLastLessEqualValue(const VALUE_TYPE &v, KEY_TYPE &key, VALUE_TYPE &value) {
        Node *n = GetNodeOfLastLessEqualValue(v);

        if(n) {
            key = n->KEY;
            value = n->VALUE;
            return true;
        } else {
            return false;
        }
    }

    std::string DumpLevels() {
        std::ostringstream ss;
        Node *x = m_header->LEVEL[0].FORWARD;
        unsigned long i = 0;

        while(x) {
            ss << "(" << ++i << ") " << x << ":" << "[" << x->KEY << "]" << "=" << x->VALUE;

            for(size_t k = 0; k < x->LEVEL.size(); ++k) {
                ss << " {" << k << ":" << x->LEVEL[k].SPAN << ":" << x->LEVEL[k].FORWARD << "}";
            }

            ss << "\n";
            
            x = x->LEVEL[0].FORWARD;
        }

        ss << "(sumary) " << "[level]=" << m_level << ", " << "[length]=" << m_length << "\n";

        return ss.str();
    }
};

template<typename KeyType, typename ValueType, int MaxLevel = 32, int BranchProbPercent = 25>
class ZeeSet {
public:
    using KEY_TYPE = KeyType;
    using VALUE_TYPE = ValueType;

    ZeeSet() {
        m_skiplist = new ZeeSkiplist<KeyType, ValueType, MaxLevel, BranchProbPercent>();
    }

    ~ZeeSet() {
        delete m_skiplist;
    }

    unsigned long Length() {
        return m_skiplist->Length();
    }

    unsigned long MaxRank() {
        return m_skiplist->MaxRank();
    }

    size_t Count() {
        return m_dict.size();
    }

    void Clear() {
        m_dict.clear();
        delete m_skiplist;
        m_skiplist = new ZeeSkiplist<KeyType, ValueType, MaxLevel, BranchProbPercent>();
    }

    void Update(const KEY_TYPE &key, const VALUE_TYPE &value) {
        auto iter = m_dict.find(key);

        if(iter == m_dict.end()) {
            m_skiplist->Insert(key, value);
            m_dict[key] = value;
        } else {
            m_skiplist->Update(key, iter->second, value);
            m_dict[key] = value;
        }
    }

    void Delete(const KEY_TYPE &key) {
        auto iter = m_dict.find(key);

        if(iter == m_dict.end()){
            return;
        }

        m_skiplist->Delete(key, iter->second);
        m_dict.erase(iter);
    }

    unsigned long GetRankOfElement(const KEY_TYPE &key) {
        auto iter = m_dict.find(key);

        if(iter == m_dict.end()) {
            return 0;
        }

        return m_skiplist->GetRankOfElement(key, iter->second);
    }

    bool GetElementByRank(unsigned long rank, KEY_TYPE &key, VALUE_TYPE &value) {
        return m_skiplist->GetElementByRank(rank, key, value);
    }

    void GetElementsByRangedRank(unsigned long rank_low, unsigned long rank_high, std::function<void(unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value)> cb) {
        m_skiplist->GetElementsByRangedRank(rank_low, rank_high, cb);
    }

    void ForeachElements(std::function<void(unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value)> cb) {
        m_skiplist->ForeachElements(cb);
    }

    void DeleteByRangedRank(unsigned long rank_low, unsigned long rank_high, std::function<void(unsigned long, const KEY_TYPE &key, const VALUE_TYPE &value)> cb) {
        m_skiplist->DeleteByRangedRank(rank_low, rank_high, [this, cb](unsigned long rank, const KEY_TYPE &key, const VALUE_TYPE &value){
                    this->m_dict.erase(key);

                    if(cb) {
                        cb( rank, key, value );
                    }
                });
    }

    bool GetElementOfFirstGreaterValue(const VALUE_TYPE &v, KEY_TYPE &key, VALUE_TYPE &value) {
        return m_skiplist->GetElementOfFirstGreaterValue(v, key, value);
    }

    bool GetElementOfFirstGreaterEqualValue(const VALUE_TYPE &v, KEY_TYPE &key, VALUE_TYPE &value) {
        return m_skiplist->GetElementOfFirstGreaterEqualValue(v, key, value);
    }

    bool GetElementOfLastLessValue(const VALUE_TYPE &v, KEY_TYPE &key, VALUE_TYPE &value) {
        return m_skiplist->GetElementOfLastLessValue(v, key, value);
    }

    bool GetElementOfLastLessEqualValue(const VALUE_TYPE &v, KEY_TYPE &key, VALUE_TYPE &value) {
        return m_skiplist->GetElementOfLastLessEqualValue(v, key, value);
    }

    std::string DumpLevels() {
        return m_skiplist->DumpLevels();
    }
private:
    ZeeSkiplist<KeyType, ValueType, MaxLevel, BranchProbPercent> *m_skiplist = NULL;
    std::map<KEY_TYPE, VALUE_TYPE> m_dict;
};

#endif

