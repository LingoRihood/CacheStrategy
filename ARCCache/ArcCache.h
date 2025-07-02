#pragma once

#include "../CachePolicy.h"
#include "ArcLruPart.h"
#include "ArcLfuPart.h"
#include <memory>

namespace CacheStrategy {
template<typename Key, typename Value>
class ArcCache: public CachePolicy<Key, Value> {
public:
    explicit ArcCache(size_t capacity = 10, size_t transformThreshold = 2)
        : capacity_(capacity)
        , transformThreshold_(transformThreshold)
        , lruPart_(std::make_unique<ArcLruPart<Key, Value>>(capacity, transformThreshold))
        , lfuPart_(std::make_unique<ArcLfuPart<Key, Value>>(capacity, transformThreshold)) {

        }

    ~ArcCache() override = default;

    void put(Key key, Value value) override {
        // 幽灵缓存中记录已被淘汰的数据，用于决定LRU与LFU容量的动态调整
        checkGhostCaches(key);

        // 检查 LFU 部分是否存在该键
        bool inLfu = lfuPart_->contain(key);
        // 更新 LRU 部分缓存
        lruPart_->put(key, value);
        // 如果 LFU 部分存在该键，则更新 LFU 部分
        if(inLfu) {
            lfuPart_->put(key, value);
        }
    }

    bool get(Key key, Value& value) override {
        // 先检查幽灵缓存，调整缓存容量。
        checkGhostCaches(key);

        // shouldTransform含义：
        // 数据频繁被访问，标记为热数据，将从LRU迁移到LFU管理。
        bool shouldTransform = false;
        // 从 LRU 部分尝试获取数据：
        if(lruPart_->get(key, value, shouldTransform)) {
            if(shouldTransform) {
             // 若存在，shouldTransform 表示访问次数超过阈值，将数据迁移到 LFU。
                lfuPart_->put(key, value);
            }
            return true;
        }
        // 若 LRU 部分未命中，则从 LFU 部分尝试获取数据。
        return lfuPart_->get(key, value);
    }

    Value get(Key key) override {
        Value value{};
        get(key, value);
        return value;
    }

private:
    bool checkGhostCaches(Key key) {
        bool inGhost = false;
        if(lruPart_->checkGhost(key)) {
            if(lfuPart_->decreaseCapacity()) {
                lruPart_->increaseCapacity();
            }
            inGhost = true;
        } else if(lfuPart_->checkGhost(key)) {
            if(lruPart_->decreaseCapacity()) {
                lfuPart_->increaseCapacity();
            }
            inGhost = true;
        }
        return inGhost;
    }

private:
    size_t capacity_;
    size_t transformThreshold_;
    std::unique_ptr<ArcLruPart<Key, Value>> lruPart_;
    std::unique_ptr<ArcLfuPart<Key, Value>> lfuPart_;
};
}