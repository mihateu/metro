#include "pathfinder.h"

#define TRANSFER_PENALTY 0

typedef struct {
	int* stations;
	int* dist;
	int size;
	int capacity;
} PriorityQueue;

Station* stations = NULL;
static int max_edge_weight = 1; // C для алгоритма Дайала

int get_stations(char* filename) {

	FILE* file = fopen(filename, "r");
	if (!file) {
		printf("Unable to open stations file.\n");
		return pf_file_error;
	}

	stations = (Station*)calloc(STATIONSCOUNT, sizeof(Station));
	if (!stations) {
		fclose(file);
		return pf_file_error;
	}

	char line[128] = { 0 };
	
	int i = 0;
	while (fgets(line, (int)sizeof(line), file)) {
		if (line[0] == '#') continue;

		int id, line_id;

		sscanf(line, "%d;%d", &id, &line_id);

		stations[i].id = id;
		stations[i].line_id = line_id;
		stations[i].neighbors_count = 0;
		stations[i].neighbors_edges = NULL;

		i++;
	}

	fclose(file);
	return pf_ok;
}

int get_edges(char* filename) {

	FILE* file = fopen(filename, "r");
	if (!file) {
		printf("Unable to open edges file.\n");
		return pf_file_error;
	}

	char line[128] = { 0 };

	while (fgets(line, (int)sizeof(line), file)) {
		if (line[0] == '#') continue;

		int from, to, time, transfer;
		sscanf(line, "%d;%d;%d;%d", &from, &to, &time, &transfer);

		stations[from - 1].neighbors_count++;
		stations[to - 1].neighbors_count++;
	}

	for (int i = 0; i < STATIONSCOUNT; i++) {
		if (stations[i].neighbors_count > 0) {
			stations[i].neighbors_edges = (Edge**)calloc(stations[i].neighbors_count, sizeof(Edge*));
			if (!stations[i].neighbors_edges) {
				fclose(file);
				return pf_error;
			}
		}
	}

	rewind(file);

	int* current_edge = (int*)calloc(STATIONSCOUNT, sizeof(int));
	if (!current_edge) {
		fclose(file);
		return pf_memory_error;
	}

	while (fgets(line, (int)sizeof(line), file)) {
		if (line[0] == '#') continue;

		int from, to, time, transfer;
		sscanf(line, "%d;%d;%d;%d", &from, &to, &time, &transfer);
		if (time < 0) {
			free(current_edge);
			fclose(file);
			return pf_error;
		}

		int w = time + TRANSFER_PENALTY; // +1 — возможное ожидание поезда при пересадке
		if (w > max_edge_weight)
			max_edge_weight = w;

		Edge* edge1 = (Edge*)malloc(sizeof(Edge));
		if (!edge1) {
			fclose(file);
			return pf_memory_error;
		}
		edge1->to = to - 1;
		edge1->time = time;
		edge1->transfer = transfer;

		stations[from - 1].neighbors_edges[current_edge[from - 1]] = edge1;
		current_edge[from - 1]++;

		Edge* edge2 = (Edge*)malloc(sizeof(Edge));
		if (!edge2) {
			fclose(file);
			return pf_memory_error;
		}
		edge2->to = from - 1;
		edge2->time = time;
		edge2->transfer = transfer;

		stations[to - 1].neighbors_edges[current_edge[to - 1]] = edge2;
		current_edge[to - 1]++;
	}

	free(current_edge);
	fclose(file);

	return pf_ok;
}

void free_all() {
	if (!stations) return;

	if (stations) {
		for (int i = 0; i < STATIONSCOUNT; i++) {
			if (stations[i].neighbors_edges) {
				for (int j = 0; j < stations[i].neighbors_count; j++)
					free(stations[i].neighbors_edges[j]);
				free(stations[i].neighbors_edges);
			}
		}
		free(stations);
		stations = NULL;
	}
}

static void free_queue(PriorityQueue* pq) {
	free(pq->stations);
	free(pq->dist);
	free(pq);
}

static PriorityQueue* make_queue(int capacity) {
	PriorityQueue* pq = (PriorityQueue*)malloc(sizeof(PriorityQueue));
	if (!pq) return NULL;

	pq->stations = (int*)malloc(capacity * sizeof(int));
	if (!pq->stations) return NULL;

	pq->dist = (int*)malloc(capacity * sizeof(int));
	if (!pq->dist) return NULL;

	pq->size = 0;
	pq->capacity = capacity;
	return pq;
}

static void push_queue(PriorityQueue* pq, int station, int dist) {
	pq->stations[pq->size] = station;
	pq->dist[pq->size] = dist;
	pq->size++;

	for (int i = pq->size - 1; i > 0; i--) {
		if (pq->dist[i] < pq->dist[i - 1]) {
			int temp_station = pq->stations[i];
			int temp_dist = pq->dist[i];
			
			pq->stations[i] = pq->stations[i - 1];
			pq->dist[i] = pq->dist[i - 1];
			pq->stations[i - 1] = temp_station;
			pq->dist[i - 1] = temp_dist;
		} 
		else
			break;
	}
}

