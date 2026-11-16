#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define STATIONSCOUNT 10000

typedef enum {pf_ok, pf_error, pf_file_error, pf_memory_error} pfStatus;

typedef struct {
	int to;
	int time;
	int transfer;
} Edge;

typedef struct {
	int id;
	int line_id;
	int neighbors_count;
	Edge** neighbors_edges;
} Station;

typedef struct {
	int* stations;
	int length;
	int total_time;
	int transfers;
} Path;

int get_stations(char* filename);
int get_edges(char* filename);
void free_all();

Path* fshortpath(int start, int end);
void printPath(Path* path);
void freePath(Path* path);
