#pragma once

# include <cstring>
# include <list>
# include <memory>
# include <mutex>
# include <unordered_map>
# include <vector>

#include "CopCachePolicy.h"

namespace CopCache {
	//模板,提前声明CopLruCache中的模板
	template <typename Key, typename Value> class CopLruCache;
	 
	template <typename Key,typename Value>
	class LruNode {
	private:
		Key key_;
		Value value_;
		size_t accessCount_; //访问次数
		//智能指针，指向节点的哨兵节点指针
		std::shared_ptr<LruNode<Key, Value>> prev_;
		std::shared_ptr<LruNode<Key, Value>> next_;

	public:

		//构造函数初始化列表
		LruNode(Key key, Value value) 
			: key_(key)
			, value_(value)
			, accessCount_(1)
			, prev_(nullptr)
			, next_(nullptr)
		{}

	
		//提供可以访问成员属性的访问器
		Key getKey() const { return key_; }//获取键
		Value getValue() const { return value_; }//获取值
		void setValue(const Value& value) { value_ = value; }
		size_t getAcessCount() const { return accessCount_; }//常函数，获取访问次数值
		void incrementAccessCount() { ++accessCount_; }//增加访问次数值

		//友元类
		friend class CopLruCache<Key, Value>;
	};

	//继承类模板并将派生类模板化
	template <typename Key,typename Value>
	class CopLruCache : public CopCachePolicy<Key, Value>
	{

	public:
		//创建类型别名，Lru节点类
		using LruNodeType = LruNode<Key, Value>;
		//指向Lru节点的智能指针
		using NodePtr = std::shared_ptr <LruNodeType>;
		//节点哈希表,存储键与指针的关系
		using NodeMap = std::unordered_map <Key, NodePtr>;

		//构造函数
		CopLruCache(int capacity) 
			:capacity_(capacity) 
		{
			initializeList();
		}
		//析构函数,覆写并使用默认实现
		~CopLruCache() override = default;
	public:
		//添加缓存函数
		void put(Key key, Value value) override
		{
			if (capacity_ <= 0) 
				return;
			

			//线程锁
			std::lock_guard<std::mutex> lock(mutex_);

			auto it = nodeMap_.find(key);
			if (it != nodeMap_.end()) {
				//如果已经在容器中存在，则更新
				updateExistingNode(it->second, value);
				return;

			}
			//若不存在，则更新
			addNewNode(key, value);
		}

		//获取节点值,bool 型可以避免在访问不到值时需要返回值的情况
		bool get(Key key, Value& value) override
		{
			std::lock_guard<std::mutex> lock(mutex_);
			auto it = nodeMap_.find(key);
			if (it != nodeMap_.end()) {
				moveToMostRecent(it->second);
				//如果该键有对应节点，那么将引用的value修改为对应节点值
				value = it->second->getValue();
				return true;
			}
			//否则返回false
			return false;
		}

		//get的函数重载
		Value get(Key key) override
		{
			//对value进行初始化
			Value value{};
			get(key, value);
			return value;
		}

		void remove(Key key) {

			std::lock_guard<std::mutex> lock(mutex_);
			auto it = nodeMap_.find(key);
			if (it != nodeMap_.end()) {
				removeNode(it->second);
				nodeMap_.erase(it);
			}
		}
	private:
		int    capacity_;//缓存容量
		NodeMap nodeMap_;// 节点哈希表
		std::mutex mutex_;
		NodePtr dummyHead_;
		NodePtr dummyTail_;//哨兵头尾节点

	private:
		//初始化链表
		void initializeList() {
			//创建头尾哨兵节点，初始值为空
			dummyHead_ = std::make_shared<LruNodeType>(Key(), Value());
			dummyTail_ = std::make_shared<LruNodeType>(Key(), Value());
			//将首尾哨兵节点相连，初始化双向链表
			dummyHead_->next_ = dummyTail_;
			dummyTail_->prev_ = dummyHead_;
		}

		//更新存在缓存中的节点值
		void updateExistingNode(NodePtr node, const Value& value)
		{
			node->setValue(value);
			moveToMostRecent(node);//执行操作后，需要将节点移动到最新位置
		}

		//添加新节点
		void addNewNode(const Key& key, const Value& value)
		{
			if (nodeMap_.size() >= capacity_) {
				evictLeastRecent();
			}//如果内存已满则驱逐最近最少访问

			NodePtr newNode = std::make_shared<LruNodeType>(key, value);
			insertNode(newNode);
			nodeMap_[key] = newNode;
		}

		//将该节点移动到最新位置
		void moveToMostRecent(NodePtr node) {
			removeNode(node);
			insertNode(node);
		}

