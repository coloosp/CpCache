#pragma once

#include <cmath>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "CopCachePolicy.h"

namespace CopCache {

	//��ǰ����lfuΪģ��
	template <typename Key, typename Value> class CopLfuCache;

	template <typename Key,typename Value>
	class FreqList//Ƶ��˫������
	{
	private:
		//����ڵ�ṹ��
		struct Node
		{
			int freq;//����Ƶ��
			Key key;
			Value value;
			std::shared_ptr<Node> pre;
			std::shared_ptr<Node> next;//ָ��ڵ������ָ�룬�ṩǰ���ͺ������ָ�룬�����Ϊ˫��������Ҫ

			//�ṩNode���޲ι��캯��,��ʼ���б�
			Node()
				:freq(1),pre(nullptr),next(nullptr){}
			//�вι��캯��
			Node(Key key,Value value)
				:freq(1),key(key),value(value),pre(nullptr),next(nullptr){}
		};
		using NodePtr = std::shared_ptr<Node>;//ʹ��Nodeptr���湹������ָ��Ĵ���,��������Node�ڵ������ָ��
		int freq_;//ע�⣬��Ȼÿ���ڵ㶼���Լ��ķ��ʴ�������������ķ��ʴ���������������ķ��ʴ���
		NodePtr head_;
		NodePtr tail_;//ÿ��������ڱ��ڵ�

	public:
		explicit FreqList(int n)
			:freq_(n)//explicit��ֹ��ʽת����Ƶ������Ĺ��캯��
		{
			head_ = std::make_shared<Node>();
			tail_ = std::make_shared<Node>();
			head_->next = tail_;
			tail_->pre = head_;//��β�����������ʼ����

		}

		bool isEmpty() const//�ж��������Ƿ�Ϊ��
		{
			return head_->next == tail_;
		}

		void addNode(NodePtr node) {//��ӽڵ�
			if (!node || !head_ || !tail_)//��ӵĽڵ�����������ų�
				return;

			node->pre = tail_->pre;
			node->next = tail_;
			
			tail_->pre->next = node;
			tail_->pre = node;
			
		}

		void removeNode(NodePtr node)//�Ƴ��ڵ�
		{
			if (!node || !head_ || !tail_) {
				return;
			}

			if (!node->pre || !node->next)
				return;

			node->pre->next = node->next;
			node->next->pre = node->pre;
			node->pre = nullptr;
			node->next = nullptr;
		}

		NodePtr getFirstNode() const {//��ȡ��Ӧ������׸��ڵ�
			return head_->next;
		}

		friend class CopLfuCache<Key, Value>;
		//�������lfu��ӵ�Ƶ������������Ϊ��Ԫ����





	};

	template <typename Key,typename Value>
	class CopLfuCache :public CopCachePolicy<Key, Value>
	{
	public:
		//�������
		using Node = typename FreqList<Key, Value>::Node;//����Ƶ�������еĽڵ㹹�캯��	
		using NodePtr = std::shared_ptr<Node>;//������������Ҫ����д�����ڵ�ָ��Ĵ���
		using NodeMap = std::unordered_map<Key, NodePtr>;//�ڵ��ϣ��

		//���캯��,�������ƽ������Ƶ�Σ����ҽ���ʼ��ƽ������Ƶ�κͷ���Ƶ���ܺ�����Ϊ0
		CopLfuCache(int capacity,int maxAverageNum = 10)
			:capacity_(capacity),minFreq_(INT8_MAX),maxAverageNum_(maxAverageNum),
			curAverageNum_(0),curTotalNum_(0)
		{}

		~CopLfuCache() override = default;//ʹ��Ĭ����������

		void put(Key key, Value value) override
		{
			if (capacity_ == 0)
				return;

			//�߳���
			std::lock_guard <std::mutex> lock(mutex_);
			auto it = nodeMap_.find(key);
			//������ڹ�ϣ�����ܹ��ҵ���Ӧ�ڵ�
			if (it != nodeMap_.end())
			{
				//���½ڵ�ֵ
				it->second->value = value;

				//��Ϊ������Ҫ����һ�η��ʴ���
				getInternal(it->second, value);
				return;

			}

			putInternal(key, value);
		}


		bool get(Key key, Value& value) override
		{
			std::lock_guard <std::mutex> lock(mutex_);
			auto it = nodeMap_.find(key);
			if (it != nodeMap_.end()) {
				//������value�޸ĺ󴫳�
				getInternal(it->second, value);
				return true;
			}
			return false;
		}

		Value get(Key key) override
		{
			Value value;
			get(key, value);
			return value;
		}


		//��ջ��棬��սڵ��ϣ���Ƶ��Ƶ�������ϣ��
		void purge()
		{
			nodeMap_.clear();
			freqToFreqList_.clear();
		}



