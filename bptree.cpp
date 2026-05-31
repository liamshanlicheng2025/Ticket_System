#include "bptree.hpp"
#include <cstdio>
#include <cstring>
#include <climits>

BPTree::BPTree(const char* fname) {
    std::strncpy(filename, fname, 63);
    filename[63] = '\0';
    cache_slots = new CacheSlot[CACHE_SIZE];
    initCacheIndex();
    tree_file = std::fopen(filename, "r+b");
    if (!tree_file) {
        tree_file = std::fopen(filename, "w+b");
        initHeader();
        writeHeader();
        std::fflush(tree_file);
        return;
    }
    if (std::fread(&header, sizeof(Header), 1, tree_file) != 1 ||
        header.magic != MAGIC || header.version != VERSION) {
        std::fclose(tree_file);
        tree_file = std::fopen(filename, "w+b");
        initHeader();
        writeHeader();
        std::fflush(tree_file);
    }
}

BPTree::~BPTree() {
    flushAll();
    if (tree_file) std::fclose(tree_file);
    delete[] cache_slots;
}

void BPTree::initHeader() {
    std::memset(&header, 0, sizeof(header));
    header.magic = MAGIC;
    header.version = VERSION;
    header.root = -1;
    header.node_count = 0;
    header.free_head = -1;
}

void BPTree::writeHeader() {
    if (!tree_file) return;
    std::fseek(tree_file, 0, SEEK_SET);
    std::fwrite(&header, sizeof(Header), 1, tree_file);
}

void BPTree::readNodeDisk(int id, Node& node) {
    std::fseek(tree_file, static_cast<long>(sizeof(Header)) + static_cast<long>(id) * sizeof(Node), SEEK_SET);
    if (std::fread(&node, sizeof(Node), 1, tree_file) != 1) {
        std::memset(&node, 0, sizeof(Node));
        node.next = -1;
    }
}

void BPTree::writeNodeDisk(int id, const Node& node) {
    std::fseek(tree_file, static_cast<long>(sizeof(Header)) + static_cast<long>(id) * sizeof(Node), SEEK_SET);
    std::fwrite(&node, sizeof(Node), 1, tree_file);
}

void BPTree::flushSlot(int slot_id) {
    CacheSlot& slot = cache_slots[slot_id];
    if (slot.valid && slot.dirty) {
        writeNodeDisk(slot.id, slot.node);
        slot.dirty = false;
    }
}

int BPTree::cacheHash(int id) {
    unsigned int x = static_cast<unsigned int>(id);
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    return static_cast<int>(x & (CACHE_HASH_SIZE - 1));
}

void BPTree::initCacheIndex() {
    for (int i = 0; i < CACHE_HASH_SIZE; ++i) cache_head[i] = -1;
    for (int i = 0; i < CACHE_SIZE; ++i) {
        cache_slots[i].valid = false;
        cache_slots[i].stamp = 0;
        cache_slots[i].hash_next = -1;
    }
    cache_clock = 0;
}

int BPTree::findCacheSlot(int id) {
    int bucket = cacheHash(id);
    for (int s = cache_head[bucket]; s != -1; s = cache_slots[s].hash_next)
        if (cache_slots[s].id == id) return s;
    return -1;
}

void BPTree::linkCacheSlot(int slot_id, int id) {
    int bucket = cacheHash(id);
    cache_slots[slot_id].hash_next = cache_head[bucket];
    cache_head[bucket] = slot_id;
}

void BPTree::unlinkCacheSlot(int id) {
    int bucket = cacheHash(id);
    int prev = -1;
    int cur = cache_head[bucket];
    while (cur != -1) {
        int next = cache_slots[cur].hash_next;
        if (cache_slots[cur].id == id) {
            if (prev == -1) cache_head[bucket] = next;
            else cache_slots[prev].hash_next = next;
            cache_slots[cur].hash_next = -1;
            return;
        }
        prev = cur;
        cur = next;
    }
}

int BPTree::chooseCacheSlot() {
    for (int i = 0; i < CACHE_SIZE; ++i)
        if (!cache_slots[i].valid) return i;
    int victim = 0;
    for (int i = 1; i < CACHE_SIZE; ++i)
        if (cache_slots[i].stamp < cache_slots[victim].stamp) victim = i;
    flushSlot(victim);
    unlinkCacheSlot(cache_slots[victim].id);
    cache_slots[victim].valid = false;
    return victim;
}

