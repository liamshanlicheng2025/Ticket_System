#ifndef BPTREE_HPP
#define BPTREE_HPP
#include <cstring>
#include <cstdio>

class BPTree {
public:
    static const int KEY_SIZE = 64;
    static const int ORDER = 100;

    BPTree(const char *filename);
    ~BPTree();

    void insert(const char key[KEY_SIZE], int val);
    void remove(const char key[KEY_SIZE], int val);
    bool findFirst(const char key[KEY_SIZE], int &val);

    void scanPrefix(const char *prefix, int prefixLen,
                    bool (*callback)(const char key[KEY_SIZE], int val, void *ctx),
                    void *ctx);

private:
    struct Node {
        int type;          // 0:internal, 1:leaf
        int size;
        int next;
        int children[ORDER + 1];
        char keys[ORDER][KEY_SIZE];
        int vals[ORDER];
    };

    struct Header {
        unsigned int magic;
        unsigned int version;
        int root;
        int node_count;
        int free_head;
        char padding[4076];
    };

    struct CacheSlot {
        bool valid;
        bool dirty;
        int id;
        int hash_next;
        unsigned long long stamp;
        Node node;
    };

    static const int MIN_KEYS = ORDER / 2;
    static const int CACHE_SIZE = 200;
    static const int CACHE_HASH_SIZE = 1024;
    static const int MAX_HEIGHT = 16;
    static const unsigned int MAGIC = 0x33545042u;
    static const unsigned int VERSION = 5;

    FILE *tree_file;
    Header header;
    CacheSlot *cache_slots;   // 改为堆分配指针
    int cache_head[CACHE_HASH_SIZE];
    unsigned long long cache_clock;
    char filename[64];

    // 禁止拷贝
    BPTree(const BPTree&);
    BPTree& operator=(const BPTree&);

    void initHeader();
    void writeHeader();
    void readNodeDisk(int id, Node &node);
    void writeNodeDisk(int id, const Node &node);
    void flushSlot(int slot_id);
    int cacheHash(int id);
    void initCacheIndex();
    int findCacheSlot(int id);
    void linkCacheSlot(int slot_id, int id);
    void unlinkCacheSlot(int id);
    int chooseCacheSlot();
    Node& getNode(int id);
    void markDirty(int id);
    Node& initNodeInCache(int id, int type);
    int newNode(int type);
    void releaseNode(int id);
    void flushAll();

    int internalFindChild(const Node &node, const char key[KEY_SIZE], int val);
    int internalFindChildGE(const Node &node, const char key[KEY_SIZE], int val);
    int findLeaf(const char key[KEY_SIZE], int val, int path[] = nullptr,
                 int pos_in_parent[] = nullptr, int *depth_out = nullptr);
    int findLeafGE(const char key[KEY_SIZE], int val);
    bool firstItem(int node_id, char key[KEY_SIZE], int &val);
    void propagateFirstChange(int path[], int pos_in_parent[], int depth, int child_id);
    void refreshInternalNode(int node_id);

    int leafInsertPos(const Node &leaf, const char key[KEY_SIZE], int val);
    int insertInLeaf(int leaf_id, const char key[KEY_SIZE], int val);
    void splitLeaf(int leaf_id, const char key[KEY_SIZE], int val,
                   char push_key[KEY_SIZE], int &push_val, int &new_leaf_id);
    void insertInInternal(int parent_id, int child_pos,
                          const char key[KEY_SIZE], int val, int child_id);
    void insertEntry(const char key[KEY_SIZE], int val);

    void leafRemoveAt(int leaf_id, int pos);
    void borrowLeafLeft(int leaf_id, int left_id, int parent_id, int key_idx);
    void borrowLeafRight(int leaf_id, int right_id, int parent_id, int key_idx);
    void mergeLeaf(int left_id, int right_id, int parent_id, int key_idx);
    void borrowInternalLeft(int node_id, int left_id, int parent_id, int key_idx);
    void borrowInternalRight(int node_id, int right_id, int parent_id, int key_idx);
    void mergeInternal(int left_id, int right_id, int parent_id, int key_idx);
    void handleUnderflow(int node_id, int parent_id, int child_pos);
    void deleteEntry(const char key[KEY_SIZE], int val);

    static void copy_key(char dest[KEY_SIZE], const char src[KEY_SIZE]) {
        std::memcpy(dest, src, KEY_SIZE);
    }
    static int key_cmp(const char *a, const char *b) {
        return std::memcmp(a, b, KEY_SIZE);
    }
    static int item_cmp(const char *k1, int v1, const char *k2, int v2) {
        int c = key_cmp(k1, k2);
        if (c) return c;
        if (v1 < v2) return -1;
        if (v1 > v2) return 1;
        return 0;
    }
};

#endif
