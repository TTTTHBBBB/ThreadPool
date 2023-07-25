#include"threadTest.h"

const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_IDLE_TIME = 60;	//��λ����
int Thread::m_generateId = 0;

/*
=========Thread=========
*/
Thread::Thread(ThreadFunc func):m_threadId(m_generateId),m_func(func){
	//���ع��캯������ʼ��
}

void Thread::start()
{//�����߳�
	std::thread t(m_func,m_threadId);
	//������Ϊt���̣߳������Ǻ���m_func
	t.detach();
}

int Thread::getID()const
{//��ȡ�߳�ID
	return m_threadId;
}
/*
=========ThreadPool=========
*/
ThreadPool::ThreadPool()
	:m_initThreadSize(0)//��ʼ�̵߳�����
	, m_taskSize(0)//���������
	, m_idleThreadSize(0)//�����̵߳�����
	, m_curThreadSize(0)//��ǰ�̳߳�����߳�����
	, m_taskqueMaxThresHold(TASK_MAX_THRESHHOLD)//����������޵���ֵ
	, m_threadSizeThreshHold(THREAD_MAX_THRESHHOLD)//�߳��������޵���ֵ
	, m_poolMode(PoolMode::MODE_FIXED)//��ǰ�̵߳Ĺ���ģʽ
	, m_isPoolRunning(false)//��ǰ�̵߳�����״̬
{
}

ThreadPool::~ThreadPool()
{
	m_isPoolRunning = false;

	std::unique_lock<std::mutex>lock(m_taskQueMtx);//��ȡ��

	m_notEmpty.notify_all();//����������ǰ���в��ջ��������������߳�
	m_exitCond.wait(lock, [&]()->bool {return m_threads.size() == 0; });
    //һֱ�ȵ��߳�ȫ�����գ�m_threadsa�߳��б�
}

void ThreadPool::threadFunc(int threadId)
{//�̺߳���
	auto begin = std::chrono::high_resolution_clock().now();
	while (true)
	{
		Task task;
		{
			std::unique_lock<std::mutex>lock(m_taskQueMtx);
			std::cout << "tid:" << std::this_thread::get_id() << "���ڳ��Ի�ȡ����" << std::endl;

			while (m_taskque.size()==0)
			{
				if (!m_isPoolRunning)
				{
					m_threads.erase(threadId);
					std::cout << "thread id"<<std::this_thread::get_id<<"exit" << std::endl;
					m_exitCond.notify_all();
					return;
				}
				if (m_poolMode == PoolMode::MODE_CACHED)
				{
					if (std::cv_status::timeout ==
						m_notEmpty.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - begin);
						if (dur.count() >= THREAD_MAX_IDLE_TIME
							&& m_curThreadSize > m_initThreadSize)
						{
							m_threads.erase(threadId); 
							m_curThreadSize--;
							m_idleThreadSize--;

							std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
								<< std::endl;
							return;
						}
					}
				}
				else
				{
					//�ȴ�empty����
					m_notEmpty.wait(lock);
				}
			}
			m_idleThreadSize--;
			std::cout << "tid:" << std::this_thread::get_id() << "��ȡ����ɹ�" << std::endl;

			task = m_taskque.front();
			m_taskque.pop();
			m_taskSize--;

			if (m_taskque.size() > 0)
			{
				m_notEmpty.notify_all();
			}
			m_notFull.notify_all();
		}
		if (task != nullptr)
		{
			task();
		}
		m_idleThreadSize++;
		begin = std::chrono::high_resolution_clock().now();
	}
}

void ThreadPool::start(int initThreadSize)
{
	m_isPoolRunning = true;
	m_curThreadSize = initThreadSize;
	m_initThreadSize = initThreadSize;

	//��������
	for(int i=1;i<=initThreadSize;i++)
	{
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this,std::placeholders::_1));
		int threadId = ptr->getID();
		m_threads.emplace(threadId,std::move(ptr));
	}
}

bool ThreadPool::checkRunningState()const
{
	return m_isPoolRunning;
}