BPTree::Node& BPTree::getNode(int id) {
    if (id == -1) {
        static Node dummy;
        std::memset(&dummy, 0, sizeof(dummy));
        dummy.next = -1;
        return dummy;
    }
    int found = findCacheSlot(id);
    if (found != -1) {
        CacheSlot& slot = cache_slots[found];
        slot.stamp = ++cache_clock;
        return slot.node;
    }
    int slot_id = chooseCacheSlot();
    CacheSlot& slot = cache_slots[slot_id];
    slot.valid = true;
    slot.dirty = false;
    slot.id = id;
    slot.stamp = ++cache_clock;
    readNodeDisk(id, slot.node);
    linkCacheSlot(slot_id, id);
    return slot.node;
}

void BPTree::markDirty(int id) {
    int found = findCacheSlot(id);
    if (found != -1) cache_slots[found].dirty = true;
}

BPTree::Node& BPTree::initNodeInCache(int id, int type) {
    int slot_id = findCacheSlot(id);
    bool linked = (slot_id != -1);
    if (!linked) slot_id = chooseCacheSlot();
    CacheSlot& slot = cache_slots[slot_id];
    slot.valid = true;
    slot.dirty = true;
    slot.id = id;
    slot.stamp = ++cache_clock;
    if (!linked) linkCacheSlot(slot_id, id);
    std::memset(&slot.node, 0, sizeof(Node));
    slot.node.type = type;
    slot.node.size = 0;
    slot.node.next = -1;
    for (int i = 0; i <= ORDER; ++i) slot.node.children[i] = -1;
    return slot.node;
}

int BPTree::newNode(int type) {
    int id;
    if (header.free_head != -1) {
        id = header.free_head;
        Node& old = getNode(id);
        header.free_head = old.next;
    } else {
        id = header.node_count++;
    }
    initNodeInCache(id, type);
    return id;
}

void BPTree::releaseNode(int id) {
    if (id == -1) return;
    Node& node = getNode(id);
    node.type = 1;  // leaf
    node.size = 0;
    node.next = header.free_head;
    header.free_head = id;
    markDirty(id);
}

void BPTree::flushAll() {
    for (int i = 0; i < CACHE_SIZE; ++i) flushSlot(i);
    writeHeader();
    if (tree_file) std::fflush(tree_file);
}

