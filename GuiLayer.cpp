#include "GuiLayer.h"
#include "imgui/imgui.h"
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include "Matrix.h" 

extern std::vector<TestResult> ALL_RESULTS;
extern void run_all_benchmarks(const std::vector<int>& sizes, const std::vector<int>& thread_configs);

// ZMIENNE STANU
static int custom_N = 8;
static int custom_threads_verify = 4;
static bool show_solution = false;
static std::vector<double> current_solution_X;

static bool bench_size_s = true;
static bool bench_size_m = true;
static bool bench_size_l = true;
static bool bench_size_xl = true;

static bool t_1 = true;
static bool t_2 = true;
static bool t_4 = true;
static bool t_8 = true;
static bool t_16 = true;
static bool t_32 = true;
static bool t_64 = true;

static int chart_size_selection = 1024;
static float plot_data_cpp[70] = { 0 };
static float plot_data_asm[70] = { 0 };

void UpdatePlots(int size_filter)
{
    std::fill(std::begin(plot_data_cpp), std::end(plot_data_cpp), 0.0f);
    std::fill(std::begin(plot_data_asm), std::end(plot_data_asm), 0.0f);

    for (const auto& res : ALL_RESULTS)
    {
        if (res.size == size_filter)
        {
            int t = res.threads;
            if (t > 0 && t <= 64)
            {
                if (res.implementation.find("C++ Multi") != std::string::npos)
                    plot_data_cpp[t] = (float)res.avg_time_ms;
                else if (res.implementation.find("ASM") != std::string::npos)
                    plot_data_asm[t] = (float)res.avg_time_ms;
            }
        }
    }
}

// Funkcja pomocnicza do generowania raportów TXT dla S/M/L/XL
void GenerateDebugFiles()
{
    std::vector<int> sizes_to_export = { 128, 256, 512, 1024 };

    for (int n : sizes_to_export)
    {
        // 1. Wczytaj lub wygeneruj macierz początkową
        Matrix m(n);
        std::string bin_file = "matrix_" + std::to_string(n) + ".bin";
        try {
            m.load_from_file(bin_file);
        }
        catch (...) {
            m.generate_random();
            m.save_to_file(bin_file);
        }

        // 2. Zapisz stan POCZĄTKOWY (INPUT)
        std::string txt_in = "DEBUG_Matrix_" + std::to_string(n) + "_INPUT.txt";
        m.export_to_txt(txt_in);

        // 3. Rozwiąż (używając ASM dla szybkości)
        // Używamy 1 wątku dla stabilności eksportu, czas tu nie gra roli
        m.eliminate(Matrix::ASM_SIMD, 8);

        // 4. Zapisz stan KOŃCOWY (OUTPUT - cała macierz zer i jedynek)
        std::string txt_out = "DEBUG_Matrix_" + std::to_string(n) + "_OUTPUT.txt";
        m.export_to_txt(txt_out);

        // 5. Zapisz tylko WYNIKI (SOLUTION X)
        std::string txt_sol = "DEBUG_Matrix_" + std::to_string(n) + "_SOLUTION.txt";
        m.export_solution_txt(txt_sol);
    }
}

