#ifndef SM_BENCH_H
#define SM_BENCH_H

#include <jsapi.h>

typedef void (*TimerCallback)(void *timer);

typedef int ExitCode;
const ExitCode BENCH_EXIT_OK = 0;
const ExitCode BENCH_EXIT_ERR = -1;

struct WasmBenchConfig {
    /// The working directory where benchmarks should be executed.
    const char *working_dir_ptr;
    size_t working_dir_len;

    /// The file path that should be created and used as `stdout`.
    const char *stdout_path_ptr;
    size_t stdout_path_len;

    /// The file path that should be created and used as `stderr`.
    const char *stderr_path_ptr;
    size_t stderr_path_len;

    /// The (optional) file path that should be opened and used as `stdin`. If
    /// not provided, then the WASI context will not have a `stdin` initialized.
    const char *stdin_path_ptr;
    size_t stdin_path_len;

    /// The functions to start and stop performance timers/counters during Wasm
    /// compilation.
    void *compilation_timer;
    TimerCallback compilation_start;
    TimerCallback compilation_end;

    /// The functions to start and stop performance timers/counters during Wasm
    /// instantiation.
    void *instantiation_timer;
    TimerCallback instantiation_start;
    TimerCallback instantiation_end;

    /// The functions to start and stop performance timers/counters during Wasm
    /// execution.
    void *execution_timer;
    TimerCallback execution_start;
    TimerCallback execution_end;

    /// The (optional) flags to use when running Wasmtime. These correspond to
    /// the flags used when running Wasmtime from the command line.
    const char *execution_flags_ptr;
    size_t execution_flags_len;  
};

extern "C" ExitCode wasm_bench_create(WasmBenchConfig config, void **out_bench_pt)
  __attribute__((visibility("default")));

extern "C" ExitCode wasm_bench_free(void *state)
  __attribute__((visibility("default")));

extern "C" ExitCode wasm_bench_compile(void *state, const char *wasm_bytes, size_t wasm_bytes_length)
  __attribute__((visibility("default")));

extern "C" ExitCode wasm_bench_instantiate(void *state)
  __attribute__((visibility("default")));

extern "C" ExitCode wasm_bench_execute(void *state)
  __attribute__((visibility("default")));

void bench_init() __attribute__((constructor));
void bench_fini() __attribute__((destructor));

#endif // SM_BENCH_H