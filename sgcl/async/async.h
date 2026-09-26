//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The async module: coroutines and generators, the scheduler, executors and strands, task-local values, channels, broadcasts and select, timers and the clock, timeouts, the signals of the process, stop tokens, the synchronization of tasks, task groups, promises, the blocking pool, the reactor.
#pragma once

#include "generator.h"
#include "blocking.h"
#include "broadcast.h"
#include "channel.h"
#include "condition_variable.h"
#include "coroutine.h"
#include "event.h"
#include "executor.h"
#include "mutex.h"
#include "once.h"
#include "promise.h"
#include "reactor.h"
#include "scheduler.h"
#include "select.h"
#include "semaphore.h"
#include "shared_mutex.h"
#include "signal.h"
#include "stop_token.h"
#include "task_group.h"
#include "task_local.h"
#include "timeout.h"
#include "timer.h"
#include "wait_group.h"
#include "when.h"
