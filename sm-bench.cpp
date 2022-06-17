#include <jsapi.h>
#include <jsfriendapi.h>

#include <js/Initialization.h>
#include <js/Exception.h>
#include <js/Context.h>
#include <js/ContextOptions.h>
#include <js/CompilationAndEvaluation.h>
#include <js/SourceText.h>
#include <js/WasmModule.h>
#include <js/ArrayBuffer.h>

#include <memory>
#include <string>
#include <fstream>

#include "sm-bench.h"
#include "wasi-imports.h"
#include "bench-state.h"


static JSObject* CreateGlobal(JSContext* cx) {
  JS::RealmOptions options;

  static JSClass SmBenchGlobal = {
      "SmBenchGlobal",
      JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_RESERVED_SLOTS(1),
      &JS::DefaultGlobalClassOps};

  return JS_NewGlobalObject(cx, &SmBenchGlobal, nullptr,
                            JS::FireOnNewGlobalHook, options);
}

static void ReportAndClearException(JSContext* cx) {
  JS::ExceptionStack stack(cx);
  if (!JS::StealPendingExceptionStack(cx, &stack)) {
    fprintf(stderr, "Uncatchable exception thrown, out of memory or something");
    exit(1);
  }

  JS::ErrorReportBuilder report(cx);
  if (!report.init(cx, stack, JS::ErrorReportBuilder::WithSideEffects)) {
    fprintf(stderr, "Couldn't build error report");
    exit(1);
  }

  JS::PrintError(stderr, report, true);
}

/// Exposes a C-compatible way of creating the engine from the bytes of a single
/// Wasm module.
///
/// On success, the `out_bench_ptr` is initialized to a pointer to a structure
/// that contains the engine's initialized state, and `0` is returned. On
/// failure, a non-zero status code is returned and `out_bench_ptr` is left
/// untouched.
ExitCode wasm_bench_create(WasmBenchConfig config, void **out_bench_pt)
{
  auto bench = std::make_unique<BenchState>();
  bench->working_dir = std::string(config.working_dir_ptr, config.working_dir_len);
  bench->stdout.open(std::string(config.stdout_path_ptr, config.stdout_path_len));
  bench->stderr.open(std::string(config.stderr_path_ptr, config.stderr_path_len));
  if (config.stdin_path_ptr) {
    bench->stdin = std::ifstream(std::string(config.stdin_path_ptr, config.stdin_path_len));
  }
  if (config.execution_flags_ptr) {
    bench->execution_flags = std::string(config.execution_flags_ptr, config.execution_flags_len);
  }
  bench->compilation_timer = config.compilation_timer;
  bench->compilation_start = config.compilation_start;
  bench->compilation_end = config.compilation_end;
  bench->instantiation_timer = config.instantiation_timer;
  bench->instantiation_start = config.instantiation_start;
  bench->instantiation_end = config.instantiation_end;
  bench->execution_timer = config.execution_timer;
  bench->execution_start = config.execution_start;
  bench->execution_end = config.execution_end;

  JSContext* cx = JS_NewContext(JS::DefaultHeapMaxBytes);
  if (!cx) {
    return BENCH_EXIT_ERR;
  }

  if (!JS::InitSelfHostedCode(cx)) {
    return BENCH_EXIT_ERR;
  }

  bool enableIon = true;
  bool enableBaseline = false;
  if (bench->execution_flags) {
    const std::string& flags = bench->execution_flags.value();
    if (flags == "baseline") {
      enableIon = false;
      enableBaseline = true;
    } else if (flags == "tier") {
      enableBaseline = true;
    }
  }

  JS::ContextOptionsRef(cx)
    .setWasm(true)
    .setWasmBaseline(enableBaseline)
    .setWasmIon(enableIon);

  JS::RootedObject global(cx, CreateGlobal(cx));
  if (!global) {
    return BENCH_EXIT_ERR;
  }
  JS_SetReservedSlot(global, 0, JS::PrivateValue(bench.get()));

  bench->js.emplace(cx);
  bench->js->global = global;

  *out_bench_pt = bench.release();
  return BENCH_EXIT_OK;
}

/// Free the engine state allocated by this library.
ExitCode wasm_bench_free(void *state)
{
  std::unique_ptr<BenchState> bench(static_cast<BenchState*>(state));

  JSContext *cx = bench->js->cx;
  bench->js.reset();
  JS_DestroyContext(cx);
  bench.reset();

  return BENCH_EXIT_OK;
}

static JSObject* GetWasm(JSContext *cx, JS::HandleObject global)
{
  JS::RootedValue wasm(cx);
  if (!JS_GetProperty(cx, global, "WebAssembly", &wasm)) return nullptr;
  return &wasm.toObject();
}

