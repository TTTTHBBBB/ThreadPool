#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <thread>
#include <future>
#include <iostream>

//线程池支持的模式
enum class PoolMode
{
	MODE_FIXED,//固定数量的线程
	MODE_CACHED,//线程数量可以动态增长
};
//线程类型
class Thread {
public:
	//定义线程函数的传参类型->回调函数
	using ThreadFunc = std::function<void(int)>;
	Thread(ThreadFunc func);
    ~Thread()=default;

	void start();//启动线程
	int getID() const;
	/*
     获取线程ID,
	 函数名后加const,只读函数不能修改类内成员否则报错
	 函数名前加const,返回值为const
	*/
private:
	ThreadFunc m_func;
	static int m_generateId;
	int m_threadId;//保存线程ID
};

//线程池类型
class ThreadPool {
public:
	ThreadPool();
	~ThreadPool();
	
	//设置线程池的工作模式
	void setModle(PoolMode mode);
	//设置task任务队列上限的阈值
	void setTaskQueMaxThrshHold(int thrshhold);
	//设置线程池cached模式下线程阈值
	void setThraedSizeThrshHold(int thrshhold);

	//给线程池提交任务
	template<typename Func,typename... Args>
	auto submit(Func&& func, Args&&...args)->std::future<decltype(func(args...))>
	{
		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>(
		std::bind(std::forward<Func>(func),std::forward<Args>(args)...));
		std::future<RType>result = task->get_future;
		std::unique_lock<std::mutex> lock(m_taskQueMtx);
		//上锁
		if (m_notFull.wait_for(lock, std::chrono::seconds(1),
			[&]()->bool {return m_taskque.size() < (size_t)m_taskqueMaxThresHold; }))
		{
			std::cerr << "task queue is full, submit task failed." << std::endl;
			auto task = std::make_shared<std::packaged_task<RType()>>(
				[]()->RType {return RType(); });
			(*task)();
			return task->get_future();
		}
		m_taskque.emplace([task]() {(*task)(); });
		m_taskSize++;

		if (m_poolMode==PoolMode::MODE_CACHED && m_taskSize > m_idleThreadSize
			&& m_curThreadSize<m_threadSizeThreshHold)
		/*
		 当前模式为cached模式
		 当前任务的数量大于空闲线程的数量
		 当前线程池里线程的总数量小于线程的阈值上限
		*/
		{
			std::cout << ">>> create new thread..." << std::endl;
			auto ptr = std::unique_ptr<Thread>(
				std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			//创建新的线程对象
			int threadId = ptr->getId();
			m_threads.emplace(threadId, std::move(ptr));
			//启动线程
			m_threads[threadId]->start();
			//修改线程个数相关的变量
			m_curThreadSize++;
			m_idleThreadSize++;
		}
	}
	//开启线程池
	void start(int initThreadSize=std::thread::hardware_concurrency());
	//禁用拷贝构造函数
	ThreadPool(const ThreadPool &) = delete;
	//禁用赋值运算符重载函数
	ThreadPool& operator = (const ThreadPool &) = delete;

private:
	//定义线程函数
	void threadFunc(int thbreadId);
	//检测pool的运行状态
	bool checkRunningState() const;
private:
	//定义一个哈希map存储智能指针，指针类型为Thread类
	std::unordered_map<int, std::unique_ptr<Thread>> m_threads;
	//初始的线程数量
	int m_initThreadSize;

	//记录当前线程池里面线程的总数量
	std::atomic_int m_curThreadSize;

	//线程数量上限阈值
	int m_threadSizeThreshHold;

	//记录空闲线程的数量
	std::atomic_int m_idleThreadSize;

	//Task任务 函数对象
	using Task = std::function<void()>;

	//任务队列
	std::queue<Task> m_taskque;

	//任务的数量
	std::atomic_int m_taskSize;
	//任务队列数量上限的阈值
	int m_taskqueMaxThresHold;

	//包装任务队列的线程安全
	std::mutex m_taskQueMtx;

	//表示任务队列不满
	std::condition_variable m_notFull;
	//表示任务队列不空
	std::condition_variable m_notEmpty;

	//表示等待线程资源全部回收
	std::condition_variable m_exitCond;

	//当前线程池的工作模式
	PoolMode m_poolMode;

	//表示当前线程池的启动状态
	std::atomic_bool m_isPoolRunning;
};
#endif 
