#pragma once

//调用上一个文件夹的头文件
#include"../CopCachePolicy.h"
#include"CopArcLruPart.h"
#include"CopArcLfuPart.h"
#include<memory>

namespace CopCache
{
	template <typename Key,typename Value>
	class CopArcCache : public CopCachePolicy <Key, Value>
	{
	public:
		explicit CopArcCache(size_t capacity=10,size_t transformThreshold =2)
		
			:capacity_(capacity)
			, transformThreshold_(transformThreshold)
			,lruPart_(std::make_unique<ArcLruPart<Key,Value>> (capacity,transformThreshold))
			,lfuPart_(std::make_unique<ArcLfuPart<Key,Value>>(capacity,transformThreshold))
		{}

		~CopArcCache() override = default;


		void put(Key key, Value value) override
		{
			bool inGhost = checkGhostCaches(key);

			//这里表示缓存命中，不要被代码的表象迷惑，这里有两层含义，第一，命中缓存，且lfu中不存在，则lru放入，lfu放入。
			//第二，命中缓存，lfu中也存在该数据，则更新该数据，lru依旧放入(别忘了put函数有更新和放入两个功能)
			if (!inGhost)
			{
				if (lruPart_->put(key, value))
				{
					lfuPart_->put(key, value);
				}
			}

			//如果幽灵命中，两种情况都应该放入lru中，这是因为幽灵命中的数据访问次数只会有1，lfu需要一定访问次数才能进入
			//不过在我们checkGhostCaches函数中已经判断怎么去增减缓存，所以不用担心
			else
			{
				lruPart_->put(key, value);
			}
		}

		bool get(Key key, Value& value) override
		{
			checkGhostCaches(key);

			bool shouldTransform = false;

			if (lruPart_->get(key, value, shouldTransform))
			{
				//如果在lru里面的该节点在访问时超过转换阈值，此时需要在lfu缓存中添加
				if (shouldTransform)
				{
					lfuPart_->put(key, value);
				}
				return true;
			}
			return lfuPart_->get(key, value);
		}

		Value get(Key key) override
		{
			Value value{};
			get(key, value);
			return value;
		}


	private:
		//检查幽灵缓存，观察在哪里的幽灵缓存命中了
		bool checkGhostCaches(Key key)
		{
			//标记是否节点命中幽灵缓存
			bool inGhost = false;
			//如果在lru命中，则提高lru的缓存容量并降低lfu的容量,注意这里需要lfu正确降低容量，才能让lru提高，因为容量有限
			if (lruPart_->checkGhost(key))
			{
				if (lfuPart_->decreaseCapacity())
				{
					lruPart_->increaseCapacity();
				}
				inGhost = true;
			}
			//反之一样
			else if (lfuPart_->checkGhost(key))
			{
				if (lruPart_->decreaseCapacity())
				{
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




}// coloop