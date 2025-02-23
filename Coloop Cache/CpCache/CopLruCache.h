#pragma once

# include <cstring>
# include <list>
# include <memory>
# include <mutex>
# include <unordered_map>
# include <vector>

#include "CopCachePolicy.h"

namespace CopCache {
	//ģ��,��ǰ����CopLruCache�е�ģ��
	template <typename Key, typename Value> class CopLruCache;
	 
	template <typename Key,typename Value>
	class LruNode {
	private:
		Key key_;
		Value value_;
		size_t accessCount_; //���ʴ���
		//����ָ�룬ָ��ڵ���ڱ��ڵ�ָ��
		std::shared_ptr<LruNode<Key, Value>> prev_;
		std::shared_ptr<LruNode<Key, Value>> next_;

	public:

		//���캯����ʼ���б�
		LruNode(Key key, Value value) 
			: key_(key)
			, value_(value)
			, accessCount_(1)
			, prev_(nullptr)
			, next_(nullptr)
		{}

	
		//�ṩ���Է��ʳ�Ա���Եķ�����
		Key getKey() const { return key_; }//��ȡ��
		Value getValue() const { return value_; }//��ȡֵ
		void setValue(const Value& value) { value_ = value; }
		size_t getAcessCount() const { return accessCount_; }//����������ȡ���ʴ���ֵ
		void incrementAccessCount() { ++accessCount_; }//���ӷ��ʴ���ֵ

		//��Ԫ��
		friend class CopLruCache<Key, Value>;
	};

	//�̳���ģ�岢��������ģ�廯
	template <typename Key,typename Value>
	class CopLruCache : public CopCachePolicy<Key, Value>
	{

	public:
		//�������ͱ�����Lru�ڵ���
		using LruNodeType = LruNode<Key, Value>;
		//ָ��Lru�ڵ������ָ��
		using NodePtr = std::shared_ptr <LruNodeType>;
		//�ڵ��ϣ��,�洢����ָ��Ĺ�ϵ
		using NodeMap = std::unordered_map <Key, NodePtr>;

		//���캯��
		CopLruCache(int capacity) 
			:capacity_(capacity) 
		{
			initializeList();
		}
		//��������,��д��ʹ��Ĭ��ʵ��
		~CopLruCache() override = default;
	public:
		//��ӻ��溯��
		void put(Key key, Value value) override
		{
			if (capacity_ <= 0) 
				return;
			

			//�߳���
			std::lock_guard<std::mutex> lock(mutex_);

			auto it = nodeMap_.find(key);
			if (it != nodeMap_.end()) {
				//����Ѿ��������д��ڣ������
				updateExistingNode(it->second, value);
				return;

			}
			//�������ڣ������
			addNewNode(key, value);
		}

		//��ȡ�ڵ�ֵ,bool �Ϳ��Ա����ڷ��ʲ���ֵʱ��Ҫ����ֵ�����
		bool get(Key key, Value& value) override
		{
			std::lock_guard<std::mutex> lock(mutex_);
			auto it = nodeMap_.find(key);
			if (it != nodeMap_.end()) {
				moveToMostRecent(it->second);
				//����ü��ж�Ӧ�ڵ㣬��ô�����õ�value�޸�Ϊ��Ӧ�ڵ�ֵ
				value = it->second->getValue();
				return true;
			}
			//���򷵻�false
			return false;
		}

		//get�ĺ�������
		Value get(Key key) override
		{
			//��value���г�ʼ��
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
		int    capacity_;//��������
		NodeMap nodeMap_;// �ڵ��ϣ��
		std::mutex mutex_;
		NodePtr dummyHead_;
		NodePtr dummyTail_;//�ڱ�ͷβ�ڵ�

	private:
		//��ʼ������
		void initializeList() {
			//����ͷβ�ڱ��ڵ㣬��ʼֵΪ��
			dummyHead_ = std::make_shared<LruNodeType>(Key(), Value());
			dummyTail_ = std::make_shared<LruNodeType>(Key(), Value());
			//����β�ڱ��ڵ���������ʼ��˫������
			dummyHead_->next_ = dummyTail_;
			dummyTail_->prev_ = dummyHead_;
		}

		//���´��ڻ����еĽڵ�ֵ
		void updateExistingNode(NodePtr node, const Value& value)
		{
			node->setValue(value);
			moveToMostRecent(node);//ִ�в�������Ҫ���ڵ��ƶ�������λ��
		}

		//����½ڵ�
		void addNewNode(const Key& key, const Value& value)
		{
			if (nodeMap_.size() >= capacity_) {
				evictLeastRecent();
			}//����ڴ�����������������ٷ���

			NodePtr newNode = std::make_shared<LruNodeType>(key, value);
			insertNode(newNode);
			nodeMap_[key] = newNode;
		}

		//���ýڵ��ƶ�������λ��
		void moveToMostRecent(NodePtr node) {
			removeNode(node);
			insertNode(node);
		}

		//�Ƴ��ڵ�
		void removeNode(NodePtr node) {
			node->prev_->next_ = node->next_;
			node->next_->prev_ = node->prev_;
		}

		//��β������ڵ�
		void insertNode(NodePtr node) {
			node->next_ = dummyTail_;
			node->prev_ = dummyTail_->prev_;
			dummyTail_->prev_->next_ = node;
			dummyTail_->prev_ = node;
		}

		//����������ٷ���
		void evictLeastRecent() {
			NodePtr leastRecent = dummyHead_->next_;
			removeNode(leastRecent);
			nodeMap_.erase(leastRecent->getKey());//�ӹ�ϣ�����Ƴ���Ӧ��
		}


	
	}; 

