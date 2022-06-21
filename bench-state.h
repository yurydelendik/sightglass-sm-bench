#ifndef BENCH_STATE_H
#define BENCH_STATE_H

#include <jsapi.h>

#include <string>
#include <vector>
#include <optional>
#include <fstream>

struct JSEngineState {
    JSContext *cx;
    JS::PersistentRootedObject global;
    JS::PersistentRootedObject module;
    JS::PersistentRootedObject instance;

    JSEngineState(JSContext *cx_) : cx(cx_), global(cx_), module(cx_), instance(cx_) {}
};

struct FdEntry {
    std::optional<std::fstream> file;
    std::string path;
    bool is_dir;
    uint32_t flags;

    FdEntry() = default;
    FdEntry(std::fstream&& file_) : file(std::move(file_)), path(), is_dir(false) {}
    FdEntry(std::fstream&& file_, std::string&& path_) : file(std::move(file_)), path(path_), is_dir(false) {}
};

const size_t PREOPEN_DIR_FD = 3;

struct BenchState {
    typedef void (*TimerCallback)(void *timer);

    std::optional<JSEngineState> js;

    std::optional<std::string> execution_flags;
    std::vector<std::optional<FdEntry>> fd_table;

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
