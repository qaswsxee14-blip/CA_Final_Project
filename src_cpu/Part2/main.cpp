// // #include <iostream>
// // #include <chrono>

// // // 透過相對路徑引入生成的測資標頭檔
// // #include "../include/dataset.h"

// // int main() {
// //     std::cout << "=== Part 2: RVV Inline Assembly ===" << std::endl;

// //     int num_windows = RX_LEN - PREAMBLE_LEN + 1;
// //     float* correlation_result = new float[num_windows];

// //     const float* rx_pattern_0 = &RX_SIGNALS[0 * RX_LEN];

// //     float max_corr = -1e9;
// //     int detected_idx = -1;

// //     std::cout << "\n開始執行 CPU RVV (Inline Assembly) 向量運算..." << std::endl;

// //     auto st = std::chrono::high_resolution_clock::now();

// //     // ---------------------------------------------------------
// //     // RVV 雙層迴圈 (Inline Assembly 實作)
// //     // ---------------------------------------------------------

// //     for (int i = 0; i < num_windows; ++i) {

// //         int avl = PREAMBLE_LEN;                 // 剩下需要處理的元素數量
// //         const float* ptr_p = PREAMBLE;          // Preamble 讀取指標
// //         const float* ptr_rx = &rx_pattern_0[i]; // Rx 讀取指標

// //         float final_sum = 0.0f;                 // 用來接最後結果的 C++ 變數
// //         float initial_sum = 0.0f;               // 歸約初始值 0.0f

// //         // 宣告一個長度為 1 的陣列，用來把結果從純量暫存器搬到記憶體，再交給 C++
// //         float reduction_result[32] = {0.0f}; // 開大一點，防止 vse32.v 越界寫入

// //         while (avl > 0) {
// //             size_t vl; // 儲存硬體回傳的有效向量長度

// //             // --- 核心 Inline Assembly 區塊 ---
// //             asm volatile(
// //                 // 1. 設定向量長度與型態 (e32 代表 32-bit float, m1 代表使用 1 個暫存器群組)
// //                 "vsetvli %[vl], %[avl], e32, m1, ta, ma \n\t"

// //                 // 2. 向量載入 (Load)
// //                 "vle32.v v8, (%[ptr_p]) \n\t"    // 從 ptr_p 載入 Preamble 到 v8
// //                 "vle32.v v16, (%[ptr_rx]) \n\t"  // 從 ptr_rx 載入 Rx 到 v16

// //                 // 3. 向量乘法 (Multiply)
// //                 "vfmul.vv v24, v8, v16 \n\t"     // v24 = v8 * v16

// //                 // 4. 準備歸約初始值 (將 initial_sum 放進純量浮點暫存器 f0，再搬到 v0 的第 0 個位置)
// //                 "flw f0, (%[init_sum_ptr]) \n\t"
// //                 "vfmv.s.f v0, f0 \n\t"

// //                 // 5. 向量歸約 (Reduction Sum)
// //                 // 將 v24 的所有元素加總，再加上 v0[0]，結果存回 v0[0]
// //                 "vfredusum.vs v0, v24, v0 \n\t"

// //                 // 6. 將結果搬回記憶體 (儲存 v0 的第一個元素到 reduction_result)
// //                 "vse32.v v0, (%[res_ptr]) \n\t"

// //                 // 輸出/輸入對應綁定
// //                 : [vl] "=r" (vl)                                // 輸出 vl 到暫存器
// //                 : [avl] "r" (avl),                              // 輸入 avl
// //                   [ptr_p] "r" (ptr_p),                          // 輸入 Preamble 指標
// //                   [ptr_rx] "r" (ptr_rx),                        // 輸入 Rx 指標
// //                   [init_sum_ptr] "r" (&initial_sum),            // 輸入初始值的記憶體位址
// //                   [res_ptr] "r" (reduction_result)              // 輸入存放結果的記憶體位址
// //                 // 告訴編譯器我們弄髒了哪些暫存器，叫它不要亂用
// //                 : "f0", "memory"
// //             );

// //             // 把剛剛那輪的歸約結果累加到 final_sum 中
// //             final_sum += reduction_result[0];

// //             // 更新指標與剩下的元素數量
// //             ptr_p  += vl;
// //             ptr_rx += vl;
// //             avl    -= vl;
// //         }

