#ifndef MANAGERS_HPP
#define MANAGERS_HPP
#include "bptree.hpp"
#include <cstdio>
#include <cstring>

const int MAX_STATION = 100;
const int ONLINE_HASH_SIZE = 8192;

struct OnlineEntry {
    char name[21];
    int privilege;
    int next;
};

struct UserRecord {
    char username[21];
    char password[31];
    char name[31];
    char mail[31];
    int privilege;
};

struct TrainRecord {
    char trainID[21];
    int stationNum;
    int seatNum;
    int startTime;
    int saleDate1, saleDate2;
    char type;
    bool released;
    char stations[MAX_STATION][31];
    int prices[MAX_STATION - 1];
    int travelTimes[MAX_STATION - 1];
    int stopoverTimes[MAX_STATION - 2];
};

struct OrderRecord {
    char username[21];
    char trainID[21];
    int date;
    int fromIdx;
    int toIdx;
    int num;
    int price;
    int status;          // 0:success 1:pending 2:refunded
    long long timestamp;
};

struct SeatRecord {
    char trainID[21];
    int date;
    int remain[MAX_STATION - 1];
};

// 安全的 scanPrefix 回调上下文
struct ScanCtx {
    int ids[3000];
    int cnt;
};

struct OrderScanCtx {
    int ids[3000];
    int cnt;
    const char *username;
    int usernameLen;
};

inline bool usernameMatchesKey(const char key[64], const char *username, int len) {
    if (std::memcmp(key, username, len) != 0) return false;
    if (len < 20 && key[len] != '\0') return false;
    return true;
}

// 工具函数
int dateToDays(int month, int day);
void daysToDate(int days, int &month, int &day);
int timeToMin(int h, int m);
void minToTime(int mins, int &h, int &m);
void makeKey64(char dest[64], const char *s, int len);

class UserManager {
public:
    FILE *file;
    BPTree index;
    int userCount;
    OnlineEntry *onlinePool;
    int *onlineHead;
    int onlineCnt;
    int onlineCap;
    int onlineFree;

    UserManager();
    ~UserManager();
    int addUser(const char *cur, const char *username, const char *pass,
                const char *name, const char *mail, int priv, bool first);
    int login(const char *username, const char *pass);
    int logout(const char *username);
    int queryProfile(const char *cur, const char *username);
    int modifyProfile(const char *cur, const char *username,
                      const char *pass, const char *name,
                      const char *mail, int priv);
    bool isOnline(const char *username);
    int getPrivilege(const char *username);
private:
    void expandOnline();
    static unsigned int onlineHash(const char *s);
};

class TrainManager {
public:
    FILE *file;
    BPTree index;
    int trainCount;

    TrainManager();
    ~TrainManager();
    int addTrain(const char *trainID, int stationNum, int seatNum,
                 const char stations[][31], int prices[], int startTime,
                 int travelTimes[], int stopoverTimes[],
                 int sale1, int sale2, char type);
    int deleteTrain(const char *trainID);
    int releaseTrain(const char *trainID);
    int queryTrain(const char *trainID, int date);
    bool getTrain(const char *trainID, TrainRecord &rec);
    bool isReleased(const char *trainID);
};

class OrderManager {
public:
    FILE *file;
    BPTree index;
    int orderCount;

    OrderManager();
    ~OrderManager();
    int addOrder(const char *username, const char *trainID, int date,
                 int fromIdx, int toIdx, int num, int price,
                 int status, long long ts);
    void queryOrder(const char *username, long long timestamp);
    bool refundOrder(const char *username, int nth, long long timestamp,
                     OrderRecord &order);
    bool getOrder(int orderId, OrderRecord &rec);
};

class SeatManager {
public:
    FILE *file;
    BPTree index;

    SeatManager();
    ~SeatManager();
    int querySeat(const char *trainID, int date, int fromIdx, int toIdx);
    bool buySeat(const char *trainID, int date, int fromIdx, int toIdx, int num);
    void refundSeat(const char *trainID, int date, int fromIdx, int toIdx, int num);
    void initSeats(const char *trainID, int date, int totalSeats, int segCount);
};

class StationIndex {
public:
    BPTree index;
    StationIndex();
    ~StationIndex();
    void addStation(const char *station, const char *trainID, int trainRecId);
    void getTrainsByStation(const char *station, int *outIds, int &cnt);
};

class PendingManager {
public:
    BPTree index;
    OrderManager *orderMan;
    SeatManager *seatMan;
    TrainManager *trainMan;

    PendingManager(OrderManager *om, SeatManager *sm, TrainManager *tm);
    ~PendingManager();
    void addPending(const char *trainID, int date, long long ts, int orderId);
    void processRefund(const char *trainID, int date);
};

#endif
