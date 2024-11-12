#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>
#include <cmath>
#include <thread>
#include <chrono>
#include <algorithm> // ��� ���������� �������
#include <windows.h>  // ��� ������ � ��������� Windows API

// ��������� ��� �������� ������ � �����
struct MatchData {
    bool result; // 0 ��� ���������, 1 ��� ������
    int duration; // ����������������� � �������
    double kda; // (kills + assists) / duration
};

// ������� ��� �������� CSV �����
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

        // ������ ��������� ����� (0 ��� 1)
        std::getline(ss, value, ',');
        match.result = (value == "1");

        // ������ ����������������� � ������� "MM:SS"
        std::getline(ss, value, ',');
        size_t colonPos = value.find(':');
        if (colonPos != std::string::npos) {
            int minutes = std::stoi(value.substr(0, colonPos));
            // ���������� �������, �����������, ��� ��� ����� ������ ������
            match.duration = minutes;
        }
        else {
            match.duration = 0; // ��������� ������ � ������������ �������� �����������������
        }

        // ������ KDA: kills/deaths/assists
        std::getline(ss, value, ',');
        size_t firstSlash = value.find('/');
        size_t secondSlash = value.find('/', firstSlash + 1);
        if (firstSlash != std::string::npos && secondSlash != std::string::npos) {
            int kills = std::stoi(value.substr(0, firstSlash));
            int deaths = std::stoi(value.substr(firstSlash + 1, secondSlash - firstSlash - 1));
            int assists = std::stoi(value.substr(secondSlash + 1));
            match.kda = static_cast<double>(kills + assists) / match.duration; // ������ KDA
        }
        else {
            match.kda = 0.0; // ��������� ������ � ������������ �������� KDA
        }

        matchData.push_back(match);
    }

    file.close();

    // �������������� ������, ����� ����� ��������� ����� ��� �������
    std::reverse(matchData.begin(), matchData.end());
    return matchData;
}

// ������� ��� �������������� ������ const char* � LPCWSTR
LPCWSTR toLPCWSTR(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring wide_str(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wide_str[0], size_needed);
    return wide_str.c_str();
}

// ������� ��� �������� � ������ ������� Rust �� ������������ ����������
void* loadRustLibrary(const std::string& libraryPath) {
    // ���������� LoadLibraryA ��� ����� � ������� ASCII (LPCSTR)
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
    std::string libraryPath = "dotabuff_parser.dll";  // ���������, ��� ���� ����������
    void* handle = loadRustLibrary(libraryPath);

    // ���� ������� � ����������
    typedef char* (*fetch_player_matches_t)(const char*);
    fetch_player_matches_t fetch_player_matches = (fetch_player_matches_t)GetProcAddress((HMODULE)handle, "fetch_player_matches");

    if (!fetch_player_matches) {
        std::cerr << "Error finding fetch_player_matches function: " << GetLastError() << std::endl;
        exit(1);
    }

    // ����� ������� Rust
    char* result = fetch_player_matches(playerId.c_str());
    std::cout << "Player matches:\n" << result << std::endl;

    // �� �������� ���������� ������
    free(result);

    // �������� ����������
    FreeLibrary((HMODULE)handle);
}

// ������� ��� ���������� ����� �����/���������
int calculateWinLossStreak(const std::vector<MatchData>& matches) {
    int streak = 0;
    bool lastResult = matches.front().result; // �������� � ������ ���������� �����

    for (const auto& match : matches) {
        if (match.result == lastResult) {
            streak++;
        }
        else {
            break;
        }
    }

    return (lastResult ? streak : -streak); // ������������� ��� ����� �����, ������������� ��� ����� ���������
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

    // ����������� ����� �����/��������� ��� �������������� �����������
    double streak = my_max(maxWinStreak, maxLossStreak);
    return static_cast<double>(streak) / matches.size(); // ����� �� ����� ���������� ������, ����� �������������
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

// ���������� ��������������� ������������ ��� KDA � ����������������� ������
double logNormalize(double value, double maxValue) {
    if (value <= 0) return 0.0;
    return std::log1p(value) / std::log1p(maxValue); // log(1+x) ��� �����������
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

    return (lastResult ? streak : -streak); // ������������� ��� ����� �����, ������������� ��� ����� ���������
}

double predictWinChance(const std::vector<MatchData>& matches) {
    // ������� ��� ������������
    double averageKDA = calculateAverageKDA(matches);
    double averageKDALast10 = calculateAverageKDALastMatches(matches, 10);

    double normalizedStreak = calculateWinLossStreakNormalized(matches); // ��������������� ����� �����/���������
    int recentStreak = calculateRecentStreak(matches, 10); // ����� �����/��������� �� ��������� 10 ������

    double averageDuration = calculateAverageDuration(matches);
    double recentAverageDuration = calculateRecentAverageDuration(matches);
    double maxDuration = my_max(averageDuration, recentAverageDuration);
    double normalizedDuration = logNormalize(averageDuration, maxDuration); // ����������� ������� ������������ ������

    // ��������� ���������� ��� ��������� ������������
    double weightedKDA = 0.6 * averageKDA + 0.4 * averageKDALast10;
    double maxKDA = my_max(weightedKDA, averageKDALast10);
    double normalizedKDA = logNormalize(weightedKDA, maxKDA); // ����������� � ������ �������� ��������

    // ������������ ������� ������������ ��� ����������������� �������
    double durationWeight = 0.2; // ������� ������� ������������ ��� ���������� ������� ������� ��������

    // ����������� ������ ����������� � ����������������� ������
    double winChance = 0.35 * normalizedKDA + 0.35 * normalizedStreak + 0.15 * normalizedDuration + 0.15 * (recentStreak / 10.0);

    // ������������� ������� ��� ����������� ����������
    double sigmoidWinChance = 1.0 / (1.0 + std::exp(-4.0 * (winChance - 0.5))); // ����������� � ����� ��������� �������������

    // ���������� �����������, �������� �������������� ���������
    return sigmoidWinChance;
}


int main() {
    std::string playerId;
    std::cout << "Enter Player Id(from dotabuff): ";
    std::cin >> playerId;

    // ��������� ��������� �� Rust ��� �������� ������
    runRustProgram(playerId);

    // ���� ���������� ��������� Rust
    std::this_thread::sleep_for(std::chrono::seconds(5)); // ���� ���������� ������� ��� ����������

    // ������ ������ �� CSV �����
    std::vector<MatchData> matches = parseCsvFile(playerId + "_matches.csv");

    if (matches.empty()) {
        std::cerr << "Failed to load match data" << std::endl;
        return 1;
    }

    // ������������� ���� �� ������
    double winChance = predictWinChance(matches);
    std::cout << "Predicted win chance: " << winChance * 100 << "%" << std::endl;

    system("pause");
    return 0;
}