// //         correlation_result[i] = final_sum;

// //         if (final_sum > max_corr) {
// //             max_corr = final_sum;
// //             detected_idx = i;
// //         }
// //     }

// //     auto ed = std::chrono::high_resolution_clock::now();
// //     std::chrono::duration<double> elapsed = ed - st;

// //     // ---------------------------------------------------------
// //     // 驗證與結果輸出
// //     // ---------------------------------------------------------
// //     std::cout << "\n[運算結果]" << std::endl;
// //     std::cout << "偵測到的 Preamble 起始位置: " << detected_idx << std::endl;
// //     std::cout << "最大相關性數值 (Peak Value): " << max_corr << std::endl;
// //     std::cout << "CPU 執行時間: " << elapsed.count() * 1000 << " ms\n" << std::endl;

// //     if (detected_idx == GROUND_TRUTH[0]) {
// //         std::cout << "=> 驗證成功！演算法完美找到前導碼位置。" << std::endl;
// //     } else {
// //         std::cout << "=> 驗證失敗！找出的位置與正確解答不符。" << std::endl;
// //     }

// //     delete[] correlation_result;

// //     return 0;
// // }

// ===================================================================================================

// perf最好 但不嚴謹
// asm 1：初始化 v24 = 0
// C++ while loop
//     asm 2：vle32.v + vfmacc.vv，累加到 v24
// asm 3：vfredusum.vs，把 v24 加總

// 比較快
// 指令數比較少
// 寫法比較接近「C++ 控制迴圈 + asm 做核心運算」

// v24 的值跨越多個 asm volatile block。
// GCC 不知道 v24 是 live accumulator。
// 理論上比較不嚴謹。

// simInsts = 1,052,398
// numCycles = 3,757,794
// CPU time = 1.59004 ms

// #include <iostream>
// #include <chrono>

// #include "../include/dataset.h"

// int main()
// {
//     std::cout << "=== Part 2: RVV Vector Reduction Optimized ===" << std::endl;

//     std::cout << "前導碼長度 (PREAMBLE_LEN): " << PREAMBLE_LEN << std::endl;
//     std::cout << "接收訊號長度 (RX_LEN): " << RX_LEN << std::endl;
//     std::cout << "總測試 Pattern 數量: " << NUM_PATTERNS << std::endl;

//     const float *rx_pattern_0 = &RX_SIGNALS[0 * RX_LEN];

//     std::cout << "\n[驗證資料]" << std::endl;
//     std::cout << "Preamble[0] = " << PREAMBLE[0] << std::endl;
//     std::cout << "Rx_Signal_0[0] = " << rx_pattern_0[0] << std::endl;
//     std::cout << "Pattern 0 的正確前導碼起始位置: " << GROUND_TRUTH[0] << std::endl;

//     int num_windows = RX_LEN - PREAMBLE_LEN + 1;
//     float *correlation_result = new float[num_windows];

//     float max_corr = -1e9f;
//     int detected_idx = -1;

//     std::cout << "\n開始執行 RVV Vector Reduction 運算..." << std::endl;

//     auto st = std::chrono::high_resolution_clock::now();

//     // ---------------------------------------------------------
//     // Part 2: RVV Vector Reduction
//     // 每個 output window 使用 vector accumulator
//     // 最後只做一次 vfredusum
//     // ---------------------------------------------------------
//     for (int i = 0; i < num_windows; ++i)
//     {
//         const float *ptr_p = PREAMBLE;
//         const float *ptr_rx = rx_pattern_0 + i;

//         int remain = PREAMBLE_LEN;

//         float zero = 0.0f;
//         float final_sum = 0.0f;

//         size_t vlmax = 0;

//         // 設定最大 vector length，並初始化 vector accumulator v24 = 0
//         asm volatile(
//             "vsetvli %[vl], %[avl], e32, m1, ta, ma \n\t"
//             "flw f0, (%[zero_ptr])                  \n\t"
//             "vfmv.v.f v24, f0                       \n\t"
//             : [vl] "=r"(vlmax)
//             : [avl] "r"(remain),
//               [zero_ptr] "r"(&zero)
//             : "f0", "memory");

