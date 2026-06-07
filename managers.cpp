#include "managers.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>

int dateToDays(int month, int day) {
    static int cum[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    return cum[month - 1] + (day - 1);
}
void daysToDate(int days, int &month, int &day) {
    static int cum[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};
    for (int i = 0; i < 12; ++i) {
        if (days < cum[i + 1]) {
            month = i + 1;
            day = days - cum[i] + 1;
            return;
        }
    }
    month = 12; day = 31;
}
int timeToMin(int h, int m) { return h * 60 + m; }
void minToTime(int mins, int &h, int &m) { h = mins / 60; m = mins % 60; }
void makeKey64(char dest[64], const char *s, int len) {
    std::memset(dest, 0, 64);
    if (s) std::memcpy(dest, s, len < 64 ? len : 64);
}

// UserManager
unsigned int UserManager::onlineHash(const char *s) {
    unsigned int h = 0;
    while (*s) {
        h = h * 31 + static_cast<unsigned char>(*s);
        ++s;
    }
    return h & (ONLINE_HASH_SIZE - 1);
}

void UserManager::expandOnline() {
    int newCap = onlineCap ? onlineCap * 2 : 64;
    OnlineEntry *newPool = new OnlineEntry[newCap];
    if (onlinePool) {
        std::memcpy(newPool, onlinePool, sizeof(OnlineEntry) * onlineCap);
        delete[] onlinePool;
    }
    for (int i = onlineCap; i < newCap - 1; ++i) newPool[i].next = i + 1;
    newPool[newCap - 1].next = onlineFree;
    onlineFree = onlineCap;
    onlinePool = newPool;
    onlineCap = newCap;
}

UserManager::UserManager() : index("user_index.dat"), onlineCnt(0), userCount(0),
        onlinePool(nullptr), onlineHead(nullptr), onlineCap(0), onlineFree(-1) {
    file = std::fopen("users.dat", "r+b");
    if (!file) file = std::fopen("users.dat", "w+b");
    if (file) {
        std::fseek(file, 0, SEEK_END);
        userCount = std::ftell(file) / sizeof(UserRecord);
    }
    onlineHead = new int[ONLINE_HASH_SIZE];
    for (int i = 0; i < ONLINE_HASH_SIZE; ++i) onlineHead[i] = -1;
    expandOnline();
}
UserManager::~UserManager() {
    if (file) std::fclose(file);
    delete[] onlinePool;
    delete[] onlineHead;
}

int UserManager::addUser(const char *cur, const char *username, const char *pass,
                         const char *name, const char *mail, int priv, bool first) {
    char key[64];
    makeKey64(key, username, std::strlen(username));
    int tmp;
    if (index.findFirst(key, tmp)) return -1;
    if (!first) {
        if (!isOnline(cur)) return -1;
        if (getPrivilege(cur) <= priv) return -1;
    }
    UserRecord rec;
    std::memset(&rec, 0, sizeof(rec));
    std::strncpy(rec.username, username, 20);
    std::strncpy(rec.password, pass, 30);
    std::strncpy(rec.name, name, 30);
    std::strncpy(rec.mail, mail, 30);
    rec.privilege = first ? 10 : priv;
    std::fseek(file, userCount * sizeof(UserRecord), SEEK_SET);
    std::fwrite(&rec, sizeof(UserRecord), 1, file);
    index.insert(key, userCount++);
    return 0;
}

int UserManager::login(const char *username, const char *pass) {
    char key[64];
    makeKey64(key, username, std::strlen(username));
    int id;
    if (!index.findFirst(key, id)) return -1;
    UserRecord rec;
    std::fseek(file, id * sizeof(UserRecord), SEEK_SET);
    std::fread(&rec, sizeof(UserRecord), 1, file);
    if (std::strcmp(rec.password, pass)) return -1;
    unsigned int h = onlineHash(username);
    for (int e = onlineHead[h]; e != -1; e = onlinePool[e].next)
        if (!std::strcmp(onlinePool[e].name, username)) return -1;
    if (onlineFree == -1) expandOnline();
    int slot = onlineFree;
    onlineFree = onlinePool[slot].next;
    std::strncpy(onlinePool[slot].name, username, 20);
    onlinePool[slot].privilege = rec.privilege;
    onlinePool[slot].next = onlineHead[h];
    onlineHead[h] = slot;
    ++onlineCnt;
    return 0;
}

int UserManager::logout(const char *username) {
    unsigned int h = onlineHash(username);
    int prev = -1;
    for (int e = onlineHead[h]; e != -1; e = onlinePool[e].next) {
        if (!std::strcmp(onlinePool[e].name, username)) {
            if (prev == -1) onlineHead[h] = onlinePool[e].next;
            else onlinePool[prev].next = onlinePool[e].next;
            onlinePool[e].next = onlineFree;
            onlineFree = e;
            --onlineCnt;
            return 0;
        }
        prev = e;
    }
    return -1;
}

bool UserManager::isOnline(const char *username) {
    unsigned int h = onlineHash(username);
    for (int e = onlineHead[h]; e != -1; e = onlinePool[e].next)
        if (!std::strcmp(onlinePool[e].name, username)) return true;
    return false;
}

int UserManager::getPrivilege(const char *username) {
    unsigned int h = onlineHash(username);
    for (int e = onlineHead[h]; e != -1; e = onlinePool[e].next)
        if (!std::strcmp(onlinePool[e].name, username)) return onlinePool[e].privilege;
    return -1;
}

int UserManager::queryProfile(const char *cur, const char *username) {
    if (!isOnline(cur)) return -1;
    int curPriv = getPrivilege(cur);
    char key[64];
    makeKey64(key, username, std::strlen(username));
    int id;
    if (!index.findFirst(key, id)) return -1;
    UserRecord rec;
    std::fseek(file, id * sizeof(UserRecord), SEEK_SET);
    std::fread(&rec, sizeof(UserRecord), 1, file);
    if (curPriv <= rec.privilege && std::strcmp(cur, username)) return -1;
    return id;
}

int UserManager::modifyProfile(const char *cur, const char *username,
                               const char *pass, const char *name,
                               const char *mail, int priv) {
    if (!isOnline(cur)) return -1;
    int curPriv = getPrivilege(cur);
    char key[64];
    makeKey64(key, username, std::strlen(username));
    int id;
    if (!index.findFirst(key, id)) return -1;
    UserRecord rec;
    std::fseek(file, id * sizeof(UserRecord), SEEK_SET);
    std::fread(&rec, sizeof(UserRecord), 1, file);
    if (curPriv <= rec.privilege && std::strcmp(cur, username)) return -1;
    if (priv != -1 && priv >= curPriv) return -1;
    if (pass) std::strncpy(rec.password, pass, 30);
    if (name) std::strncpy(rec.name, name, 30);
    if (mail) std::strncpy(rec.mail, mail, 30);
    if (priv != -1) {
        rec.privilege = priv;
        unsigned int h = onlineHash(username);
        for (int e = onlineHead[h]; e != -1; e = onlinePool[e].next)
            if (!std::strcmp(onlinePool[e].name, username)) { onlinePool[e].privilege = priv; break; }
    }
    std::fseek(file, id * sizeof(UserRecord), SEEK_SET);
    std::fwrite(&rec, sizeof(UserRecord), 1, file);
    return id;
}

// TrainManager
TrainManager::TrainManager() : index("train_index.dat"), trainCount(0) {
    file = std::fopen("trains.dat", "r+b");
    if (!file) file = std::fopen("trains.dat", "w+b");
    if (file) {
        static char buf[65536];
        std::setvbuf(file, buf, _IOFBF, sizeof(buf));
        std::fseek(file, 0, SEEK_END);
        trainCount = std::ftell(file) / sizeof(TrainRecord);
    }
}
TrainManager::~TrainManager() { if (file) std::fclose(file); }

int TrainManager::addTrain(const char *trainID, int stationNum, int seatNum,
                           const char stations[][31], int prices[], int startTime,
                           int travelTimes[], int stopoverTimes[],
                           int sale1, int sale2, char type) {
    char key[64];
    makeKey64(key, trainID, std::strlen(trainID));
    int tmp;
    if (index.findFirst(key, tmp)) return -1;
    TrainRecord rec;
    std::memset(&rec, 0, sizeof(rec));
    std::strncpy(rec.trainID, trainID, 20);
    rec.stationNum = stationNum;
    rec.seatNum = seatNum;
    rec.startTime = startTime;
    rec.saleDate1 = sale1;
    rec.saleDate2 = sale2;
    rec.type = type;
    rec.released = false;
    for (int i = 0; i < stationNum; ++i) std::strncpy(rec.stations[i], stations[i], 30);
    for (int i = 0; i < stationNum - 1; ++i) {
        rec.prices[i] = prices[i];
        rec.travelTimes[i] = travelTimes[i];
    }
    for (int i = 0; i < stationNum - 2; ++i) rec.stopoverTimes[i] = stopoverTimes[i];
    std::fseek(file, trainCount * sizeof(TrainRecord), SEEK_SET);
    std::fwrite(&rec, sizeof(TrainRecord), 1, file);
    index.insert(key, trainCount++);
    return 0;
}

int TrainManager::deleteTrain(const char *trainID) {
    char key[64];
    makeKey64(key, trainID, std::strlen(trainID));
    int id;
    if (!index.findFirst(key, id)) return -1;
    TrainRecord rec;
    std::fseek(file, id * sizeof(TrainRecord), SEEK_SET);
    std::fread(&rec, sizeof(TrainRecord), 1, file);
    if (rec.released) return -1;
    index.remove(key, id);
    return 0;
}

int TrainManager::releaseTrain(const char *trainID) {
    char key[64];
    makeKey64(key, trainID, std::strlen(trainID));
    int id;
    if (!index.findFirst(key, id)) return -1;
    TrainRecord rec;
    std::fseek(file, id * sizeof(TrainRecord), SEEK_SET);
    std::fread(&rec, sizeof(TrainRecord), 1, file);
    if (rec.released) return -1;
    rec.released = true;
    std::fseek(file, id * sizeof(TrainRecord), SEEK_SET);
    std::fwrite(&rec, sizeof(TrainRecord), 1, file);
    return 0;
}

int TrainManager::queryTrain(const char *trainID, int date) {
    char key[64];
    makeKey64(key, trainID, std::strlen(trainID));
    int id;
    if (!index.findFirst(key, id)) return -1;
    TrainRecord rec;
    std::fseek(file, id * sizeof(TrainRecord), SEEK_SET);
    std::fread(&rec, sizeof(TrainRecord), 1, file);
    if (date < rec.saleDate1 || date > rec.saleDate2) return -1;
    return id;
}

bool TrainManager::getTrain(const char *trainID, TrainRecord &rec) {
    char key[64];
    makeKey64(key, trainID, std::strlen(trainID));
    int id;
    if (!index.findFirst(key, id)) return false;
    std::fseek(file, id * sizeof(TrainRecord), SEEK_SET);
    std::fread(&rec, sizeof(TrainRecord), 1, file);
    return true;
}

bool TrainManager::isReleased(const char *trainID) {
    TrainRecord rec;
    return getTrain(trainID, rec) && rec.released;
}

// OrderManager
OrderManager::OrderManager() : index("order_index.dat"), orderCount(0) {
    file = std::fopen("orders.dat", "r+b");
    if (!file) file = std::fopen("orders.dat", "w+b");
    if (file) {
        std::fseek(file, 0, SEEK_END);
        orderCount = std::ftell(file) / sizeof(OrderRecord);
    }
}
OrderManager::~OrderManager() { if (file) std::fclose(file); }

static void writeBE64(char *dest, unsigned long long val) {
    for (int i = 7; i >= 0; --i) {
        dest[7 - i] = (char)((val >> (i * 8)) & 0xFF);
    }
}

int OrderManager::addOrder(const char *username, const char *trainID, int date,
                           int fromIdx, int toIdx, int num, int price,
                           int status, long long ts) {
    OrderRecord rec;
    std::memset(&rec, 0, sizeof(rec));
    std::strncpy(rec.username, username, 20);
    std::strncpy(rec.trainID, trainID, 20);
    rec.date = date;
    rec.fromIdx = fromIdx;
    rec.toIdx = toIdx;
    rec.num = num;
    rec.price = price;
    rec.status = status;
    rec.timestamp = ts;
    std::fseek(file, orderCount * sizeof(OrderRecord), SEEK_SET);
    std::fwrite(&rec, sizeof(OrderRecord), 1, file);
    char key[64];
    std::memset(key, 0, 64);
    std::memcpy(key, username, std::strlen(username));
    unsigned long long rev_ts = 0x7FFFFFFFFFFFFFFFULL - (unsigned long long)ts;
    writeBE64(key + 20, rev_ts);
    index.insert(key, orderCount++);
    return orderCount - 1;
}

bool OrderManager::refundOrder(const char *username, int nth, long long timestamp,
                               OrderRecord &order) {
    OrderScanCtx ctx;
    ctx.cnt = 0;
    ctx.username = username;
    ctx.usernameLen = std::strlen(username);
    char prefix[64];
    std::memset(prefix, 0, 64);
    std::memcpy(prefix, username, ctx.usernameLen);
    index.scanPrefix(prefix, ctx.usernameLen,
                     [](const char key[64], int val, void *ctx) -> bool {
                         OrderScanCtx *d = (OrderScanCtx*)ctx;
                         if (d->cnt >= 5000) return false;
                         if (!usernameMatchesKey(key, d->username, d->usernameLen)) return true;
                         d->ids[d->cnt++] = val;
                         return true;
                     }, &ctx);
    if (nth < 1 || nth > ctx.cnt) return false;
    int orderId = ctx.ids[nth - 1];
    std::fseek(file, orderId * sizeof(OrderRecord), SEEK_SET);
    std::fread(&order, sizeof(OrderRecord), 1, file);
    if (order.status == 2) return false;
    order.status = 2;
    std::fseek(file, orderId * sizeof(OrderRecord), SEEK_SET);
    std::fwrite(&order, sizeof(OrderRecord), 1, file);
    return true;
}

bool OrderManager::getOrder(int orderId, OrderRecord &rec) {
    std::fseek(file, orderId * sizeof(OrderRecord), SEEK_SET);
    return std::fread(&rec, sizeof(OrderRecord), 1, file) == 1;
}

// SeatManager
SeatManager::SeatManager() : index("seat_index.dat") {
    file = std::fopen("seats.dat", "r+b");
    if (!file) file = std::fopen("seats.dat", "w+b");
    if (file) {
        static char buf[65536];
        std::setvbuf(file, buf, _IOFBF, sizeof(buf));
    }
    seatIdCache = new SeatIdCache[SEAT_ID_CACHE_SIZE];
    for (int i = 0; i < SEAT_ID_CACHE_SIZE; ++i) seatIdCache[i].valid = false;
}
SeatManager::~SeatManager() {
    if (file) std::fclose(file);
    delete[] seatIdCache;
}

static void seatKey(char key[64], const char *trainID, int date) {
    std::memset(key, 0, 64);
    std::memcpy(key, trainID, std::strlen(trainID));
    std::memcpy(key + 20, &date, sizeof(date));
}

unsigned int SeatManager::seatIdHashFunc(const char *trainID, int date) {
    unsigned int h = (unsigned int)date;
    for (int i = 0; trainID[i]; ++i) {
        h = h * 31 + (unsigned char)trainID[i];
    }
    return h;
}

int SeatManager::findSeatIdCache(const char *trainID, int date) {
    unsigned int h = seatIdHashFunc(trainID, date);
    for (int i = 0; i < 4; ++i) {
        int idx = (int)((h + (unsigned int)(i * i)) & (SEAT_ID_CACHE_SIZE - 1));
        if (seatIdCache[idx].valid && seatIdCache[idx].date == date
            && std::strcmp(seatIdCache[idx].trainID, trainID) == 0) {
            return seatIdCache[idx].recId;
        }
    }
    return -2;
}

void SeatManager::updateSeatIdCache(const char *trainID, int date, int recId) {
    unsigned int h = seatIdHashFunc(trainID, date);
    for (int i = 0; i < 4; ++i) {
        int idx = (int)((h + (unsigned int)(i * i)) & (SEAT_ID_CACHE_SIZE - 1));
        if (!seatIdCache[idx].valid) {
            std::strncpy(seatIdCache[idx].trainID, trainID, 20);
            seatIdCache[idx].trainID[20] = '\0';
            seatIdCache[idx].date = date;
            seatIdCache[idx].recId = recId;
            seatIdCache[idx].valid = true;
            return;
        }
        if (seatIdCache[idx].date == date && std::strcmp(seatIdCache[idx].trainID, trainID) == 0) {
            seatIdCache[idx].recId = recId;
            return;
        }
    }
    int idx = (int)(h & (SEAT_ID_CACHE_SIZE - 1));
    std::strncpy(seatIdCache[idx].trainID, trainID, 20);
    seatIdCache[idx].trainID[20] = '\0';
    seatIdCache[idx].date = date;
    seatIdCache[idx].recId = recId;
    seatIdCache[idx].valid = true;
}

int SeatManager::querySeat(const char *trainID, int date, int fromIdx, int toIdx) {
    if (fromIdx < 0 || toIdx > MAX_STATION - 1 || fromIdx >= toIdx) return -1;
    int recId = -1;
    int cached = findSeatIdCache(trainID, date);
    if (cached >= 0) {
        recId = cached;
    } else {
        char key[64];
        seatKey(key, trainID, date);
        if (!index.findFirst(key, recId)) {
            return -1;
        }
        updateSeatIdCache(trainID, date, recId);
    }
    SeatRecord rec;
    std::fseek(file, recId * sizeof(SeatRecord), SEEK_SET);
    if (std::fread(&rec, sizeof(SeatRecord), 1, file) != 1) return -1;
    int minSeat = rec.remain[fromIdx];
    for (int i = fromIdx + 1; i < toIdx; ++i)
        if (rec.remain[i] < minSeat) minSeat = rec.remain[i];
    return minSeat;
}

bool SeatManager::buySeat(const char *trainID, int date, int fromIdx, int toIdx, int num) {
    int recId = -1;
    int cached = findSeatIdCache(trainID, date);
    if (cached >= 0) {
        recId = cached;
    } else {
        char key[64];
        seatKey(key, trainID, date);
        if (!index.findFirst(key, recId)) return false;
        updateSeatIdCache(trainID, date, recId);
    }
    SeatRecord rec;
    std::fseek(file, recId * sizeof(SeatRecord), SEEK_SET);
    if (std::fread(&rec, sizeof(SeatRecord), 1, file) != 1) return false;
    // 检查范围
    if (fromIdx < 0 || toIdx > MAX_STATION - 1 || fromIdx >= toIdx) return false;
    for (int i = fromIdx; i < toIdx; ++i) {
        if (rec.remain[i] < num) return false;
    }
    for (int i = fromIdx; i < toIdx; ++i) rec.remain[i] -= num;
    std::fseek(file, recId * sizeof(SeatRecord), SEEK_SET);
    std::fwrite(&rec, sizeof(SeatRecord), 1, file);
    return true;
}

void SeatManager::refundSeat(const char *trainID, int date, int fromIdx, int toIdx, int num) {
    int recId = -1;
    int cached = findSeatIdCache(trainID, date);
    if (cached >= 0) {
        recId = cached;
    } else {
        char key[64];
        seatKey(key, trainID, date);
        if (!index.findFirst(key, recId)) return;
        updateSeatIdCache(trainID, date, recId);
    }
    SeatRecord rec;
    std::fseek(file, recId * sizeof(SeatRecord), SEEK_SET);
    std::fread(&rec, sizeof(SeatRecord), 1, file);
    for (int i = fromIdx; i < toIdx; ++i) rec.remain[i] += num;
    std::fseek(file, recId * sizeof(SeatRecord), SEEK_SET);
    std::fwrite(&rec, sizeof(SeatRecord), 1, file);
}

void SeatManager::initSeats(const char *trainID, int date, int totalSeats, int segCount) {
    int recId = -1;
    int cached = findSeatIdCache(trainID, date);
    if (cached >= 0) {
        recId = cached;
    } else {
        char key[64];
        seatKey(key, trainID, date);
        if (index.findFirst(key, recId)) {
            updateSeatIdCache(trainID, date, recId);
            return;
        }
    }
    if (recId >= 0) return;
    SeatRecord rec;
    std::memset(&rec, 0, sizeof(rec));
    std::strncpy(rec.trainID, trainID, 20);
    rec.date = date;
    for (int i = 0; i < segCount; ++i) rec.remain[i] = totalSeats;
    // 先移动到文件末尾，获取当前大小，计算出新记录的 id
    std::fseek(file, 0, SEEK_END);
    long size = std::ftell(file);
    recId = (int)(size / sizeof(SeatRecord));
    std::fwrite(&rec, sizeof(SeatRecord), 1, file);
    char key[64];
    seatKey(key, trainID, date);
    index.insert(key, recId);
    updateSeatIdCache(trainID, date, recId);
}

// StationIndex
StationIndex::StationIndex() : index("station_index.dat") {}
StationIndex::~StationIndex() {}

void StationIndex::addStation(const char *station, const char *trainID, int trainRecId) {
    char key[64];
    std::memset(key, 0, 64);
    std::memcpy(key, station, std::strlen(station));
    std::memcpy(key + 31, trainID, std::strlen(trainID));
    index.insert(key, trainRecId);
}

static bool stationScanCallback(const char key[64], int val, void *ctx) {
    struct Ctx { int ids[5000]; int cnt; const char *station; int len; };
    Ctx *d = (Ctx*)ctx;
    if (d->cnt >= 5000) return false;
    if (std::memcmp(key, d->station, d->len) != 0) return true;
    if (key[d->len] != '\0') return true;
    d->ids[d->cnt++] = val;
    return true;
}

void StationIndex::getTrainsByStation(const char *station, int *outIds, int &cnt) {
    struct Ctx { int ids[5000]; int cnt; const char *station; int len; };
    Ctx ctx;
    ctx.cnt = 0;
    ctx.station = station;
    ctx.len = std::strlen(station);
    char prefix[64];
    std::memset(prefix, 0, 64);
    std::memcpy(prefix, station, ctx.len);
    index.scanPrefix(prefix, ctx.len, stationScanCallback, &ctx);
    cnt = ctx.cnt;
    for (int i = 0; i < cnt; ++i) outIds[i] = ctx.ids[i];
}

// PendingManager
PendingManager::PendingManager(OrderManager *om, SeatManager *sm, TrainManager *tm)
        : index("pending_index.dat"), orderMan(om), seatMan(sm), trainMan(tm) {}
PendingManager::~PendingManager() {}

void PendingManager::addPending(const char *trainID, int date, long long ts, int orderId) {
    char key[64];
    std::memset(key, 0, 64);
    std::memcpy(key, trainID, std::strlen(trainID));
    std::memcpy(key + 20, &date, sizeof(date));
    writeBE64(key + 24, (unsigned long long)ts);
    index.insert(key, orderId);
}

void PendingManager::processRefund(const char *trainID, int date) {
    char prefix[64];
    std::memset(prefix, 0, 64);
    std::memcpy(prefix, trainID, std::strlen(trainID));
    std::memcpy(prefix + 20, &date, sizeof(date));
    ScanCtx ctx;
    ctx.cnt = 0;
    index.scanPrefix(prefix, 24,
                     [](const char key[64], int val, void *ctx) -> bool {
                         ScanCtx *d = (ScanCtx*)ctx;
                         if (d->cnt < 5000) { d->ids[d->cnt++] = val; return true; }
                         return false;
                     }, &ctx);
    for (int i = 0; i < ctx.cnt; ++i) {
        int orderId = ctx.ids[i];
        OrderRecord order;
        if (!orderMan->getOrder(orderId, order) || order.status != 1) continue;
        if (seatMan->querySeat(order.trainID, date, order.fromIdx, order.toIdx) == -1) {
            TrainRecord trec;
            if (!trainMan->getTrain(order.trainID, trec)) continue;
            seatMan->initSeats(order.trainID, date, trec.seatNum, trec.stationNum - 1);
        }
        int minSeat = seatMan->querySeat(order.trainID, date, order.fromIdx, order.toIdx);
        if (minSeat >= order.num) {
            if (seatMan->buySeat(order.trainID, date, order.fromIdx, order.toIdx, order.num)) {
                order.status = 0;
                std::fseek(orderMan->file, orderId * sizeof(OrderRecord), SEEK_SET);
                std::fwrite(&order, sizeof(OrderRecord), 1, orderMan->file);
                char fullKey[64];
                std::memset(fullKey, 0, 64);
                std::memcpy(fullKey, order.trainID, std::strlen(order.trainID));
                std::memcpy(fullKey + 20, &date, sizeof(date));
                writeBE64(fullKey + 24, (unsigned long long)order.timestamp);
                index.remove(fullKey, orderId);
            }
        }
    }
}
