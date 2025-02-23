#pragma once

#include "CopArcCacheNode.h"
#include <unordered_map>
#include <mutex>

namespace CopCache {
	template <typename Key,typename Value>
	class ArcLruPart
	{
	public:
		//���ͱ���
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

			//�߳���
			std::lock_guard <std::mutex> lock(mutex_);
			auto it = MainCache_.find(key);
			if (it != MainCache_.end()) {
				return updateExistingNode(it->second, value);
			}
			return addNewNode(key, value);
		}

		//������������ ֵ �� �Ƿ�ﵽת����ֵ�ж�
		bool get(Key key, Value& value, bool& shouldTransform)
		{
			std::lock_guard <std::mutex> lock(mutex_);

			auto it = MainCache_.find(key);
			if (it != MainCache_.end())
			{
				//���·���Ƶ�Σ����ж��Ƿ�ﵽת����ֵ
				shouldTransform = updateNodeAccess(it->second);
				value = it->second->getValue();
				return true;
			}
			return false;
		}

		//������黺�����Ƿ���ڶ��ڽڵ�
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

		//���ӻ�������
		void increaseCapacity() { ++capacity_; }

		// ���ٻ�������
		bool decreaseCapacity()
		{
			if (capacity_ <= 0) return false;
			//������ʱ��Ҫ����һλ����
			if (MainCache_.size() == capacity_)
			{
				evictLeastRecent();
			}
			--capacity_;
			return true;
		}



	private:
		//��ʼ��
		void initializeLists() {
			//����������
			mainHead_ = std::make_shared<NodeType>();
			mainTail_ = std::make_shared<NodeType>();
			mainHead_->next_ = mainTail_;
			mainTail_->prev_ = mainHead_;

			//������������
			ghostHead_ = std::make_shared<NodeType>();
			ghostTail_ = std::make_shared<NodeType>();
			ghostHead_->next_ = ghostTail_;
			ghostTail_->prev_ = ghostHead_;
		}

		//�����ڻ����е�ֵ
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
				//���������Ҫ����������ٷ��ʽڵ�
				evictLeastRecent();
			}

			//�����½ڵ㲢���뻺���У������ڵ����ͷ��
			NodePtr newNode = std::make_shared<NodeType>(key, value);
			MainCache_[key] = newNode;
			addToFront(newNode);
			return true;

		}

		bool updateNodeAccess(NodePtr node)
		{
			//���½ڵ�ķ��ʴ����������ж��Ƿ����ת����ֵ
			moveToFront(node);
			node->incrementAccessCount();
			return node->getAccessCount() >= transformThreshold_;
		}

		//�ƶ��ڵ���ͷ��
		void moveToFront(NodePtr node)
		{
			//�ӵ�ǰλ���Ƴ�
			node->prev_->next_ = node->next_;
			node->next_->prev_ = node->prev_;

			
			addToFront(node);

		}

		//����ӽڵ���ͷ��
		void addToFront(NodePtr node)
		{
			node->next_ = mainHead_->next_;
			node->prev_ = mainHead_;
			mainHead_->next_->prev_ = node;
			mainHead_->next_ = node;
		}

		//����������ٷ��ʽڵ�
		void evictLeastRecent()
		{
			//����β����Ԫ����������ٷ���Ԫ��
			NodePtr leastRecent = mainTail_->prev_;
			//����Ϊ��������
			if (leastRecent == mainHead_)
				return;
			
			//�����������Ƴ�
			removeFromMain(leastRecent);

			//���Ƴ��ڵ���ӵ����黺�棬�����������ͬ���ķ�ʽ����
			if (GhostCache_.size() >= ghostCapacity_)
			{
				removeOldestGhost();
			}

			addToGhost(leastRecent);

			//����ӳ��ɾ�����ڽڵ�
			MainCache_.erase(leastRecent->getKey());

		}

		void removeFromMain(NodePtr node)
		{
			node->prev_->next_ = node->next_;
			node->next_->prev_ = node->prev_;

		}

		//�����黺�����Ƴ���д��������ֻ��Ϊ�˺�����
		void removeFromGhost(NodePtr node)
		{
			node->prev_->next_ = node->next_;
			node->next_->prev_ = node->prev_;
		}

		// ��ӵ����黺����
		void addToGhost(NodePtr node)
		{
			//���÷��ʴ���
			node->accessCount_=1;

			//��ӵ����黺��ͷ��
			node->next_ = ghostHead_->next_;
			node->prev_ = ghostHead_;
			ghostHead_->next_->prev_ = node;
			ghostHead_->next_ = node;

			//��ӵ����黺��ӳ��
			GhostCache_[node->getKey()] = node;
		}

		//�����黺���������������ʹ�ýڵ㣬��֮ǰ��д��һ��
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
		size_t transformThreshold_;//ת����ֵ
		std::mutex mutex_;

		//�����������黺��

		NodeMap MainCache_;
		NodeMap GhostCache_;

		//�������ڱ��ڵ�ָ��
		NodePtr mainHead_;
		NodePtr mainTail_;

		//���������ڱ��ڵ�ָ��
		NodePtr ghostHead_;
		NodePtr ghostTail_;


	};






} //coloop