//         // 每次處理 vlmax 個 float
//         // v24 是 vector accumulator
//         while (remain >= (int)vlmax)
//         {
//             asm volatile(
//                 "vle32.v v8, (%[ptr_p])      \n\t" // v8  = preamble chunk
//                 "vle32.v v16, (%[ptr_rx])    \n\t" // v16 = rx chunk
//                 "vfmacc.vv v24, v8, v16      \n\t" // v24 += v8 * v16
//                 :
//                 : [ptr_p] "r"(ptr_p),
//                   [ptr_rx] "r"(ptr_rx)
//                 : "memory");

//             ptr_p += vlmax;
//             ptr_rx += vlmax;
//             remain -= (int)vlmax;
//         }

//         // 整個 window 的 vector accumulation 結束後，只做一次 reduction
//         asm volatile(
//             "flw f0, (%[zero_ptr])              \n\t"
//             "vfmv.s.f v0, f0                    \n\t"
//             "vfredusum.vs v0, v24, v0           \n\t"
//             "vfmv.f.s f1, v0                    \n\t"
//             "fsw f1, (%[out_ptr])               \n\t"
//             :
//             : [zero_ptr] "r"(&zero),
//               [out_ptr] "r"(&final_sum)
//             : "f0", "f1", "memory");

//         // 保險處理：如果 PREAMBLE_LEN 不是 vlmax 的整數倍，剩下的用 scalar 補完
//         // 目前 PREAMBLE_LEN = 256，通常不會進來
//         for (int j = 0; j < remain; ++j)
//         {
//             final_sum += ptr_p[j] * ptr_rx[j];
//         }

//         correlation_result[i] = final_sum;

//         if (final_sum > max_corr)
//         {
//             max_corr = final_sum;
//             detected_idx = i;
//         }
//     }

//     auto ed = std::chrono::high_resolution_clock::now();
//     std::chrono::duration<double> elapsed = ed - st;

//     std::cout << "\n[運算結果]" << std::endl;
//     std::cout << "偵測到的 Preamble 起始位置: " << detected_idx << std::endl;
//     std::cout << "最大相關性數值 (Peak Value): " << max_corr << std::endl;
//     std::cout << "CPU 執行時間: " << elapsed.count() * 1000 << " ms\n"
//               << std::endl;

//     if (detected_idx == GROUND_TRUTH[0])
//     {
//         std::cout << "=> 驗證成功！演算法找到正確前導碼位置。" << std::endl;
//     }
//     else
//     {
//         std::cout << "=> 驗證失敗！找出的位置與正確解答不符。" << std::endl;
//     }

//     delete[] correlation_result;

//     return 0;
// }

// ===================================================================================================================

// 比較safe
// 一個大 asm block：
//     初始化 v24 = 0
//     用 assembly branch 做 loop
//     vfmacc.vv 累加
//     最後 vfredusum.vs

// 比較嚴謹
// v24 從初始化、累加、reduction 都在同一個 asm block
// compiler 比較不可能插手破壞 v24

// 比較慢
// assembly loop 內多了 mv、branch、vsetvli、pointer update 等指令
// compiler 比較難幫你最佳化

// simInsts = 1,282,886
// numCycles = 5,691,396
// CPU time = 2.55706 ms

#include <iostream>
#include <chrono>

#include "../include/dataset.h"

