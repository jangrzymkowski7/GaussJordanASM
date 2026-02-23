#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <numeric>
#include <algorithm>

// Struktura wyników (musi być widoczna dla GUI i Main)
struct TestResult {
    int size;
    std::string implementation;
    int threads;
    double avg_time_ms;
    std::vector<double> result_sample; // Próbka wyniku do wyświetlenia
};

// Deklaracja funkcji asemblerowej (z pliku .asm)
extern "C" void SubtractRowASM(double* dest, const double* src, double factor, int length);

class Matrix
{
public:
    // Typ implementacji
    enum ImplementationType { CPP_STANDARD, CPP_MULTITHREAD, ASM_SIMD };

private:
    std::vector<double> data;
    int size;
    int columns;

    // Funkcje pomocnicze
    void subtract_row_single_thread(int dest_row, int src_row, double factor);
    void subtract_row_asm(int dest_row, int src_row, double factor);
    void elimination_worker(int start_row, int end_row, int pivot_row, double factor_unused, ImplementationType impl);

public:
    Matrix(int n);

    void generate_random();
    void print() const;

    // I/O plików binarnych (dla spójności testów)
    void save_to_file(const std::string& filename) const;
    void load_from_file(const std::string& filename);

    // --- NOWE FUNKCJE EKSPORTU (TXT) ---
    void export_to_txt(const std::string& filename) const;
    void export_solution_txt(const std::string& filename) const;
    // -----------------------------------

    // Odczyt wyniku
    std::vector<double> get_solution_vector() const;

    // Główna funkcja
    double eliminate(ImplementationType impl, int num_threads = 1);

    static int get_logical_processors() { return (int)std::thread::hardware_concurrency(); }

    // Dostęp do danych
    double& at(int r, int c) { return data[r * columns + c]; }
    const double& at(int r, int c) const { return data[r * columns + c]; }
};