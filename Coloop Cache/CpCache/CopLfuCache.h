#pragma once

#include <cmath>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "CopCachePolicy.h"

namespace CopCache {

	//提前声明lfu为模板
	template <typename Key, typename Value> class CopLfuCache;

	template <typename Key,typename Value>
	class FreqList//频数双向链表
	{
	private:
		//构造节点结构体
		struct Node
		{
			int freq;//访问频次
			Key key;
			Value value;
			std::shared_ptr<Node> pre;
			std::shared_ptr<Node> next;//指向节点的智能指针，提供前驱和后继两种指针，构造成为双向链表需要

			//提供Node的无参构造函数,初始化列表
			Node()
				:freq(1),pre(nullptr),next(nullptr){}
			//有参构造函数
			Node(Key key,Value value)
				:freq(1),key(key),value(value),pre(nullptr),next(nullptr){}
		};
		using NodePtr = std::shared_ptr<Node>;//使用Nodeptr代替构造智能指针的代码,用来声明Node节点的智能指针
		int freq_;//注意，虽然每个节点都有自己的访问次数，但是这里的访问次数代表该链表代表的访问次数
		NodePtr head_;
		NodePtr tail_;//每个链表的哨兵节点

	public:
		explicit FreqList(int n)
			:freq_(n)//explicit防止隐式转换，频数链表的构造函数
		{
			head_ = std::make_shared<Node>();
			tail_ = std::make_shared<Node>();
			head_->next = tail_;
			tail_->pre = head_;//首尾相连，构造初始链表

		}

		bool isEmpty() const//判断链表中是否为空
		{
			return head_->next == tail_;
		}

		void addNode(NodePtr node) {//添加节点
			if (!node || !head_ || !tail_)//添加的节点存在问题则排除
				return;

			node->pre = tail_->pre;
			node->next = tail_;
			
			tail_->pre->next = node;
			tail_->pre = node;
			
		}

		void removeNode(NodePtr node)//移除节点
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

		NodePtr getFirstNode() const {//获取对应链表的首个节点
			return head_->next;
		}

		friend class CopLfuCache<Key, Value>;
		//将后面的lfu添加到频数链表类中作为友元函数





	};

	template <typename Key,typename Value>
	class CopLfuCache :public CopCachePolicy<Key, Value>
	{
	public:
		//代码别名
		using Node = typename FreqList<Key, Value>::Node;//代替频数链表中的节点构造函数	
		using NodePtr = std::shared_ptr<Node>;//更换作用域需要重新写声明节点指针的代替
		using NodeMap = std::unordered_map<Key, NodePtr>;//节点哈希表

		//构造函数,设置最大平均访问频次，并且将初始的平均访问频次和访问频次总和设置为0
		CopLfuCache(int capacity,int maxAverageNum = 10)
			:capacity_(capacity),minFreq_(INT8_MAX),maxAverageNum_(maxAverageNum),
			curAverageNum_(0),curTotalNum_(0)
		{}

		~CopLfuCache() override = default;//使用默认析构函数

