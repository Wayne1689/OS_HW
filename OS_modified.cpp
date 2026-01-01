#include <filesystem>
#include <iostream>
#include <chrono>
#include <cmath>
#include <vector>
#include <thread>
#include <algorithm>
#include <pthread.h>
#include <cstdio> // for snprintf

#include "BitmapPlusPlus.hpp"

// [優化] 整數邊界檢查
inline uint8_t fastBound(int val) {
    if (val < 0) return 0;
    if (val > 255) return 255;
    return static_cast<uint8_t>(val);
}

// 定義執行緒所需的資料
struct ThreadData {
    int thread_id;
    int total_threads; // 新增：總執行緒數
    int total_files;   // 新增：總檔案數
    const char* src_prefix;
    const char* tar_prefix;
};

// [優化] 濾波函數：指標操作 + 整數運算 + 展開迴圈
void filtingRow(bmp::Bitmap &des, const bmp::Bitmap &src, int row) {
    int width = src.width();
    
    // 預先計算三列的指標 (上、中、下)
    auto row_top = src.cbegin() + (row - 1) * width;
    auto row_mid = src.cbegin() + row * width;
    auto row_bot = src.cbegin() + (row + 1) * width;

    // 目標寫入位置 (從 x=1 開始)
    auto target = des.begin() + row * width + 1;

    // 內層迴圈展開 (Loop Unrolling) - 針對 Sobel X
    for (int x = 1; x < width - 1; ++x) {
        // 使用指標位移，不使用 array indexing
        auto p1 = row_top + (x - 1);
        auto p3 = row_top + (x + 1);
        auto p4 = row_mid + (x - 1);
        auto p6 = row_mid + (x + 1);
        auto p7 = row_bot + (x - 1);
        auto p9 = row_bot + (x + 1);

        // 純整數運算 (Sobel X: 1, 0, -1, 2, 0, -2, 1, 0, -1)
        int r = (p1->r - p3->r) + 2 * (p4->r - p6->r) + (p7->r - p9->r);
        int g = (p1->g - p3->g) + 2 * (p4->g - p6->g) + (p7->g - p9->g);
        int b = (p1->b - p3->b) + 2 * (p4->b - p6->b) + (p7->b - p9->b);

        *target = bmp::Pixel(fastBound(r), fastBound(g), fastBound(b));
        ++target;
    }
}

// [修改] 靜態分配的執行緒函數
void* processImages_Static(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    char infilename[256];
    char outfilename[256];

    // ============================================================
    // 關鍵修改：靜態分配 (Static Partitioning)
    // 每個執行緒自己算好要跑哪幾張圖，完全不用搶鎖
    // 公式確保餘數會被平均分配
    // ============================================================
    int start_index = (data->thread_id * data->total_files) / data->total_threads;
    int end_index   = ((data->thread_id + 1) * data->total_files) / data->total_threads;

    // 該執行緒只負責 [start_index, end_index) 這個範圍
    for (int i = start_index; i < end_index; ++i) {
        
        // 組合檔名
        std::snprintf(infilename, sizeof(infilename), "%s%d.bmp", data->src_prefix, i);
        std::snprintf(outfilename, sizeof(outfilename), "%s%d.bmp", data->tar_prefix, i);

        // 讀檔 -> 處理 -> 存檔 (例外處理包在裡面避免崩潰)
        try {
            bmp::Bitmap srcImage;
            srcImage.load(infilename); // 讀取

            bmp::Bitmap desImage(srcImage.width(), srcImage.height());

            // 逐行處理 (略過上下邊界)
            for (int R = 1; R < srcImage.height() - 1; ++R) {
                filtingRow(desImage, srcImage, R);
            }

            desImage.save(outfilename); // 存檔
        } catch (const std::exception &e) {
            std::cerr << "Thread " << data->thread_id << " failed on file " << i << ": " << e.what() << std::endl;
            // 繼續處理下一張，不中斷
            continue; 
        }
    }
    
    pthread_exit(NULL);
    return NULL;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cout << "Usage: " << argv[0] << " <SOURCE_PREFIX> <TARGET_PREFIX> <AMOUNT>" << std::endl;
        return 0;
    }

    try {
        int amountoffile = std::atoi(argv[3]);
        
        auto beginTime = std::chrono::steady_clock::now();

        // 偵測核心數
        unsigned int num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;
        
        // 如果檔案比核心少，就不用開那麼多執行緒 (例如 2 張圖開 10 個緒沒意義)
        if (amountoffile < (int)num_threads) num_threads = amountoffile;

        std::cout << "System supports " << num_threads << " threads. Processing " << amountoffile << " files." << std::endl;

        std::vector<pthread_t> threads(num_threads);
        std::vector<ThreadData> thread_data(num_threads);

        // 建立執行緒
        for(int i = 0 ; i < num_threads ; i++) {
            thread_data[i].thread_id = i;
            thread_data[i].total_threads = num_threads; // 傳入總數
            thread_data[i].total_files = amountoffile;  // 傳入總數
            thread_data[i].src_prefix = argv[1];
            thread_data[i].tar_prefix = argv[2];

            int rc = pthread_create(&threads[i], NULL, processImages_Static, (void*)&thread_data[i]);
            
            if (rc) {
                std::cerr << "Error: unable to create thread," << rc << std::endl;
                exit(-1);
            }
        }

        // 等待執行緒結束
        for (int i = 0; i < num_threads; i++) {
            void* status;
            pthread_join(threads[i], &status);
        }

        auto endTime = std::chrono::steady_clock::now();
        double totaltime = std::chrono::duration<double>(endTime - beginTime).count();
        
        std::cout << "Takes " << totaltime << " secs" << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}
