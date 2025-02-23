#pragma once
namespace CopCache {//限定在CopCache名字空间
	//模板
	template <typename Key, typename Value>
	class CopCachePolicy {//抽象类
	public:
		//析构
		virtual ~CopCachePolicy() {};
		
		//添加缓存接口，纯虚函数
		virtual void put(Key key, Value value) = 0;

		//Key 是传入的参数 访问成功则返回true，并修改传入的value值
		virtual bool get(Key key, Value& value) = 0;

		//重载函数，如果能在缓存中找到key，返回对应value
		virtual Value get(Key key) = 0;
	};

}// coloop