		void put(Key key, Value value) override
		{
			if (capacity_ == 0)
				return;

			//线程锁
			std::lock_guard <std::mutex> lock(mutex_);
			auto it = nodeMap_.find(key);
			//如果能在哈希表中能够找到对应节点
			if (it != nodeMap_.end())
			{
				//更新节点值
				it->second->value = value;

				//因为访问需要增加一次访问次数
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
				//将传入value修改后传出
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


		//清空缓存，清空节点哈希表和频次频次链表哈希表
		void purge()
		{
			nodeMap_.clear();
			freqToFreqList_.clear();
		}



	private:
		//先声明函数，后面会在类外逐步实现（总之先喊出来说明自己要用）
		void putInternal(Key key, Value value);//添加缓存
		void getInternal(NodePtr node, Value& value);//获取缓存

		void kickOut();//移除缓存中的过期数据

		void removeFromFreqList(NodePtr node);//从频数链表中移除节点

		void addToFreqList(NodePtr node);//添加到频数中
		
		void addFreqNum();//增加平均访问等频率
		void decreaseFreqNum(int num);//减少平均访问等频率

		void handleOverMaxAverageNum();//处理当前平均访问频数超过上限的情况
		void updateMinFreq();//上传最小频数






	private:
		int capacity_;//缓存容量
		int minFreq_;//最小访问频次，用于找到链表末端
		int maxAverageNum_;//最大平均访问频次
		int curAverageNum_;//当前平均访问频次
		int curTotalNum_;//当前所有访问频次总数
		std::mutex mutex_;
		NodeMap nodeMap_;
		std::unordered_map<int, FreqList<Key, Value>*> freqToFreqList_;//访问频次对该访问频次链表的映射哈希表
		
	};

	//逐步类外实现函数

	//获取节点值
	template <typename Key,typename Value>
	void CopLfuCache<Key, Value> ::getInternal(NodePtr node, Value& value)
	{
		//和lru不同，在lfu获取节点后，需要移除当前节点，并且将该节点移动到+1的访问频次链表中
		// 获取值
		value=node->value;
		//从原来的链表中移除节点,将访问频次+1
		removeFromFreqList(node);
		node->freq++;
		addToFreqList(node);
		
		if (node->freq - 1 == minFreq_ && freqToFreqList_[node->freq - 1]->isEmpty())
			minFreq_++;


		//同样，需要更新总访问频次和当前平均访问频次

		addFreqNum();
			
	}

	//加入节点到缓存
	template <typename Key, typename Value>
	void CopLfuCache<Key, Value> ::putInternal(Key key, Value value)
	{
		//当调用put函数时，如果节点未在缓存中，则需要放入缓存链表中
		if (nodeMap_.size() == capacity_)
		{
			//缓存满时需要驱逐节点
			kickOut();
		}
		//构造节点并将节点放入哈希表和链表中
		NodePtr node = std::make_shared<Node>(key, value);
		nodeMap_[key] = node;
		addToFreqList(node);
		//并且更新中访问频数和当前平均访问频次
		addFreqNum();
		//因为新添加了节点，minFreq更新，minFrq初始化时可能有0的存在
		minFreq_ = std::min(minFreq_, 1);

	}

	//驱逐最久最少用的节点
	template <typename Key, typename Value>
	void CopLfuCache<Key, Value> ::kickOut()
	{
		//获取链表中访问频次最低且时间最久的节点，删除并更新访问频次总数和平均值
		NodePtr node = freqToFreqList_[minFreq_]->getFirstNode();
		removeFromFreqList(node);
		nodeMap_.erase(node->key);
		decreaseFreqNum(node->freq);

	}

	//移除对应节点
	template <typename Key, typename Value>
	void CopLfuCache<Key, Value> ::removeFromFreqList(NodePtr node)
	{
		//节点为空则不处理
		if (!node)
			return;
		//调用freqList类的函数处理节点
		auto freq = node->freq;
		freqToFreqList_[freq]->removeNode(node);
	}

	//添加节点到链表中
	template <typename Key, typename Value>
	void CopLfuCache<Key, Value> ::addToFreqList(NodePtr node)
	{
		if (!node)
			return;
		//添加到相应的频次链表中，但先需要判断是否存在该链表，不存在还需要构造一个
		auto freq = node->freq;
		if (freqToFreqList_.find(node->freq) == freqToFreqList_.end())
		{
			freqToFreqList_[node->freq] = new FreqList<Key, Value>(node->freq);

		}
		freqToFreqList_[freq]->addNode(node);

	}

	//增加频次总数和平均数
	template <typename Key, typename Value>
	void CopLfuCache<Key, Value> ::addFreqNum()
	{
		//当前总数增加
		curTotalNum_++;
		//判断哈希表是否为空，计算平均频次数
		if (nodeMap_.empty())
			curAverageNum_ = 0;
		else
			curAverageNum_ = curTotalNum_ / nodeMap_.size();

		//如果当前平均访问频数已经超过最大限制，启动处理函数
		if (curAverageNum_ > maxAverageNum_)
		{
			handleOverMaxAverageNum();
		}
	}

	//对应上面减少这些值
	template <typename Key, typename Value>
	void CopLfuCache<Key, Value> ::decreaseFreqNum(int num)
	{
		//减少平均访问频次和总访问频次
		curTotalNum_ -= num;
		//然后更新平均数即可
		if (nodeMap_.empty())
			curAverageNum_ = 0;
		else
			curAverageNum_ = curTotalNum_ / nodeMap_.size();

	}

	//处理函数，处理平均值已经大于设置限制
	template <typename Key, typename Value>
	void CopLfuCache<Key, Value> ::handleOverMaxAverageNum()
	{
		if (nodeMap_.empty())
			return;

		//因为当前平均访问频次超过了最大平均访问频次限制，所以所有节点的访问频次会减去(maxAVerageNum_/2)
		//遍历所有节点
		for (auto it = nodeMap_.begin(); it != nodeMap_.end(); ++it) {

			if (!it->second)
				continue;

			NodePtr node = it->second;
			//先从当前频率链表中移除
			removeFromFreqList(node);

			//减半频率并且保证频率至少为1
			node->freq -= maxAverageNum_ / 2;
			if (node->freq < 1)
				node->freq = 1;

			//重新添加到新的频率链表
			addToFreqList(node);
		}
		//更新完成后重置最小频率
		updateMinFreq();
	}

	template <typename Key, typename Value>
	void CopLfuCache<Key, Value> ::updateMinFreq()
	{
		minFreq_ = INT8_MAX;
		//扫描所有节点，并更新出最小访问频次
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


	//和lru相同的切片，提高并行编程的效率
	template<typename Key,typename Value>
	class CopHashLfuCache
	{
	public:
		//构造函数
		CopHashLfuCache(size_t capacity, int sliceNum, int maxAverageNum = 10)
			:sliceNum_(sliceNum > 0 ? sliceNum : std::thread::hardware_concurrency())
			, capacity_(capacity)
		{
			size_t sliceSize = std::ceil(capacity_ / static_cast<double>(sliceNum_));//每个lfu切片的容量大小，向上取整
			for (int i = 0; i < sliceNum_; ++i) {
				//同样，根据需要的切片数量来构造相应缓存容量的缓存切片，并将其指针放入容器中
				lfuSliceCaches_.emplace_back(new CopLfuCache<Key, Value>(sliceSize, maxAverageNum));
			}
		}

		void put(Key key, Value value)
		{
			//利用key的哈希值生成哈希索引，修改（添加）节点
			size_t sliceIndex = Hash(key) % sliceNum_;
			return lfuSliceCaches_[sliceIndex]->put(key, value);
		}

		bool get(Key key, Value& value)
		{
			//同理，传参获取对应节点值
			size_t sliceIndex = Hash(key) % sliceNum_;
			return lfuSliceCaches_[sliceIndex]->get(key, value);
		}

		Value get(Key key) {
			Value value;
			get(key, value);
			return value;
		}

		//清除所有切片的所有缓存(节点哈希表，频次频次链表哈希表)
		void purge()
		{
			for (auto& lfuSlice : lfuSliceCaches_)
				lfuSlice->purge();
		}
			
	private:
		//将key值转换成对应的哈希值
		size_t Hash(Key key) {
			std::hash <Key> hashFunc;
			return hashFunc(key);
		}
	private:
		size_t capacity_;//缓存总容量
		int sliceNum_;//缓存分片数量
		std::vector<std::unique_ptr<CopLfuCache<Key, Value>>> lfuSliceCaches_;//缓存分片的存储容器
	};


}//namepace coloop