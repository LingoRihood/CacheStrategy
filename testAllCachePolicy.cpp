#include <iostream>
#include <string>
// 这一行代码包含了 chrono 头文件，它提供了计时功能，包括高精度的时钟和时间单位。这是 C++11 中新增的功能，用来处理时间和日期。
#include <chrono>
#include <vector>
#include <iomanip>
#include <random>
#include <algorithm>

#include "CachePolicy.h"
#include "kLfuCache.h"
#include "kLruCache.h"
#include "/home/ubuntu/proj/ARCCache/ArcCache.h"

class Timer {
public:
    // high_resolution_clock 是 C++ 标准库中最精确的时钟，它通常基于系统的高精度计时器，适用于要求高精度计时的场合。
    // std::chrono::high_resolution_clock::now() 返回一个当前时刻的时间点，start_ 用来存储这个时间点。
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}

    double elapsed() {
        auto now = std::chrono::high_resolution_clock::now();

        // std::chrono::duration_cast<std::chrono::milliseconds>(...) 将时间差转换为毫秒数。duration_cast 是一个模板函数，用于将时间间隔从一种单位转换为另一种单位。在这里，将时间间隔从原始单位转换为毫秒（milliseconds）
        // .count() 返回毫秒数的整数值，表示经过的时间（单位：毫秒）。
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
    }
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

// 辅助函数：打印结果
void printResults(const std::string& testName, int capacity, const std::vector<int>& get_operations, const std::vector<int>& hits) {
    std::cout << "=== " << testName << " 结果汇总 ===" << std::endl;
    std::cout << "缓存大小: " << capacity << std::endl;

    // 假设对应的算法名称已在测试函数中定义
    std::vector<std::string> names;
    if(hits.size() == 3) {
        names = {"LRU", "LFU", "ARC"};
    } else if(hits.size() == 4) {
        names = {"LRU", "LFU", "ARC", "LRU-K"};
    } else if(hits.size() == 5) {
        names = {"LRU", "LFU", "ARC", "LRU-K", "LFU-Aging"};
    }

    for(size_t i = 0; i < hits.size(); ++i) {
        double hitRate = 100.0 * hits[i] / get_operations[i];

        // std::fixed 设置输出为固定小数格式。
        // std::setprecision(2) 设置输出的小数点后保留两位。
        std::cout << (i < names.size() ? names[i]: "Algorithm " + std::to_string(i + 1)) << " - 命中率："<< std::fixed<< std::setprecision(2)<< hitRate<< "% ";

        // 添加具体命中次数和总操作次数
        std::cout << "(" << hits[i] << "/" << get_operations[i] << ")" << std::endl;
    }

    std::cout << std::endl;  // 添加空行，使输出更清晰
}

