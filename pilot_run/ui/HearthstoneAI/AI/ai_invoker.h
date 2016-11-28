#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <list>
#include <functional>

#include "json/value.h"
#include "game-ai/game-ai.h"
#include "game-ai/task.h"

class AIInvoker
{
public:
	AIInvoker();

	bool Initialize();

	void CreateNewTask(Json::Value const& game);
	void CancelAllTasks();
	void GenerateCurrentBestMoves();
	void BoardActionStart(Json::Value const& game);

	void Cleanup();

private:
	enum State
	{
		STATE_NOT_SPAWNED,
		STATE_RUNNING,
		STATE_STOPPED
	};

	class Job
	{
	public:
		virtual ~Job() {}
	};

	class NewGameJob : public Job
	{
	public:
		Json::Value game;
	};

	class ActionStartJob : public Job
	{
	public:
		Json::Value game;
	};

private: // non-thread-safe functions
	void MainLoop();
	static void ThreadCallBack(AIInvoker * instance);

	void ProcessPendingOperations();

	void WaitCurrentJob();
	void HandleCurrentJob();
	void StopCurrentJob();

	void HandleNewJob(Job * job);
	void HandleNewJob(NewGameJob * job);
	void HandleNewJob(ActionStartJob * job);

	void HandleJob(NewGameJob * job);

	void GenerateCurrentBestMoves_Internal();
	void InitializeTasks(Json::Value const& game);
	void UpdateBoard(Json::Value const& game);

private: // thread-safe functions
	void SetState(State state);
	State GetState();

	void DiscardAndSetPendingJob(Job * new_job);
	Job * GetAndClearPendingJob();

private:
	std::thread main_thread;

	std::mutex mtx_state;
	State state;

	std::mutex mtx_pending_job;
	Job * pending_job;

	std::mutex mtx_pending_operations;
	std::list<std::function<void(AIInvoker*)>> pending_operations;

	std::unique_ptr<Job> current_job;

	bool abort_flag;
	bool flag_generate_best_move;
	bool running;

	std::chrono::time_point<std::chrono::steady_clock> last_report_iteration_count;

private: // context for current job
	std::vector<MCTS*> mcts;
	std::vector<Task*> tasks;
	std::map<Task*, std::unique_ptr<Task::Notifier>> task_done_notifiers;
};