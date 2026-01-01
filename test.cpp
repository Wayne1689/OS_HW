#include <filesystem>
#include <iostream>
#include <chrono>      // 修正：原版漏掉計時用的標頭檔
#include <cmath>
#include <vector>
#include <thread> 
#include <algorithm>
#include <pthread.h>

#include "BitmapPlusPlus.hpp" // 確保檔名與你的 hpp 一致




// 邊界檢查：將計算結果限制在 0-255 之間
inline uint8_t boundPixel(double val) {
    if (val < 0) return 0;
    if (val > 255) return 255;
    return static_cast<uint8_t>(round(val));
}


const double W[] = {
    1, 0, -1,
    2, 0, -2,
    1, 0, -1
};

struct ThreadData {
    int thread_id;
    int start_index;     
    int end_index; 
    const char* src_prefix; 
    const char* tar_prefix; 
};





int g_current_file_index = 0;
int g_total_files = 0;
pthread_mutex_t g_task_mutex = PTHREAD_MUTEX_INITIALIZER;


// 濾波函數：優化後的版本，移除動態記憶體配置並使用指標操作
void filtingRow(bmp::Bitmap &des, const bmp::Bitmap &src, const int row, const double W[], const int kSize) {
    const int offset = kSize >> 1;
    const int width = src.width();
    
    // 預先計算每一列的起始指標，避免在迴圈中重複計算
    // 使用 vector 儲存指標，避免 new/delete
    std::vector<const bmp::Pixel*> src_rows(kSize);
    
    for (int r = -offset; r <= offset; ++r) {
        // 取得來源圖該列的起始位置 (第 0 行)
        // src.cbegin() 回傳的是 iterator，取值後取位址得到 raw pointer
        src_rows[r + offset] = &(*(src.cbegin() + (row + r) * width));
    }

    // 設定目標圖片的寫入位置 (從 offset 開始)
    bmp::Pixel* tar = &(*(des.begin() + row * width + offset));

    // 針對該列的每個像素進行卷積 (避開邊界)
    // col 代表當前處理的中心點位置
    for (int col = offset; col < width - offset; ++col) {
        double r_sum = 0, g_sum = 0, b_sum = 0;
        int w_idx = 0;

        // 卷積運算
        for (int i = 0; i < kSize; ++i) {
            // 取得對應列的指標，並直接定位到當前視窗的起始位置
            // 視窗範圍: [col - offset, col + offset]
            // src_rows[i] 指向該列第 0 個像素
            const bmp::Pixel* p = src_rows[i] + (col - offset);
            
            for (int j = 0; j < kSize; ++j) {
                double weight = W[w_idx++];
                // 簡單優化：權重為 0 則不計算
                if (weight != 0) {
                    r_sum += weight * p[j].r;
                    g_sum += weight * p[j].g;
                    b_sum += weight * p[j].b;
                }
            }
        }

        // 寫入結果
        tar->r = boundPixel(r_sum);
        tar->g = boundPixel(g_sum);
        tar->b = boundPixel(b_sum);
        ++tar;
    }
}


void* processImages(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    char infilename[256], outfilename[256];

    while (true) {
        int filecount = -1;

        // Critical section: get a task
        pthread_mutex_lock(&g_task_mutex);
        if (g_current_file_index < g_total_files) {
            filecount = g_current_file_index;
            g_current_file_index++;
        }
        pthread_mutex_unlock(&g_task_mutex);

        // If no more tasks, break loop
        if (filecount == -1) {
            break;
        }

        sprintf(infilename, "%s%d.bmp", data->src_prefix, filecount);
        sprintf(outfilename, "%s%d.bmp", data->tar_prefix, filecount);

        std::cout << "Thread " << data->thread_id << " trying: [" << infilename << "]" << std::endl;

        try {
            bmp::Bitmap srcImage;
            srcImage.load(infilename); // 讀取

            bmp::Bitmap desImage(srcImage.width(), srcImage.height());

            // 逐行處理
            for (int R = 1; R < srcImage.height() - 1; ++R) {
                filtingRow(desImage, srcImage, R, W, 3);
            }

            desImage.save(outfilename); // 存檔
        } catch (const bmp::Exception &e) {
            std::cerr << "Thread " << data->thread_id << " Error: " << e.what() << std::endl;
        }
    }
    
    pthread_exit(NULL); // 結束執行緒
    return NULL;
}



int main(int argc, char** argv) {





    // 檢查參數
    if (argc < 4) {
        std::cout << "Usage: " << argv[0] << " <SOURCE_BMP_PREFIX> <TARGET_BMP_PREFIX> <AMOUNT_OF_FILE>" << std::endl;
        return 0;
    }

    try {
        // 修正：加大陣列長度防止路徑太長崩潰
        char infilename[256], outfilename[256];
        int amountoffile = atoi(argv[3]);
        

        auto beginTime = std::chrono::steady_clock::now();

        unsigned int num_threads = std::thread::hardware_concurrency();
        std::cout << "System supports " << num_threads << " threads." << std::endl;
        if (num_threads == 0) num_threads = std::min(4,amountoffile);


        std::vector<pthread_t> threads(num_threads);
        std::vector<ThreadData> thread_data(num_threads);

        // Initialize global task queue
        g_total_files = amountoffile;
        g_current_file_index = 0;

        for(int i = 0 ; i < num_threads ; i ++)
        {
            thread_data[i].thread_id = i;
            thread_data[i].src_prefix = argv[1];
            thread_data[i].tar_prefix = argv[2];

            // start_index and end_index are no longer used with dynamic task assignment
            thread_data[i].start_index = 0;
            thread_data[i].end_index = 0;

            int rc = pthread_create(&threads[i], NULL, processImages, (void*)&thread_data[i]);
            
            if (rc) {
                std::cerr << "Error:unable to create thread," << rc << std::endl;
                exit(-1);
            }

        }


        for (int i = 0; i < num_threads; i++) {
            void* status;
            int rc = pthread_join(threads[i], &status);
            if (rc) {
                std::cerr << "Error:unable to join thread," << rc << std::endl;
                exit(-1);
            }
        }



        // 單線程：一張一張檔案依序處理
        // for (int filecount = 0; filecount < amountoffile; filecount++) {
        //     // 格式化檔名，例如: input0.bmp, input1.bmp...
        //     sprintf(infilename, "%s%d.bmp", argv[1], filecount);
        //     sprintf(outfilename, "%s%d.bmp", argv[2], filecount);

        //     bmp::Bitmap srcImage;
        //     srcImage.load(infilename); // 讀取圖檔

        //     // 建立一張同樣大小的結果圖
        //     bmp::Bitmap desImage(srcImage.width(), srcImage.height());

        //     // 逐行處理 (略過最上與最下行)
        //     for (int R = 1; R < srcImage.height() - 1; ++R) {
        //         filtingRow(desImage, srcImage, R, W, 3);
        //     }

        //     desImage.save(outfilename); // 存檔
        // }

        auto endTime = std::chrono::steady_clock::now();
        double totaltime = std::chrono::duration<double>(endTime - beginTime).count();
        
        std::cout << "Takes " << totaltime << " secs" << std::endl;

    } catch (const bmp::Exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    pthread_mutex_destroy(&g_task_mutex);
    return 0;
}
