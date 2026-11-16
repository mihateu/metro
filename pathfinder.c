#include "pathfinder.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define TRANSFER_PENALTY 0

typedef struct {
	int* head; // head[b] - первая станция в корзине b (если -1, то корзина пуста)
	int* next; // next[v] - следующая станция в той же корзине
	int* prev; // prev[v] - предыдущая станция в той же корзине
	int* bucket; // bucket[v] - номер корзины станции v (если -1, то не в очереди)
	int nbuckets; // C + 1, где C - максимальный вес ребра
	int size; // число станций в очереди
	int capacity; // число станций
	int cur; // текущая минимальная дистанция
} PriorityQueue;

static Station* stations = NULL;
static int max_edge_weight = 1; // C для алгоритма Дайала

// прочитать файл data/stations.dat и занести id, line_id, 
// а также проинициализировать neighbors_count, edges в массиве stations
int get_stations(char* filename) {

	FILE* file = fopen(filename, "r");
	if (!file) {
		printf("Unable to open stations file.\n");
		return pf_file_error;
	}

	stations = (Station*)calloc(STATIONS_COUNT, sizeof(Station));
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
		stations[i].edges = NULL;

		i++;
	}

	fclose(file);
	return pf_ok;
}

// прочитать файл data/edges.dat и заполнить neighbors_count и neigbors_edges массива stations
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

	for (int i = 0; i < STATIONS_COUNT; i++) {
		if (stations[i].neighbors_count > 0) {
			stations[i].edges = (Edge**)calloc(stations[i].neighbors_count, sizeof(Edge*));
			if (!stations[i].edges) {
				fclose(file);
				return pf_error;
			}
		}
	}

	rewind(file);

	int* current_edge = (int*)calloc(STATIONS_COUNT, sizeof(int));
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

		int w = time + TRANSFER_PENALTY; // +1 - возможное ожидание поезда при пересадке
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

		stations[from - 1].edges[current_edge[from - 1]] = edge1;
		current_edge[from - 1]++;

		Edge* edge2 = (Edge*)malloc(sizeof(Edge));
		if (!edge2) {
			fclose(file);
			return pf_memory_error;
		}
		edge2->to = from - 1;
		edge2->time = time;
		edge2->transfer = transfer;

		stations[to - 1].edges[current_edge[to - 1]] = edge2;
		current_edge[to - 1]++;
	}

	free(current_edge);
	fclose(file);

	return pf_ok;
}

// очистить выделенную под граф память
void free_all() {
	if (!stations) 
		return;

	if (stations) {
		for (int i = 0; i < STATIONS_COUNT; i++) {
			if (stations[i].edges) {
				for (int j = 0; j < stations[i].neighbors_count; j++)
					free(stations[i].edges[j]);
				free(stations[i].edges);
			}
		}
		free(stations);
		stations = NULL;
	}
}

// очистить память, выделенную под конкретную очередь
static void free_queue(PriorityQueue* pq) {
	if (!pq) return;
	free(pq->head);
	free(pq->next);
	free(pq->prev);
	free(pq->bucket);
	free(pq);
}

// выделяет память под одну очередь и все её динамические поля,
// а также инициализирует нужные поля
static PriorityQueue* make_queue(int capacity) {
	PriorityQueue* pq = (PriorityQueue*)calloc(1, sizeof(PriorityQueue));
	if (!pq) return NULL;

	pq->nbuckets = max_edge_weight + 1;
	pq->capacity = capacity;
	pq->head = (int*)malloc(pq->nbuckets * sizeof(int));
	pq->next = (int*)malloc(capacity * sizeof(int));
	pq->prev = (int*)malloc(capacity * sizeof(int));
	pq->bucket = (int*)malloc(capacity * sizeof(int));
	if (!pq->head || !pq->next || !pq->prev || !pq->bucket) {
		free_queue(pq);
		return NULL;
	}

	for (int b = 0; b < pq->nbuckets; b++) 
		pq->head[b] = -1;
	for (int v = 0; v < capacity; v++)
		pq->bucket[v] = -1;

	pq->size = 0;
	pq->cur = 0;
	return pq;
}