/// Compile the Wasm benchmark module.
ExitCode wasm_bench_compile(void *state, const char *wasm_bytes, size_t wasm_bytes_length)
{
  auto bench = static_cast<BenchState*>(state);
  JSContext* cx = bench->js->cx;
  JSAutoRealm ar(cx, bench->js->global);

  // Construct Wasm module from bytes.
  JSObject* arrayBuffer = JS::NewArrayBufferWithUserOwnedContents(cx,
    wasm_bytes_length, (void*)wasm_bytes);
  if (!arrayBuffer) return BENCH_EXIT_ERR;
  JS::RootedValueArray<1> args(cx);
  args[0].setObject(*arrayBuffer);

  JS::RootedObject wasm(cx, GetWasm(cx, bench->js->global));
  JS::RootedValue wasmModule(cx);
  if (!JS_GetProperty(cx, wasm, "Module", &wasmModule)) return BENCH_EXIT_ERR;

  bench->compilation_start(bench->compilation_timer);

  JS::RootedObject module_(cx);
  if (!Construct(cx, wasmModule, args, &module_)) {
    ReportAndClearException(cx);
    return BENCH_EXIT_ERR;
  }

  bench->compilation_end(bench->compilation_timer);

  bench->js->module = module_;

  return BENCH_EXIT_OK;
}

static bool BenchStart(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  BenchState* bench = JS::GetMaybePtrFromReservedSlot<BenchState>(global, 0);
  bench->execution_start(bench->execution_timer);
  return true;
}

static bool BenchEnd(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  BenchState* bench = JS::GetMaybePtrFromReservedSlot<BenchState>(global, 0);
  bench->execution_end(bench->execution_timer);
  return true;
}

static JSObject* BuildImports(JSContext *cx)
{
  // Construct Wasm module instance with required imports.
  // Build "bench" imports object.
  JS::RootedObject benchImportObj(cx, JS_NewPlainObject(cx));
  if (!benchImportObj) return nullptr;
  if (!JS_DefineFunction(cx, benchImportObj, "start", BenchStart, 0, 0)) return nullptr;
  if (!JS_DefineFunction(cx, benchImportObj, "end", BenchEnd, 0, 0)) return nullptr;
  JS::RootedValue benchImport(cx, JS::ObjectValue(*benchImportObj));
  // Build wasi imports object.
  JS::RootedObject wasiImportObj(cx, BuildWasiImports(cx));
  JS::RootedValue wasiImport(cx, JS::ObjectValue(*wasiImportObj));
  // Build imports bag.
  JS::RootedObject imports(cx, JS_NewPlainObject(cx));
  if (!imports) return nullptr;
  if (!JS_SetProperty(cx, imports, "bench", benchImport)) return nullptr;
  if (!JS_SetProperty(cx, imports, "wasi_snapshot_preview1", wasiImport)) return nullptr;
  return imports;
}

/// Instantiate the Wasm benchmark module.
ExitCode wasm_bench_instantiate(void *state)
{
  auto bench = static_cast<BenchState*>(state);
  JSContext* cx = bench->js->cx;
  JSAutoRealm ar(cx, bench->js->global);

  JS::RootedObject imports(cx, BuildImports(cx));
  if (!imports) return BENCH_EXIT_ERR;

  JS::RootedValueArray<2> args(cx);
  args[0].setObject(*bench->js->module.get()); // module
  args[1].setObject(*imports.get());// imports

  JS::RootedObject wasm(cx, GetWasm(cx, bench->js->global));
  JS::RootedValue wasmInstance(cx);
  if (!JS_GetProperty(cx, wasm, "Instance", &wasmInstance)) return BENCH_EXIT_ERR;

  bench->instantiation_start(bench->instantiation_timer);

  JS::RootedObject instance_(cx);
  if (!Construct(cx, wasmInstance, args, &instance_)) {
    ReportAndClearException(cx);
    return BENCH_EXIT_ERR;
  }

  bench->instantiation_end(bench->instantiation_timer);

  JS::RootedValue exports(cx);
  if (!JS_GetProperty(cx, instance_, "exports", &exports)) return false;
  JS::RootedObject exportsObj(cx, &exports.toObject());
  JS::RootedValue memory(cx);
  if (!JS_GetProperty(cx, exportsObj, "memory", &memory)) return false;
  if (!JS_SetProperty(cx, bench->js->global, "memory", memory)) return false;

  bench->js->instance = instance_;

  return BENCH_EXIT_OK;
}

/// Execute the Wasm benchmark module.
ExitCode wasm_bench_execute(void *state)
{
  auto bench = static_cast<BenchState*>(state);
  JSContext* cx = bench->js->cx;
  JSAutoRealm ar(cx, bench->js->global);

  // Find `_start` method in exports.
  JS::RootedValue exports(cx);
  if (!JS_GetProperty(cx, bench->js->instance, "exports", &exports)) return BENCH_EXIT_ERR;
  JS::RootedObject exportsObj(cx, &exports.toObject());
  JS::RootedValue start(cx);
  if (!JS_GetProperty(cx, exportsObj, "_start", &start)) return BENCH_EXIT_ERR;

  // BenchResult::Start/End are called from wasm

  JS::RootedValue rval(cx);
  if (!Call(cx, JS::UndefinedHandleValue, start, JS::HandleValueArray::empty(), &rval)) {
    JS::RootedValue exc(cx);
    if (!JS_GetPendingException(cx, &exc)) return BENCH_EXIT_ERR;
    JS_ClearPendingException(cx);
    // likely wasm procexit
    // ReportAndClearException(cx);
  }
  return BENCH_EXIT_OK;
}


void bench_init() {
  if (!JS_Init()) {
    fprintf(stderr, "JS engine is not initialized\n");
    exit(1);
  }
}

void bench_fini() {
  JS_ShutDown();
}
