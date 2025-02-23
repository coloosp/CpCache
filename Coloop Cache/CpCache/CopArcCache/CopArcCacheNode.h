#pragma once
# include <memory>

namespace CopCache {
	template <typename Key,typename Value>
	class ArcNode
	{
	private:
		Key key_;
		Value value_;
		size_t accessCount_;//对于该节点的访问频次
		std::shared_ptr<ArcNode> prev_;
		std::shared_ptr<ArcNode> next_;//前驱指针与后继指针
	public:
		//含参构造函数与无参构造函数
		ArcNode():accessCount_(1),prev_(nullptr),next_(nullptr){}

		ArcNode(Key key,Value value)
			:key_(key)
			,value_(value)
			,accessCount_(1)
			,prev_(nullptr)
			,next_(nullptr)
		{}

		//定义获取键，值，访问频次函数
		Key getKey() const { return key_; }
		Value getValue() const { return value_; }
		size_t getAccessCount() const { return accessCount_; }

		//定义修改值与访问频次的函数
		void setValue(const Value& value) { value_ = value; }
		void incrementAccessCount() { ++accessCount_; }

		//将lru与lfu定义为友元类，便于访问节点类
		template <typename K, typename V> friend class ArcLruPart;
		template <typename K, typename V> friend class ArcLfuPart;

	};







}// coloop