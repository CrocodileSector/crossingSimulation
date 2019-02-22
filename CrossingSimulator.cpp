#include <iostream>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <string>
#include <cstdio>
#include <ctime>

static long long c = 0;
static int offset = 0;

/*struct Vehicle
{
	static int id;

	std::mutex m_mutex;

	Vehicle()
	{ 
		std::unique_lock<std::mutex> lock(m_mutex);
		id += 1;
	}
};

int Vehicle::id = 0;*/

class Lane
{
	std::string m_name;
	std::shared_ptr<std::thread> m_runner;
	std::condition_variable m_cv;
	std::mutex m_mutex;
	std::atomic_bool m_run;

	short m_nTimeout;
	int m_accumulator;

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
		m_run = true;

		if(!m_runner)
			m_runner = std::make_shared<std::thread>(&Lane::Run, this);
	}

	void Run()
	{
		if (m_run)
		{
			std::unique_lock<std::mutex> lock(m_mutex);

			std::clock_t start;
			long double duration = 0;

			start = std::clock();

			while (duration < m_nTimeout)
			{
				//Vehicle v[2];

				duration = (std::clock() - start) / (double)CLOCKS_PER_SEC;

				if (static_cast<int>(duration) % 500 == 0)
					c += 2;
			}

			
			m_accumulator = c - offset - m_accumulator;
			offset = c - m_accumulator;

			std::cout << "On " << m_name << " passed " << m_accumulator << " vehicles..." << std::endl;
		}

		OnComplete();
	}

	void OnWait()
	{
		std::unique_lock<std::mutex> lock(m_mutex);

		m_run = false;
		m_cv.wait(lock, std::bind(&Lane::IsRunning, this));
	}

	void OnComplete()
	{
		std::cout << "Locking " << m_name << std::endl;

		OnWait();
	}

	bool IsRunning()
	{
		return m_run;
	}

	void Notify()
	{
		std::unique_lock<std::mutex> lock(m_mutex);

		m_run = true;
		m_cv.notify_one();

		//Run();
	}

	~Lane()
	{
		if (m_runner->joinable())
			m_runner->join();
	}
};

class Crossing
{
public:

	void Start()
	{
		const short noOfLanes = 2;

		std::vector<std::shared_ptr<Lane>> lanes;

		lanes.push_back(std::make_shared<Lane>("Vertical lane"));
		lanes.push_back(std::make_shared<Lane>("Horizontal lane"));

		std::thread laneController
		([&lanes, &noOfLanes]
		{
			bool running = true;
			while (running)
			{
				try
				{
					size_t pos = 0;
					std::shared_ptr<Lane> it = lanes[pos];

					it->Start();

					while (it->IsRunning())
						;

					if (!it->IsRunning())
					{
						if (pos == noOfLanes)
							pos = 0;
						else
							pos++;

						it = lanes[pos];
						it->Notify();
					}
				}
				catch (std::exception &e)
				{
					throw std::runtime_error(e.what());
				}
			}
		
		});

		std::this_thread::sleep_for(std::chrono::minutes(10));

		laneController.join();
		
	}
};



int main()
{
	Crossing c;
	c.Start();
}
