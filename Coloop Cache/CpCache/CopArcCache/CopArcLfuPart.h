#pragma once
# include "CopArcCacheNode.h"
# include <unordered_map>
#include <map>
#include <mutex>
#include <list>

namespace CopCache
{
	template <typename Key,typename Value>
	class ArcLfuPart
	{
	public:
		//类型别名
		using NodeType = ArcNode<Key, Value>;
		using NodePtr = std::shared_ptr<NodeType>;
		using NodeMap = std::unordered_map <Key, NodePtr>;//节点哈希表
		using FreqMap = std::map<size_t, std::list<NodePtr>>;//频数链表哈希表

		explicit ArcLfuPart(size_t capacity, size_t transformThreshold)
			:capacity_(capacity)
			, ghostCpacity_(capacity)
			,transformThreshold_(transformThreshold)
			,minFreq_(0)
		{
			intializeLists();
		}
		
		bool put(Key key, Value value)
		{
			if (capacity_ == 0)
				return false;

			std::lock_guard<std::mutex> lock(mutex_);
			auto it = mainCache_.find(key);
			if (it != mainCache_.end())
			{
				return updateExistingNode(it->second, value);

			}
			return addNewNode(key, value);
		}

		bool get(Key key, Value& value)
		{
			std::lock_guard<std::mutex> lock(mutex_);
			auto it = mainCache_.find(key);
			if (it != mainCache_.end()){
				//访问后需要更新频次
				updateNodeFrequency(it->second);
				value = it->second->getValue();
				return true;

			}
			return false;
		}

		bool checkGhost(Key key)
		{
			auto it = ghostCache_.find(key);
			if (it != ghostCache_.end())
			{
				removeFromGhost(it->second);
				ghostCache_.erase(it);
				return true;
			}
			return false;
		}


		void increaseCapacity() {
			++capacity_;
		}

		bool decreaseCapacity()
		{
			if (capacity_ <= 0)
				return false;
			if (mainCache_.size() == capacity_)
			{
				evictLeastFrequent();
			}
			--capacity_;
			return true;
		}


	private:
		void intializeLists() 
		{
			ghostHead_ = std::make_shared <NodeType>();
			ghostTail_ = std::make_shared <NodeType>();

			ghostHead_->next_ = ghostTail_;
			ghostTail_->prev_ = ghostHead_;
		}

		bool updateExistingNode(NodePtr node, const Value& value)
		{
			node->setValue(value);
			updateNodeFrequency(node);
			return true;
		}

		bool addNewNode(const Key& key, const Value& value)
		{
			if (mainCache_.size() >= capacity_) 
			{
				evictLeastFrequent();
			}
			NodePtr newNode = std::make_shared <NodeType>(key, value);
			mainCache_[key] = newNode;

			//将新节点添加到频率为1的链表中
			//若不存在，还需要构造一个频率1的链表
			if (freqMap_.find(key) == freqMap_.end())
			{
				freqMap_[1] = std::list<NodePtr>();
			}
			freqMap_[1].push_back(newNode);
			//因为有新节点添加，最小频数修改为1
			minFreq_ = 1;

			return true;
		}

		//更改节点频次
		void updateNodeFrequency(NodePtr node)
		{
			//提前记录节点的新旧频次
			size_t oldFreq = node->getAccessCount();
			node->incrementAccessCount();
			size_t newFreq = node->getAccessCount();

			//将节点从旧频次的链表中移除
			auto& oldList = freqMap_[oldFreq];
			oldList.remove(node);
			//如果移除节点后，该链表为空,需要回收该链表
			if (oldList.empty())
			{
				freqMap_.erase(oldFreq);
				//并且，如果该链表是最低频次链表，删除后还需要更新最低频次
				if (oldFreq == minFreq_)
				{
					minFreq_ = newFreq;
				}
			}

			//将节点添加到新频次链表
			if (freqMap_.find(newFreq) == freqMap_.end())
			{
				//如果原来不存在对应频次的链表，还需要构造一个
				freqMap_[newFreq] = std::list <NodePtr>();
			}
			freqMap_[newFreq].push_back(node);
		}

		void evictLeastFrequent()
		{
			if (freqMap_.empty())
				return;

			//首先获取最低频次链表
			auto& minFreqList = freqMap_[minFreq_];
			if (minFreqList.empty())
				return;

			//移除最低频次最少使用的节点,并且此时需要保留要被删除的节点，后续需要放入幽灵缓存
			NodePtr deleteNode = minFreqList.front();
			minFreqList.pop_front();//弹出最前端节点

			//如果删除节点后对应链表为空，还需要删除该链表
			if (minFreqList.empty())
			{
				freqMap_.erase(minFreq_);
				//因为最低频次链表被删除，还需要更新最低频次
				if (!freqMap_.empty())
				{
					minFreq_ = freqMap_.begin()->first;
				}
			}

			//将节点放入幽灵缓存
			if (ghostCache_.size() >= ghostCpacity_)
			{
				removeOldestGhost();

			}
			addToGhost(deleteNode);
			
			//最后将该节点从主缓存移除
			mainCache_.erase(deleteNode->getKey());

		}

		void removeFromGhost(NodePtr node)
		{
			node->prev_->next_ = node->next_;
			node->next_->prev_ = node->prev_;

		}

		void addToGhost(NodePtr node)
		{
			node->next_ = ghostTail_;
			node->prev_ = ghostTail_->prev_;
			ghostTail_->prev_->next_ =node;
			ghostTail_->prev_ =node;

			ghostCache_[node->getKey()] =node;
		}

		void removeOldestGhost()
		{
			NodePtr oldestGhost = ghostHead_->next_;
			if (oldestGhost != ghostTail_)
			{
				removeFromGhost(oldestGhost);
				ghostCache_.erase(oldestGhost->getKey());

			}
		}
		


	private:

		size_t capacity_;
		size_t ghostCpacity_;
		size_t transformThreshold_;
		size_t minFreq_;
		std::mutex mutex_;

		NodeMap mainCache_;//主缓存，也是键值节点哈希表
		NodeMap ghostCache_;//幽灵缓存
		FreqMap freqMap_;//频次链表哈希表，存储频次与对应频次链表的映射关系

		//这里不定义频次缓存的哨兵节点的原因是，频次链表哈希表可以根据频次调度到对应链表，对应链表自带有front

		NodePtr ghostHead_;
		NodePtr ghostTail_;
	};

} // coloop