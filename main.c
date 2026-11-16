#ifdef _WIN32
	#define _CRT_SECURE_NO_WARNINGS
	#include <locale.h>
	#include <windows.h>
	void set_locale() {
		setlocale(LC_ALL, ".UTF-8");
		SetConsoleCP(65001);
		SetConsoleOutputCP(65001);
	}
#else
	void set_locale() {
		return;
	}
#endif

#include <time.h>

#include "pathfinder.h"

#define STATIONS_DAT "data/stations.dat"
#define EDGES_DAT "data/edges.dat"

int input_station_number(const char* prompt) {
	int number;

	printf("%s", prompt);
	int result = scanf("%d", &number);
	
	if (result != 1) {
		int c;
		while ((c = getchar()) != '\n' && c != EOF); // нечисловой ввод; чистим остаток строки
		printf("Введена некорректная станция.\n");
		return -1;
	}
	
	if (number < 1 || number > STATIONS_COUNT) {
		printf("Введена некорректная станция.\n");
		return -1;
	}
	
	return number;
}

static unsigned long int next = 89890;

int rand(void)
{
  next = next * 1103515245 + 12345;
  return (unsigned int)(next/65536) % 32768;
}

int main() {
	set_locale();

	int count = 10000;

	struct timespec t0, t1;
	clock_gettime(CLOCK_MONOTONIC, &t0); // начинаем замеры

	if (get_stations(STATIONS_DAT) != pf_ok) {
		printf("Error occured while stations info loading.\n");
		return 1;
	}

	if (get_edges(EDGES_DAT) != pf_ok) {
		printf("Error occured while edges info loading.\n");
		return 1;
	}
	
	for (int i = 0; i < count; i++) {
		int start = rand() % STATIONS_COUNT;
		int end = rand() % STATIONS_COUNT;

		while (start == end)
			end = rand() % STATIONS_COUNT;

		Path* path = find_path(start, end);

		if (path)
			free_path(path);
	}
	
	/*
	do {

		int start = input_station_number("Введите номер начальной станции: ");
		int end = input_station_number("Введите номер конечной станции: ");

		if (start == -1 || end == -1)
			goto barrier;

		clock_gettime(CLOCK_MONOTONIC, &t0);

		Path* path = find_path(start - 1, end - 1);

		clock_gettime(CLOCK_MONOTONIC, &t1);

		double time = (t1.tv_sec - t0.tv_sec) * 1e6 + (t1.tv_nsec - t0.tv_nsec) / 1e3;
		
		if (path) {
			print_path(path);
			free_path(path);
		}
		else
			printf("Маршрут не найден.\n");

		printf("Время поиска маршрута: %.1f мкс\n", time);
barrier:
		printf("\n#######################################################\n\n");

	} while (0);
	*/

	free_all();

	clock_gettime(CLOCK_MONOTONIC, &t1); // заканчиваем замеры
	double elapsed_time = (t1.tv_sec - t0.tv_sec) * 1e6 + (t1.tv_nsec - t0.tv_nsec) / 1e3;

	printf("Количество поисков: %d\n", count);
	printf("Общее время: %.2f мкс\n", elapsed_time);

	printf("Программа завершена корректно.\n");

	return 0;
}