// удаляет станцию из корзины, в которой она лежала
static void unlink_station(PriorityQueue* pq, int station) {
	int bucket = pq->bucket[station]; // узнаем, в какой корзине находится станция

	if (pq->prev[station] != -1) // если у станции есть предыдущая (мы не в начале списка), то склеиваем prev и next, вычеркивая station между ними
		pq->next[pq->prev[station]] = pq->next[station];
	else
		pq->head[bucket] = pq->next[station]; // если у станции нет предыдущей, то мы в начале списка и достаточно заменить station на next[station]

	if (pq->next[station] != -1) // если мы не в конце списка, то необходимо изменить значение предыдущей станции для next[station], чтобы оно вело не в station, а в prev[station]
		pq->prev[pq->next[station]] = pq->prev[station];

	pq->bucket[station] = -1; // станция больше не находится ни в какой корзине
	pq->size--; // из очереди выняли одну станцию - размер уменьшился на единицу
}

static void push_queue(PriorityQueue* pq, int station, int dist) {
	if (pq->bucket[station] != -1) // если станция уже в какой-то корзине, то вынимаем из старой корзины
		unlink_station(pq, station);

	// очередь пуста или станция ближе текущего минимума: сдвигаем cur назад,
	// иначе станция окажется "позади" cur и будет извлечена с огромной задержкой
	if (pq->size == 0 || dist < pq->cur)
		pq->cur = dist;

	int bucket = dist % pq->nbuckets; // вычисляем, в какую корзину положить станцию
	pq->prev[station] = -1; // добавляем в начало корзины, то есть в голову списка, у головы нет предыдущего элемента
	pq->next[station] = pq->head[bucket]; // сдвигаем предыдущую голову вперед
	if (pq->head[bucket] != -1)
		pq->prev[pq->head[bucket]] = station; // если корзина была не пустая, то для прошлого элемента в начале списка подвязываем индекс вставленной станции в prev
	pq->head[bucket] = station; // вставка в начало корзины
	pq->bucket[station] = bucket; // теперь станция лежит в корзине под номером bucket
	pq->size++; // вставили станцию в очередь - она увеличилась в размере на единицу
}

int pop_queue(PriorityQueue* pq) {
	if (pq->size == 0) // если очередь пуста, то и брать нечего
		return -1;

	// все дистанции в очереди лежат в [cur, cur + C], поэтому цикл конечен
	while (pq->head[pq->cur % pq->nbuckets] == -1) // проходим циклично по корзинам в поиске первой непустой
		pq->cur++;

	int station = pq->head[pq->cur % pq->nbuckets]; // забираем первую станцию из найденной корзины
	unlink_station(pq, station); // поскольку станцию забрали, сделали pop, то ее необходимо удалить из очереди
	return station; // возвращаем номер взятой станции
}

typedef struct {
	int* dist[2]; // dist[0][station] - время от start до станции, dist[1][station] - время от end до станции
	int* transfers[2]; // transfers[0][station] - пересадки на пути от start до станции, transfers[1][station] - от end до станции
	int* parent[2]; // хранит информацию, из какой станции алгоритм пришел в текущую (инфа о родительской станции)
					// parent[0][station] - предыдущая станция на пути от start до станции, parent[1][station] - предыдущая станция от end до станции
	
	PriorityQueue* queue[2]; // два направления - две очереди (queue[0] - от start, queue[1] - от end)
	
	int best_time; // минимальное время полного маршрута, найденное к текущему моменту
	int best_transfers; // количество пересадок у лучшего маршрута, сохраненного в best_time
	int best_forward; // номер станции, на которой заканчивается прямая часть маршрута
	int best_backward; // номер станции, на которой заканчивается обратная часть маршрута
} BidirectionState;

// возвращает минимальное расстояние в очереди, не извлекая станцию и не изменяя очередь
static int peek_queue_distance(PriorityQueue* pq) {
	if (!pq || pq->size == 0) // нет очереди - нет минимального расстояния
		return INT_MAX;
	
	int dist = pq->cur; // pq->cur - текущая минимальная дистанция, с которой работает очередь

	while (pq->head[dist % pq->nbuckets] == -1) { // ищем первую непустую корзину; на ней dist и покажет минимальное расстояние в очереди
		if (dist == INT_MAX) // на случай переполнения dist
			return INT_MAX;
		dist++;
	}

	return dist;
}

