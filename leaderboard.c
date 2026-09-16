// leaderboard.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "leaderboard.h"

#define MAX_LINE_LENGTH 128

void SaveToLeaderboard(const char* playerName, int score) {
    FILE* file = fopen("leaderboard.txt", "a");
    if (file == NULL) {
        fprintf(stderr, "Error: could not open leaderboard.txt for writing\n");
        return;
    }

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", t);

    fprintf(file, "%s,%d,%s\n", playerName, score, timeStr);
    fclose(file);
}
