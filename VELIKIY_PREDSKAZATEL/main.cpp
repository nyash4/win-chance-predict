#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>
#include <cmath>
#include <thread>
#include <chrono>
#include <algorithm> // Для переворота вектора
#include <windows.h>  // Для работы с функциями Windows API

// Структура для хранения данных о матче
struct MatchData {
    bool result; // 0 для поражения, 1 для победы
    int duration; // продолжительность в минутах
    double kda; // (kills + assists) / duration
};

// Функция для парсинга CSV файла
std::vector<MatchData> parseCsvFile(const std::string& filename) {
    std::vector<MatchData> matchData;
    std::ifstream file(filename);
    std::string line;

    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return matchData;
    }

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string value;
        MatchData match;

        // Парсим результат матча (0 или 1)
        std::getline(ss, value, ',');
        match.result = (value == "1");

        // Парсим продолжительность в формате "MM:SS"
        std::getline(ss, value, ',');
        size_t colonPos = value.find(':');
        if (colonPos != std::string::npos) {
            int minutes = std::stoi(value.substr(0, colonPos));
            // Сбрасываем секунды, предполагая, что нам нужны только минуты
            match.duration = minutes;
        }
        else {
            match.duration = 0; // Обработка случая с неправильным форматом продолжительности
        }

        // Парсим KDA: kills/deaths/assists
        std::getline(ss, value, ',');
        size_t firstSlash = value.find('/');
        size_t secondSlash = value.find('/', firstSlash + 1);
        if (firstSlash != std::string::npos && secondSlash != std::string::npos) {
            int kills = std::stoi(value.substr(0, firstSlash));
            int deaths = std::stoi(value.substr(firstSlash + 1, secondSlash - firstSlash - 1));
            int assists = std::stoi(value.substr(secondSlash + 1));
            match.kda = static_cast<double>(kills + assists) / match.duration; // Расчет KDA
        }
        else {
            match.kda = 0.0; // Обработка случая с неправильным форматом KDA
        }

        matchData.push_back(match);
    }

    file.close();

    // Переворачиваем данные, чтобы самые последние матчи шли первыми
    std::reverse(matchData.begin(), matchData.end());
    return matchData;
}

// Функция для преобразования строки const char* в LPCWSTR
LPCWSTR toLPCWSTR(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring wide_str(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wide_str[0], size_needed);
    return wide_str.c_str();
}

// Функция для загрузки и вызова функции Rust из динамической библиотеки
void* loadRustLibrary(const std::string& libraryPath) {
    // Используем LoadLibraryA для строк в формате ASCII (LPCSTR)
    void* handle = LoadLibraryA(libraryPath.c_str());
    if (!handle) {
        std::cerr << "Error loading Rust library: " << GetLastError() << std::endl;
        exit(1);
    }
    return handle;
}

extern "C" {
    char* fetch_player_matches(const char* player_id);
}

void runRustProgram(const std::string& playerId) {
    std::string libraryPath = "dotabuff_parser.dll";  // Убедитесь, что путь правильный
    void* handle = loadRustLibrary(libraryPath);

    // Ищем функцию в библиотеке
    typedef char* (*fetch_player_matches_t)(const char*);
    fetch_player_matches_t fetch_player_matches = (fetch_player_matches_t)GetProcAddress((HMODULE)handle, "fetch_player_matches");

    if (!fetch_player_matches) {
        std::cerr << "Error finding fetch_player_matches function: " << GetLastError() << std::endl;
        exit(1);
    }

    // Вызов функции Rust
    char* result = fetch_player_matches(playerId.c_str());
    std::cout << "Player matches:\n" << result << std::endl;

    // Не забудьте освободить память
    free(result);

    // Закрытие библиотеки
    FreeLibrary((HMODULE)handle);
}

// Функция для вычисления серии побед/поражений
int calculateWinLossStreak(const std::vector<MatchData>& matches) {
    int streak = 0;
    bool lastResult = matches.front().result; // Начинаем с самого последнего матча

    for (const auto& match : matches) {
        if (match.result == lastResult) {
            streak++;
        }
        else {
            break;
        }
    }

    return (lastResult ? streak : -streak); // Положительное для серии побед, отрицательное для серии поражений
}

template <typename T>
T my_min(T a, T b) {
    return (a < b) ? a : b;
}

template <typename T>
T my_max(T a, T b) {
    return (a > b) ? a : b;
}

double calculateAverageKDA(const std::vector<MatchData>& matches) {
    double totalKDA = 0.0;
    int validMatches = 0;

    for (const auto& match : matches) {
        if (match.duration > 0) {
            totalKDA += match.kda;
            validMatches++;
        }
    }

    return (validMatches > 0) ? totalKDA / validMatches : 0.0;
}