	//LRU�Ż���LRU-k�汾���̳�Lru��,��ע����ģ�壬������ģ�廯
	template <typename Key,typename Value>
	class CopLruKCache : public CopLruCache <Key, Value>
	{
	public:
		//���캯��
		CopLruKCache(int capacity,int historyCapacity,int k)
			:CopLruCache<Key,Value>(capacity)//ʹ�û����ʼ���ڴ棬��֤�����ڴ��һ����
			//������CopLruCache���󣬲�������ָ��ָ����󣬴������ݷ��ʶ����е�����Ҳ����ѭLRU�㷨
			,historyList_(std::make_unique<CopLruCache<Key,size_t>>(historyCapacity))
			,k_(k)
		{}

		Value get(Key key) 
		{
			//��ȡ���ݷ��ʴ���
			int historyCount = historyList_->get(key);
			//����key���������ʴ���
			historyList_->put(key, ++historyCount);

			//��ȡ�����еĶ�Ӧֵ������ڻ����еĻ�������ע������ʹ��lru�е�get����
			return CopLruCache<Key, Value> ::get(key);

		}

		void put(Key key,Value value) {
			//����ڻ����д��ڣ���ֱ�Ӹ���ֵ
			if (CopLruCache<Key, Value>::get(key) != "") {
				CopLruCache<Key, Value>::put(key, value);
			}

			//��������ڣ������ӵ����ݷ��ʶ����У������Ӵ���
			int historyCount = historyList_->get(key);
			historyList_->put(key, ++historyCount);

			//���������ʷ���ʴ����ﵽ���ޣ��ͻ���ӽ�������
			if (historyCount >= k_) {
				//�Ƴ���ʷ���ʼ�¼
				historyList_->remove(key);
				//����Lru�������put������ӽ�������
				CopLruCache<Key, Value>::put(key, value);
			}
		}


	private:
		int k_;//�������������ʷ��¼���뻺����еı�׼
		std::unique_ptr<CopLruCache<Key, size_t>> historyList_;//����������ʷ��¼���У���ֵ���Ӧ�ڵ�ķ��ʴ���ӳ��
	};

	//lru��ϣ�Ż�,��߸߲���ʹ�õ�����
	template<typename Key,typename Value>
	class CopHashLruCache
	{
	public:
		CopHashLruCache(size_t capacity, int sliceNum) :
			capacity_(capacity),
			sliceNum_(sliceNum > 0 ? sliceNum : std::thread::hardware_concurrency())//�����Ƭ��������ͳ�ʼ����Ƭ������ʹ��Ĭ��ֵ
		{
			size_t sliceSize = std::ceil(capacity / static_cast<double> (sliceNum_));//��ȡÿһ����Ƭ�Ĵ�С,����ȡ��
			for (int i = 0; i < sliceNum_; i++) {
				lruSliceCaches_.emplace_back(new CopLruCache<Key, Value>(sliceSize));//���������Ƭ��������һ����С����lru������ӵ�������
			}
		}

		void put(Key key, Value value) {
			//��ȡKey��hashֵ�����������Ƭ����
			size_t sliceIndex = Hash(key) % sliceNum_;
			//Ȼ�����ö�Ӧ������ֵȥ��ȡ��Ӧ�����ָ�룬�Ӷ����øû����put�������޸Ļ����нڵ�Ķ�Ӧֵ
			return lruSliceCaches_[sliceIndex]->put(key, value);
		}
		
		//�жϻ������Ƿ����key��Ӧ�Ľڵ�,valueΪ��������
		bool get(Key key,Value value) {
			size_t sliceIndex = Hash(key) % sliceNum_;
			return lruSliceCaches_[sliceIndex]->get(key, value);
		}

		Value get(Key key) {
			Value value;
			memset(&value, 0, sizeof(value));//��ʼ��ֵ�����ں���ץȡ��Ӧֵ
			get(key, value);
			return value;//���������û�нڵ�ͻ᷵�س�ʼ��ֵ
		}
	private:
		//��keyת���ɶ�Ӧ��hashֵ
		size_t Hash(Key key) {
			std::hash<Key> hashFunc;
			return hashFunc(key);
		}

	private:
		size_t capacity_;//������
		int sliceNum_;//��Ƭ����
		std::vector<std::unique_ptr<CopLruCache<Key, Value>>> lruSliceCaches_;//��ƬLRU��������
	};


}//namespace coloop