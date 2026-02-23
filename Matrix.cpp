#include "Matrix.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <random>
#include <iomanip>
#include <thread>
#include <fstream>
#include <vector>

Matrix::Matrix(int n) : size(n), columns(n + 1)
{
    if (n <= 0) throw std::invalid_argument("Size must be > 0");
    data.resize(size * columns);
}

void Matrix::generate_random()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> distrib(1.0, 10.0);

    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < columns; ++j) {
            at(i, j) = distrib(gen);
        }
    }
}

void Matrix::print() const
{
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < columns; ++j) {
            std::cout << std::fixed << std::setprecision(2) << at(i, j) << "\t";
        }
        std::cout << "\n";
    }
}

// --- ZAPIS I ODCZYT ---
void Matrix::save_to_file(const std::string& filename) const
{
    std::ofstream ofs(filename, std::ios::out | std::ios::binary);
    if (!ofs.is_open()) throw std::runtime_error("Cannot open file for writing");
    ofs.write(reinterpret_cast<const char*>(&size), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(&columns), sizeof(int));
    ofs.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(double));
    ofs.close();
}

void Matrix::load_from_file(const std::string& filename)
{
    std::ifstream ifs(filename, std::ios::in | std::ios::binary);
    if (!ifs.is_open()) throw std::runtime_error("Cannot open file for reading");
    int loaded_size, loaded_columns;
    ifs.read(reinterpret_cast<char*>(&loaded_size), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&loaded_columns), sizeof(int));
    if (loaded_size != size || loaded_columns != columns) {
        size = loaded_size; columns = loaded_columns;
        data.resize(size * columns);
    }
    ifs.read(reinterpret_cast<char*>(data.data()), data.size() * sizeof(double));
    ifs.close();
}

// --- POBIERANIE WYNIKU X ---
std::vector<double> Matrix::get_solution_vector() const
{
    std::vector<double> solution(size);
    for (int i = 0; i < size; ++i) {
        solution[i] = at(i, columns - 1);
    }
    return solution;
}

// --- CORE CALCULATION METHODS ---

void Matrix::subtract_row_single_thread(int dest_row, int src_row, double factor)
{
    int offset = dest_row * columns;
    int src_offset = src_row * columns;
    for (int j = 0; j < columns; ++j)
    {
        data[offset + j] -= factor * data[src_offset + j];
    }
}

void Matrix::subtract_row_asm(int dest_row, int src_row, double factor)
{
    double* dest_ptr = &data[dest_row * columns];
    const double* src_ptr = &data[src_row * columns];
    int len = columns;

    // Wyrównanie do 4 double (AVX 256-bit)
    int simd_len = len & ~3;

    if (simd_len > 0)
    {
        SubtractRowASM(dest_ptr, src_ptr, factor, simd_len);
    }

    // Reszta (0-3 elementy)
    for (int j = simd_len; j < len; ++j)
    {
        dest_ptr[j] -= factor * src_ptr[j];
    }
}

void Matrix::elimination_worker(int start_row, int end_row, int pivot_row, double factor_unused, ImplementationType impl)
{
    for (int i = start_row; i < end_row; ++i)
    {
        if (i != pivot_row)
        {
            double current_factor = at(i, pivot_row);

            if (impl == CPP_MULTITHREAD)
                subtract_row_single_thread(i, pivot_row, current_factor);
            else if (impl == ASM_SIMD)
                subtract_row_asm(i, pivot_row, current_factor);

            at(i, pivot_row) = 0.0;
        }
    }
}

double Matrix::eliminate(ImplementationType impl, int num_threads)
{
    auto start = std::chrono::high_resolution_clock::now();

    for (int k = 0; k < size; ++k)
    {
        // 1. Pivot
        int pivot_row = k;
        double max_val = std::abs(at(k, k));
        for (int i = k + 1; i < size; ++i) {
            if (std::abs(at(i, k)) > max_val) {
                max_val = std::abs(at(i, k));
                pivot_row = i;
            }
        }
        if (max_val == 0.0) continue; // Singular

        // 2. Swap
        if (pivot_row != k) {
            for (int j = 0; j < columns; ++j) std::swap(at(k, j), at(pivot_row, j));
        }

        // 3. Normalize
        double pivot_val = at(k, k);
        for (int j = 0; j < columns; ++j) at(k, j) /= pivot_val;

        // 4. Eliminate
        if (impl == CPP_STANDARD)
        {
            for (int i = 0; i < size; ++i)
            {
                if (i != k)
                {
                    double factor = at(i, k);
                    subtract_row_single_thread(i, k, factor);
                    at(i, k) = 0.0;
                }
            }
        }
        else
        {
            // Multi-thread logic (Common for C++ Multi and ASM SIMD)
            num_threads = std::min({ num_threads, size, 64 });
            std::vector<std::thread> threads;
            int rows_per_thread = size / num_threads;
            int start_row = 0;

            for (int t = 0; t < num_threads; ++t)
            {
                int end_row = (t == num_threads - 1) ? size : start_row + rows_per_thread;
                threads.emplace_back(&Matrix::elimination_worker, this, start_row, end_row, k, at(k, k), impl);
                start_row = end_row;
            }
            for (auto& t : threads) if (t.joinable()) t.join();
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();

}
    void Matrix::export_to_txt(const std::string & filename) const
    {
        std::ofstream ofs(filename);
        if (!ofs.is_open()) return; // Ignoruj błędy, żeby nie crashować

        ofs << "Matrix Size: " << size << " rows x " << columns << " cols\n";
        ofs << "--------------------------------------------\n";

        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < columns; ++j) {
                // Formatowanie: stała szerokość, 4 miejsca po przecinku
                ofs << std::fixed << std::setprecision(4) << std::setw(10) << at(i, j) << " ";
            }
            ofs << "\n";
        }
        ofs.close();
    }

    // Zapisuje tylko wynik (Wektor X)
    void Matrix::export_solution_txt(const std::string & filename) const
    {
        std::ofstream ofs(filename);
        if (!ofs.is_open()) return;

        ofs << "Solution Vector X (Size: " << size << ")\n";
        ofs << "--------------------------------\n";

        std::vector<double> sol = get_solution_vector();
        for (int i = 0; i < size; ++i) {
            ofs << "X[" << i << "] = " << std::fixed << std::setprecision(6) << sol[i] << "\n";
        }
        ofs.close();
    }