		//移除节点
		void removeNode(NodePtr node) {
			node->prev_->next_ = node->next_;
			node->next_->prev_ = node->prev_;
		}

		//从尾部插入节点
		void insertNode(NodePtr node) {
			node->next_ = dummyTail_;
			node->prev_ = dummyTail_->prev_;
			dummyTail_->prev_->next_ = node;
			dummyTail_->prev_ = node;
		}

		//驱逐最近最少访问
		void evictLeastRecent() {
			NodePtr leastRecent = dummyHead_->next_;
			removeNode(leastRecent);
			nodeMap_.erase(leastRecent->getKey());//从哈希表中移除对应键
		}


	
	}; 

	//LRU优化：LRU-k版本。继承Lru类,标注父类模板，将子类模板化
	template <typename Key,typename Value>
	class CopLruKCache : public CopLruCache <Key, Value>
	{
	public:
		//构造函数
		CopLruKCache(int capacity,int historyCapacity,int k)
			:CopLruCache<Key,Value>(capacity)//使用基类初始化内存，保证缓存内存的一致性
			//创建了CopLruCache对象，并将智能指针指向对象，存入数据访问队列中的数据也会遵循LRU算法
			,historyList_(std::make_unique<CopLruCache<Key,size_t>>(historyCapacity))
			,k_(k)
		{}

		Value get(Key key) 
		{
			//获取数据访问次数
			int historyCount = historyList_->get(key);
			//存在key则新增访问次数
			historyList_->put(key, ++historyCount);

			//获取缓存中的对应值（如果在缓存中的话），标注作用域，使用lru中的get函数
			return CopLruCache<Key, Value> ::get(key);

		}

		void put(Key key,Value value) {
			//如果在缓存中存在，则直接更新值
			if (CopLruCache<Key, Value>::get(key) != "") {
				CopLruCache<Key, Value>::put(key, value);
			}

			//如果不存在，则会添加到数据访问队列中，并增加次数
			int historyCount = historyList_->get(key);
			historyList_->put(key, ++historyCount);

			//如果数据历史访问次数达到上限，就会添加进缓存中
			if (historyCount >= k_) {
				//移除历史访问记录
				historyList_->remove(key);
				//调用Lru作用域的put函数添加进缓存中
				CopLruCache<Key, Value>::put(key, value);
			}
		}


	private:
		int k_;//定义访问数据历史记录进入缓存队列的标准
		std::unique_ptr<CopLruCache<Key, size_t>> historyList_;//访问数据历史记录队列，键值与对应节点的访问次数映射
	};

	//lru哈希优化,提高高并发使用的性能
	template<typename Key,typename Value>
	class CopHashLruCache
	{
	public:
		CopHashLruCache(size_t capacity, int sliceNum) :
			capacity_(capacity),
			sliceNum_(sliceNum > 0 ? sliceNum : std::thread::hardware_concurrency())//如果切片数大于零就初始化切片，否则使用默认值
		{
			size_t sliceSize = std::ceil(capacity / static_cast<double> (sliceNum_));//获取每一个分片的大小,向上取整
			for (int i = 0; i < sliceNum_; i++) {
				lruSliceCaches_.emplace_back(new CopLruCache<Key, Value>(sliceSize));//创建缓存分片（创建的一个个小缓存lru）并添加到容器中
			}
		}

		void put(Key key, Value value) {
			//获取Key的hash值，并计算出分片索引
			size_t sliceIndex = Hash(key) % sliceNum_;
			//然后利用对应的索引值去获取对应缓存的指针，从而调用该缓存的put函数来修改缓存中节点的对应值
			return lruSliceCaches_[sliceIndex]->put(key, value);
		}
		
		//判断缓存中是否存在key对应的节点,value为传出参数
		bool get(Key key,Value value) {
			size_t sliceIndex = Hash(key) % sliceNum_;
			return lruSliceCaches_[sliceIndex]->get(key, value);
		}

		Value get(Key key) {
			Value value;
			memset(&value, 0, sizeof(value));//初始化值，便于后面抓取对应值
			get(key, value);
			return value;//如果缓存中没有节点就会返回初始化值
		}
	private:
		//将key转换成对应的hash值
		size_t Hash(Key key) {
			std::hash<Key> hashFunc;
			return hashFunc(key);
		}

	private:
		size_t capacity_;//总容量
		int sliceNum_;//切片数量
		std::vector<std::unique_ptr<CopLruCache<Key, Value>>> lruSliceCaches_;//切片LRU缓存数组
	};


}//namespace coloop