#pragma once

#include <stdio.h>

#define STATIONS_COUNT 10000

#define true 1
#define false 0

typedef enum {pf_ok, pf_error, pf_file_error, pf_memory_error} pfStatus;

typedef struct {
	int to; // номер станции, куда направлено ребро
	int time; // время между станциями
	int transfer; // является ли узел пересадочным
} Edge;

typedef struct {
	int id; // номер станции
	int line_id; // номер линии, к которой станция принадлежит
	int neighbors_count; // количество соседей у станции
	Edge* edges; // массив ребер, ведущих к соседям
} Station;

typedef struct {
	int* stations; // массив номеров станций, через которые проходит маршрут
	int length; // количество станций в stations
	int total_time; // суммарное время прохождения маршрута
	int transfers; // суммарное количество пересадок
} Path;

int get_stations(char* filename);
int get_edges(char* filename);
void free_all();

Path* find_path(int start, int end);
void print_path(Path* path);
void free_path(Path* path);