	private:
		//�������������������������ʵ�֣���֮�Ⱥ�����˵���Լ�Ҫ�ã�
		void putInternal(Key key, Value value);//��ӻ���
		void getInternal(NodePtr node, Value& value);//��ȡ����

		void kickOut();//�Ƴ������еĹ�������

		void removeFromFreqList(NodePtr node);//��Ƶ���������Ƴ��ڵ�

		void addToFreqList(NodePtr node);//��ӵ�Ƶ����
		
		void addFreqNum();//����ƽ�����ʵ�Ƶ��
		void decreaseFreqNum(int num);//����ƽ�����ʵ�Ƶ��

		void handleOverMaxAverageNum();//����ǰƽ������Ƶ���������޵����
		void updateMinFreq();//�ϴ���СƵ��






	private:
		int capacity_;//��������
		int minFreq_;//��С����Ƶ�Σ������ҵ�����ĩ��
		int maxAverageNum_;//���ƽ������Ƶ��
		int curAverageNum_;//��ǰƽ������Ƶ��
		int curTotalNum_;//��ǰ���з���Ƶ������
		std::mutex mutex_;
		NodeMap nodeMap_;
		std::unordered_map<int, FreqList<Key, Value>*> freqToFreqList_;//����Ƶ�ζԸ÷���Ƶ�������ӳ���ϣ��
		
	};

	//������ʵ�ֺ���

	//��ȡ�ڵ�ֵ
	template <typename Key,typename Value>
	void CopLfuCache<Key, Value> ::getInternal(NodePtr node, Value& value)
	{
		//��lru��ͬ����lfu��ȡ�ڵ����Ҫ�Ƴ���ǰ�ڵ㣬���ҽ��ýڵ��ƶ���+1�ķ���Ƶ��������
		// ��ȡֵ
		value=node->value;
		//��ԭ�����������Ƴ��ڵ�,������Ƶ��+1
		removeFromFreqList(node);
		node->freq++;
		addToFreqList(node);
		
		if (node->freq - 1 == minFreq_ && freqToFreqList_[node->freq - 1]->isEmpty())
			minFreq_++;


		//ͬ������Ҫ�����ܷ���Ƶ�κ͵�ǰƽ������Ƶ��

		addFreqNum();
			
	}

	//����ڵ㵽����
	template <typename Key, typename Value>
	void CopLfuCache<Key, Value> ::putInternal(Key key, Value value)
	{
		//������put����ʱ������ڵ�δ�ڻ����У�����Ҫ���뻺��������
		if (nodeMap_.size() == capacity_)
		{
			//������ʱ��Ҫ����ڵ�
			kickOut();
		}
		//����ڵ㲢���ڵ�����ϣ���������
		NodePtr node = std::make_shared<Node>(key, value);
		nodeMap_[key] = node;
		addToFreqList(node);
		//���Ҹ����з���Ƶ���͵�ǰƽ������Ƶ��
		addFreqNum();
		//��Ϊ������˽ڵ㣬minFreq���£�minFrq��ʼ��ʱ������0�Ĵ���
		minFreq_ = std::min(minFreq_, 1);

	}

	//������������õĽڵ�
	template <typename Key, typename Value>
	void CopLfuCache<Key, Value> ::kickOut()
	{
		//��ȡ�����з���Ƶ�������ʱ����õĽڵ㣬ɾ�������·���Ƶ��������ƽ��ֵ
		NodePtr node = freqToFreqList_[minFreq_]->getFirstNode();
		removeFromFreqList(node);
		nodeMap_.erase(node->key);
		decreaseFreqNum(node->freq);

	}

	//�Ƴ���Ӧ�ڵ�
	template <typename Key, typename Value>
	void CopLfuCache<Key, Value> ::removeFromFreqList(NodePtr node)
	{
		//�ڵ�Ϊ���򲻴���
		if (!node)
			return;
		//����freqList��ĺ�������ڵ�
		auto freq = node->freq;
		freqToFreqList_[freq]->removeNode(node);
	}

	//��ӽڵ㵽������
	template <typename Key, typename Value>
	void CopLfuCache<Key, Value> ::addToFreqList(NodePtr node)
	{
		if (!node)
			return;
		//��ӵ���Ӧ��Ƶ�������У�������Ҫ�ж��Ƿ���ڸ����������ڻ���Ҫ����һ��
		auto freq = node->freq;
		if (freqToFreqList_.find(node->freq) == freqToFreqList_.end())
		{
			freqToFreqList_[node->freq] = new FreqList<Key, Value>(node->freq);

		}
		freqToFreqList_[freq]->addNode(node);

	}

