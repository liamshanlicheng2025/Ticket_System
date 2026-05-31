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

void quickSortTickets(TicketResult arr[], int left, int right, bool byTime) {
    if (left >= right) return;
    int i = left, j = right;
    TicketResult pivot = arr[(left + right) / 2];
    while (i <= j) {
        bool less;
        if (byTime) {
            less = (arr[i].sortKey1 < pivot.sortKey1) ||
                   (arr[i].sortKey1 == pivot.sortKey1 && std::strcmp(arr[i].tieTrainID, pivot.tieTrainID) < 0);
        } else {
            less = (arr[i].sortKey1 < pivot.sortKey1) ||
                   (arr[i].sortKey1 == pivot.sortKey1 && std::strcmp(arr[i].tieTrainID, pivot.tieTrainID) < 0);
        }
        while (less) {
            ++i;
            if (byTime) {
                less = (arr[i].sortKey1 < pivot.sortKey1) ||
                       (arr[i].sortKey1 == pivot.sortKey1 && std::strcmp(arr[i].tieTrainID, pivot.tieTrainID) < 0);
            } else {
                less = (arr[i].sortKey1 < pivot.sortKey1) ||
                       (arr[i].sortKey1 == pivot.sortKey1 && std::strcmp(arr[i].tieTrainID, pivot.tieTrainID) < 0);
            }
        }
        bool greater;
        if (byTime) {
            greater = (arr[j].sortKey1 > pivot.sortKey1) ||
                      (arr[j].sortKey1 == pivot.sortKey1 && std::strcmp(arr[j].tieTrainID, pivot.tieTrainID) > 0);
        } else {
            greater = (arr[j].sortKey1 > pivot.sortKey1) ||
                      (arr[j].sortKey1 == pivot.sortKey1 && std::strcmp(arr[j].tieTrainID, pivot.tieTrainID) > 0);
        }
        while (greater) {
            --j;
            if (byTime) {
                greater = (arr[j].sortKey1 > pivot.sortKey1) ||
                          (arr[j].sortKey1 == pivot.sortKey1 && std::strcmp(arr[j].tieTrainID, pivot.tieTrainID) > 0);
            } else {
                greater = (arr[j].sortKey1 > pivot.sortKey1) ||
                          (arr[j].sortKey1 == pivot.sortKey1 && std::strcmp(arr[j].tieTrainID, pivot.tieTrainID) > 0);
            }
        }
        if (i <= j) {
            TicketResult tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
            ++i; --j;
        }
    }
    quickSortTickets(arr, left, j, byTime);
    quickSortTickets(arr, i, right, byTime);
}

const char* parseLine(char *line, long long &ts, char params[256][2][256], int &paramCount) {
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
            while (*p && *p != ' ' && *p != '\n') params[paramCount][1][valLen++] = *p++;
            params[paramCount][1][valLen] = '\0';
            params[paramCount][0][0] = key; params[paramCount][0][1] = '\0';
            ++paramCount;
        } else {
            ++p;
        }
    }
    return cmd;
}

const char* getParam(char params[256][2][256], int paramCount, char key) {
    for (int i = 0; i < paramCount; ++i)
        if (params[i][0][0] == key) return params[i][1];
    return nullptr;
}

