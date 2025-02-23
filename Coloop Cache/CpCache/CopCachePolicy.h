#pragma once
namespace CopCache {//�޶���CopCache���ֿռ�
	//ģ��
	template <typename Key, typename Value>
	class CopCachePolicy {//������
	public:
		//����
		virtual ~CopCachePolicy() {};
		
		//��ӻ���ӿڣ����麯��
		virtual void put(Key key, Value value) = 0;

		//Key �Ǵ���Ĳ��� ���ʳɹ��򷵻�true�����޸Ĵ����valueֵ
		virtual bool get(Key key, Value& value) = 0;

		//���غ�����������ڻ������ҵ�key�����ض�Ӧvalue
		virtual Value get(Key key) = 0;
	};

}// coloop