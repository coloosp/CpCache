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
		//���ͱ���
		using NodeType = ArcNode<Key, Value>;
		using NodePtr = std::shared_ptr<NodeType>;
		using NodeMap = std::unordered_map <Key, NodePtr>;//�ڵ��ϣ��
		using FreqMap = std::map<size_t, std::list<NodePtr>>;//Ƶ�������ϣ��

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
				//���ʺ���Ҫ����Ƶ��
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

			//���½ڵ���ӵ�Ƶ��Ϊ1��������
			//�������ڣ�����Ҫ����һ��Ƶ��1������
			if (freqMap_.find(key) == freqMap_.end())
			{
				freqMap_[1] = std::list<NodePtr>();
			}
			freqMap_[1].push_back(newNode);
			//��Ϊ���½ڵ���ӣ���СƵ���޸�Ϊ1
			minFreq_ = 1;

			return true;
		}

		//���Ľڵ�Ƶ��
		void updateNodeFrequency(NodePtr node)
		{
			//��ǰ��¼�ڵ���¾�Ƶ��
			size_t oldFreq = node->getAccessCount();
			node->incrementAccessCount();
			size_t newFreq = node->getAccessCount();

			//���ڵ�Ӿ�Ƶ�ε��������Ƴ�
			auto& oldList = freqMap_[oldFreq];
			oldList.remove(node);
			//����Ƴ��ڵ�󣬸�����Ϊ��,��Ҫ���ո�����
			if (oldList.empty())
			{
				freqMap_.erase(oldFreq);
				//���ң���������������Ƶ������ɾ������Ҫ�������Ƶ��
				if (oldFreq == minFreq_)
				{
					minFreq_ = newFreq;
				}
			}

			//���ڵ���ӵ���Ƶ������
			if (freqMap_.find(newFreq) == freqMap_.end())
			{
				//���ԭ�������ڶ�ӦƵ�ε���������Ҫ����һ��
				freqMap_[newFreq] = std::list <NodePtr>();
			}
			freqMap_[newFreq].push_back(node);
		}

		void evictLeastFrequent()
		{
			if (freqMap_.empty())
				return;

			//���Ȼ�ȡ���Ƶ������
			auto& minFreqList = freqMap_[minFreq_];
			if (minFreqList.empty())
				return;

			//�Ƴ����Ƶ������ʹ�õĽڵ�,���Ҵ�ʱ��Ҫ����Ҫ��ɾ���Ľڵ㣬������Ҫ�������黺��
			NodePtr deleteNode = minFreqList.front();
			minFreqList.pop_front();//������ǰ�˽ڵ�

			//���ɾ���ڵ���Ӧ����Ϊ�գ�����Ҫɾ��������
			if (minFreqList.empty())
			{
				freqMap_.erase(minFreq_);
				//��Ϊ���Ƶ������ɾ��������Ҫ�������Ƶ��
				if (!freqMap_.empty())
				{
					minFreq_ = freqMap_.begin()->first;
				}
			}

			//���ڵ�������黺��
			if (ghostCache_.size() >= ghostCpacity_)
			{
				removeOldestGhost();

			}
			addToGhost(deleteNode);
			
			//��󽫸ýڵ���������Ƴ�
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

		NodeMap mainCache_;//�����棬Ҳ�Ǽ�ֵ�ڵ��ϣ��
		NodeMap ghostCache_;//���黺��
		FreqMap freqMap_;//Ƶ�������ϣ���洢Ƶ�����ӦƵ�������ӳ���ϵ

		//���ﲻ����Ƶ�λ�����ڱ��ڵ��ԭ���ǣ�Ƶ�������ϣ����Ը���Ƶ�ε��ȵ���Ӧ������Ӧ�����Դ���front

		NodePtr ghostHead_;
		NodePtr ghostTail_;
	};

} // coloop