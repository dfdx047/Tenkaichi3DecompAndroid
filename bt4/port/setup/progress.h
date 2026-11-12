#pragma once
#include <algorithm>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Structured worker events, independent of translated status text.
struct SetupProgress {
    uint64_t bytes=0,totalBytes=0;
    unsigned files=0,totalFiles=0;
    double fraction=0;

    bool read(const char *text) {
        unsigned long long done=0,total=0;unsigned count=0,all=0;char extra;
        if(sscanf(text,"%llu %llu %u %u %c",&done,&total,&count,&all,&extra)!=4 ||
           total==0 || done>total || all==0 || count>all)return false;
        bytes=done;totalBytes=total;files=count;totalFiles=all;
        fraction=std::max(fraction,(double)done/(double)total);
        return true;
    }
};

// Extraction and optional language installation carry most of the work.
// These are stage budgets, not a time estimate. No timer advances progress.
static inline double SetupStepWeight(const char *title) {
    if(!strcmp(title,"Game data") || !strcmp(title,"Datos del juego"))return 90;
    if(!strcmp(title,"Language package") || !strcmp(title,"English language"))return 90;
    if(!strcmp(title,"Disc image") || !strcmp(title,"Imagen ISO"))return 2;
    if(!strcmp(title,"Native data tables") || !strcmp(title,"Executable data"))return 3;
    if(!strcmp(title,"Build"))return 30;
    return 5;
}

static inline double SetupPercent(double completed,double total,bool installed) {
    if(installed)return 100;
    return total>0?std::max(0.0,std::min(99.9,completed*100/total)):0;
}
