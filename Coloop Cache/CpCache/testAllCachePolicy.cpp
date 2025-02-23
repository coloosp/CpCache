#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <iomanip>
#include <random>
#include <algorithm>

#include "CopCachePolicy.h"
#include "CopLfuCache.h"
#include "CopLruCache.h"
#include "CopArcCache/CopArcCache.h"

//��ʱ��
class Timer {
public:
	Timer(): start_(std::chrono::high_resolution_clock::now())
	{}
	//����Ӹö��󵽵���elapsed������ʱ���������غ��뼶��ľ���
	double elapsed() {
		auto now = std::chrono::high_resolution_clock::now();//��¼�����������ʱ��ʱ���
		return std::chrono::duration_cast<std::chrono::milliseconds> (now - start_).count();
	}

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> start_;//�洢��ʱ������ʼʱ���


};

//������������ӡ���
void printResult(const std::string& testName, int capacity,
	const std::vector<int>& get_operations,
	const std::vector<int>& hits)
{
	//��������ʣ�С���㱣����λ
	std::cout << "cache size: " << capacity << std::endl;
	std::cout << "LRU - Hit rate: " << std::fixed << std::setprecision(2)
		<< (100.0 * hits[0] / get_operations[0]) << "%" << std::endl;
	std::cout << "LFU - Hit rate: " << std::fixed << std::setprecision(2)
		<< (100.0 * hits[1] / get_operations[1]) << "%" << std::endl;
	std::cout << "ARC - Hit rate: " << std::fixed << std::setprecision(2)
		<< (100.0 * hits[2] / get_operations[2]) << "%" << std::endl;
}

void testHotDataAccess() {
	std::cout << "\n=== Test scenario 1: Testing hotspot data access ===" << std::endl;
	
	const int CAPACITY = 50; //���ӻ�������
	const int OPERATIONS = 500000; //���Ӳ�������
	const int HOT_KEYS = 20; //�����ȵ����ݵ��������ȵ����ݷ�Χ
	const int COLD_KEYS = 5000; //�����ݷ�Χ

	CopCache::CopLruCache<int, std::string> lru(CAPACITY);
	CopCache::CopLfuCache<int, std::string> lfu(CAPACITY);
	CopCache::CopArcCache<int, std::string> arc(CAPACITY);
	
	std::random_device rd;//��������������������������������
	std::mt19937 gen(rd());//������������α�������


	std::array<CopCache::CopCachePolicy<int, std::string>*, 3> caches = { &lru,&lfu,&arc };
	std::vector<int> hits(3, 0);
	std::vector<int> get_operations(3, 0);

	//������������
	for (int i = 0; i < caches.size(); ++i)
	{
		//�Ƚ���һϵ��put����
		for (int op = 0; op < OPERATIONS; ++op) {
			int key;
			if (op % 100 < 70) {//70%�ȵ�����
				key = gen() % HOT_KEYS;
			}
			else { //30%������
				key = HOT_KEYS + (gen() % COLD_KEYS);
			}
			std::string value = "value" + std::to_string(key);
			caches[i]->put(key, value);
		}

		//Ȼ����������get����
		for (int get_op = 0; get_op < OPERATIONS; ++get_op) {
			int key;
			if (get_op % 100 < 70) { //70%���ʷ����ȵ�
				key = gen() % HOT_KEYS;
			}
			else { //30%���ʷ���������
				key = HOT_KEYS + (gen() % COLD_KEYS);
			}

			std::string result;//��������
			get_operations[i]++;//��Ӧ������������
			//�ж��ڻ������Ƿ���ڶ�Ӧkeyֵ,���������м�һ
			if (caches[i]->get(key, result)) {
				hits[i]++;
			}
		}
	}
	printResult("Hotspot data access test", CAPACITY, get_operations, hits);
}