void testHotDataAccess() {
    std::cout << "\n=== 测试场景1：热点数据访问测试 ===" << std::endl;

    const int CAPACITY = 20;         // 缓存容量
    const int OPERATIONS = 500000;   // 总操作次数
    const int HOT_KEYS = 20;         // 热点数据数量
    const int COLD_KEYS = 5000;      // 冷数据数量

    CacheStrategy::KLruCache<int, std::string> lru(CAPACITY);
    CacheStrategy::KLfuCache<int, std::string> lfu(CAPACITY);
    CacheStrategy::ArcCache<int, std::string> arc(CAPACITY);
    // 为LRU-K设置合适的参数：
    // - 主缓存容量与其他算法相同
    // - 历史记录容量设为可能访问的所有键数量
    // - k=2表示数据被访问2次后才会进入缓存，适合区分热点和冷数据
    CacheStrategy::KLruKCache<int, std::string> lruk(CAPACITY, HOT_KEYS + COLD_KEYS, 2);
    CacheStrategy::KLfuCache<int, std::string> lfuAging(CAPACITY, 20000);

    // std::random_device 产生的随机数不受程序的初始化和状态影响，因此它是“非确定性”的
    std::random_device rd;
    // std::mt19937 是 C++ 标准库中的一个伪随机数生成器，它是基于 梅森旋转算法 (Mersenne Twister) 的。这个算法能够生成高质量的伪随机数，它的周期非常长(大约是2^19937 - 1), 适合用于需要大量随机数的应用。

    // rd() 调用会生成一个整数，作为伪随机数生成器的种子。
    // 这个种子值会用来初始化 std::mt19937 生成器，使得每次运行程序时，如果 rd() 生成的种子不同，gen 生成的随机数序列也会不同。
    std::mt19937 gen(rd());

    // 基类指针指向派生类对象，添加LFU-Aging
    // std::array 是一个静态大小的数组，大小在编译时必须已知，并且在运行时不能更改。
    std::vector<CacheStrategy::CachePolicy<int, std::string>*> caches = {&lru, &lfu, &arc, &lruk, &lfuAging};

    // 始大小为5，并且每个元素都被初始化为0
    std::vector<int> hits(5, 0);
    std::vector<int> get_operations(5, 0);
    std::vector<std::string> names = {"LRU", "LFU", "ARC", "LRU-K", "LFU-Aging"};

    // 为所有的缓存对象进行相同的操作序列测试
    for(int i = 0; i < caches.size(); ++i) {
        // 先预热缓存，插入一些数据
        for(int key = 0; key < HOT_KEYS; ++key) {
            std::string value = "value" + std::to_string(key);
            caches[i]->put(key, value);
        }

        // 交替进行put和get操作，模拟真实场景
        for(int op = 0; op < OPERATIONS; ++op) {
            // 大多数缓存系统中读操作比写操作频繁
            // 所以设置30%概率进行写操作
            bool isPut = (gen() % 100 < 30);
            int key;

            // 70%概率访问热点数据，30%概率访问冷数据
            if(gen() % 100 < 70) {
                // 热点数据
                // 生成热点数据的索引，取值范围是 [0, HOT_KEYS - 1]。
                // 假如 HOT_KEYS = 20，则热点数据索引范围为 0～19。
                key = gen() % HOT_KEYS;
            } else {
                // 冷数据
                // 冷数据的索引范围从 HOT_KEYS 开始，到 HOT_KEYS + COLD_KEYS - 1 为止。
                // 如果 HOT_KEYS = 20，COLD_KEYS = 5000，那么冷数据的索引范围就是：
                // 20 ~ 5019。
                // 这意味着冷数据的范围较大，但访问概率较低。
                key = HOT_KEYS + (gen() % COLD_KEYS);
            }

            if(isPut) {
                // 执行put操作
                std::string value = "value" + std::to_string(key) + "_v" + std::to_string(op % 100);
                caches[i]->put(key, value);
            } else {
                // 执行get操作并记录命中情况
                std::string result;
                get_operations[i]++;
                if(caches[i]->get(key, result)) {
                    hits[i]++;
                }
            }
        }
    }
    // 打印测试结果
    printResults("热点数据访问测试", CAPACITY, get_operations, hits);
}