void DrawApp()
{
    ImGui::StyleColorsDark();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 480), ImGuiCond_Always); // Zwiększyłem wysokość

    // --- OKNO 1: SANDBOX ---
    ImGui::Begin("1. Sandbox & Verification", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Custom Matrix Solver");
    ImGui::Separator();

    ImGui::InputInt("Size N", &custom_N);
    if (custom_N < 2) custom_N = 2;
    if (custom_N > 2048) custom_N = 2048;
    ImGui::SliderInt("Threads", &custom_threads_verify, 1, 64);

    if (ImGui::Button("Solve & Save TXT", ImVec2(-1, 30)))
    {
        Matrix m(custom_N);
        m.generate_random();

        // Zapisz INPUT
        m.export_to_txt("Sandbox_Custom_" + std::to_string(custom_N) + "_INPUT.txt");

        m.eliminate(Matrix::ASM_SIMD, custom_threads_verify);

        // Zapisz OUTPUT
        m.export_to_txt("Sandbox_Custom_" + std::to_string(custom_N) + "_OUTPUT.txt");
        m.export_solution_txt("Sandbox_Custom_" + std::to_string(custom_N) + "_SOLUTION.txt");

        current_solution_X = m.get_solution_vector();
        show_solution = true;
    }

    // Info dla użytkownika
    if (show_solution) ImGui::TextDisabled("(Files saved to .exe folder)");

    if (show_solution)
    {
        ImGui::Spacing();
        ImGui::Text("Result Vector X:");
        ImGui::BeginChild("SolutionScroll", ImVec2(0, 100), true);
        for (size_t i = 0; i < current_solution_X.size(); ++i)
        {
            if (i >= 100) { ImGui::Text("... (limited)"); break; }
            ImGui::Text("X[%llu] = %.5f", i, current_solution_X[i]);
        }
        ImGui::EndChild();
    }
    ImGui::End();

    // --- OKNO 2: BENCHMARK CONFIG ---
    ImGui::SetNextWindowPos(ImVec2(340, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(400, 480), ImGuiCond_Always);

    ImGui::Begin("2. Benchmark Configuration", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    // (Checkboxy rozmiarów i wątków - BEZ ZMIAN)
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Select Data Sets");
    ImGui::Checkbox("S (128)", &bench_size_s); ImGui::SameLine();
    ImGui::Checkbox("M (256)", &bench_size_m);
    ImGui::Checkbox("L (512)", &bench_size_l); ImGui::SameLine();
    ImGui::Checkbox("XL (1024)", &bench_size_xl);

    ImGui::Separator();
    ImGui::Columns(2, "threads_cols", false);
    ImGui::Checkbox("1 Thread", &t_1);   ImGui::NextColumn();
    ImGui::Checkbox("2 Threads", &t_2);  ImGui::NextColumn();
    ImGui::Checkbox("4 Threads", &t_4);  ImGui::NextColumn();
    ImGui::Checkbox("8 Threads", &t_8);  ImGui::NextColumn();
    ImGui::Checkbox("16 Threads", &t_16); ImGui::NextColumn();
    ImGui::Checkbox("32 Threads", &t_32); ImGui::NextColumn();
    ImGui::Checkbox("64 Threads", &t_64); ImGui::NextColumn();
    ImGui::Columns(1);

    if (ImGui::Button("Select All")) { t_1 = t_2 = t_4 = t_8 = t_16 = t_32 = t_64 = true; }
    ImGui::SameLine();
    if (ImGui::Button("Clear All")) { t_1 = t_2 = t_4 = t_8 = t_16 = t_32 = t_64 = false; }

    ImGui::Spacing();
    ImGui::Separator();

    // --- NOWY PRZYCISK DO EKSPORTU PLIKÓW ---
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Documentation Tools");
    if (ImGui::Button("GENERATE DEBUG FILES (.txt)", ImVec2(-1, 30)))
    {
        GenerateDebugFiles();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Generates Input/Output/Solution text files for S, M, L, XL matrices.");

    ImGui::Spacing();
    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.0f, 1.0f));
    if (ImGui::Button("RUN SELECTED TESTS", ImVec2(-1, 50)))
    {
        std::vector<int> sizes_to_test;
        if (bench_size_s) sizes_to_test.push_back(128);
        if (bench_size_m) sizes_to_test.push_back(256);
        if (bench_size_l) sizes_to_test.push_back(512);
        if (bench_size_xl) sizes_to_test.push_back(1024);

        std::vector<int> threads_to_test;
        if (t_1) threads_to_test.push_back(1);
        if (t_2) threads_to_test.push_back(2);
        if (t_4) threads_to_test.push_back(4);
        if (t_8) threads_to_test.push_back(8);
        if (t_16) threads_to_test.push_back(16);
        if (t_32) threads_to_test.push_back(32);
        if (t_64) threads_to_test.push_back(64);

        if (!sizes_to_test.empty() && !threads_to_test.empty())
        {
            run_all_benchmarks(sizes_to_test, threads_to_test);
            chart_size_selection = sizes_to_test.back();
            UpdatePlots(chart_size_selection);
        }
    }
    ImGui::PopStyleColor();
    ImGui::End();

    // --- OKNO 3: ANALIZA (BEZ ZMIAN) ---
    ImGui::SetNextWindowPos(ImVec2(760, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_Always);

    ImGui::Begin("3. Analysis & Results", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    // (Kod okna 3 taki sam jak w poprzedniej wersji)
    if (ALL_RESULTS.empty())
    {
        ImGui::TextDisabled("No benchmark data available.");
    }
    else
    {
        ImGui::Text("Visualize Speedup for Size:"); ImGui::SameLine();
        if (ImGui::RadioButton("S", chart_size_selection == 128)) { chart_size_selection = 128; UpdatePlots(128); } ImGui::SameLine();
        if (ImGui::RadioButton("M", chart_size_selection == 256)) { chart_size_selection = 256; UpdatePlots(256); } ImGui::SameLine();
        if (ImGui::RadioButton("L", chart_size_selection == 512)) { chart_size_selection = 512; UpdatePlots(512); } ImGui::SameLine();
        if (ImGui::RadioButton("XL", chart_size_selection == 1024)) { chart_size_selection = 1024; UpdatePlots(1024); }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
        ImGui::PlotHistogram("##cpp_plot", plot_data_cpp, 70, 0, NULL, 0.0f, FLT_MAX, ImVec2(-1, 80));
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
        ImGui::PlotHistogram("##asm_plot", plot_data_asm, 70, 0, NULL, 0.0f, FLT_MAX, ImVec2(-1, 80));
        ImGui::PopStyleColor();
        ImGui::Separator();

        if (ImGui::BeginTable("ResTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
        {
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 40.0f); ImGui::TableSetupColumn("Impl"); ImGui::TableSetupColumn("Thr", ImGuiTableColumnFlags_WidthFixed, 40.0f); ImGui::TableSetupColumn("Time"); ImGui::TableSetupColumn("Result X[0]..");
            ImGui::TableHeadersRow();
            for (const auto& res : ALL_RESULTS) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%d", res.size);
                ImGui::TableSetColumnIndex(1);
                if (res.implementation.find("ASM") != std::string::npos) ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", res.implementation.c_str());
                else if (res.implementation.find("Standard") != std::string::npos) ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "%s", res.implementation.c_str());
                else ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1), "%s", res.implementation.c_str());
                ImGui::TableSetColumnIndex(2); ImGui::Text("%d", res.threads);
                ImGui::TableSetColumnIndex(3); ImGui::Text("%.4f", res.avg_time_ms);
                ImGui::TableSetColumnIndex(4);
                if (!res.result_sample.empty()) ImGui::Text("{%.2f, %.2f...}", res.result_sample[0], res.result_sample.size() > 1 ? res.result_sample[1] : 0.0);
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}