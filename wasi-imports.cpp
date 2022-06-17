#include <jsfriendapi.h>

#include <js/Initialization.h>
#include <js/Exception.h>
#include <js/Context.h>
#include <js/CompilationAndEvaluation.h>
#include <js/SourceText.h>
#include <js/WasmModule.h>
#include <js/ArrayBuffer.h>
#include <js/ArrayBufferMaybeShared.h>

#include <fstream>
#include <mutex>

#include "wasi-imports.h"
#include "bench-state.h"

static bool GetWasmMemory(JSContext* cx, JS::HandleObject global, uint8_t **data_out, size_t *len_out)
{
  JS::RootedValue memory(cx);
  if (!JS_GetProperty(cx, global, "memory", &memory)) return false;
  JS::RootedObject memoryObj(cx, &memory.toObject());
  JS::RootedValue buffer(cx);
  if (!JS_GetProperty(cx, memoryObj, "buffer", &buffer)) return false;
  uint8_t *data; bool shared; size_t length;
  JS::GetArrayBufferMaybeSharedLengthAndData(&buffer.toObject(), &length, &shared, &data);
  *data_out = data;
  *len_out = length;
  return true;
}

bool WasiFdClose(JSContext* cx, unsigned argc, JS::Value* vp)
{
    fprintf(stderr, "-----WasiFdClose\n");
    return false;
}
bool WasiFdFilestatGet(JSContext* cx, unsigned argc, JS::Value* vp)
{
  fprintf(stderr, "-----WasiFdFilestatGet\n");
  return false;
}
bool WasiFdSeek(JSContext* cx, unsigned argc, JS::Value* vp)
{
  fprintf(stderr, "-----WasiFdSeek\n");
  return false;
}
bool WasiFdRead(JSContext* cx, unsigned argc, JS::Value* vp)
{
  fprintf(stderr, "-----WasiFdRead\n");
  return false;
}
bool WasiFdWrite(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  BenchState* state = JS::GetMaybePtrFromReservedSlot<BenchState>(global, 0);
  uint8_t *data; size_t length;
  if (!GetWasmMemory(cx, global, &data, &length)) return false;

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int fd = args.get(0).toInt32();
  int ptr = args.get(1).toInt32();
  int len = args.get(2).toInt32();
  int written_ptr = args.get(3).toInt32();

  std::ofstream* out = nullptr;
  switch (fd) {
    case 1: out = &state->stdout; break;
    case 2: out = &state->stderr; break;
    default: return false;
  }

  uint32_t* iocv = (uint32_t*)(data + ptr);
  int total = 0;
  for (int i = 0; i < len; i++) {
      const char* p = (const char*)(data + iocv[i * 2]);
      uint32_t l = iocv[i * 2 + 1];
      out->write(p, (size_t)l);
      total += l;
  }
  *(uint32_t*)(data + written_ptr) = total;

  args.rval().setInt32(0);
  return true;
}
bool WasiPathOpen(JSContext* cx, unsigned argc, JS::Value* vp)
{
  fprintf(stderr, "-----WasiPathOpen\n");
  return false;
}
bool WasiFdPrestatGet(JSContext* cx, unsigned argc, JS::Value* vp)
{
  fprintf(stderr, "-----WasiFdPrestatGet\n");
  return false;
}
bool WasiFdPrestatDirName(JSContext* cx, unsigned argc, JS::Value* vp)
{
  fprintf(stderr, "-----WasiFdPrestatDirName\n");
  return false;
}
bool WasiProcExit(JSContext* cx, unsigned argc, JS::Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS_ReportErrorUTF8(cx, "procexit(%d)", args.get(0).toInt32());
    return false;
}
bool WasiEnvironSizesGet(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  uint8_t *data; size_t length;
  if (!GetWasmMemory(cx, global, &data, &length)) return false;

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int count_ptr = args.get(0).toInt32();
  int size_ptr = args.get(1).toInt32();
  *(uint32_t*)(data + count_ptr) = 0;
  *(uint32_t*)(data + size_ptr) = 0;

  args.rval().setInt32(0);
  return true;
}
bool WasiEnvironGet(JSContext* cx, unsigned argc, JS::Value* vp)
{
  fprintf(stderr, "-----WasiEnvironGet\n");
  return false;
}

JSObject* BuildWasiImports(JSContext *cx)
{
  JS::RootedObject wasiImportObj(cx, JS_NewPlainObject(cx));
  if (!wasiImportObj) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "fd_close", WasiFdClose, 1, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "fd_filestat_get", WasiFdFilestatGet, 2, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "fd_seek", WasiFdSeek, 4, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "fd_read", WasiFdRead, 4, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "fd_write", WasiFdWrite, 4, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "path_open", WasiPathOpen, 9, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "fd_prestat_get", WasiFdPrestatGet, 2, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "fd_prestat_dir_name", WasiFdPrestatDirName, 3, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "proc_exit", WasiProcExit, 1, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "environ_sizes_get", WasiEnvironSizesGet, 2, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "environ_get", WasiEnvironGet, 2, 0)) return nullptr;
  return wasiImportObj;
}