void testLoopPattern() {
    std::cout << "\n=== 测试场景2：循环扫描测试 ===" << std::endl;
    
    const int CAPACITY = 50;          // 缓存容量
    const int LOOP_SIZE = 500;        // 循环范围大小
    const int OPERATIONS = 200000;    // 总操作次数

    CacheStrategy::KLruCache<int, std::string> lru(CAPACITY);
    CacheStrategy::KLfuCache<int, std::string> lfu(CAPACITY);
    CacheStrategy::ArcCache<int, std::string> arc(CAPACITY);

    // 为LRU-K设置合适的参数：
    // - 历史记录容量设为总循环大小的两倍，覆盖范围内和范围外的数据
    // - k=2，对于循环访问，这是一个合理的阈值
    CacheStrategy::KLruKCache<int, std::string> lruk(CAPACITY, LOOP_SIZE * 2, 2);
    CacheStrategy::KLfuCache<int, std::string> lfuAging(CAPACITY, 3000);

    std::vector<CacheStrategy::CachePolicy<int, std::string>*> caches = {&lru, &lfu, &arc, &lruk, &lfuAging};
    std::vector<int> hits(5, 0);
    std::vector<int> get_operations(5, 0);
    std::vector<std::string> names = {"LRU", "LFU", "ARC", "LRU-K", "LFU-Aging"};
    
    std::random_device rd;
    std::mt19937 gen(rd());

    // 为每种缓存算法运行相同的测试
    for(int i = 0; i < caches.size(); ++i) {
        // 先预热一部分数据（只加载20%的数据）
        for(int key = 0; key < LOOP_SIZE / 5; ++key) {
            std::string value = "loop" + std::to_string(key);
            caches[i]->put(key, value);
        }

        // 设置循环扫描的当前位置
        int current_pos = 0;

        // 交替进行读写操作，模拟真实场景
        for(int op = 0; op < OPERATIONS; ++op) {
            // 20%概率是写操作，80%概率是读操作
            bool isPut = (gen() % 100 < 20);
            int key;

            // 按照不同模式选择键
            // 60%顺序扫描
            if(op % 100 < 60) {
                key = current_pos;
                current_pos = (current_pos + 1) % LOOP_SIZE;
            } else if(op % 100 < 90) {
                // 30%随机跳跃
                key = gen() % LOOP_SIZE;
            } else {
                // 10%访问范围外数据
                key = LOOP_SIZE + (gen() % LOOP_SIZE);
            }

            if(isPut) {
                // 执行put操作，更新数据
                std::string value = "loop" + std::to_string(key) + "_v" + std::to_string(op % 100);
                caches[i]->put(key, value);
            } else {
                // 执行get操作并记录命中情况
                std::string result;
                get_operations[i]++;
                if(caches[i]->get(key, result)) {
                    hits[i]++;
                }
            }
        }
    }
    printResults("循环扫描测试", CAPACITY, get_operations, hits);
}

