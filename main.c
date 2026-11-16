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
	if (scanf("%d", &number) != 1 || number < 1 || number > 10000) {
		printf("Введена некорректная станция.\n");
		return -1;
	}
	
	return number;
}

int main() {
	set_locale();

	if (get_stations(STATIONS_DAT) != pf_ok) {
		printf("Error occured while stations info loading.\n");
		return 1;
	}

	if (get_edges(EDGES_DAT) != pf_ok) {
		printf("Error occured while edges info loading.\n");
		return 1;
	}
	
	int start, end;
	struct timespec t0, t1;

	do {

		start = input_station_number("Введите номер начальной станции: ");
		end = input_station_number("Введите номер конечной станцит: ");

		if (start == -1 || end == -1)
			goto barrier;

		clock_gettime(CLOCK_MONOTONIC, &t0);

		Path* path = fshortpath(start - 1, end - 1);

		clock_gettime(CLOCK_MONOTONIC, &t1);

		double time = (t1.tv_sec - t0.tv_sec) * 1e6 + (t1.tv_nsec - t0.tv_nsec) / 1e3;
		
		if (path) {
			printPath(path);
			freePath(path);
		}
		else
			printf("Маршрут не найден.\n");

		printf("Время поиска маршрута: %.1f мкс\n", time);
barrier:
		printf("\n#######################################################\n\n");

	} while (1);

	free_all();
	printf("Программа завершена корректно.\n");

	return 0;
}