// обновляет лучший найденный маршрут
// best_forward и best_backward - концы места соединения
static void update_best(BidirectionState* state,	int candidate_time,	int candidate_transfers, int forward, int backward) {
	if (candidate_time < 0 || candidate_time >= INT_MAX) // проверяем правильность времени в пути
		return;
	
	if (candidate_time < state->best_time || 
		(candidate_time == state->best_time && candidate_transfers < state->best_transfers)) { // проверка, лучше ли новый маршрут (меньше ли время или меньше ли кол-во пересадок)
			// если маршрут лучше, то обновляем лучшие значения в структуре BidirectionState* state
			state->best_time = candidate_time;
			state->best_transfers = candidate_transfers;
			state->best_forward = forward;
			state->best_backward = backward;
		}
}

// основной цикл двунаправленного поиска
// на каждой итерации берется станция из той очереди, у которой минимальное расстояние меньше
static void search(BidirectionState* state) {
	while (true) {
		int min_forward = peek_queue_distance(state->queue[0]); // минимальное расстояние в очереди прямого поиска
		int min_backward = peek_queue_distance(state->queue[1]); // минимальное расстояние в очереди обратного поиска

		if (min_forward == INT_MAX && min_backward == INT_MAX) // обе очереди пусты, искать больше нечего
			break;

		if (state->best_time != INT_MAX) { // уже существует кандидат на полный маршрут

			// считаем наименьшее время, которое можем получить, объединив две части поиска
			int lower = (min_forward == INT_MAX ? 0 : min_forward) + (min_backward == INT_MAX ? 0 : min_backward);

			// если наименьшее время, которое можно получить сейчас, не менее лучшего, то нет смысла продолжать
			if (lower >= state->best_time)
				break;
		}

		int side = (min_forward <= min_backward) ? 0 : 1; // 0 - расширяем прямой поиск, 1 - обратный (пустая очередь имеет INT_MAX и не выбирается)
		int other = 1 - side; // вычисляем противоположное направление (1 - 0 = 1, 1 - 1 = 0)

		int current = pop_queue(state->queue[side]); // достаем станцию из очереди выбранной стороны

		int current_dist = state->dist[side][current]; // получаем известное минимальное время до текущей станции
		int current_transfers = state->transfers[side][current]; // получаем известное количество пересадок на маршруте

		if (state->dist[other][current] != INT_MAX) { // если у другого направления есть известный путь до current, то можно объединить два найденных пути через эту станцию
			int candidate_time = current_dist + state->dist[other][current]; // общее время = время прямого пути + время обратного пути
			int candidate_transfers = current_transfers + state->transfers[other][current]; // общее количество пересадок = прямые пересадки + обратные пересадки

			update_best(state, candidate_time, candidate_transfers, current, current); // на случай, если нашли более крутой маршрут, вызываем update_best, которая решит, обновлять ли имеющийся маршрут или нет
		}

		for (int i = 0; i < stations[current].neighbors_count; i++) { // проход по всем известным соседям станции
			Edge* edge = stations[current].edges[i]; // указатель на конкретное ребро графа
			int neighbor = edge->to; // номер станции-соседа для текущей

			int line_changed = (stations[current].line_id != stations[neighbor].line_id) ? 1 : 0; // если между текущей станцией и станцией-соседом необходима пересадка, то поднимаем флаг
			int new_dist = current_dist + edge->time + (line_changed ? TRANSFER_PENALTY : 0); // новое время до соседа = время до текущей станции + время перегона + штраф за возможную пересадку
			if (new_dist >= INT_MAX) // на случай некорректного расстояния (для особой осторожности, по идее не должно срабатывать)
				continue;
			int new_transfers = current_transfers + line_changed; // новое количество пересадок = количество пересадок до текущей станции + нужна ли пересадка, чтобы добраться до соседа

			if (state->dist[other][neighbor] != INT_MAX) { // если известен путь до соседа с другой стороны, то можно соединить маршруты
				
				// new_dist - время от начала текущего направления (start или end) до соседа (уже включен перегон current->neighbor)
				// state->dist[other][neighbor] - время от neighbor до конца противоположного направления (end или start)
				int candidate_time = new_dist + state->dist[other][neighbor]; 
				// new_transfers - количество пересадок от начала текущего направления до соседа
				// state->transfers - количество пересадок от соседа до конца противоположного направления
				int candidate_transfers = new_transfers + state->transfers[other][neighbor];
				
				int forward = (side == 0) ? current : neighbor; // если текущая сторона прямая, то forward (станция со стороны прямого поиска) = current и наоборот
				int backward = (side == 0) ? neighbor : current; // если текущая сторона обратная, то backward (станция со стороны обратного поиска) = neighbor и наоборот

				update_best(state, candidate_time, candidate_transfers, forward, backward); // на случай, если наш склеенный маршрут окажется лучшим
			}

			// мы рассмотрели соседа; если в этом рассмотрении оказалось, что рассмотренным путем пойти быстрее до него или меньше число пересадок при равном времени,
			// то обновляем для соседа лучшее время до станции, количество пересадок, откуда пришли к соседу, получив такие результаты
			if (new_dist < state->dist[side][neighbor] ||
				(new_dist == state->dist[side][neighbor] && new_transfers < state->transfers[side][neighbor])) {
				state->dist[side][neighbor] = new_dist;
				state->transfers[side][neighbor] = new_transfers;
				state->parent[side][neighbor] = current;

				push_queue(state->queue[side], neighbor, new_dist); // поскольку нашли лучшие значения, пересчитаем остальные возможные маршруты
			}
		}
	}
}

