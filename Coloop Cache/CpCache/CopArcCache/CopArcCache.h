#pragma once

//������һ���ļ��е�ͷ�ļ�
#include"../CopCachePolicy.h"
#include"CopArcLruPart.h"
#include"CopArcLfuPart.h"
#include<memory>

namespace CopCache
{
	template <typename Key,typename Value>
	class CopArcCache : public CopCachePolicy <Key, Value>
	{
	public:
		explicit CopArcCache(size_t capacity=10,size_t transformThreshold =2)
		
			:capacity_(capacity)
			, transformThreshold_(transformThreshold)
			,lruPart_(std::make_unique<ArcLruPart<Key,Value>> (capacity,transformThreshold))
			,lfuPart_(std::make_unique<ArcLfuPart<Key,Value>>(capacity,transformThreshold))
		{}

		~CopArcCache() override = default;


		void put(Key key, Value value) override
		{
			bool inGhost = checkGhostCaches(key);

			//�����ʾ�������У���Ҫ������ı����Ի����������㺬�壬��һ�����л��棬��lfu�в����ڣ���lru���룬lfu���롣
			//�ڶ������л��棬lfu��Ҳ���ڸ����ݣ�����¸����ݣ�lru���ɷ���(������put�����и��ºͷ�����������)
			if (!inGhost)
			{
				if (lruPart_->put(key, value))
				{
					lfuPart_->put(key, value);
				}
			}

			//����������У����������Ӧ�÷���lru�У�������Ϊ�������е����ݷ��ʴ���ֻ����1��lfu��Ҫһ�����ʴ������ܽ���
			//����������checkGhostCaches�������Ѿ��ж���ôȥ�������棬���Բ��õ���
			else
			{
				lruPart_->put(key, value);
			}
		}

		bool get(Key key, Value& value) override
		{
			checkGhostCaches(key);

			bool shouldTransform = false;

			if (lruPart_->get(key, value, shouldTransform))
			{
				//�����lru����ĸýڵ��ڷ���ʱ����ת����ֵ����ʱ��Ҫ��lfu���������
				if (shouldTransform)
				{
					lfuPart_->put(key, value);
				}
				return true;
			}
			return lfuPart_->get(key, value);
		}

		Value get(Key key) override
		{
			Value value{};
			get(key, value);
			return value;
		}


	private:
		//������黺�棬�۲�����������黺��������
		bool checkGhostCaches(Key key)
		{
			//����Ƿ�ڵ��������黺��
			bool inGhost = false;
			//�����lru���У������lru�Ļ�������������lfu������,ע��������Ҫlfu��ȷ����������������lru��ߣ���Ϊ��������
			if (lruPart_->checkGhost(key))
			{
				if (lfuPart_->decreaseCapacity())
				{
					lruPart_->increaseCapacity();
				}
				inGhost = true;
			}
			//��֮һ��
			else if (lfuPart_->checkGhost(key))
			{
				if (lruPart_->decreaseCapacity())
				{
					lfuPart_->increaseCapacity();
				}
				inGhost = true;
			}
			return inGhost;
		}


	private:
		size_t capacity_;
		size_t transformThreshold_;
		std::unique_ptr<ArcLruPart<Key, Value>> lruPart_;
		std::unique_ptr<ArcLfuPart<Key, Value>> lfuPart_;


	};




}// coloop