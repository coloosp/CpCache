#pragma once

#include "CopArcCacheNode.h"
#include <unordered_map>
#include <mutex>

namespace CopCache {
	template <typename Key,typename Value>
	class ArcLruPart
	{
	public:
		//类型别名
		using NodeType = ArcNode<Key, Value>;
		using NodePtr = std::shared_ptr<NodeType>;
		using NodeMap = std::unordered_map<Key, NodePtr>;

		explicit ArcLruPart(size_t capacity, size_t transformThreshold) 
			:capacity_(capacity)
			,ghostCapacity_(capacity)
			,transformThreshold_(transformThreshold)
		{
			initializeLists();
		}

		bool put(Key key, Value value)
		{
			if (capacity_ == 0) return false;

			//线程锁
			std::lock_guard <std::mutex> lock(mutex_);
			auto it = MainCache_.find(key);
			if (it != MainCache_.end()) {
				return updateExistingNode(it->second, value);
			}
			return addNewNode(key, value);
		}

		//传出两个参数 值 和 是否达到转换阈值判断
		bool get(Key key, Value& value, bool& shouldTransform)
		{
			std::lock_guard <std::mutex> lock(mutex_);

			auto it = MainCache_.find(key);
			if (it != MainCache_.end())
			{
				//更新访问频次，并判断是否达到转换阈值
				shouldTransform = updateNodeAccess(it->second);
				value = it->second->getValue();
				return true;
			}
			return false;
		}

		//检查幽灵缓存中是否存在对于节点
		bool checkGhost(Key key)
		{
			auto it = GhostCache_.find(key);
			if (it != GhostCache_.end())
			{
				removeFromGhost(it->second);
				GhostCache_.erase(it);
				return true;
			}
			return false;
		}

		//增加缓存容量
		void increaseCapacity() { ++capacity_; }

		// 减少缓存容量
		bool decreaseCapacity()
		{
			if (capacity_ <= 0) return false;
			//缓存满时需要驱逐一位缓存
			if (MainCache_.size() == capacity_)
			{
				evictLeastRecent();
			}
			--capacity_;
			return true;
		}



	private:
		//初始化
		void initializeLists() {
			//连接主链表
			mainHead_ = std::make_shared<NodeType>();
			mainTail_ = std::make_shared<NodeType>();
			mainHead_->next_ = mainTail_;
			mainTail_->prev_ = mainHead_;

			//连接幽灵链表
			ghostHead_ = std::make_shared<NodeType>();
			ghostTail_ = std::make_shared<NodeType>();
			ghostHead_->next_ = ghostTail_;
			ghostTail_->prev_ = ghostHead_;
		}

		//更新在缓存中的值
		bool updateExistingNode(NodePtr node, const Value& value) 
		{
			node->setValue(value);
			moveToFront(node);
			return true;
		}

		bool addNewNode(const Key& key, const Value& value)
		{
			if (MainCache_.size() >= capacity_)
			{
				//满缓存后需要驱逐最近最少访问节点
				evictLeastRecent();
			}

			//构造新节点并存入缓存中，并将节点插入头部
			NodePtr newNode = std::make_shared<NodeType>(key, value);
			MainCache_[key] = newNode;
			addToFront(newNode);
			return true;

		}

		bool updateNodeAccess(NodePtr node)
		{
			//更新节点的访问次数，并且判断是否符合转换阈值
			moveToFront(node);
			node->incrementAccessCount();
			return node->getAccessCount() >= transformThreshold_;
		}

		//移动节点至头部
		void moveToFront(NodePtr node)
		{
			//从当前位置移除
			node->prev_->next_ = node->next_;
			node->next_->prev_ = node->prev_;

			
			addToFront(node);

		}

		//并添加节点至头部
		void addToFront(NodePtr node)
		{
			node->next_ = mainHead_->next_;
			node->prev_ = mainHead_;
			mainHead_->next_->prev_ = node;
			mainHead_->next_ = node;
		}

		//驱逐最近最少访问节点
		void evictLeastRecent()
		{
			//这里尾部的元素是最近最少访问元素
			NodePtr leastRecent = mainTail_->prev_;
			//缓存为空则跳过
			if (leastRecent == mainHead_)
				return;
			
			//从主链表中移除
			removeFromMain(leastRecent);

			//将移除节点添加到幽灵缓存，如果缓存满则同样的方式清理
			if (GhostCache_.size() >= ghostCapacity_)
			{
				removeOldestGhost();
			}

			addToGhost(leastRecent);

			//根据映射删除对于节点
			MainCache_.erase(leastRecent->getKey());

		}

		void removeFromMain(NodePtr node)
		{
			node->prev_->next_ = node->next_;
			node->next_->prev_ = node->prev_;

		}

		//从幽灵缓存中移除，写两个函数只是为了好区分
		void removeFromGhost(NodePtr node)
		{
			node->prev_->next_ = node->next_;
			node->next_->prev_ = node->prev_;
		}

		// 添加到幽灵缓存中
		void addToGhost(NodePtr node)
		{
			//重置访问次数
			node->accessCount_=1;

			//添加到幽灵缓存头部
			node->next_ = ghostHead_->next_;
			node->prev_ = ghostHead_;
			ghostHead_->next_->prev_ = node;
			ghostHead_->next_ = node;

			//添加到幽灵缓存映射
			GhostCache_[node->getKey()] = node;
		}

		//从幽灵缓存中驱逐最近最少使用节点，和之前的写法一样
		void removeOldestGhost()
		{
			NodePtr oldestGhost = ghostTail_->prev_;
			if (oldestGhost == ghostHead_)
				return;

			removeFromGhost(oldestGhost);
			GhostCache_.erase(oldestGhost->getKey());
		}



	private:

		size_t ghostCapacity_;
		size_t capacity_;
		size_t transformThreshold_;//转换阈值
		std::mutex mutex_;

		//主缓存与幽灵缓存

		NodeMap MainCache_;
		NodeMap GhostCache_;

		//主链表哨兵节点指针
		NodePtr mainHead_;
		NodePtr mainTail_;

		//幽灵链表哨兵节点指针
		NodePtr ghostHead_;
		NodePtr ghostTail_;


	};






} //coloop