// главная функция алгоритма поиска
Path* find_path(int start, int end) {
	if (!stations || start < 0 || start >= STATIONS_COUNT || end < 0 || end >= STATIONS_COUNT) // проверка корректности входных данных
		return NULL;
	
	if (start == end) { // вырожденный случай: начало совпало с концом, искать ничего не надо
		Path* path = (Path*)malloc(sizeof(Path)); // выделяем память под структуру Path
		if (!path)
			return NULL;
		
		path->stations = (int*)malloc(sizeof(int)); // выделяем память под один номер станции, больше не нужно
		if (!path->stations) {
			free(path);
			return NULL;
		}

		path->stations[0] = start; // единственная станция маршрута
		path->length = 1; // в маршруте одна станция
		path->total_time = 0; // никуда не нужно ехать
		path->transfers = 0; // а зачем?

		return path;
	}

	BidirectionState state = { 0 }; // инициализируем и забиваем нулями структуру BidirectionalState
    state.best_time = INT_MAX; // INT_MAX, поскольку ищем минимальное время
    state.best_transfers = INT_MAX; // INT_MAX, поскольку ищем минимальное время
    state.best_forward = -1; // станции соединения не определены, пускай будет -1
    state.best_backward = -1; // станции соединения не определены, пускай будет -1

	Path* result = NULL; // тут будет результат

	for (int side = 0; side < 2; side++) { // выделяем память для двух направлений и сразу инициализируем
		state.dist[side] = (int*)malloc(STATIONS_COUNT * sizeof(int));
		state.transfers[side] = (int*)malloc(STATIONS_COUNT * sizeof(int));
		state.parent[side] = (int*)malloc(STATIONS_COUNT * sizeof(int));

		if (!state.dist[side] || !state.transfers[side] || !state.parent[side])
			goto cleanup;
		
		for (int i = 0; i < STATIONS_COUNT; i++) {
			state.dist[side][i] = INT_MAX;
			state.transfers[side][i] = INT_MAX;
			state.parent[side][i] = -1;
		}

		state.queue[side] = make_queue(STATIONS_COUNT);

		if (!state.queue[side])
			goto cleanup;
	}

	// инициализация начальных станций для обоих направлений
	state.dist[0][start] = 0;
    state.transfers[0][start] = 0;

    state.dist[1][end] = 0;
    state.transfers[1][end] = 0;

	// помещение начальных станций в соответствующие очереди для начала поиска
    push_queue(state.queue[0], start, 0);
    push_queue(state.queue[1], end, 0);

	search(&state); // запускаем двунаправленный поиск в текущем потоке

	if (state.best_time == INT_MAX || state.best_forward < 0 || state.best_backward < 0) // если маршрут не найден, то сразу очищаем выделенную память, find_path вернет NULL
        goto cleanup;

	// восстановление прямой части маршрута
	int forward_len = 0; // счетчик количества станций в этой части
    int current = state.best_forward; // номер станции, на которой закончилась прямая часть найденного маршрута
    int found_start = 0; // флаг, показывающий, удалось ли дойти до начальной станции

	// в цикле восстанавливаем маршрут, идя через массив parent по пройденным станциям
	while (current != -1 && forward_len < STATIONS_COUNT) {
        forward_len++;

        if (current == start) {
            found_start = 1;
            break;
        }

        current = state.parent[0][current];
    }

	if (!found_start)
        goto cleanup;

	// восстановление обратной части маршрута
	int backward_len = 0; // счетчик количества станций в этой части
    current = state.best_backward; // номер станции, на которой закончилась обратная часть найденного маршрута
    int found_end = 0; // флаг, показывающий, удалось ли дойти до конечной станции

	// в цикле восстанавливаем маршрут, идя через массив parent по пройденным станциям
    while (current != -1 && backward_len < STATIONS_COUNT) {
        backward_len++;

        if (current == end) {
            found_end = 1;
            break;
        }

        current = state.parent[1][current];
    }

    if (!found_end) {
        goto cleanup;
	}

	int same_meeting = (state.best_forward == state.best_backward) ? 1 : 0; // 1, если проходы встретились на одной станции, 0, если соединились через общее ребро 

    int path_len = forward_len + backward_len - same_meeting; // вычитаем same_meeting, чтобы не посчитать одну станцию дважды (случай A-B и B-C: тут 3 станции, а не 4)

    int* forward_path = (int*)malloc(forward_len * sizeof(int)); // выделяем память под временный массив прямой части

    if (!forward_path) {
        goto cleanup;
	}

	current = state.best_forward;

    for (int i = 0; i < forward_len; i++) {
        forward_path[i] = current; // вписываем в массив в обратном порядке пройденные станции
        current = state.parent[0][current];
    }

    result = (Path*)malloc(sizeof(Path)); // выделяем память под итоговый маршрут

    if (!result) {
        free(forward_path);
        goto cleanup;
    }

    result->stations = (int*)malloc(path_len * sizeof(int)); // выделяем память под всю цепочку станций

	if (!result->stations) {
        free(result);
        result = NULL;
        free(forward_path);
        goto cleanup;
    }

    result->length = path_len; // финальная длина маршрута
    result->total_time = state.best_time; // финальное время маршрута
    result->transfers = state.best_transfers; // финальное число пересадок на маршруте

    int pos = 0;

    for (int i = forward_len - 1; i >= 0; i--)
        result->stations[pos++] = forward_path[i]; // заполнение результата прямой частью (в обратном порядке, поскольку и клали в forward_path в обратном порядке станции)

    free(forward_path); // больше ни к чему временный массив этот нам

	current = state.best_backward;

    if (same_meeting)
        current = state.parent[1][current]; // если оба прохода встретились на одной станции, то она уже была учтена, двигаем указатель к предыдущей

    while (current != -1 && pos < path_len) {
        result->stations[pos++] = current; // в обратном порядке как раз и развернем так, чтобы end лежал в конце; удобно

        if (current == end)
            break;

        current = state.parent[1][current];
    }

	if (pos != path_len || result->stations[0] != start || result->stations[path_len - 1] != end) { // если возникла проблема с итоговым маршрутом (вдруг будет)
        free_path(result);
        result = NULL;
    }

cleanup: // очищаем все, что выделили, для обоих сторон
    for (int side = 0; side < 2; side++) {
        free(state.dist[side]);
        free(state.transfers[side]);
        free(state.parent[side]);
        free_queue(state.queue[side]);
    }

    return result; // возвращаем найденный маршрут
}

void print_path(Path* path) {
	if (!path || path->length == 0) {
		printf("Маршрут не был найден.\n");
		return;
	}
	
	int current_line = stations[(int)path->stations[0]].line_id; // получаем номер линии начальной станции
	printf("%d", stations[(int)path->stations[0]].id); // печатаем номер начальной станции

	for (int i = 1; i < path->length; i++) { // идем по всему маршруту
		int next_line = stations[(int)path->stations[i]].line_id; // получаем номер линии следующей станции

		if (next_line != current_line) { // если номера линий не совпадают, то, очевидно, была пересадка
			printf("\n -- пересадка на линию %d\n", next_line);
			printf("%d", stations[(int)path->stations[i]].id);
			current_line = next_line; // теперь мы на этой линии находимся
		}
		else
			printf(" -> %d", stations[(int)path->stations[i]].id); // если не пересадка, то просто перегон на другую станцию этой же линии
	}

	printf("\nОбщее время: %d минут\n", path->total_time);
	printf("Пересадок: %d\n", path->transfers);
}

// освобождает память, выделенную под структуру Path и массив stations внутри нее
void free_path(Path* path) {
	if (path) {
		free(path->stations);
		free(path);
	}
}