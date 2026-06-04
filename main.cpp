#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <new>
#include "managers.hpp"

struct TicketResult {
    char trainID[21];
    char from[31], to[31];
    int lMonth, lDay, lHour, lMin;
    int aMonth, aDay, aHour, aMin;
    int price;
    int seat;
    int sortKey1;
    char tieTrainID[21];
};

bool ticketLess(const TicketResult &a, const TicketResult &b, bool byTime) {
    (void)byTime;
    if (a.sortKey1 != b.sortKey1) return a.sortKey1 < b.sortKey1;
    return std::strcmp(a.tieTrainID, b.tieTrainID) < 0;
}

void quickSortTickets(TicketResult arr[], int left, int right, bool byTime) {
    if (left >= right) return;
    int i = left, j = right;
    TicketResult pivot = arr[(left + right) / 2];
    while (i <= j) {
        while (i <= right && ticketLess(arr[i], pivot, byTime)) ++i;
        while (j >= left && ticketLess(pivot, arr[j], byTime)) --j;
        if (i <= j) {
            TicketResult tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
            ++i; --j;
        }
    }
    quickSortTickets(arr, left, j, byTime);
    quickSortTickets(arr, i, right, byTime);
}

const char* parseLine(char *line, long long &ts, char params[256][2][5000], int &paramCount) {
    char *p = line;
    ts = 0;
    while (*p == ' ') ++p;
    if (*p == '[') {
        ++p;
        ts = strtoll(p, &p, 10);
        ++p;
    }
    while (*p == ' ') ++p;
    static char cmd[32];
    int cmdLen = 0;
    while (*p && *p != ' ' && *p != '\n') cmd[cmdLen++] = *p++;
    cmd[cmdLen] = '\0';
    paramCount = 0;
    while (*p) {
        while (*p == ' ') ++p;
        if (*p == '\0' || *p == '\n') break;
        if (*p == '-') {
            ++p;
            char key = *p++;
            while (*p == ' ') ++p;
            int valLen = 0;
            while (*p && *p != ' ' && *p != '\n') {
                params[paramCount][1][valLen++] = *p++;
                params[paramCount][1][valLen] = '\0';
            }
            params[paramCount][1][valLen] = '\0';
            params[paramCount][0][0] = key; params[paramCount][0][1] = '\0';
            ++paramCount;
        } else {
            ++p;
        }
    }
    return cmd;
}

const char* getParam(char params[256][2][5000], int paramCount, char key) {
    for (int i = 0; i < paramCount; ++i)
        if (params[i][0][0] == key) return params[i][1];
    return nullptr;
}

void getStationTime(const TrainRecord &rec, int startDate, int stationIdx,
                    int &arrMin, int &levMin, int &arrDayOff, int &levDayOff) {
    if (stationIdx < 0 || stationIdx >= rec.stationNum) {
        arrMin = levMin = 0;
        arrDayOff = levDayOff = 0;
        return;
    }
    if (stationIdx == 0) {
        arrMin = rec.startTime;
        levMin = rec.startTime;
        arrDayOff = 0;
        levDayOff = 0;
        return;
    }
    int curMin = rec.startTime;
    int dayOffset = 0;
    for (int i = 0; i < stationIdx; ++i) {
        curMin += rec.travelTimes[i];
        while (curMin >= 1440) { curMin -= 1440; ++dayOffset; }
        if (i < stationIdx - 1) {
            if (rec.stationNum > 2 && i < rec.stationNum - 2) {
                curMin += rec.stopoverTimes[i];
                while (curMin >= 1440) { curMin -= 1440; ++dayOffset; }
            }
        }
    }
    arrMin = curMin;
    arrDayOff = dayOffset;
    if (stationIdx < rec.stationNum - 1) {
        levMin = arrMin;
        if (rec.stationNum > 2 && stationIdx - 1 < rec.stationNum - 2) {
            levMin += rec.stopoverTimes[stationIdx - 1];
        }
        while (levMin >= 1440) { levMin -= 1440; ++dayOffset; }
    } else {
        levMin = arrMin;
    }
    levDayOff = dayOffset;
}

void printAbsTime(int startDate, int dayOffset, int minute) {
    int month, day;
    daysToDate(startDate + dayOffset, month, day);
    int h, m;
    minToTime(minute, h, m);
    printf("%02d-%02d %02d:%02d", month, day, h, m);
}