int pop_queue(PriorityQueue* pq) {
	if (pq->size == 0) return -1;

	int result = pq->stations[0];

	for (int i = 0; i < pq->size - 1; i++) {
		pq->stations[i] = pq->stations[i + 1];
		pq->dist[i] = pq->dist[i + 1];
	}
	pq->size--;

	return result;
}

Path* fshortpath(int start, int end) {
	if (start < 0 || start >= STATIONSCOUNT || end < 0 || end >= STATIONSCOUNT)
		return NULL;

	// dist и transfers одним блоком: free(dist) освобождает оба, очистку менять не нужно
	int* dist = (int*)calloc(2 * STATIONSCOUNT, sizeof(int));
	if (!dist)
		return NULL;
	int* transfers = dist + STATIONSCOUNT;
	int* previous = (int*)calloc(STATIONSCOUNT, sizeof(int));
	if (!previous) {
		free(dist);
		return NULL;
	}
	int* visited = (int*)calloc(STATIONSCOUNT, sizeof(int));
	if (!visited) {
		free(dist);
		free(previous);
		return NULL;
	}
	int* previous_transfer = (int*)calloc(STATIONSCOUNT, sizeof(int));
	if (!previous_transfer) {
		free(dist);
		free(previous);
		free(visited);
		return NULL;
	}

	for (int i = 0; i < STATIONSCOUNT; i++) {
		dist[i] = INT_MAX;
		previous[i] = -1;
		previous_transfer[i] = 0;
		transfers[i] = INT_MAX;
	}

	dist[start] = 0;
	transfers[start] = 0;

	PriorityQueue* pq = make_queue(STATIONSCOUNT);
	if (!pq) {
		free(dist);
		free(previous);
		free(visited);
		free(previous_transfer);
		return NULL;
	}
	push_queue(pq, start, dist[start]);

	while (pq->size != 0) {
		int current = pop_queue(pq);
		if (current < 0 || current >= STATIONSCOUNT)
			break;

		if (visited[current]) 
			continue;
		visited[current] = 1;

		if (current == end) 
			break;

		for (int i = 0; i < stations[current].neighbors_count; i++) {
			Edge* edge = stations[current].neighbors_edges[i];
			int neighbor = edge->to;

			if (visited[neighbor]) 
				continue;

			int new_dist = dist[current] + edge->time;
			int new_transfers = transfers[current];
			if (stations[current].line_id != stations[neighbor].line_id) {
				new_dist += TRANSFER_PENALTY; // если пересадка, то докидываем время ожидания поезда
				new_transfers++;
			}

			if (new_dist < dist[neighbor] || (new_dist == dist[neighbor] && new_transfers < transfers[neighbor])) {
				dist[neighbor] = new_dist;
				transfers[neighbor] = new_transfers;
				previous[neighbor] = current;
				previous_transfer[neighbor] = edge->transfer;
				push_queue(pq, neighbor, new_dist);
			}
		}
	}

	if (dist[end] == INT_MAX) {
		free(dist); free(previous); free(visited); free(previous_transfer); free_queue(pq);
		return NULL;
	}

	int pathlen = 0;
	int current = end;
	while (current != -1) {
		pathlen++;
		current = previous[current];
	}

	Path* path = (Path*)malloc(sizeof(Path));
	if (!path) {
		free(dist); free(previous); free(visited); free(previous_transfer); free_queue(pq);
		return NULL;
	}
	path->stations = (int*)malloc(pathlen * sizeof(int));
	if (!path->stations) {
		free(path);
		free(dist); free(previous); free(visited); free(previous_transfer); free_queue(pq);
		return NULL;
	}
	path->length = pathlen;
	path->total_time = dist[end];
	path->transfers = 0;

	current = end;
	for (int i = pathlen - 1; i >= 0; i--) {
		path->stations[i] = current;
		current = previous[current];
	}

	path->transfers = transfers[end];

	free(dist); free(previous); free(visited); free(previous_transfer); free_queue(pq);
	return path;
}

void printPath(Path* path) {
	if (!path || path->length == 0) {
		printf("Маршрут не был найден.\n");
		return;
	}
	
	int current_line = stations[(int)path->stations[0]].line_id;
	printf("%d", stations[(int)path->stations[0]].id);

	for (int i = 1; i < path->length; i++) {
		int next_line = stations[(int)path->stations[i]].line_id;

		if (next_line != current_line) {
			printf("\n -- пересадка на линию %d\n", next_line);
			printf("%d", stations[(int)path->stations[i]].id);
			current_line = next_line;
		}
		else
			printf(" -> %d", stations[(int)path->stations[i]].id);
	}

	printf("\nОбщее время: %d минут\n", path->total_time);
	printf("Пересадок: %d\n", path->transfers);
}

void freePath(Path* path) {
	if (path) {
		free(path->stations);
		free(path);
	}
}