	//����Ƶ��������ƽ����
	template <typename Key, typename Value>
	void CopLfuCache<Key, Value> ::addFreqNum()
	{
		//��ǰ��������
		curTotalNum_++;
		//�жϹ�ϣ���Ƿ�Ϊ�գ�����ƽ��Ƶ����
		if (nodeMap_.empty())
			curAverageNum_ = 0;
		else
			curAverageNum_ = curTotalNum_ / nodeMap_.size();

		//�����ǰƽ������Ƶ���Ѿ�����������ƣ�����������
		if (curAverageNum_ > maxAverageNum_)
		{
			handleOverMaxAverageNum();
		}
	}

	//��Ӧ���������Щֵ
	template <typename Key, typename Value>
	void CopLfuCache<Key, Value> ::decreaseFreqNum(int num)
	{
		//����ƽ������Ƶ�κ��ܷ���Ƶ��
		curTotalNum_ -= num;
		//Ȼ�����ƽ��������
		if (nodeMap_.empty())
			curAverageNum_ = 0;
		else
			curAverageNum_ = curTotalNum_ / nodeMap_.size();

	}

	//������������ƽ��ֵ�Ѿ�������������
	template <typename Key, typename Value>
	void CopLfuCache<Key, Value> ::handleOverMaxAverageNum()
	{
		if (nodeMap_.empty())
			return;

		//��Ϊ��ǰƽ������Ƶ�γ��������ƽ������Ƶ�����ƣ��������нڵ�ķ���Ƶ�λ��ȥ(maxAVerageNum_/2)
		//�������нڵ�
		for (auto it = nodeMap_.begin(); it != nodeMap_.end(); ++it) {

			if (!it->second)
				continue;

			NodePtr node = it->second;
			//�ȴӵ�ǰƵ���������Ƴ�
			removeFromFreqList(node);

			//����Ƶ�ʲ��ұ�֤Ƶ������Ϊ1
			node->freq -= maxAverageNum_ / 2;
			if (node->freq < 1)
				node->freq = 1;

			//������ӵ��µ�Ƶ������
			addToFreqList(node);
		}
		//������ɺ�������СƵ��
		updateMinFreq();
	}

	template <typename Key, typename Value>
	void CopLfuCache<Key, Value> ::updateMinFreq()
	{
		minFreq_ = INT8_MAX;
		//ɨ�����нڵ㣬�����³���С����Ƶ��
		for (const auto& pair : freqToFreqList_)
		{
			if (pair.second && !pair.second->isEmpty())
			{
				minFreq_ = std::min(minFreq_, pair.first);
			}
		}

		if (minFreq_ == INT8_MAX)
			minFreq_ = 1;
	}


	//��lru��ͬ����Ƭ����߲��б�̵�Ч��
	template<typename Key,typename Value>
	class CopHashLfuCache
	{
	public:
		//���캯��
		CopHashLfuCache(size_t capacity, int sliceNum, int maxAverageNum = 10)
			:sliceNum_(sliceNum > 0 ? sliceNum : std::thread::hardware_concurrency())
			, capacity_(capacity)
		{
			size_t sliceSize = std::ceil(capacity_ / static_cast<double>(sliceNum_));//ÿ��lfu��Ƭ��������С������ȡ��
			for (int i = 0; i < sliceNum_; ++i) {
				//ͬ����������Ҫ����Ƭ������������Ӧ���������Ļ�����Ƭ��������ָ�����������
				lfuSliceCaches_.emplace_back(new CopLfuCache<Key, Value>(sliceSize, maxAverageNum));
			}
		}

		void put(Key key, Value value)
		{
			//����key�Ĺ�ϣֵ���ɹ�ϣ�������޸ģ���ӣ��ڵ�
			size_t sliceIndex = Hash(key) % sliceNum_;
			return lfuSliceCaches_[sliceIndex]->put(key, value);
		}

		bool get(Key key, Value& value)
		{
			//ͬ�����λ�ȡ��Ӧ�ڵ�ֵ
			size_t sliceIndex = Hash(key) % sliceNum_;
			return lfuSliceCaches_[sliceIndex]->get(key, value);
		}

		Value get(Key key) {
			Value value;
			get(key, value);
			return value;
		}

		//���������Ƭ�����л���(�ڵ��ϣ��Ƶ��Ƶ�������ϣ��)
		void purge()
		{
			for (auto& lfuSlice : lfuSliceCaches_)
				lfuSlice->purge();
		}
			
	private:
		//��keyֵת���ɶ�Ӧ�Ĺ�ϣֵ
		size_t Hash(Key key) {
			std::hash <Key> hashFunc;
			return hashFunc(key);
		}
	private:
		size_t capacity_;//����������
		int sliceNum_;//�����Ƭ����
		std::vector<std::unique_ptr<CopLfuCache<Key, Value>>> lfuSliceCaches_;//�����Ƭ�Ĵ洢����
	};


}//namepace coloop