void getStationTime(const TrainRecord &rec, int startDate, int stationIdx,
                    int &arrMin, int &levMin, int &dayOffset) {
    int curMin = rec.startTime;
    dayOffset = 0;
    for (int i = 0; i < stationIdx; ++i) {
        curMin += rec.travelTimes[i];
        if (i < stationIdx - 1) curMin += rec.stopoverTimes[i];
        while (curMin >= 1440) { curMin -= 1440; ++dayOffset; }
    }
    arrMin = curMin;
    if (stationIdx < rec.stationNum - 1) {
        levMin = arrMin + rec.stopoverTimes[stationIdx];
        while (levMin >= 1440) { levMin -= 1440; ++dayOffset; }
    } else {
        levMin = arrMin;
    }
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
    while (std::fgets(line, sizeof(line), stdin)) {
        long long ts;
        char params[256][2][256];
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
                std::fread(&rec, sizeof(UserRecord), 1, users.file);
                printf("[%lld] %s %s %s %d\n", ts, rec.username, rec.name, rec.mail, rec.privilege);
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
                std::fread(&rec, sizeof(UserRecord), 1, users.file);
                printf("[%lld] %s %s %s %d\n", ts, rec.username, rec.name, rec.mail, rec.privilege);
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
                trains.index.findFirst(key, trainId);
                for (int idx = 0; idx < stationNum; ++idx)
                    stIndex.addStation(stations[idx], i, trainId);
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
                std::fread(&rec, sizeof(TrainRecord), 1, trains.file);
                printf("[%lld] %s %c\n", ts, rec.trainID, rec.type);
                for (int idx = 0; idx < rec.stationNum; ++idx) {
                    int arrMin, levMin, dayOff;
                    getStationTime(rec, date, idx, arrMin, levMin, dayOff);
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
                        printAbsTime(date, dayOff, levMin);
                    } else if (idx == rec.stationNum - 1) {
                        printAbsTime(date, dayOff, arrMin);
                        printf(" -> xx-xx xx:xx");
                    } else {
                        printAbsTime(date, dayOff, arrMin);
                        printf(" -> ");
                        printAbsTime(date, dayOff, levMin);
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
            int trainsS[1000], cntS;
            stIndex.getTrainsByStation(s, trainsS, cntS);
            TicketResult results[1000];
            int resCnt = 0;
            for (int i = 0; i < cntS; ++i) {
                int tid = trainsS[i];
                TrainRecord rec;
                std::fseek(trains.file, tid * sizeof(TrainRecord), SEEK_SET);
                std::fread(&rec, sizeof(TrainRecord), 1, trains.file);
                if (!rec.released) continue;
                int idxS = -1;
                for (int k = 0; k < rec.stationNum; ++k)
                    if (std::strcmp(rec.stations[k], s) == 0) { idxS = k; break; }
                if (idxS == -1) continue;
                int idxT = -1;
                for (int k = idxS + 1; k < rec.stationNum; ++k)
                    if (std::strcmp(rec.stations[k], t) == 0) { idxT = k; break; }
                if (idxT == -1) continue;
                int sArr, sLev, sDayOff;
                getStationTime(rec, date, idxS, sArr, sLev, sDayOff);
                int startDate = date - sDayOff;
                if (startDate < rec.saleDate1 || startDate > rec.saleDate2) continue;
                int price = 0;
                for (int k = idxS; k < idxT; ++k) price += rec.prices[k];
                int tArr, tLev, tDayOff;
                getStationTime(rec, startDate, idxT, tArr, tLev, tDayOff);
                int totalMin = (tDayOff - sDayOff) * 1440 + tArr - sLev;
                int minSeat = 0x7fffffff;
                for (int k = idxS; k < idxT; ++k) {
                    int segSeat = seats.querySeat(rec.trainID, startDate, k, k + 1);
                    if (segSeat < 0) segSeat = rec.seatNum;
                    if (segSeat < minSeat) minSeat = segSeat;
                }
                TicketResult res;
                std::strcpy(res.trainID, rec.trainID);
                std::strcpy(res.from, s);
                std::strcpy(res.to, t);
                int lM, lD, lH, lMin;
                daysToDate(date, lM, lD); minToTime(sLev, lH, lMin);
                res.lMonth = lM; res.lDay = lD; res.lHour = lH; res.lMin = lMin;
                int aM, aD, aH, aMin;
                daysToDate(startDate + tDayOff, aM, aD); minToTime(tArr, aH, aMin);
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
            int trainsS[1000], cntS;
            stIndex.getTrainsByStation(s, trainsS, cntS);
            int trainsT[1000], cntT;
            stIndex.getTrainsByStation(t, trainsT, cntT);
            bool found = false;
            TicketResult best1, best2;
            long long bestTotalTime = 0x7fffffffffffffffLL, bestTotalPrice = 0x7fffffffffffffffLL;
            char bestID1[21] = "", bestID2[21] = "";
            for (int i = 0; i < cntS; ++i) {
                int tid1 = trainsS[i];
                TrainRecord rec1;
                std::fseek(trains.file, tid1 * sizeof(TrainRecord), SEEK_SET);
                std::fread(&rec1, sizeof(TrainRecord), 1, trains.file);
                if (!rec1.released) continue;
                int idxS = -1;
                for (int k = 0; k < rec1.stationNum; ++k)
                    if (std::strcmp(rec1.stations[k], s) == 0) { idxS = k; break; }
                if (idxS == -1) continue;
                int sArr1, sLev1, sDayOff1;
                getStationTime(rec1, date, idxS, sArr1, sLev1, sDayOff1);
                int startDate1 = date - sDayOff1;
                if (startDate1 < rec1.saleDate1 || startDate1 > rec1.saleDate2) continue;
                for (int k = idxS + 1; k < rec1.stationNum; ++k) {
                    const char *stationK = rec1.stations[k];
                    if (std::strcmp(stationK, t) == 0) continue;
                    int arrK, levK, dayOffK;
                    getStationTime(rec1, startDate1, k, arrK, levK, dayOffK);
                    int arriveDateK = startDate1 + dayOffK;
                    for (int j = 0; j < cntT; ++j) {
                        int tid2 = trainsT[j];
                        if (tid2 == tid1) continue;
                        TrainRecord rec2;
                        std::fseek(trains.file, tid2 * sizeof(TrainRecord), SEEK_SET);
                        std::fread(&rec2, sizeof(TrainRecord), 1, trains.file);
                        if (!rec2.released) continue;
                        int idxK2 = -1, idxT2 = -1;
                        for (int x = 0; x < rec2.stationNum; ++x) {
                            if (std::strcmp(rec2.stations[x], stationK) == 0) idxK2 = x;
                            if (idxK2 != -1 && std::strcmp(rec2.stations[x], t) == 0 && x > idxK2) { idxT2 = x; break; }
                        }
                        if (idxK2 == -1 || idxT2 == -1) continue;
                        for (int delta = -1; delta <= 1; ++delta) {
                            int tryStart = arriveDateK + delta;
                            if (tryStart < rec2.saleDate1 || tryStart > rec2.saleDate2) continue;
                            int kArr2, kLev2, kDayOff2;
                            getStationTime(rec2, tryStart, idxK2, kArr2, kLev2, kDayOff2);
                            int firstArrAbs = arriveDateK * 1440 + arrK;
                            int secondDepAbs = (tryStart + kDayOff2) * 1440 + kLev2;
                            if (secondDepAbs < firstArrAbs) continue;
                            int tArr2, tLev2, tDayOff2;
                            getStationTime(rec2, tryStart, idxT2, tArr2, tLev2, tDayOff2);
                            int secondArrAbs = (tryStart + tDayOff2) * 1440 + tArr2;
                            int firstDepAbs = date * 1440 + sLev1;
                            long long totalTime = secondArrAbs - firstDepAbs;
                            int price1 = 0;
                            for (int x = idxS; x < k; ++x) price1 += rec1.prices[x];
                            int price2 = 0;
                            for (int x = idxK2; x < idxT2; ++x) price2 += rec2.prices[x];
                            long long totalPrice = price1 + price2;
                            bool better = false;
                            if (!found) better = true;
                            else if (byTime) {
                                if (totalTime < bestTotalTime) better = true;
                                else if (totalTime == bestTotalTime && totalPrice < bestTotalPrice) better = true;
                                else if (totalTime == bestTotalTime && totalPrice == bestTotalPrice) {
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
                                int ss1 = 0x7fffffff;
                                for (int x = idxS; x < k; ++x) {
                                    int seg = seats.querySeat(rec1.trainID, startDate1, x, x+1);
                                    if (seg < 0) seg = rec1.seatNum;
                                    if (seg < ss1) ss1 = seg;
                                }
                                best1.seat = ss1;
                                std::strcpy(best2.trainID, rec2.trainID);
                                std::strcpy(best2.from, stationK);
                                std::strcpy(best2.to, t);
                                daysToDate(tryStart + kDayOff2, lM, lD); minToTime(kLev2, lH, lMin);
                                best2.lMonth = lM; best2.lDay = lD; best2.lHour = lH; best2.lMin = lMin;
                                daysToDate(tryStart + tDayOff2, lM, lD); minToTime(tArr2, lH, lMin);
                                best2.aMonth = lM; best2.aDay = lD; best2.aHour = lH; best2.aMin = lMin;
                                best2.price = price2;
                                int ss2 = 0x7fffffff;
                                for (int x = idxK2; x < idxT2; ++x) {
                                    int seg = seats.querySeat(rec2.trainID, tryStart, x, x+1);
                                    if (seg < 0) seg = rec2.seatNum;
                                    if (seg < ss2) ss2 = seg;
                                }
                                best2.seat = ss2;
                            }
                        }
                    }
                }
            }
            if (!found) printf("[%lld] 0\n", ts);
            else {
                printf("[%lld] %s %s %02d-%02d %02d:%02d -> %s %02d-%02d %02d:%02d %d %d\n",
                       ts, best1.trainID, best1.from, best1.lMonth, best1.lDay, best1.lHour, best1.lMin,
                       best1.to, best1.aMonth, best1.aDay, best1.aHour, best1.aMin, best1.price, best1.seat);
                printf("[%lld] %s %s %02d-%02d %02d:%02d -> %s %02d-%02d %02d:%02d %d %d\n",
                       ts, best2.trainID, best2.from, best2.lMonth, best2.lDay, best2.lHour, best2.lMin,
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
            if (!users.isOnline(u)) { printf("[%lld] -1\n", ts); continue; }
            TrainRecord rec;
            if (!trains.getTrain(i, rec) || !rec.released) { printf("[%lld] -1\n", ts); continue; }
            int mon, day;
            std::sscanf(d, "%d-%d", &mon, &day);
            int date = dateToDays(mon, day);
            int idxF = -1, idxT = -1;
            for (int k = 0; k < rec.stationNum; ++k) {
                if (std::strcmp(rec.stations[k], f) == 0) idxF = k;
                if (idxF != -1 && std::strcmp(rec.stations[k], t) == 0) { idxT = k; break; }
            }
            if (idxF == -1 || idxT == -1) { printf("[%lld] -1\n", ts); continue; }
            int fArr, fLev, fDayOff;
            getStationTime(rec, date, idxF, fArr, fLev, fDayOff);
            int startDate = date - fDayOff;
            if (startDate < rec.saleDate1 || startDate > rec.saleDate2) { printf("[%lld] -1\n", ts); continue; }
            int num = std::atoi(n);
            int totalPrice = 0;
            for (int k = idxF; k < idxT; ++k) totalPrice += rec.prices[k];
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
            char prefix[64]; std::memset(prefix, 0, 64); std::memcpy(prefix, u, std::strlen(u));
            int ids[1000], cnt = 0;
            orders.index.scanPrefix(prefix, std::strlen(u),
                                    [](const char key[64], int val, void *ctx) -> bool {
                                        int *p = (int*)ctx;
                                        if (p[0] < 1000) { p[1 + p[0]] = val; ++p[0]; return true; }
                                        return false;
                                    }, &cnt);
            printf("[%lld] %d\n", ts, cnt);
            for (int i = 0; i < cnt; ++i) {
                OrderRecord ord;
                orders.getOrder(ids[i], ord);
                const char *statusStr = (ord.status == 0) ? "success" : (ord.status == 1 ? "pending" : "refunded");
                TrainRecord trec;
                trains.getTrain(ord.trainID, trec);
                int fArr, fLev, fDayOff;
                getStationTime(trec, ord.date, ord.fromIdx, fArr, fLev, fDayOff);
                int startDate = ord.date - fDayOff;
                int tArr, tLev, tDayOff;
                getStationTime(trec, startDate, ord.toIdx, tArr, tLev, tDayOff);
                int lM, lD, lH, lMin, aM, aD, aH, aMin;
                daysToDate(ord.date, lM, lD); minToTime(fLev, lH, lMin);
                daysToDate(startDate + tDayOff, aM, aD); minToTime(tArr, aH, aMin);
                printf("[%s] %s %s %02d-%02d %02d:%02d -> %s %02d-%02d %02d:%02d %d %d\n",
                       statusStr, ord.trainID, trec.stations[ord.fromIdx],
                       lM, lD, lH, lMin, trec.stations[ord.toIdx],
                       aM, aD, aH, aMin, ord.price, ord.num);
            }
        }
        else if (std::strcmp(cmd, "refund_ticket") == 0) {
            const char *u = getParam(params, paramCnt, 'u');
            const char *n = getParam(params, paramCnt, 'n');
            int nth = n ? std::atoi(n) : 1;
            if (!users.isOnline(u)) { printf("[%lld] -1\n", ts); continue; }
            // 先获取用户第 nth 订单信息，以便在退款后处理座位恢复
            char prefix[64]; std::memset(prefix, 0, 64); std::memcpy(prefix, u, std::strlen(u));
            int ids[1000], cnt = 0;
            orders.index.scanPrefix(prefix, std::strlen(u),
                                    [](const char key[64], int val, void *ctx) -> bool {
                                        int *p = (int*)ctx;
                                        if (p[0] < 1000) { p[1 + p[0]] = val; ++p[0]; return true; }
                                        return false;
                                    }, &cnt);
            if (nth < 1 || nth > cnt) { printf("[%lld] -1\n", ts); continue; }
            int orderId = ids[nth - 1];
            OrderRecord ord;
            if (!orders.getOrder(orderId, ord)) { printf("[%lld] -1\n", ts); continue; }
            int originalStatus = ord.status;
            if (originalStatus == 2) { printf("[%lld] -1\n", ts); continue; }
            // 执行退款（修改状态为 2）
            if (!orders.refundOrder(u, nth, ts, ord)) { printf("[%lld] -1\n", ts); continue; }
            // 如果原来是成功购票，则恢复座位并触发候补
            if (originalStatus == 0) {
                TrainRecord rec;
                if (trains.getTrain(ord.trainID, rec)) {
                    int fArr, fLev, fDayOff;
                    getStationTime(rec, ord.date, ord.fromIdx, fArr, fLev, fDayOff);
                    int startDate = ord.date - fDayOff;
                    seats.refundSeat(ord.trainID, startDate, ord.fromIdx, ord.toIdx, ord.num);
                    pending.processRefund(ord.trainID, startDate);
                }
            }
            printf("[%lld] 0\n", ts);
        }
        else if (std::strcmp(cmd, "clean") == 0) {
            // 显式调用析构函数关闭所有文件，删除数据文件，再重新构造对象
            users.~UserManager();
            trains.~TrainManager();
            orders.~OrderManager();
            seats.~SeatManager();
            stIndex.~StationIndex();
            pending.~PendingManager();
            // 删除所有相关数据文件
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
            // 使用 placement new 重新构造，所有对象回到初始空状态
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