void testWorkloadShift() {
    std::cout << "\n=== 测试场景3：工作负载剧烈变化测试 ===" << std::endl;
    
    const int CAPACITY = 30;            // 缓存容量
    const int OPERATIONS = 80000;       // 总操作次数
    // 总操作数：80000，共5个阶段，每阶段16000次操作
    const int PHASE_LENGTH = OPERATIONS / 5;  // 每个阶段的长度
    
    CacheStrategy::KLruCache<int, std::string> lru(CAPACITY);
    CacheStrategy::KLfuCache<int, std::string> lfu(CAPACITY);
    CacheStrategy::ArcCache<int, std::string> arc(CAPACITY);
    CacheStrategy::KLruKCache<int, std::string> lruk(CAPACITY, 500, 2);
    CacheStrategy::KLfuCache<int, std::string> lfuAging(CAPACITY, 10000);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::vector<CacheStrategy::CachePolicy<int, std::string>*> caches = {&lru, &lfu, &arc, &lruk, &lfuAging};
    std::vector<int> hits(5, 0);
    std::vector<int> get_operations(5, 0);
    std::vector<std::string> names = {"LRU", "LFU", "ARC", "LRU-K", "LFU-Aging"};

    // 为每种缓存算法运行相同的测试
    for(int i = 0; i < caches.size(); ++i) {
        // 先预热缓存，只插入少量初始数据
        for(int key = 0; key < 30; ++key) {
            std::string value = "init" + std::to_string(key);
            caches[i]->put(key, value);
        }

        // 进行多阶段测试，每个阶段有不同的访问模式
        for(int op = 0; op < OPERATIONS; ++op) {
            // 确定当前阶段
            int phase = op / PHASE_LENGTH;

            // 每个阶段的读写比例不同 
            int putProbability;
            switch(phase) {
                case 0: putProbability = 15; break;  // 阶段1: 热点访问，15%写入更合理
                case 1: putProbability = 30; break;  // 阶段2: 大范围随机，写比例为30%
                case 2: putProbability = 10; break;  // 阶段3: 顺序扫描，10%写入保持不变
                case 3: putProbability = 25; break;  // 阶段4: 局部性随机，微调为25%
                case 4: putProbability = 20; break;  // 阶段5: 混合访问，调整为20%
                default: putProbability = 20;
            }

            // 确定是写还是读操作
            bool isPut = (gen() % 100 < putProbability);

            // 根据不同阶段选择不同的访问模式生成key - 优化后的访问范围
            int key;
            // 阶段1: 热点访问 - 热点数量5，使热点更集中
            // 0 ~ 15999
            if(op < PHASE_LENGTH) {
                // 阶段1（热点访问）：5个热点键频繁访问；
                // 键范围：[0-4]
                key = gen() % 5;
            } else if(op < PHASE_LENGTH * 2) {
                // 16000 ~ 31999
                // 阶段2: 大范围随机 - 范围400，更适合30大小的缓存
                // 阶段2（大范围随机）：随机访问400个不同键；
                // 键范围：[0-399]
                key = gen() % 400;
            } else if(op < PHASE_LENGTH * 3) {
                // 32000 ~ 47999
                // 阶段3: 顺序扫描 - 保持100个键
                // 阶段3（顺序扫描）：顺序扫描100个键；
                // 键范围：[0-99]
                key = (op - PHASE_LENGTH * 2) % 100;
            } else if(op < PHASE_LENGTH * 4) {
                // 48000 ~ 63999
                // 阶段4: 局部性随机 - 优化局部性区域大小
                // 产生5个局部区域，每个区域大小为15个键，与缓存大小20接近但略小
                // 调整为5个局部区域
                // 阶段4（局部随机）：访问5个区域，每区域15个键；
                // 5个区域，每个区域15个键
                int locality = (op / 800) % 5;

                // 每区域15个键
                key = locality * 15 + (gen() % 15);
                // 区域一：0~14
                // 区域二：15~29
                // 区域三：30~44
                // 区域四：45~59
                // 区域五：60~74
            } else {
                // 64000 ~ 79999
                // 阶段5: 混合访问 - 增加热点访问比例
                // 阶段5（混合访问）：热点、中等范围和大范围访问的混合模式；
                // 热点[0-4]，中等[5-49]，大范围[50-399]
                int r = gen() % 100;
                if(r < 40) {
                    // 40%概率访问热点（从30%增加）
                    // 5个热点键
                    // 40%的概率访问热点数据（0~4） 
                    key = gen() % 5;
                } else if(r < 70) {
                    // 30%的概率访问中等范围数据（5~49）
                    // 30%概率访问中等范围
                    // 缩小中等范围为50个键
                    key = 5 + (gen() % 45);
                } else {
                    // 30%的概率访问大范围数据（50~399）
                    // 30%概率访问大范围（从40%减少）
                    // 大范围也相应缩小
                    key = 50 + (gen() % 350);
                }
            }

            if(isPut) {
                // 执行写操作
                std::string value = "value" + std::to_string(key) + "_p" + std::to_string(phase);
                caches[i]->put(key, value);
            } else {
                // 执行读操作并记录命中情况
                std::string result;
                get_operations[i]++;
                if(caches[i]->get(key, result)) {
                    hits[i]++;
                }
            }
        }
    }
    printResults("工作负载剧烈变化测试", CAPACITY, get_operations, hits);
}

int main() {
    // === 测试场景1：热点数据访问测试 ===
    testHotDataAccess();
    // === 测试场景2：循环扫描测试 ===
    testLoopPattern();
    // === 测试场景3：工作负载剧烈变化测试 ===
    testWorkloadShift();
    return 0;
}