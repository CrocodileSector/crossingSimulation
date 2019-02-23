#include <mutex>
#include <ctime>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <cstdio>
#include <sstream>
#include <iostream>
#include <condition_variable>

static long long c = 0;
static long long offset = 0;

class Lane
{
	std::shared_ptr<std::thread> m_runner;
	std::stringstream m_msgBuffer;
	std::condition_variable m_cv;
	std::atomic_bool m_run;
	std::string m_name;
	std::mutex m_mutex;
	long long m_accumulator;
	short m_nTimeout;

public:

	Lane() : m_name("Default lane"), m_accumulator(0), m_nTimeout(60) { }

	Lane(const std::string& name) : m_name(name), m_accumulator(0), m_nTimeout(10), m_run(true) { }

	Lane(const Lane &o) = delete;

	Lane& operator = (const Lane &o) = delete;

	Lane(const Lane &&o) : m_runner(std::move(o.m_runner)), m_name(std::move(o.m_name)), m_accumulator(o.m_accumulator) { }

	Lane& operator = (const Lane && o)
	{
		if (m_runner->joinable())
			m_runner->join();

		m_runner.reset(o.m_runner.get());
		m_name = std::move(o.m_name);
		m_accumulator = o.m_accumulator;

		return *this;
	}

	void Start()
	{
		if (!m_runner)
			m_runner = std::make_unique<std::thread>(&Lane::Run, this);
	}

	void Run()
	{
		if (m_run)
		{
			std::unique_lock<std::mutex> lock(m_mutex);

			m_msgBuffer << "Starting " << m_name << std::endl;

			std::clock_t start;
			long double duration = 0;

			start = std::clock();

			while (duration < m_nTimeout)
			{
				duration = (std::clock() - start) / (double)CLOCKS_PER_SEC;

				if (static_cast<int>(duration) % 2000 == 0)
					c += 2;
			}
			
			m_accumulator = c - offset - m_accumulator;
			offset = c - m_accumulator;

			m_msgBuffer << "On " << m_name << " passed " << m_accumulator << " vehicles..." << std::endl;
		}

		OnComplete();
	}

	void OnWait()
	{
		std::unique_lock<std::mutex> lock(m_mutex);

		m_run = false;
		m_cv.wait(lock, std::bind(&Lane::IsRunning, this));
	}

	void Poll(std::string& in_msg)
	{
		in_msg = std::string(m_msgBuffer.str());

		m_msgBuffer.str().clear();
		m_msgBuffer.clear();
	}

	void OnComplete()
	{
		m_msgBuffer << "Stoping " << m_name << std::endl;

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
	}

	~Lane()
	{
		if (m_runner->joinable())
			m_runner->join();
	}
};

class Crossing
{
	typedef std::vector<std::shared_ptr<Lane>> Lanes;

	Lanes m_lanes;
	const short noOfLanes;
	std::thread m_laneController;
	std::mutex m_mutex;

public:
	Crossing() : noOfLanes(2)
	{
		m_lanes.push_back(std::make_shared<Lane>("Vertical lane"));
		m_lanes.push_back(std::make_shared<Lane>("Horizontal lane"));
	}

	void Start()
	{
		std::thread laneController(std::bind(&Crossing::Run, this));

		WaitForMessages();

		laneController.join();
	}

	void Run()
	{
		size_t pos = 0;
		bool running = true;
		while (running)
		{
			try
			{
				if (pos == noOfLanes)
					pos = 0;

				std::shared_ptr<Lane> it;
				{
					std::unique_lock<std::mutex> lock(m_mutex);
					it = m_lanes[pos];
				}

				it->Start();

				while (it->IsRunning())
					;

				if (!it->IsRunning())
				{
					pos++;

					if (pos == noOfLanes)
						pos = 0;

					std::shared_ptr<Lane> it;
					{
						std::unique_lock<std::mutex> lock(m_mutex);
						it = m_lanes[pos];
					}

					if (!it->IsRunning())
						it->Notify();
				}
			}
			catch (std::exception &e)
			{
				throw std::runtime_error(e.what());
			}
		}
	}

	void WaitForMessages()
	{
		size_t i = 0;
		while (true)
		{
			std::this_thread::sleep_for(std::chrono::seconds(10));

			if (i == noOfLanes)
				i = 0;

			std::string msg;
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				m_lanes[i]->Poll(msg);
			}

			std::cout << msg << std::endl;

			i++;
		}
	}
};

int main()
{
	Crossing c;
	c.Start();
}