int main() {
    UserManager users;
    TrainManager trains;
    OrderManager orders;
    SeatManager seats;
    StationIndex stIndex;
    PendingManager pending(&orders, &seats, &trains);

    char line[2048];
    static char params[256][2][5000];
    while (std::fgets(line, sizeof(line), stdin)) {
        long long ts;
        int paramCnt;
        const char* cmd = parseLine(line, ts, params, paramCnt);

        if (std::strcmp(cmd, "add_user") == 0) {
            const char *c = getParam(params, paramCnt, 'c');
            const char *u = getParam(params, paramCnt, 'u');
            const char *p = getParam(params, paramCnt, 'p');
            const char *n = getParam(params, paramCnt, 'n');
            const char *m = getParam(params, paramCnt, 'm');
            const char *g = getParam(params, paramCnt, 'g');
            int priv = g ? std::atoi(g) : 0;
            bool first = (users.userCount == 0);
            int res = users.addUser(c, u, p, n, m, priv, first);
            printf("[%lld] %d\n", ts, res);
        }
        else if (std::strcmp(cmd, "login") == 0) {
            const char *u = getParam(params, paramCnt, 'u');
            const char *p = getParam(params, paramCnt, 'p');
            int res = users.login(u, p);
            printf("[%lld] %d\n", ts, res);
        }
        else if (std::strcmp(cmd, "logout") == 0) {
            const char *u = getParam(params, paramCnt, 'u');
            int res = users.logout(u);
            printf("[%lld] %d\n", ts, res);
        }
        else if (std::strcmp(cmd, "query_profile") == 0) {
            const char *c = getParam(params, paramCnt, 'c');
            const char *u = getParam(params, paramCnt, 'u');
            int id = users.queryProfile(c, u);
            if (id < 0) {
                printf("[%lld] -1\n", ts);
            } else {
                UserRecord rec;
                std::fseek(users.file, id * sizeof(UserRecord), SEEK_SET);
                if (std::fread(&rec, sizeof(UserRecord), 1, users.file) == 1)
                    printf("[%lld] %s %s %s %d\n", ts, rec.username, rec.name, rec.mail, rec.privilege);
                else
                    printf("[%lld] -1\n", ts);
            }
        }
        else if (std::strcmp(cmd, "modify_profile") == 0) {
            const char *c = getParam(params, paramCnt, 'c');
            const char *u = getParam(params, paramCnt, 'u');
            const char *p = getParam(params, paramCnt, 'p');
            const char *n = getParam(params, paramCnt, 'n');
            const char *m = getParam(params, paramCnt, 'm');
            const char *g = getParam(params, paramCnt, 'g');
            int priv = g ? std::atoi(g) : -1;
            int id = users.modifyProfile(c, u, p, n, m, priv);
            if (id < 0) {
                printf("[%lld] -1\n", ts);
            } else {
                UserRecord rec;
                std::fseek(users.file, id * sizeof(UserRecord), SEEK_SET);
                if (std::fread(&rec, sizeof(UserRecord), 1, users.file) == 1)
                    printf("[%lld] %s %s %s %d\n", ts, rec.username, rec.name, rec.mail, rec.privilege);
                else
                    printf("[%lld] -1\n", ts);
            }
        }
        else if (std::strcmp(cmd, "add_train") == 0) {
            const char *i = getParam(params, paramCnt, 'i');
            const char *n = getParam(params, paramCnt, 'n');
            const char *m = getParam(params, paramCnt, 'm');
            const char *s = getParam(params, paramCnt, 's');
            const char *p = getParam(params, paramCnt, 'p');
            const char *x = getParam(params, paramCnt, 'x');
            const char *t = getParam(params, paramCnt, 't');
            const char *o = getParam(params, paramCnt, 'o');
            const char *d = getParam(params, paramCnt, 'd');
            const char *y = getParam(params, paramCnt, 'y');
            int stationNum = std::atoi(n);
            int seatNum = std::atoi(m);
            char stations[100][31];
            const char *sp = s;
            for (int idx = 0; idx < stationNum; ++idx) {
                int len = 0;
                while (*sp && *sp != '|') stations[idx][len++] = *sp++;
                stations[idx][len] = '\0';
                if (*sp == '|') ++sp;
            }
            int prices[99];
            const char *pp = p;
            for (int idx = 0; idx < stationNum - 1; ++idx) {
                prices[idx] = std::atoi(pp);
                while (*pp && *pp != '|') ++pp;
                if (*pp == '|') ++pp;
            }
            int startH, startM;
            std::sscanf(x, "%d:%d", &startH, &startM);
            int startTime = timeToMin(startH, startM);
            int travelTimes[99];
            const char *tp = t;
            for (int idx = 0; idx < stationNum - 1; ++idx) {
                travelTimes[idx] = std::atoi(tp);
                while (*tp && *tp != '|') ++tp;
                if (*tp == '|') ++tp;
            }
            int stopoverTimes[98];
            if (stationNum > 2) {
                const char *op = o;
                for (int idx = 0; idx < stationNum - 2; ++idx) {
                    stopoverTimes[idx] = std::atoi(op);
                    while (*op && *op != '|') ++op;
                    if (*op == '|') ++op;
                }
            }
            int sMon, sDay, eMon, eDay;
            std::sscanf(d, "%d-%d|%d-%d", &sMon, &sDay, &eMon, &eDay);
            int sale1 = dateToDays(sMon, sDay);
            int sale2 = dateToDays(eMon, eDay);
            char type = y[0];
            int res = trains.addTrain(i, stationNum, seatNum, stations, prices, startTime,
                                      travelTimes, stopoverTimes, sale1, sale2, type);
            if (res == 0) {
                char key[64];
                makeKey64(key, i, std::strlen(i));
                int trainId;
                if (trains.index.findFirst(key, trainId)) {
                    for (int idx = 0; idx < stationNum; ++idx)
                        stIndex.addStation(stations[idx], i, trainId);
                }
            }
            printf("[%lld] %d\n", ts, res);
        }
        else if (std::strcmp(cmd, "delete_train") == 0) {
            const char *i = getParam(params, paramCnt, 'i');
            int res = trains.deleteTrain(i);
            printf("[%lld] %d\n", ts, res);
        }
        else if (std::strcmp(cmd, "release_train") == 0) {
            const char *i = getParam(params, paramCnt, 'i');
            int res = trains.releaseTrain(i);
            printf("[%lld] %d\n", ts, res);
        }
        else if (std::strcmp(cmd, "query_train") == 0) {
            const char *i = getParam(params, paramCnt, 'i');
            const char *d = getParam(params, paramCnt, 'd');
            int mon, day;
            std::sscanf(d, "%d-%d", &mon, &day);
            int date = dateToDays(mon, day);
            int id = trains.queryTrain(i, date);
            if (id < 0) {
                printf("[%lld] -1\n", ts);
            } else {
                TrainRecord rec;
                std::fseek(trains.file, id * sizeof(TrainRecord), SEEK_SET);
                if (std::fread(&rec, sizeof(TrainRecord), 1, trains.file) != 1) {
                    printf("[%lld] -1\n", ts);
                    continue;
                }
                printf("[%lld] %s %c\n", ts, rec.trainID, rec.type);
                for (int idx = 0; idx < rec.stationNum; ++idx) {
                    int arrMin, levMin, arrDayOff, levDayOff;
                    getStationTime(rec, date, idx, arrMin, levMin, arrDayOff, levDayOff);
                    int priceSum = 0;
                    for (int j = 0; j < idx; ++j) priceSum += rec.prices[j];
                    int seatRem = 0;
                    if (idx < rec.stationNum - 1) {
                        if (!rec.released) seatRem = rec.seatNum;
                        else {
                            seatRem = seats.querySeat(rec.trainID, date, idx, idx + 1);
                            if (seatRem < 0) seatRem = rec.seatNum;
                        }
                    }
                    printf("%s ", rec.stations[idx]);
                    if (idx == 0) {
                        printf("xx-xx xx:xx -> ");
                        printAbsTime(date, levDayOff, levMin);
                    } else if (idx == rec.stationNum - 1) {
                        printAbsTime(date, arrDayOff, arrMin);
                        printf(" -> xx-xx xx:xx");
                    } else {
                        printAbsTime(date, arrDayOff, arrMin);
                        printf(" -> ");
                        printAbsTime(date, levDayOff, levMin);
                    }
                    printf(" %d", priceSum);
                    if (idx == rec.stationNum - 1) printf(" x\n");
                    else printf(" %d\n", seatRem);
                }
            }
        }
        else if (std::strcmp(cmd, "query_ticket") == 0) {
            const char *s = getParam(params, paramCnt, 's');
            const char *t = getParam(params, paramCnt, 't');
            const char *d = getParam(params, paramCnt, 'd');
            const char *p = getParam(params, paramCnt, 'p');
            bool byTime = (p && std::strcmp(p, "time") == 0);
            int mon, day;
            std::sscanf(d, "%d-%d", &mon, &day);
            int date = dateToDays(mon, day);
            int trainsS[5000], cntS;
            stIndex.getTrainsByStation(s, trainsS, cntS);
            TicketResult results[5000];
            int resCnt = 0;
            int prefixArrMin[MAX_STATION], prefixArrDay[MAX_STATION];
            int prefixLevMin[MAX_STATION], prefixLevDay[MAX_STATION];
            int prefixPrice[MAX_STATION];
            for (int i = 0; i < cntS; ++i) {
                int tid = trainsS[i];
                const TrainRecord &rec = trains.getTrainById(tid);
                if (!rec.released) continue;
                int idxS = -1;
                for (int k = 0; k < rec.stationNum; ++k)
                    if (std::strcmp(rec.stations[k], s) == 0) { idxS = k; break; }
                if (idxS == -1) continue;
                int idxT = -1;
                for (int k = idxS + 1; k < rec.stationNum; ++k)
                    if (std::strcmp(rec.stations[k], t) == 0) { idxT = k; break; }
                if (idxT == -1) continue;
                // 预计算该车次各站的时间、价格前缀
                prefixArrMin[0] = rec.startTime;
                prefixArrDay[0] = 0;
                prefixLevMin[0] = rec.startTime;
                prefixLevDay[0] = 0;
                prefixPrice[0] = 0;
                for (int k = 1; k < rec.stationNum; ++k) {
                    int curMin = prefixLevMin[k - 1] + rec.travelTimes[k - 1];
                    int dayOff = prefixLevDay[k - 1];
                    while (curMin >= 1440) { curMin -= 1440; ++dayOff; }
                    prefixArrMin[k] = curMin;
                    prefixArrDay[k] = dayOff;
                    if (k < rec.stationNum - 1) {
                        curMin += rec.stopoverTimes[k - 1];
                        while (curMin >= 1440) { curMin -= 1440; ++dayOff; }
                        prefixLevMin[k] = curMin;
                        prefixLevDay[k] = dayOff;
                    } else {
                        prefixLevMin[k] = prefixArrMin[k];
                        prefixLevDay[k] = prefixArrDay[k];
                    }
                    prefixPrice[k] = prefixPrice[k - 1] + rec.prices[k - 1];
                }
                int startDate = date - prefixLevDay[idxS];
                if (startDate < rec.saleDate1 || startDate > rec.saleDate2) continue;
                int totalMin = (prefixArrDay[idxT] - prefixLevDay[idxS]) * 1440
                             + prefixArrMin[idxT] - prefixLevMin[idxS];
                int price = prefixPrice[idxT] - prefixPrice[idxS];
                int minSeat = seats.querySeat(rec.trainID, startDate, idxS, idxT);
                if (minSeat < 0) minSeat = rec.seatNum;
                TicketResult res;
                std::strcpy(res.trainID, rec.trainID);
                std::strcpy(res.from, s);
                std::strcpy(res.to, t);
                int lM, lD, lH, lMin;
                daysToDate(date, lM, lD); minToTime(prefixLevMin[idxS], lH, lMin);
                res.lMonth = lM; res.lDay = lD; res.lHour = lH; res.lMin = lMin;
                int aM, aD, aH, aMin;
                daysToDate(startDate + prefixArrDay[idxT], aM, aD); minToTime(prefixArrMin[idxT], aH, aMin);
                res.aMonth = aM; res.aDay = aD; res.aHour = aH; res.aMin = aMin;
                res.price = price;
                res.seat = minSeat;
                if (byTime) res.sortKey1 = totalMin;
                else res.sortKey1 = price;
                std::strcpy(res.tieTrainID, rec.trainID);
                results[resCnt++] = res;
            }
            quickSortTickets(results, 0, resCnt - 1, byTime);
            printf("[%lld] %d\n", ts, resCnt);
            for (int i = 0; i < resCnt; ++i) {
                printf("%s %s %02d-%02d %02d:%02d -> %s %02d-%02d %02d:%02d %d %d\n",
                       results[i].trainID, results[i].from,
                       results[i].lMonth, results[i].lDay, results[i].lHour, results[i].lMin,
                       results[i].to,
                       results[i].aMonth, results[i].aDay, results[i].aHour, results[i].aMin,
                       results[i].price, results[i].seat);
            }
        }
        else if (std::strcmp(cmd, "query_transfer") == 0) {
            const char *s = getParam(params, paramCnt, 's');
            const char *t = getParam(params, paramCnt, 't');
            const char *d = getParam(params, paramCnt, 'd');
            const char *p = getParam(params, paramCnt, 'p');
            bool byTime = (p && std::strcmp(p, "time") == 0);
            int mon, day;
            std::sscanf(d, "%d-%d", &mon, &day);
            int date = dateToDays(mon, day);
            int trainsS[5000], cntS;
            stIndex.getTrainsByStation(s, trainsS, cntS);
            int trainsT[5000], cntT;
            stIndex.getTrainsByStation(t, trainsT, cntT);
            bool found = false;
            TicketResult best1, best2;
            long long bestTotalTime = 0x7fffffffffffffffLL, bestTotalPrice = 0x7fffffffffffffffLL;
            char bestID1[21] = "", bestID2[21] = "";
            for (int i = 0; i < cntS; ++i) {
                int tid1 = trainsS[i];
                const TrainRecord &rec1 = trains.getTrainById(tid1);
                if (!rec1.released) continue;
                int idxS = -1;
                for (int k = 0; k < rec1.stationNum; ++k)
                    if (std::strcmp(rec1.stations[k], s) == 0) { idxS = k; break; }
                if (idxS == -1) continue;
                int sArr1, sLev1, sArrDayOff1, sLevDayOff1;
                getStationTime(rec1, date, idxS, sArr1, sLev1, sArrDayOff1, sLevDayOff1);
                int startDate1 = date - sLevDayOff1;
                if (startDate1 < rec1.saleDate1 || startDate1 > rec1.saleDate2) continue;
                for (int k = idxS + 1; k < rec1.stationNum; ++k) {
                    const char *stationK = rec1.stations[k];
                    if (std::strcmp(stationK, t) == 0) continue;
                    int arrK, levK, arrDayOffK, levDayOffK;
                    getStationTime(rec1, startDate1, k, arrK, levK, arrDayOffK, levDayOffK);
                    int arriveDateK = startDate1 + arrDayOffK;
                    int firstArrAbs = arriveDateK * 1440 + arrK;
                    int firstDepAbs = date * 1440 + sLev1;
                    int price1 = 0;
                    for (int x = idxS; x < k; ++x) price1 += rec1.prices[x];
                    int ss1 = seats.querySeat(rec1.trainID, startDate1, idxS, k);
                    if (ss1 < 0) ss1 = rec1.seatNum;
                    for (int j = 0; j < cntT; ++j) {
                        int tid2 = trainsT[j];
                        if (tid2 == tid1) continue;
                        const TrainRecord &rec2 = trains.getTrainById(tid2);
                        if (!rec2.released) continue;
                        int idxK2 = -1, idxT2 = -1;
                        for (int x = 0; x < rec2.stationNum; ++x) {
                            if (std::strcmp(rec2.stations[x], stationK) == 0) idxK2 = x;
                            if (idxK2 != -1 && std::strcmp(rec2.stations[x], t) == 0 && x > idxK2) { idxT2 = x; break; }
                        }
                        if (idxK2 == -1 || idxT2 == -1) continue;
                        int kArr2, kLev2, kArrDayOff2, kLevDayOff2;
                        getStationTime(rec2, rec2.saleDate1, idxK2, kArr2, kLev2, kArrDayOff2, kLevDayOff2);
                        int tArr2, tLev2, tArrDayOff2, tLevDayOff2;
                        getStationTime(rec2, rec2.saleDate1, idxT2, tArr2, tLev2, tArrDayOff2, tLevDayOff2);
                        int minD = arriveDateK - kLevDayOff2;
                        if (kLev2 < arrK) ++minD;
                        int tryStart = minD;
                        if (tryStart < rec2.saleDate1) tryStart = rec2.saleDate1;
                        if (tryStart > rec2.saleDate2) continue;
                        int secondDepAbs = (tryStart + kLevDayOff2) * 1440 + kLev2;
                        if (secondDepAbs < firstArrAbs) {
                            ++tryStart;
                            if (tryStart > rec2.saleDate2) continue;
                        }
                        int secondArrAbs = (tryStart + tArrDayOff2) * 1440 + tArr2;
                        long long totalTime = secondArrAbs - firstDepAbs;
                        int price2 = 0;
                        for (int x = idxK2; x < idxT2; ++x) price2 += rec2.prices[x];
                        long long totalPrice = price1 + price2;
                        bool better = false;
                        if (!found) better = true;
                        else if (byTime) {
                            if (totalTime < bestTotalTime) better = true;
                            else if (totalTime == bestTotalTime && totalPrice < bestTotalPrice) better = true;
                            else if (totalTime == bestTotalPrice && totalPrice == bestTotalPrice) {
                                int cmp1 = std::strcmp(rec1.trainID, bestID1);
                                if (cmp1 < 0) better = true;
                                else if (cmp1 == 0 && std::strcmp(rec2.trainID, bestID2) < 0) better = true;
                            }
                        } else {
                            if (totalPrice < bestTotalPrice) better = true;
                            else if (totalPrice == bestTotalPrice && totalTime < bestTotalTime) better = true;
                            else if (totalPrice == bestTotalPrice && totalTime == bestTotalTime) {
                                int cmp1 = std::strcmp(rec1.trainID, bestID1);
                                if (cmp1 < 0) better = true;
                                else if (cmp1 == 0 && std::strcmp(rec2.trainID, bestID2) < 0) better = true;
                            }
                        }
                        if (better) {
                            found = true;
                            bestTotalTime = totalTime;
                            bestTotalPrice = totalPrice;
                            std::strcpy(bestID1, rec1.trainID);
                            std::strcpy(bestID2, rec2.trainID);
                            std::strcpy(best1.trainID, rec1.trainID);
                            std::strcpy(best1.from, s);
                            std::strcpy(best1.to, stationK);
                            int lM, lD, lH, lMin;
                            daysToDate(date, lM, lD); minToTime(sLev1, lH, lMin);
                            best1.lMonth = lM; best1.lDay = lD; best1.lHour = lH; best1.lMin = lMin;
                            daysToDate(arriveDateK, lM, lD); minToTime(arrK, lH, lMin);
                            best1.aMonth = lM; best1.aDay = lD; best1.aHour = lH; best1.aMin = lMin;
                            best1.price = price1;
                            best1.seat = ss1;
                            std::strcpy(best2.trainID, rec2.trainID);
                            std::strcpy(best2.from, stationK);
                            std::strcpy(best2.to, t);
                            daysToDate(tryStart + kLevDayOff2, lM, lD); minToTime(kLev2, lH, lMin);
                            best2.lMonth = lM; best2.lDay = lD; best2.lHour = lH; best2.lMin = lMin;
                            daysToDate(tryStart + tArrDayOff2, lM, lD); minToTime(tArr2, lH, lMin);
                            best2.aMonth = lM; best2.aDay = lD; best2.aHour = lH; best2.aMin = lMin;
                            best2.price = price2;
                            int ss2 = seats.querySeat(rec2.trainID, tryStart, idxK2, idxT2);
                            if (ss2 < 0) ss2 = rec2.seatNum;
                            best2.seat = ss2;
                        }
                    }
                }
            }
            if (!found) printf("[%lld] 0\n", ts);
            else {
                printf("[%lld] %s %s %02d-%02d %02d:%02d -> %s %02d-%02d %02d:%02d %d %d\n",
                       ts, best1.trainID, best1.from, best1.lMonth, best1.lDay, best1.lHour, best1.lMin,
                       best1.to, best1.aMonth, best1.aDay, best1.aHour, best1.aMin, best1.price, best1.seat);
                printf("%s %s %02d-%02d %02d:%02d -> %s %02d-%02d %02d:%02d %d %d\n",
                       best2.trainID, best2.from, best2.lMonth, best2.lDay, best2.lHour, best2.lMin,
                       best2.to, best2.aMonth, best2.aDay, best2.aHour, best2.aMin, best2.price, best2.seat);
            }
        }
        else if (std::strcmp(cmd, "buy_ticket") == 0) {
            const char *u = getParam(params, paramCnt, 'u');
            const char *i = getParam(params, paramCnt, 'i');
            const char *d = getParam(params, paramCnt, 'd');
            const char *n = getParam(params, paramCnt, 'n');
            const char *f = getParam(params, paramCnt, 'f');
            const char *t = getParam(params, paramCnt, 't');
            const char *q = getParam(params, paramCnt, 'q');
            bool acceptPending = (q && std::strcmp(q, "true") == 0);

            if (!users.isOnline(u)) {
                printf("[%lld] -1\n", ts);
                continue;
            }
            TrainRecord rec;
            if (!trains.getTrain(i, rec)) {
                printf("[%lld] -1\n", ts);
                continue;
            }
            if (!rec.released) {
                printf("[%lld] -1\n", ts);
                continue;
            }
            int mon, day;
            std::sscanf(d, "%d-%d", &mon, &day);
            int date = dateToDays(mon, day);
            int idxF = -1, idxT = -1;
            for (int k = 0; k < rec.stationNum; ++k) {
                if (std::strcmp(rec.stations[k], f) == 0) idxF = k;
                if (idxF != -1 && std::strcmp(rec.stations[k], t) == 0) { idxT = k; break; }
            }
            if (idxF == -1 || idxT == -1 || idxF >= idxT) {
                printf("[%lld] -1\n", ts);
                continue;
            }
            int fArr, fLev, fArrDayOff, fLevDayOff;
            getStationTime(rec, date, idxF, fArr, fLev, fArrDayOff, fLevDayOff);
            int startDate = date - fLevDayOff;
            if (startDate < rec.saleDate1 || startDate > rec.saleDate2) {
                printf("[%lld] -1\n", ts);
                continue;
            }
            int num = std::atoi(n);
            if (num > rec.seatNum) {
                printf("[%lld] -1\n", ts);
                continue;
            }
            int totalPrice = 0;
            for (int k = idxF; k < idxT; ++k) totalPrice += rec.prices[k];
            if (seats.querySeat(rec.trainID, startDate, idxF, idxT) == -1) {
                seats.initSeats(rec.trainID, startDate, rec.seatNum, rec.stationNum - 1);
            }
            if (seats.buySeat(rec.trainID, startDate, idxF, idxT, num)) {
                orders.addOrder(u, rec.trainID, date, idxF, idxT, num, totalPrice * num, 0, ts);
                printf("[%lld] %d\n", ts, totalPrice * num);
            } else if (acceptPending) {
                int orderId = orders.addOrder(u, rec.trainID, date, idxF, idxT, num, totalPrice * num, 1, ts);
                pending.addPending(rec.trainID, startDate, ts, orderId);
                printf("[%lld] queue\n", ts);
            } else {
                printf("[%lld] -1\n", ts);
            }
        }
        else if (std::strcmp(cmd, "query_order") == 0) {
            const char *u = getParam(params, paramCnt, 'u');
            if (!users.isOnline(u)) { printf("[%lld] -1\n", ts); continue; }
            OrderScanCtx ctx;
            ctx.cnt = 0;
            ctx.username = u;
            ctx.usernameLen = std::strlen(u);
            char prefix[64];
            std::memset(prefix, 0, 64);
            std::memcpy(prefix, u, ctx.usernameLen);
            orders.index.scanPrefix(prefix, ctx.usernameLen,
                                    [](const char key[64], int val, void *ctx) -> bool {
                                        OrderScanCtx *d = (OrderScanCtx*)ctx;
                                        if (d->cnt >= 5000) return false;
                                        if (!usernameMatchesKey(key, d->username, d->usernameLen)) return true;
                                        d->ids[d->cnt++] = val;
                                        return true;
                                    }, &ctx);
            printf("[%lld] %d\n", ts, ctx.cnt);
            for (int i = 0; i < ctx.cnt; ++i) {
                OrderRecord ord;
                if (!orders.getOrder(ctx.ids[i], ord)) continue;
                const char *statusStr = (ord.status == 0) ? "success" : (ord.status == 1 ? "pending" : "refunded");
                TrainRecord trec;
                if (!trains.getTrain(ord.trainID, trec)) continue;
                int fArr, fLev, fArrDayOff, fLevDayOff;
                getStationTime(trec, ord.date, ord.fromIdx, fArr, fLev, fArrDayOff, fLevDayOff);
                int startDate = ord.date - fLevDayOff;
                int tArr, tLev, tArrDayOff, tLevDayOff;
                getStationTime(trec, startDate, ord.toIdx, tArr, tLev, tArrDayOff, tLevDayOff);
                int lM, lD, lH, lMin, aM, aD, aH, aMin;
                daysToDate(ord.date, lM, lD); minToTime(fLev, lH, lMin);
                daysToDate(startDate + tArrDayOff, aM, aD); minToTime(tArr, aH, aMin);
                printf("[%s] %s %s %02d-%02d %02d:%02d -> %s %02d-%02d %02d:%02d %d %d\n",
                       statusStr, ord.trainID, trec.stations[ord.fromIdx],
                       lM, lD, lH, lMin, trec.stations[ord.toIdx],
                       aM, aD, aH, aMin, ord.price / ord.num, ord.num);
            }
        }
        else if (std::strcmp(cmd, "refund_ticket") == 0) {
            const char *u = getParam(params, paramCnt, 'u');
            const char *n = getParam(params, paramCnt, 'n');
            int nth = n ? std::atoi(n) : 1;
            if (!users.isOnline(u)) { printf("[%lld] -1\n", ts); continue; }
            OrderScanCtx ctx;
            ctx.cnt = 0;
            ctx.username = u;
            ctx.usernameLen = std::strlen(u);
            char prefix[64];
            std::memset(prefix, 0, 64);
            std::memcpy(prefix, u, ctx.usernameLen);
            orders.index.scanPrefix(prefix, ctx.usernameLen,
                                    [](const char key[64], int val, void *ctx) -> bool {
                                        OrderScanCtx *d = (OrderScanCtx*)ctx;
                                        if (d->cnt >= 5000) return false;
                                        if (!usernameMatchesKey(key, d->username, d->usernameLen)) return true;
                                        d->ids[d->cnt++] = val;
                                        return true;
                                    }, &ctx);
            if (nth < 1 || nth > ctx.cnt) { printf("[%lld] -1\n", ts); continue; }
            int orderId = ctx.ids[nth - 1];
            OrderRecord ord;
            if (!orders.getOrder(orderId, ord)) { printf("[%lld] -1\n", ts); continue; }
            int originalStatus = ord.status;
            if (originalStatus == 2) { printf("[%lld] -1\n", ts); continue; }
            if (!orders.refundOrder(u, nth, ts, ord)) { printf("[%lld] -1\n", ts); continue; }
            if (originalStatus == 0) {
                TrainRecord rec;
                if (!trains.getTrain(ord.trainID, rec)) { printf("[%lld] -1\n", ts); continue; }
                int fArr, fLev, fArrDayOff, fLevDayOff;
                getStationTime(rec, ord.date, ord.fromIdx, fArr, fLev, fArrDayOff, fLevDayOff);
                int startDate = ord.date - fLevDayOff;
                seats.refundSeat(ord.trainID, startDate, ord.fromIdx, ord.toIdx, ord.num);
                pending.processRefund(ord.trainID, startDate);
            }
            printf("[%lld] 0\n", ts);
        }
        else if (std::strcmp(cmd, "clean") == 0) {
            users.~UserManager();
            trains.~TrainManager();
            orders.~OrderManager();
            seats.~SeatManager();
            stIndex.~StationIndex();
            pending.~PendingManager();
            std::remove("users.dat");
            std::remove("trains.dat");
            std::remove("orders.dat");
            std::remove("seats.dat");
            std::remove("user_index.dat");
            std::remove("train_index.dat");
            std::remove("order_index.dat");
            std::remove("seat_index.dat");
            std::remove("station_index.dat");
            std::remove("pending_index.dat");
            new (&users) UserManager();
            new (&trains) TrainManager();
            new (&orders) OrderManager();
            new (&seats) SeatManager();
            new (&stIndex) StationIndex();
            new (&pending) PendingManager(&orders, &seats, &trains);
            printf("[%lld] 0\n", ts);
        }
        else if (std::strcmp(cmd, "exit") == 0) {
            printf("[%lld] bye\n", ts);
            break;
        }
    }
    return 0;
}
