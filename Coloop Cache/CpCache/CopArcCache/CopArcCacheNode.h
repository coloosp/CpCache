#pragma once
# include <memory>

namespace CopCache {
	template <typename Key,typename Value>
	class ArcNode
	{
	private:
		Key key_;
		Value value_;
		size_t accessCount_;//���ڸýڵ�ķ���Ƶ��
		std::shared_ptr<ArcNode> prev_;
		std::shared_ptr<ArcNode> next_;//ǰ��ָ������ָ��
	public:
		//���ι��캯�����޲ι��캯��
		ArcNode():accessCount_(1),prev_(nullptr),next_(nullptr){}

		ArcNode(Key key,Value value)
			:key_(key)
			,value_(value)
			,accessCount_(1)
			,prev_(nullptr)
			,next_(nullptr)
		{}

		//�����ȡ����ֵ������Ƶ�κ���
		Key getKey() const { return key_; }
		Value getValue() const { return value_; }
		size_t getAccessCount() const { return accessCount_; }

		//�����޸�ֵ�����Ƶ�εĺ���
		void setValue(const Value& value) { value_ = value; }
		void incrementAccessCount() { ++accessCount_; }

		//��lru��lfu����Ϊ��Ԫ�࣬���ڷ��ʽڵ���
		template <typename K, typename V> friend class ArcLruPart;
		template <typename K, typename V> friend class ArcLfuPart;

	};







}// coloop