void testLoopPattern() {
	std::cout << "\n=== Test scenario 2: Cyclic scan test ===" << std::endl;

	const int CAPACITY = 50;//���ӻ�������
	const int LOOP_SIZE = 500;
	const int OPERATIONS = 200000;//���Ӳ�������

	CopCache::CopLruCache<int, std::string> lru(CAPACITY);
	CopCache::CopLfuCache<int, std::string> lfu(CAPACITY);
	CopCache::CopArcCache<int, std::string> arc(CAPACITY);

	std::array<CopCache::CopCachePolicy<int, std::string>*, 3> caches = { &lru,&lfu,&arc };
	std::vector<int> hits(3, 0);
	std::vector<int> get_operations(3, 0);

	std::random_device rd;//��������������������������������
	std::mt19937 gen(rd());//������������α�������

	//���������
	for (int i = 0; i < caches.size(); ++i) {
		for (int key = 0; key < LOOP_SIZE; ++key) {//ֻ�����LOOP_SIZE��Χ������
			std::string value = "loop" + std::to_string(key);
			caches[i]->put(key, value);
		}

		//Ȼ����з��ʲ���
		int current_pos = 0;
		for (int op = 0; op < OPERATIONS; ++op) {
			int key;
			if (op % 100 < 60) { //60% ˳��ɨ��
				key = current_pos;
				current_pos = (current_pos + 1) % LOOP_SIZE;
			}
			else if (op % 100 < 90) {//30% �����Ծ
				key = gen() % LOOP_SIZE;
			}
			else { //10%Ϊ���ʷ�Χ������
				key = LOOP_SIZE + (gen() % LOOP_SIZE);
			}

			std::string result;//��������
			get_operations[i]++;
			if (caches[i]->get(key, result)) {
				hits[i]++;
			}

		}
	}

	printResult("Cyclic scan test", CAPACITY, get_operations, hits);
}

void testWorkloadShift() {
	std::cout << "\n=== Test Scenario 3: Workload change test ===" << std::endl;

	const int CAPACITY = 4;//����������С
	const int OPERATIONS = 80000;//������������
	const int PHASE_LENGTH = OPERATIONS / 5;//�зֲ�������Ϊ��ͬģʽ

	CopCache::CopLruCache<int, std::string> lru(CAPACITY);
	CopCache::CopLfuCache<int, std::string> lfu(CAPACITY);
	CopCache::CopArcCache<int, std::string> arc(CAPACITY);

	std::random_device rd;
	std::mt19937 gen(rd());

	std::array<CopCache::CopCachePolicy<int, std::string>*, 3> caches = { &lru,&lfu,&arc};
	std::vector<int> hits(3, 0);
	std::vector<int> get_operations(3, 0);

	//���һЩ��ʼ����
	for (int i = 0; i < caches.size(); ++i) {
		for (int key = 0; key < 1000; ++key) {
			std::string value = "init" + std::to_string(key);
			caches[i]->put(key, value);
		}

		//Ȼ����ж�׶β���
		for (int op = 0; op < OPERATIONS; ++op) {
			int key;
			//���ݲ�ͬ�׶�ѡ��ͬ�ķ���ģʽ
			if (op < PHASE_LENGTH) {//�ȵ����
				key = gen() % 5;
			}
			else if (op < PHASE_LENGTH * 2) {//��Χ���
				key = gen() % 1000;
			}
			else if (op < PHASE_LENGTH * 3) {//˳��ɨ��
				key = (op - PHASE_LENGTH * 2) % 100;
			}
			else if (op < PHASE_LENGTH * 4) {
				int locality = (op / 1000) % 10;
				key = locality * 20 + (gen() % 20);
			}
			else { //���ɨ��
				int r = gen() % 100;
				if (r < 30) {
					key = gen() % 5;//�ȵ�ɨ��
				}
				else if (r < 60) {
					key = 5 + (gen() % 95);//С��Χɨ��
				}
				else {
					key = 100 + (gen() % 900);//��Χɨ��
				}
			}

			std::string result;
			get_operations[i]++;
			if (caches[i]->get(key, result)) {
				hits[i]++;
			}

			//�������put���������»�������
			if (gen() % 100 < 30) { //30%���ʽ���put
				std::string value = "new" + std::to_string(key);
				caches[i]->put(key, value);
			}
		}
	}

	printResult("Drastic changes in workload testing", CAPACITY, get_operations, hits);
}

int main() {
	testHotDataAccess();//�ȵ����ݲ���
	testLoopPattern();//ѭ��ɨ�����
	testWorkloadShift();//�������ؾ��ұ仯����
	std::cout << "Oh, it's finally done!>w<"<<std::endl;
	return 0;
}