int BPTree::internalFindChild(const Node& node, const char key[KEY_SIZE], int val) {
    int lo = 0, hi = node.size;
    while (lo < hi) {
        int mid = (lo + hi) >> 1;
        if (item_cmp(node.keys[mid], node.vals[mid], key, val) <= 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

int BPTree::internalFindChildGE(const Node& node, const char key[KEY_SIZE], int val) {
    int lo = 0, hi = node.size;
    while (lo < hi) {
        int mid = (lo + hi) >> 1;
        if (item_cmp(node.keys[mid], node.vals[mid], key, val) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

int BPTree::findLeaf(const char key[KEY_SIZE], int val, int path[], int pos_in_parent[], int* depth_out) {
    int cur = header.root;
    int depth = 0;
    while (cur != -1) {
        Node& node = getNode(cur);
        if (node.type == 1) break;
        int pos = internalFindChild(node, key, val);
        if (path) path[depth] = cur;
        if (pos_in_parent) pos_in_parent[depth] = pos;
        ++depth;
        cur = node.children[pos];
    }
    if (depth_out) *depth_out = depth;
    return cur;
}

int BPTree::findLeafGE(const char key[KEY_SIZE], int val) {
    int cur = header.root;
    while (cur != -1) {
        Node& node = getNode(cur);
        if (node.type == 1) break;
        int pos = internalFindChildGE(node, key, val);
        cur = node.children[pos];
    }
    return cur;
}

bool BPTree::firstItem(int node_id, char key[KEY_SIZE], int& val) {
    if (node_id == -1) return false;
    int cur = node_id;
    while (cur != -1) {
        Node& node = getNode(cur);
        if (node.type == 1) {
            if (node.size == 0) return false;
            copy_key(key, node.keys[0]);
            val = node.vals[0];
            return true;
        }
        cur = node.children[0];
    }
    return false;
}

void BPTree::propagateFirstChange(int path[], int pos_in_parent[], int depth, int child_id) {
    char key[KEY_SIZE];
    int val;
    if (!firstItem(child_id, key, val)) return;
    for (int i = depth - 1; i >= 0; --i) {
        int parent_id = path[i];
        int child_pos = pos_in_parent[i];
        Node& parent = getNode(parent_id);
        if (child_pos > 0) {
            copy_key(parent.keys[child_pos - 1], key);
            parent.vals[child_pos - 1] = val;
            markDirty(parent_id);
            return;
        }
    }
}

void BPTree::refreshInternalNode(int node_id) {
    if (node_id == -1) return;
    Node& node = getNode(node_id);
    if (node.type != 0) return;
    for (int i = 0; i < node.size; ++i) {
        if (node.children[i + 1] != -1) {   // 跳过无效子节点
            firstItem(node.children[i + 1], node.keys[i], node.vals[i]);
        }
    }
    markDirty(node_id);
}

int BPTree::leafInsertPos(const Node& leaf, const char key[KEY_SIZE], int val) {
    int lo = 0, hi = leaf.size;
    while (lo < hi) {
        int mid = (lo + hi) >> 1;
        if (item_cmp(leaf.keys[mid], leaf.vals[mid], key, val) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

int BPTree::insertInLeaf(int leaf_id, const char key[KEY_SIZE], int val) {
    Node& leaf = getNode(leaf_id);
    int pos = leafInsertPos(leaf, key, val);
    if (pos < leaf.size && item_cmp(leaf.keys[pos], leaf.vals[pos], key, val) == 0)
        return -1;
    for (int i = leaf.size; i > pos; --i) {
        copy_key(leaf.keys[i], leaf.keys[i - 1]);
        leaf.vals[i] = leaf.vals[i - 1];
    }
    copy_key(leaf.keys[pos], key);
    leaf.vals[pos] = val;
    ++leaf.size;
    markDirty(leaf_id);
    return pos;
}

void BPTree::splitLeaf(int leaf_id, const char key[KEY_SIZE], int val,
                       char push_key[KEY_SIZE], int& push_val, int& new_leaf_id) {
    Node& leaf = getNode(leaf_id);
    char temp_keys[ORDER + 1][KEY_SIZE];
    int temp_vals[ORDER + 1];
    for (int i = 0; i < leaf.size; ++i) {
        copy_key(temp_keys[i], leaf.keys[i]);
        temp_vals[i] = leaf.vals[i];
    }
    int ins = leafInsertPos(leaf, key, val);
    for (int i = leaf.size; i > ins; --i) {
        copy_key(temp_keys[i], temp_keys[i - 1]);
        temp_vals[i] = temp_vals[i - 1];
    }
    copy_key(temp_keys[ins], key);
    temp_vals[ins] = val;
    int total = leaf.size + 1;
    int split = (ins == leaf.size) ? leaf.size : (total + 1) / 2;
    leaf.size = split;
    for (int i = 0; i < split; ++i) {
        copy_key(leaf.keys[i], temp_keys[i]);
        leaf.vals[i] = temp_vals[i];
    }
    new_leaf_id = newNode(1); // LEAF
    Node& new_leaf = getNode(new_leaf_id);
    new_leaf.size = total - split;
    for (int i = 0; i < new_leaf.size; ++i) {
        copy_key(new_leaf.keys[i], temp_keys[split + i]);
        new_leaf.vals[i] = temp_vals[split + i];
    }
    new_leaf.next = leaf.next;
    leaf.next = new_leaf_id;
    copy_key(push_key, new_leaf.keys[0]);
    push_val = new_leaf.vals[0];
    markDirty(leaf_id);
    markDirty(new_leaf_id);
}

void BPTree::insertInInternal(int parent_id, int child_pos,
                              const char key[KEY_SIZE], int val, int child_id) {
    Node& parent = getNode(parent_id);
    for (int i = parent.size; i > child_pos; --i) {
        parent.children[i + 1] = parent.children[i];
        copy_key(parent.keys[i], parent.keys[i - 1]);
        parent.vals[i] = parent.vals[i - 1];
    }
    parent.children[child_pos + 1] = child_id;
    copy_key(parent.keys[child_pos], key);
    parent.vals[child_pos] = val;
    ++parent.size;
    markDirty(parent_id);
}

void BPTree::insertEntry(const char key[KEY_SIZE], int val) {
    if (header.root == -1) {
        int leaf_id = newNode(1);
        Node& leaf = getNode(leaf_id);
        copy_key(leaf.keys[0], key);
        leaf.vals[0] = val;
        leaf.size = 1;
        markDirty(leaf_id);
        header.root = leaf_id;
        return;
    }
    int path[MAX_HEIGHT], pos_in_parent[MAX_HEIGHT], depth = 0;
    int leaf_id = findLeaf(key, val, path, pos_in_parent, &depth);
    Node& leaf = getNode(leaf_id);
    int pos = leafInsertPos(leaf, key, val);
    if (pos < leaf.size && item_cmp(leaf.keys[pos], leaf.vals[pos], key, val) == 0)
        return;
    if (leaf.size < ORDER) {
        insertInLeaf(leaf_id, key, val);
        if (pos == 0) propagateFirstChange(path, pos_in_parent, depth, leaf_id);
        return;
    }
    char up_key[KEY_SIZE];
    int up_val;
    int child_id;
    splitLeaf(leaf_id, key, val, up_key, up_val, child_id);
    propagateFirstChange(path, pos_in_parent, depth, leaf_id);
    int level = depth - 1;
    while (level >= 0) {
        int parent_id = path[level];
        int child_pos = pos_in_parent[level];
        Node& parent = getNode(parent_id);
        if (parent.size < ORDER) {
            insertInInternal(parent_id, child_pos, up_key, up_val, child_id);
            return;
        }
        char temp_keys[ORDER + 1][KEY_SIZE];
        int temp_vals[ORDER + 1];
        int temp_children[ORDER + 2];
        for (int i = 0; i <= parent.size; ++i) temp_children[i] = parent.children[i];
        for (int i = 0; i < parent.size; ++i) {
            copy_key(temp_keys[i], parent.keys[i]);
            temp_vals[i] = parent.vals[i];
        }
        for (int i = parent.size; i > child_pos; --i) temp_children[i + 1] = temp_children[i];
        temp_children[child_pos + 1] = child_id;
        for (int i = parent.size - 1; i >= child_pos; --i) {
            copy_key(temp_keys[i + 1], temp_keys[i]);
            temp_vals[i + 1] = temp_vals[i];
        }
        copy_key(temp_keys[child_pos], up_key);
        temp_vals[child_pos] = up_val;
        int total_keys = parent.size + 1;
        int split = total_keys / 2;
        char new_up_key[KEY_SIZE];
        int new_up_val = temp_vals[split];
        copy_key(new_up_key, temp_keys[split]);
        parent.size = split;
        for (int i = 0; i <= split; ++i) parent.children[i] = temp_children[i];
        for (int i = 0; i < split; ++i) {
            copy_key(parent.keys[i], temp_keys[i]);
            parent.vals[i] = temp_vals[i];
        }
        markDirty(parent_id);
        int new_internal_id = newNode(0); // INTERNAL
        Node& right = getNode(new_internal_id);
        right.size = total_keys - split - 1;
        for (int i = 0; i <= right.size; ++i) right.children[i] = temp_children[split + 1 + i];
        for (int i = 0; i < right.size; ++i) {
            copy_key(right.keys[i], temp_keys[split + 1 + i]);
            right.vals[i] = temp_vals[split + 1 + i];
        }
        markDirty(new_internal_id);
        child_id = new_internal_id;
        copy_key(up_key, new_up_key);
        up_val = new_up_val;
        --level;
    }
    int new_root = newNode(0);
    Node& root = getNode(new_root);
    root.children[0] = header.root;
    root.children[1] = child_id;
    root.size = 1;
    copy_key(root.keys[0], up_key);
    root.vals[0] = up_val;
    markDirty(new_root);
    header.root = new_root;
}

void BPTree::insert(const char key[KEY_SIZE], int val) {
    insertEntry(key, val);
}

void BPTree::leafRemoveAt(int leaf_id, int pos) {
    Node& leaf = getNode(leaf_id);
    for (int i = pos; i < leaf.size - 1; ++i) {
        copy_key(leaf.keys[i], leaf.keys[i + 1]);
        leaf.vals[i] = leaf.vals[i + 1];
    }
    --leaf.size;
    markDirty(leaf_id);
}

void BPTree::borrowLeafLeft(int leaf_id, int left_id, int parent_id, int key_idx) {
    Node& leaf = getNode(leaf_id);
    Node& left = getNode(left_id);
    int last = left.size - 1;
    for (int i = leaf.size; i > 0; --i) {
        copy_key(leaf.keys[i], leaf.keys[i - 1]);
        leaf.vals[i] = leaf.vals[i - 1];
    }
    copy_key(leaf.keys[0], left.keys[last]);
    leaf.vals[0] = left.vals[last];
    ++leaf.size;
    --left.size;
    markDirty(leaf_id);
    markDirty(left_id);
    Node& parent = getNode(parent_id);
    copy_key(parent.keys[key_idx], leaf.keys[0]);
    parent.vals[key_idx] = leaf.vals[0];
    markDirty(parent_id);
}

void BPTree::borrowLeafRight(int leaf_id, int right_id, int parent_id, int key_idx) {
    Node& leaf = getNode(leaf_id);
    Node& right = getNode(right_id);
    copy_key(leaf.keys[leaf.size], right.keys[0]);
    leaf.vals[leaf.size] = right.vals[0];
    ++leaf.size;
    for (int i = 0; i < right.size - 1; ++i) {
        copy_key(right.keys[i], right.keys[i + 1]);
        right.vals[i] = right.vals[i + 1];
    }
    --right.size;
    markDirty(leaf_id);
    markDirty(right_id);
    Node& parent = getNode(parent_id);
    copy_key(parent.keys[key_idx], right.keys[0]);
    parent.vals[key_idx] = right.vals[0];
    markDirty(parent_id);
}

void BPTree::mergeLeaf(int left_id, int right_id, int parent_id, int key_idx) {
    Node& left = getNode(left_id);
    Node& right = getNode(right_id);
    int base = left.size;
    for (int i = 0; i < right.size; ++i) {
        copy_key(left.keys[base + i], right.keys[i]);
        left.vals[base + i] = right.vals[i];
    }
    left.size += right.size;
    left.next = right.next;
    markDirty(left_id);
    Node& parent = getNode(parent_id);
    for (int i = key_idx; i < parent.size - 1; ++i) {
        copy_key(parent.keys[i], parent.keys[i + 1]);
        parent.vals[i] = parent.vals[i + 1];
        parent.children[i + 1] = parent.children[i + 2];
    }
    --parent.size;
    markDirty(parent_id);
    releaseNode(right_id);
}

void BPTree::borrowInternalLeft(int node_id, int left_id, int parent_id, int key_idx) {
    Node& node = getNode(node_id);
    Node& left = getNode(left_id);
    Node& parent = getNode(parent_id);
    int moved_child = left.children[left.size];
    for (int i = node.size; i >= 0; --i) node.children[i + 1] = node.children[i];
    for (int i = node.size - 1; i >= 0; --i) {
        copy_key(node.keys[i + 1], node.keys[i]);
        node.vals[i + 1] = node.vals[i];
    }
    node.children[0] = moved_child;
    copy_key(node.keys[0], parent.keys[key_idx]);
    node.vals[0] = parent.vals[key_idx];
    ++node.size;
    --left.size;
    copy_key(parent.keys[key_idx], left.keys[left.size]);
    parent.vals[key_idx] = left.vals[left.size];
    markDirty(node_id);
    markDirty(left_id);
    markDirty(parent_id);
}

void BPTree::borrowInternalRight(int node_id, int right_id, int parent_id, int key_idx) {
    Node& node = getNode(node_id);
    Node& right = getNode(right_id);
    Node& parent = getNode(parent_id);
    char new_parent_key[KEY_SIZE];
    int new_parent_val = right.vals[0];
    copy_key(new_parent_key, right.keys[0]);
    copy_key(node.keys[node.size], parent.keys[key_idx]);
    node.vals[node.size] = parent.vals[key_idx];
    node.children[node.size + 1] = right.children[0];
    ++node.size;
    for (int i = 0; i < right.size; ++i) right.children[i] = right.children[i + 1];
    for (int i = 0; i < right.size - 1; ++i) {
        copy_key(right.keys[i], right.keys[i + 1]);
        right.vals[i] = right.vals[i + 1];
    }
    --right.size;
    copy_key(parent.keys[key_idx], new_parent_key);
    parent.vals[key_idx] = new_parent_val;
    markDirty(node_id);
    markDirty(right_id);
    markDirty(parent_id);
}

void BPTree::mergeInternal(int left_id, int right_id, int parent_id, int key_idx) {
    Node& left = getNode(left_id);
    Node& right = getNode(right_id);
    Node& parent = getNode(parent_id);
    int base = left.size;
    copy_key(left.keys[base], parent.keys[key_idx]);
    left.vals[base] = parent.vals[key_idx];
    ++left.size;
    for (int i = 0; i <= right.size; ++i) left.children[left.size + i] = right.children[i];
    for (int i = 0; i < right.size; ++i) {
        copy_key(left.keys[left.size + i], right.keys[i]);
        left.vals[left.size + i] = right.vals[i];
    }
    left.size += right.size;
    markDirty(left_id);
    for (int i = key_idx; i < parent.size - 1; ++i) {
        copy_key(parent.keys[i], parent.keys[i + 1]);
        parent.vals[i] = parent.vals[i + 1];
        parent.children[i + 1] = parent.children[i + 2];
    }
    --parent.size;
    markDirty(parent_id);
    releaseNode(right_id);
}

void BPTree::handleUnderflow(int node_id, int parent_id, int child_pos) {
    Node& parent = getNode(parent_id);
    Node& node = getNode(node_id);
    if (child_pos > 0) {
        int left_id = parent.children[child_pos - 1];
        if (getNode(left_id).size > MIN_KEYS) {
            if (node.type == 1)
                borrowLeafLeft(node_id, left_id, parent_id, child_pos - 1);
            else
                borrowInternalLeft(node_id, left_id, parent_id, child_pos - 1);
            return;
        }
    }
    if (child_pos < parent.size) {
        int right_id = parent.children[child_pos + 1];
        if (getNode(right_id).size > MIN_KEYS) {
            if (node.type == 1)
                borrowLeafRight(node_id, right_id, parent_id, child_pos);
            else
                borrowInternalRight(node_id, right_id, parent_id, child_pos);
            return;
        }
    }
    if (child_pos > 0) {
        int left_id = parent.children[child_pos - 1];
        if (node.type == 1)
            mergeLeaf(left_id, node_id, parent_id, child_pos - 1);
        else
            mergeInternal(left_id, node_id, parent_id, child_pos - 1);
    } else {
        int right_id = parent.children[child_pos + 1];
        if (node.type == 1)
            mergeLeaf(node_id, right_id, parent_id, child_pos);
        else
            mergeInternal(node_id, right_id, parent_id, child_pos);
    }
}

void BPTree::deleteEntry(const char key[KEY_SIZE], int val) {
    if (header.root == -1) return;
    int path[MAX_HEIGHT], pos_in_parent[MAX_HEIGHT], depth = 0;
    int leaf_id = findLeaf(key, val, path, pos_in_parent, &depth);
    Node& leaf = getNode(leaf_id);
    int pos = leafInsertPos(leaf, key, val);
    if (pos == leaf.size || item_cmp(leaf.keys[pos], leaf.vals[pos], key, val) != 0)
        return;
    leafRemoveAt(leaf_id, pos);
    Node& after_remove = getNode(leaf_id);
    if (leaf_id == header.root) {
        if (after_remove.size == 0) {
            releaseNode(header.root);
            header.root = -1;
        }
        return;
    }
    if (pos == 0) propagateFirstChange(path, pos_in_parent, depth, leaf_id);
    if (after_remove.size >= MIN_KEYS) return;
    int cur_node = leaf_id;
    int cur_depth = depth;
    while (cur_depth > 0) {
        int parent_id = path[cur_depth - 1];
        int child_pos = pos_in_parent[cur_depth - 1];
        handleUnderflow(cur_node, parent_id, child_pos);
        refreshInternalNode(parent_id);
        Node& parent = getNode(parent_id);
        if (parent_id == header.root) {
            if (parent.size == 0) {
                header.root = parent.children[0];
                releaseNode(parent_id);
            }
            break;
        }
        if (parent.size >= MIN_KEYS) break;
        cur_node = parent_id;
        --cur_depth;
    }
}

void BPTree::remove(const char key[KEY_SIZE], int val) {
    deleteEntry(key, val);
}

bool BPTree::findFirst(const char key[KEY_SIZE], int &val) {
    int leaf_id = findLeaf(key, INT_MIN);
    if (leaf_id == -1) return false;
    Node& leaf = getNode(leaf_id);
    for (int i = 0; i < leaf.size; ++i) {
        if (key_cmp(leaf.keys[i], key) == 0) {
            val = leaf.vals[i];
            return true;
        }
    }
    return false;
}

void BPTree::scanPrefix(const char *prefix, int prefixLen,
                        bool (*callback)(const char[KEY_SIZE], int, void*), void *ctx) {
    char search_key[KEY_SIZE];
    std::memset(search_key, 0, KEY_SIZE);
    std::memcpy(search_key, prefix, prefixLen);
    int leaf_id = findLeafGE(search_key, INT_MIN);
    while (leaf_id != -1) {
        Node& leaf = getNode(leaf_id);
        for (int i = 0; i < leaf.size; ++i) {
            if (std::memcmp(leaf.keys[i], prefix, prefixLen) != 0)
                return;
            if (!callback(leaf.keys[i], leaf.vals[i], ctx))
                return;
        }
        leaf_id = leaf.next;
    }
}