int main()
{
    std::cout << "=== Part 2: RVV Vector Reduction Optimized Safe ===" << std::endl;

    std::cout << "前導碼長度 (PREAMBLE_LEN): " << PREAMBLE_LEN << std::endl;
    std::cout << "接收訊號長度 (RX_LEN): " << RX_LEN << std::endl;
    std::cout << "總測試 Pattern 數量: " << NUM_PATTERNS << std::endl;

    const float *rx_pattern_0 = &RX_SIGNALS[0 * RX_LEN];

    std::cout << "\n[驗證資料]" << std::endl;
    std::cout << "Preamble[0] = " << PREAMBLE[0] << std::endl;
    std::cout << "Rx_Signal_0[0] = " << rx_pattern_0[0] << std::endl;
    std::cout << "Pattern 0 的正確前導碼起始位置: "
              << GROUND_TRUTH[0] << std::endl;

    int num_windows = RX_LEN - PREAMBLE_LEN + 1;
    float *correlation_result = new float[num_windows];

    float max_corr = -1e9f;
    int detected_idx = -1;

    std::cout << "\n開始執行 RVV Vector Reduction Optimized Safe 運算..." << std::endl;

    auto st = std::chrono::high_resolution_clock::now();

    // ---------------------------------------------------------
    // Part 2: RVV Vector Reduction
    //
    // 每個 output window：
    // 1. 用 v24 當 vector accumulator
    // 2. 用 vfmacc.vv 做 vector multiply-accumulate
    // 3. 最後只做一次 vfredusum.vs
    //
    // 重點：
    // v24 初始化、累加、reduction 都放在同一個 asm block
    // 避免跨 asm block 保留 vector register 的疑慮
    // ---------------------------------------------------------

    for (int i = 0; i < num_windows; ++i)
    {
        const float *ptr_p = PREAMBLE;
        const float *ptr_rx = rx_pattern_0 + i;

        float zero = 0.0f;
        float final_sum = 0.0f;

        int len = PREAMBLE_LEN;

        asm volatile(
            // t0 = ptr_p
            // t1 = ptr_rx
            // t2 = remain length
            "mv t0, %[ptr_p]                         \n\t"
            "mv t1, %[ptr_rx]                        \n\t"
            "mv t2, %[len]                           \n\t"

            // 先用 len 設定一次 VL，得到 vlmax，存在 t4
            // 並把 vector accumulator v24 初始化成 0
            "vsetvli t4, t2, e32, m1, ta, ma          \n\t"
            "flw f0, (%[zero_ptr])                    \n\t"
            "vfmv.v.f v24, f0                         \n\t"

            // 如果 len = 0，直接跳到 reduction
            "beqz t2, 2f                              \n\t"

            // -----------------------------
            // loop:
            // 每次處理 vl 個 float
            // v24 += preamble * rx
            // -----------------------------
            "1:                                       \n\t"
            "vsetvli t3, t2, e32, m1, ta, ma          \n\t"

            // v8  = PREAMBLE chunk
            // v16 = RX chunk
            "vle32.v v8, (t0)                         \n\t"
            "vle32.v v16, (t1)                        \n\t"

            // v24 = v24 + v8 * v16
            "vfmacc.vv v24, v8, v16                   \n\t"

            // pointer += vl * sizeof(float)
            "slli t5, t3, 2                           \n\t"
            "add t0, t0, t5                           \n\t"
            "add t1, t1, t5                           \n\t"

            // remain -= vl
            "sub t2, t2, t3                           \n\t"
            "bnez t2, 1b                              \n\t"

            // -----------------------------
            // reduction:
            // 把 VL 設回 vlmax，確保 v24 所有累加 lane 都參與 reduction
            // -----------------------------
            "2:                                       \n\t"
            "vsetvli zero, t4, e32, m1, ta, ma        \n\t"
            "flw f0, (%[zero_ptr])                    \n\t"
            "vfmv.s.f v0, f0                          \n\t"
            "vfredusum.vs v0, v24, v0                 \n\t"

            // final_sum = v0[0]
            "vfmv.f.s f1, v0                          \n\t"
            "fsw f1, (%[out_ptr])                     \n\t"

            :
            : [ptr_p] "r"(ptr_p),
              [ptr_rx] "r"(ptr_rx),
              [len] "r"(len),
              [zero_ptr] "r"(&zero),
              [out_ptr] "r"(&final_sum)
            : "t0", "t1", "t2", "t3", "t4", "t5",
              "f0", "f1", "memory");

        correlation_result[i] = final_sum;

        if (final_sum > max_corr)
        {
            max_corr = final_sum;
            detected_idx = i;
        }
    }

    auto ed = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = ed - st;

    std::cout << "\n[運算結果]" << std::endl;
    std::cout << "偵測到的 Preamble 起始位置: " << detected_idx << std::endl;
    std::cout << "最大相關性數值 (Peak Value): " << max_corr << std::endl;
    std::cout << "CPU 執行時間: " << elapsed.count() * 1000 << " ms\n"
              << std::endl;

    if (detected_idx == GROUND_TRUTH[0])
    {
        std::cout << "=> 驗證成功！演算法找到正確前導碼位置。" << std::endl;
    }
    else
    {
        std::cout << "=> 驗證失敗！找出的位置與正確解答不符。" << std::endl;
    }

    delete[] correlation_result;

    return 0;
}
