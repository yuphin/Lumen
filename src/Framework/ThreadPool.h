#pragma once

// Thread Pool
namespace tp {

using JobProcedure = void (*)(void*);

struct Job {
	JobProcedure procedure = nullptr;
	void* data = nullptr;
};

struct JobCounter {
	u32 remaining = 0;
};

void init();
void destroy();
void submit(Job job, JobCounter& counter);
void wait(JobCounter& counter);

}  // namespace tp
