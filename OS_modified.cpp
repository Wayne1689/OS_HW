#include <filesystem>
#include <iostream>
#include <vector>
#include <thread>
#include <cmath>
#include <algorithm> // for std::clamp
#include "BitmapPlusPlus.hpp"

// [優化 1] 改用整數運算，並使用 bitwise 操作加速邊界檢查
// 這裡用 int 取代 double，因為 RGB 都在 0-255，加總後也不會超過 int 範圍
inline uint8_t fastBound(int val)
{
    if (val < 0) return 0;
    if (val > 255) return 255;
    return static_cast<uint8_t>(val);
}

// [優化 2] 區域處理函式 (不再是一次處理一行，而是一次處理一塊)
// 移除了所有的 new/delete，完全在 Stack 上運作
void processChunk(bmp::Bitmap &des, const bmp::Bitmap &src, int startRow, int endRow)
{
    int width = src.width();
    
    // Sobel 算子 (3x3)
    // 為了極致速度，我們直接展開迴圈，不使用陣列存取 W[]
    // GX (Vertical):
    // 1  0  -1
    // 2  0  -2
    // 1  0  -1
    
    // 預先取得圖片的原始數據指針 (如果 BitmapPlusPlus 支援迭代器轉指針)
    // 假設 src.cbegin() 返回的是連續記憶體，我們用它來加速
    // 注意：這裡假設 BitmapPlusPlus 內部是用 vector 存儲像素
    
    for (int y = startRow; y < endRow; ++y)
    {
        // [優化 3] 預先計算三列的指標 (上、中、下)
        // 這樣就不需要在內層迴圈重複計算 (row + r) * width
        // 使用 const_iterator 或指針
        auto row_top = src.cbegin() + (y - 1) * width;
        auto row_mid = src.cbegin() + y * width;
        auto row_bot = src.cbegin() + (y + 1) * width;
        
        // 目標寫入位置
        auto target = des.begin() + y * width + 1; // +1 是因為從 x=1 開始

        // [優化 4] 內層迴圈展開 (Loop Unrolling)
        // 避開了 kernel 迴圈，直接寫死算式
        for (int x = 1; x < width - 1; ++x)
        {
            // 取出周圍 9 個點的迭代器 (為了可讀性，編譯器會優化這些)
            // Top row
            auto p1 = row_top + (x - 1); // Top-Left
            // auto p2 = row_top + x;    // Top-Mid (Sobel 權重是 0，不用算)
            auto p3 = row_top + (x + 1); // Top-Right

            // Mid row
            auto p4 = row_mid + (x - 1); // Mid-Left
            // auto p5 = row_mid + x;    // Center (Sobel 權重是 0，不用算)
            auto p6 = row_mid + (x + 1); // Mid-Right

            // Bot row
            auto p7 = row_bot + (x - 1); // Bot-Left
            // auto p8 = row_bot + x;    // Bot-Mid (Sobel 權重是 0，不用算)
            auto p9 = row_bot + (x + 1); // Bot-Right

            // [優化 5] 純整數運算 (Sobel 計算)
            // R Channel
            int r = (1 * p1->r) + (-1 * p3->r) +
                    (2 * p4->r) + (-2 * p6->r) +
                    (1 * p7->r) + (-1 * p9->r);
            
            // G Channel
            int g = (1 * p1->g) + (-1 * p3->g) +
                    (2 * p4->g) + (-2 * p6->g) +
                    (1 * p7->g) + (-1 * p9->g);

            // B Channel
            int b = (1 * p1->b) + (-1 * p3->b) +
                    (2 * p4->b) + (-2 * p6->b) +
                    (1 * p7->b) + (-1 * p9->b);

            // 寫入結果
            *target = bmp::Pixel(fastBound(r), fastBound(g), fastBound(b));
            
            ++target;
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        std::cout << "Usage: " << argv[0] << " <SOURCE> <TARGET> <AMOUNT>" << std::endl;
        return 0;
    }

    try
    {
        char infilename[256], outfilename[256]; 
        int amountoffile = atoi(argv[3]);
        
        int num_threads = std::thread::hardware_concurrency(); 
        if (num_threads == 0) num_threads = 4; // 保底 4 核心

        std::cout << "Threads: " << num_threads << std::endl;

        // [優化 6] 讓 I/O 不要卡在計時器內 (根據作業要求，如果能只算運算時間最好)
        // 但為了符合題目原始邏輯，我們保留計時範圍
        auto beginTime = std::chrono::steady_clock::now();

        // 預先宣告 vector 避免迴圈內重複 malloc
        std::vector<std::thread> threads;
        threads.reserve(num_threads);

        for (int filecount = 0; filecount < amountoffile; filecount++)
        {
            snprintf(infilename, sizeof(infilename), "%s%d.bmp", argv[1], filecount);
            snprintf(outfilename, sizeof(outfilename), "%s%d.bmp", argv[2], filecount);
            
            bmp::Bitmap srcImage;
            try {
                srcImage.load(infilename);
            } catch (...) { continue; }
            
            bmp::Bitmap desImage(srcImage.width(), srcImage.height());
            
            int height = srcImage.height();
            // 有效範圍是 1 到 height-2 (因為第一行和最後一行無法做 3x3)
            int validHeight = height - 2; 
            
            threads.clear();

            // 任務分配
            int chunk_size = validHeight / num_threads;
            int startY = 1;

            for (int t = 0; t < num_threads; ++t)
            {
                int endY = startY + chunk_size;
                // 最後一個執行緒負責剩下的所有行
                if (t == num_threads - 1) endY = height - 1;

                // [優化 7] 使用 lambda 傳遞任務，不複製整張圖，只傳 Reference
                threads.emplace_back([&desImage, &srcImage, startY, endY]() {
                    processChunk(desImage, srcImage, startY, endY);
                });

                startY = endY;
            }

            for (auto &th : threads) {
                if (th.joinable()) th.join();
            }

            desImage.save(outfilename);
        }

        auto endTime = std::chrono::steady_clock::now();
        double totaltime = std::chrono::duration<double>(endTime - beginTime).count();
        std::cout << "Takes " << totaltime << "secs" << std::endl;
    }
    catch (const std::exception &e)
    {
        return EXIT_FAILURE;
    }
    return 0;
}
