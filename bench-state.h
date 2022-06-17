#ifndef BENCH_STATE_H
#define BENCH_STATE_H

#include <jsapi.h>

#include <string>
#include <optional>
#include <fstream>

struct JSEngineState {
    JSContext *cx;
    JS::PersistentRootedObject global;
    JS::PersistentRootedObject module;
    JS::PersistentRootedObject instance;

    JSEngineState(JSContext *cx_) : cx(cx_), global(cx_), module(cx_), instance(cx_) {}
};

struct BenchState {
    typedef void (*TimerCallback)(void *timer);

    std::optional<JSEngineState> js;

    std::string working_dir;
    std::ofstream stdout;
    std::ofstream stderr;
    std::optional<std::ifstream> stdin;
    std::optional<std::string> execution_flags;

    void *compilation_timer;
    TimerCallback compilation_start;
    TimerCallback compilation_end;

    void *instantiation_timer;
    TimerCallback instantiation_start;
    TimerCallback instantiation_end;

    void *execution_timer;
    TimerCallback execution_start;
    TimerCallback execution_end;
};

#endif // BENCH_STATE_H
