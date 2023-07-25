#include"threadTest.h"

const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_IDLE_TIME = 60;	//单位：秒
int Thread::m_generateId = 0;

/*
=========Thread=========
*/
Thread::Thread(ThreadFunc func):m_threadId(m_generateId),m_func(func){
	//重载构造函数，初始化
}

void Thread::start()
{//启动线程
	std::thread t(m_func,m_threadId);
	//创建名为t的线程，任务是函数m_func
	t.detach();
}

int Thread::getID()const
{//获取线程ID
	return m_threadId;
}
/*
=========ThreadPool=========
*/
ThreadPool::ThreadPool()
	:m_initThreadSize(0)//初始线程的数量
	, m_taskSize(0)//任务的数量
	, m_idleThreadSize(0)//空闲线程的数量
	, m_curThreadSize(0)//当前线程池里的线程数量
	, m_taskqueMaxThresHold(TASK_MAX_THRESHHOLD)//任务队列上限的阈值
	, m_threadSizeThreshHold(THREAD_MAX_THRESHHOLD)//线程数量上限的阈值
	, m_poolMode(PoolMode::MODE_FIXED)//当前线程的工作模式
	, m_isPoolRunning(false)//当前线程的启动状态
{
}

ThreadPool::~ThreadPool()
{
	m_isPoolRunning = false;

	std::unique_lock<std::mutex>lock(m_taskQueMtx);//获取锁

	m_notEmpty.notify_all();//条件变量当前队列不空唤醒所有阻塞的线程
	m_exitCond.wait(lock, [&]()->bool {return m_threads.size() == 0; });
    //一直等到线程全部回收，m_threadsa线程列表
}

void ThreadPool::threadFunc(int threadId)
{//线程函数
	auto begin = std::chrono::high_resolution_clock().now();
	while (true)
	{
		Task task;
		{
			std::unique_lock<std::mutex>lock(m_taskQueMtx);
			std::cout << "tid:" << std::this_thread::get_id() << "正在尝试获取任务" << std::endl;

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
					//等待empty条件
					m_notEmpty.wait(lock);
				}
			}
			m_idleThreadSize--;
			std::cout << "tid:" << std::this_thread::get_id() << "获取任务成功" << std::endl;

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

	//创建对象
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





