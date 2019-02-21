#include <iostream>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <string>
#include <cstdio>
#include <ctime>

struct Vehicle
{
	static int id;

	std::mutex m_mutex;

	Vehicle()
	{ 
		std::unique_lock<std::mutex> lock(m_mutex);
		id += 1;
	}
};

int Vehicle::id = 0;

class Lane
{
	std::string m_name;
	std::shared_ptr<std::thread> m_runner;
	std::condition_variable m_cv;
	std::mutex m_mutex;
	std::atomic_bool m_runSignal;

	short m_nTimeout;
	int m_accumulator;
	
	class Lane(const class Lane &a, const std::unique_lock<std::mutex> &)
		: m_accumulator(a.m_accumulator), m_name(std::string(a.m_name.begin(), a.m_name.end())), m_nTimeout(a.m_nTimeout)
	{

	}

public:

	Lane() : m_name("Default lane"), m_accumulator(0), m_nTimeout(60) { }

	Lane(const std::string& name) : m_name(name), m_accumulator(0), m_nTimeout(60) { }

	Lane(const Lane &o) = delete;

	Lane& operator = (const Lane &o) = delete;

	Lane(const Lane &&o) : m_runner(std::move(o.m_runner)), m_name(std::move(o.m_name)), m_accumulator(o.m_accumulator) { }

	Lane& operator  = (const Lane && o)
	{
		if (m_runner->joinable())
			m_runner->join();

		m_runner = std::move(o.m_runner);
		m_name = std::move(o.m_name);
		m_accumulator = o.m_accumulator;

		return *this;
	}

	void Start()
	{
		m_runSignal = true;

		if (m_runner)
			m_runner.reset(&std::thread(&Lane::Run, this));
	}

	void Run()
	{
		if (m_runSignal)
		{
			std::unique_lock<std::mutex> lock(m_mutex);

			std::clock_t start;
			double duration;

			start = std::clock();

			while (duration != m_nTimeout)
			{
				Vehicle v[2];

				duration = (std::clock() - start) / (double)CLOCKS_PER_SEC;
			}

			m_accumulator += Vehicle::id - m_accumulator;

			std::cout << "On lane " << m_name << " passed " << m_accumulator << " vehicles..." << std::endl;
		}

		OnComplete();
	}

	void OnWait()
	{
		std::unique_lock<std::mutex> lock(m_mutex);

		m_runSignal = false;
		m_cv.wait(lock, std::bind(&Lane::IsDone, this));
	}

	void OnComplete()
	{
		std::unique_lock<std::mutex> lock(m_mutex);

		std::cout << "Locking lane " << m_name << std::endl;

		OnWait();
	}

	bool IsDone()
	{
		std::unique_lock<std::mutex> lock(m_mutex);

		return !m_runSignal;
	}

	void Notify()
	{
		std::unique_lock<std::mutex> lock(m_mutex);

		m_runSignal = true;
		m_cv.notify_one();

		Run();
	}
};

class Crossing
{
public:

	void Start()
	{
		const short noOfLanes = 2;
		Lane lanes[noOfLanes]{ { "Vertical lane" }, { "Horizontal lane" } };

		bool running = true;
		while (running)
		{
			size_t it = 0;
			std::shared_ptr<Lane> lp = std::make_shared<Lane>(lanes[it]);

			lp->Start();

			while (!lp->IsDone())
				; 

			if (lp->IsDone())
			{
				if (it == noOfLanes)
					it = 0;
				else
					it++;

				lp = std::make_shared<Lane>(lanes[it]);
				lp->Notify();
			}
			
		}
	}
};



int main()
{
	Crossing c;
	c.Start();
}