double calculateWinLossStreakNormalized(const std::vector<MatchData>& matches) {
    int winStreak = 0;
    int lossStreak = 0;
    int maxWinStreak = 0;
    int maxLossStreak = 0;

    for (const auto& match : matches) {
        if (match.result) {
            winStreak++;
            lossStreak = 0;
        }
        else {
            lossStreak++;
            winStreak = 0;
        }
        maxWinStreak = my_max(maxWinStreak, winStreak);
        maxLossStreak = my_max(maxLossStreak, lossStreak);
    }

    // Нормализуем серию побед/поражений без искусственного ограничения
    double streak = my_max(maxWinStreak, maxLossStreak);
    return static_cast<double>(streak) / matches.size(); // Делим на общее количество матчей, чтобы нормализовать
}



double calculateAverageKDALastMatches(const std::vector<MatchData>& matches, int numMatches) {
    double totalKDA = 0.0;
    int validMatches = 0;
    int start = my_max(0, static_cast<int>(matches.size()) - numMatches);

    for (int i = start; i < matches.size(); ++i) {
        if (matches[i].duration > 0) {
            totalKDA += matches[i].kda;
            validMatches++;
        }
    }

    return (validMatches > 0) ? totalKDA / validMatches : 0.0;
}

double calculateAverageDuration(const std::vector<MatchData>& matches) {
    double totalDuration = 0.0;
    int validMatches = 0;

    for (const auto& match : matches) {
        if (match.duration > 0) {
            totalDuration += match.duration;
            validMatches++;
        }
    }

    return (validMatches > 0) ? totalDuration / validMatches : 0.0;
}

double calculateRecentAverageDuration(const std::vector<MatchData>& matches, int recentCount = 10) {
    int count = my_min(static_cast<int>(matches.size()), recentCount);
    std::vector<MatchData> recentMatches(matches.end() - count, matches.end());
    return calculateAverageDuration(recentMatches);
}

// Используем логарифмическую нормализацию для KDA и продолжительности матчей
double logNormalize(double value, double maxValue) {
    if (value <= 0) return 0.0;
    return std::log1p(value) / std::log1p(maxValue); // log(1+x) для сглаживания
}

int calculateRecentStreak(const std::vector<MatchData>& matches, int recentCount = 10) {
    int streak = 0;
    int count = my_min(static_cast<int>(matches.size()), recentCount);
    bool lastResult = matches[matches.size() - count].result;

    for (int i = matches.size() - count; i < matches.size(); ++i) {
        if (matches[i].result == lastResult) {
            streak++;
        }
        else {
            break;
        }
    }

    return (lastResult ? streak : -streak); // Положительное для серии побед, отрицательное для серии поражений
}

double predictWinChance(const std::vector<MatchData>& matches) {
    // Факторы для предсказания
    double averageKDA = calculateAverageKDA(matches);
    double averageKDALast10 = calculateAverageKDALastMatches(matches, 10);

    double normalizedStreak = calculateWinLossStreakNormalized(matches); // Нормализованная серия побед/поражений
    int recentStreak = calculateRecentStreak(matches, 10); // Серия побед/поражений за последние 10 матчей

    double averageDuration = calculateAverageDuration(matches);
    double recentAverageDuration = calculateRecentAverageDuration(matches);
    double maxDuration = my_max(averageDuration, recentAverageDuration);
    double normalizedDuration = logNormalize(averageDuration, maxDuration); // Нормализуем среднюю длительность матчей

    // Усредняем показатели для повышения стабильности
    double weightedKDA = 0.6 * averageKDA + 0.4 * averageKDALast10;
    double maxKDA = my_max(weightedKDA, averageKDALast10);
    double normalizedKDA = logNormalize(weightedKDA, maxKDA); // Нормализуем с учетом большего значения

    // Корректируем весовые коэффициенты для сбалансированного расчета
    double durationWeight = 0.2; // Снижаем влияние длительности для уменьшения эффекта больших значений

    // Композитный расчет вероятности с оптимизированными весами
    double winChance = 0.35 * normalizedKDA + 0.35 * normalizedStreak + 0.15 * normalizedDuration + 0.15 * (recentStreak / 10.0);

    // Сигмоидальная функция для сглаживания результата
    double sigmoidWinChance = 1.0 / (1.0 + std::exp(-4.0 * (winChance - 0.5))); // Сглаживание с более умеренным коэффициентом

    // Возвращаем вероятность, учитывая дополнительные улучшения
    return sigmoidWinChance;
}


int main() {
    std::string playerId;
    std::cout << "Enter Player Id(from dotabuff): ";
    std::cin >> playerId;

    // Запускаем программу на Rust для парсинга данных
    runRustProgram(playerId);

    // Ждем завершения программы Rust
    std::this_thread::sleep_for(std::chrono::seconds(5)); // Ждем достаточно времени для завершения

    // Парсим данные из CSV файла
    std::vector<MatchData> matches = parseCsvFile(playerId + "_matches.csv");

    if (matches.empty()) {
        std::cerr << "Failed to load match data" << std::endl;
        return 1;
    }

    // Предсказываем шанс на победу
    double winChance = predictWinChance(matches);
    std::cout << "Predicted win chance: " << winChance * 100 << "%" << std::endl;

    system("pause");
    return 0;
}



